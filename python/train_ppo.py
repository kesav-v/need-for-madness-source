#!/usr/bin/env python3
"""Small solo PPO for NFM gym (nplayers=1, no AI cars).

By default warm-starts from behavioral cloning on the game's Control::preform
AI (``--bc-demos``), then PPO fine-tunes. Dumps a .nfmst every 100 episodes.

Rollouts: CPU policy + ``--num-envs`` parallel sims. PPO updates use ``--device``.
"""
from __future__ import annotations

import argparse
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import List, Tuple

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.distributions import Bernoulli

from nfm_gym import NfmEnv

REPO = Path(__file__).resolve().parents[1]


class ActorCritic(nn.Module):
    def __init__(self, obs_dim: int = 52, act_dim: int = 5, hidden: int = 512):
        super().__init__()
        self.obs_dim = obs_dim
        self.hidden = hidden
        self.body = nn.Sequential(
            nn.Linear(obs_dim, hidden),
            nn.Tanh(),
            nn.Linear(hidden, hidden),
            nn.Tanh(),
            nn.Linear(hidden, hidden),
            nn.Tanh(),
            nn.Linear(hidden, hidden),
            nn.Tanh(),
        )
        self.policy = nn.Linear(hidden, act_dim)
        self.value = nn.Linear(hidden, 1)

    def forward(self, obs: torch.Tensor):
        h = self.body(obs)
        return self.policy(h), self.value(h).squeeze(-1)


def load_policy_expand(
    path: Path | str,
    *,
    obs_dim: int = 52,
    hidden: int = 512,
) -> ActorCritic:
    """Load checkpoint; zero-pad first-layer inputs if obs grew; fresh net if hidden differs."""
    ckpt = torch.load(path, map_location="cpu", weights_only=True)
    state = ckpt["model"] if isinstance(ckpt, dict) and "model" in ckpt else ckpt
    old_w = state["body.0.weight"]
    old_dim = int(old_w.shape[1])
    old_hidden = int(old_w.shape[0])
    net = ActorCritic(obs_dim=obs_dim, hidden=hidden)
    if old_hidden != hidden:
        print(
            f"[net] hidden {old_hidden}->{hidden}; starting fresh body "
            f"(obs target {obs_dim})",
            flush=True,
        )
        return net
    if old_dim == obs_dim:
        net.load_state_dict(state)
        return net
    if old_dim > obs_dim:
        raise ValueError(f"checkpoint obs_dim {old_dim} > target {obs_dim}")
    new_state = net.state_dict()
    for k, v in state.items():
        if k == "body.0.weight":
            new_state[k].zero_()
            new_state[k][:, :old_dim] = v
        elif k in new_state and new_state[k].shape == v.shape:
            new_state[k] = v
    net.load_state_dict(new_state)
    print(f"[net] expanded obs {old_dim}->{obs_dim} from {path}", flush=True)
    return net


def discounted_returns(rewards, dones, gamma: float) -> np.ndarray:
    out = np.zeros_like(rewards, dtype=np.float32)
    g = 0.0
    for t in reversed(range(len(rewards))):
        if dones[t]:
            g = 0.0
        g = rewards[t] + gamma * g
        out[t] = g
    return out


class VecNfm:
    """N parallel solo NfmEnv instances; steps run concurrently (ctypes releases GIL)."""

    def __init__(self, n: int, **env_kwargs):
        self.n = int(n)
        self.envs = [NfmEnv(**env_kwargs) for _ in range(self.n)]
        self._pool = ThreadPoolExecutor(max_workers=self.n)
        self.obs = np.zeros((self.n, 52), dtype=np.float32)
        self.ep_ret = np.zeros(self.n, dtype=np.float64)
        self.ep_len = np.zeros(self.n, dtype=np.int32)

    def close(self) -> None:
        self._pool.shutdown(wait=False)
        for e in self.envs:
            e.close()

    def reset_one(self, i: int, seed: int) -> np.ndarray:
        o, _ = self.envs[i].reset(seed=seed)
        self.obs[i] = o
        self.ep_ret[i] = 0.0
        self.ep_len[i] = 0
        return o

    def reset_all(self, seeds: List[int]) -> np.ndarray:
        assert len(seeds) == self.n
        for i, s in enumerate(seeds):
            self.reset_one(i, s)
        return self.obs.copy()

    def step(self, actions: np.ndarray):
        """actions: int8/float [N, 5] -> next_obs, rewards, dones, infos"""

        def _one(i: int):
            return self.envs[i].step(actions[i].astype(np.int8))

        results = list(self._pool.map(_one, range(self.n)))
        next_obs = np.zeros_like(self.obs)
        rewards = np.zeros(self.n, dtype=np.float32)
        dones = np.zeros(self.n, dtype=np.bool_)
        infos = []
        for i, (o, r, term, trunc, info) in enumerate(results):
            next_obs[i] = o
            rewards[i] = float(r)
            dones[i] = bool(term or trunc)
            infos.append(info)
            self.ep_ret[i] += float(r)
            self.ep_len[i] += 1
        self.obs = next_obs
        return next_obs, rewards, dones, infos


@torch.inference_mode()
def act_batch(net: ActorCritic, obs: np.ndarray, *, deterministic: bool = False):
    """CPU batched policy. Returns actions float32[N,5], logp[N], values[N]."""
    o = torch.as_tensor(obs, dtype=torch.float32)  # CPU
    logits, values = net(o)
    dist = Bernoulli(logits=logits)
    if deterministic:
        actions = (torch.sigmoid(logits) > 0.5).float()
    else:
        actions = dist.sample()
    logp = dist.log_prob(actions).sum(-1)
    return (
        actions.numpy(),
        logp.numpy().astype(np.float32),
        values.numpy().astype(np.float32),
    )


def run_episode_cpu(env: NfmEnv, net: ActorCritic, *, deterministic=False,
                    record_path=None, seed=None):
    """Single-env rollout on CPU (dumps / eval)."""
    net_cpu = net if next(net.parameters()).device.type == "cpu" else net.cpu()
    opts: dict = {"stage": env.stage, "car": env.car, "nplayers": env.nplayers,
                  "max_steps": env.max_steps}
    if record_path:
        opts["record_path"] = record_path
    obs, info = env.reset(seed=seed, options=opts)
    obs_buf, act_buf, logp_buf, rew_buf, val_buf, done_buf = (
        [],
        [],
        [],
        [],
        [],
        [],
    )
    ep_ret = 0.0
    max_clear = 0.0
    while True:
        actions, logp, values = act_batch(
            net_cpu, obs[None, :], deterministic=deterministic
        )
        a = actions[0].astype(np.int8)
        next_obs, reward, term, trunc, info = env.step(a)
        done = bool(term or trunc)
        obs_buf.append(obs)
        act_buf.append(actions[0].astype(np.float32))
        logp_buf.append(float(logp[0]))
        rew_buf.append(float(reward))
        val_buf.append(float(values[0]))
        done_buf.append(done)
        ep_ret += float(reward)
        max_clear = max(max_clear, float(info.get("clear", 0)))
        obs = next_obs
        if done:
            break
    if record_path:
        env.finish_recording()
    return {
        "obs": np.asarray(obs_buf, dtype=np.float32),
        "actions": np.asarray(act_buf, dtype=np.float32),
        "logp": np.asarray(logp_buf, dtype=np.float32),
        "rewards": np.asarray(rew_buf, dtype=np.float32),
        "values": np.asarray(val_buf, dtype=np.float32),
        "dones": np.asarray(done_buf, dtype=np.bool_),
        "return": ep_ret,
        "frames": int(info.get("frame", len(rew_buf))),
        "clear": max_clear,
        "dest": float(obs[14]),
    }


def resolve_train_device(name: str) -> torch.device:
    if name == "auto":
        if torch.backends.mps.is_available():
            return torch.device("mps")
        if torch.cuda.is_available():
            return torch.device("cuda")
        return torch.device("cpu")
    return torch.device(name)


def collect_ai_demos(
    env: NfmEnv, n_demos: int, seed0: int, *, record_first: Path | None = None
) -> Tuple[np.ndarray, np.ndarray, List[dict]]:
    """Roll out Control::preform; return obs[N,52], actions[N,5], episode stats."""
    obs_list: List[np.ndarray] = []
    act_list: List[np.ndarray] = []
    stats: List[dict] = []
    for d in range(n_demos):
        rec = None
        if record_first is not None and d == 0:
            rec = str(record_first)
        obs, _ = env.reset(seed=seed0 + d, options={"record_path": rec} if rec else {})
        ep_ret = 0.0
        frames = 0
        max_clear = 0
        while True:
            next_obs, reward, term, trunc, info = env.step_ai()
            obs_list.append(obs.copy())
            act_list.append(info["action"].astype(np.float32))
            ep_ret += float(reward)
            frames += 1
            max_clear = max(max_clear, int(info.get("clear", 0)))
            obs = next_obs
            if term or trunc:
                break
        if rec:
            env.finish_recording()
        stats.append(
            {
                "return": ep_ret,
                "frames": frames,
                "clear": max_clear,
                "end_clear": int(info.get("clear", max_clear)),
            }
        )
        print(
            f"[bc] demo {d+1}/{n_demos} frames={frames} clear={max_clear} "
            f"ret={ep_ret:.1f}",
            flush=True,
        )
    return (
        np.asarray(obs_list, dtype=np.float32),
        np.asarray(act_list, dtype=np.float32),
        stats,
    )


def behavioral_clone(
    net: ActorCritic,
    obs: np.ndarray,
    actions: np.ndarray,
    device: torch.device,
    *,
    epochs: int = 15,
    batch_size: int = 512,
    lr: float = 1e-3,
    clear_weight: float = 0.0,
) -> dict:
    """BCE on action logits.

    If clear_weight > 0, upsample late-race frames (obs[:,14] = clear) so
    checkpoints 3–6 are not drowned by the long approach to CP1.
    """
    net.to(device)
    opt = torch.optim.Adam(net.parameters(), lr=lr)
    x = torch.as_tensor(obs, device=device)
    y = torch.as_tensor(actions, device=device)
    n = x.shape[0]
    if clear_weight > 0:
        clears = torch.as_tensor(obs[:, 14], device=device)
        w = 1.0 + clear_weight * clears
        w = w / w.mean()
    else:
        w = None
    last = {}
    for ep in range(epochs):
        if w is not None:
            # Sample with replacement proportional to late-race weight.
            perm = torch.multinomial(w, n, replacement=True)
        else:
            perm = torch.randperm(n, device=device)
        total_loss = 0.0
        n_batches = 0
        for start in range(0, n, batch_size):
            mb = perm[start : start + batch_size]
            logits, _ = net(x[mb])
            loss = F.binary_cross_entropy_with_logits(logits, y[mb])
            opt.zero_grad()
            loss.backward()
            opt.step()
            total_loss += float(loss.item())
            n_batches += 1
        last = {"bc_loss": total_loss / max(n_batches, 1), "epoch": ep + 1}
        if (ep + 1) % 5 == 0 or ep == 0:
            print(
                f"[bc] epoch {ep+1}/{epochs} loss={last['bc_loss']:.4f}",
                flush=True,
            )
    net.to("cpu")
    return last


def _load_best_bc(net: ActorCritic, out_dir: Path) -> None:
    path = out_dir / "policy_bc.pt"
    if path.exists():
        ckpt = torch.load(path, map_location="cpu", weights_only=True)
        net.load_state_dict(ckpt["model"])


def eval_clear(
    env: NfmEnv, net: ActorCritic, seed: int, *, record_path: str | None = None
) -> dict:
    """Deterministic rollout with stall-cut disabled (need long routes for CP2+)."""
    env.set_stall_cut(False)
    dump = run_episode_cpu(
        env, net, deterministic=True, record_path=record_path, seed=seed
    )
    env.set_stall_cut(True)
    return dump


def eval_clear_suite(
    env: NfmEnv,
    net: ActorCritic,
    seeds: List[int],
    *,
    record_path: str | None = None,
) -> dict:
    """Eval on several seeds; returns best clear (and that run's dump)."""
    best = {"clear": -1.0}
    for i, seed in enumerate(seeds):
        rec = record_path if (record_path and i == 0) else None
        dump = eval_clear(env, net, seed, record_path=rec)
        if dump["clear"] > best["clear"]:
            best = {**dump, "seed": seed}
    return best


def dagger_aggregate(
    env: NfmEnv,
    net: ActorCritic,
    *,
    n_eps: int,
    seed0: int,
    beta: float,
    max_steps: int,
) -> Tuple[np.ndarray, np.ndarray]:
    """Roll out mix of expert/student; label every state with query_ai()."""
    obs_list: List[np.ndarray] = []
    act_list: List[np.ndarray] = []
    env.set_stall_cut(False)
    env.max_steps = max_steps
    for d in range(n_eps):
        obs, _ = env.reset(seed=seed0 + d)
        while True:
            a_exp = env.query_ai().astype(np.float32)
            obs_list.append(obs.copy())
            act_list.append(a_exp)
            if np.random.random() < beta:
                a = a_exp.astype(np.int8)
            else:
                a, _, _ = act_batch(net, obs[None, :], deterministic=False)
                a = a[0].astype(np.int8)
            obs, _, term, trunc, _ = env.step(a)
            if term or trunc:
                break
    env.set_stall_cut(True)
    return (
        np.asarray(obs_list, dtype=np.float32),
        np.asarray(act_list, dtype=np.float32),
    )


def bc_until_clear(
    net: ActorCritic,
    env: NfmEnv,
    demo_obs: np.ndarray,
    demo_act: np.ndarray,
    device: torch.device,
    *,
    target_clear: int,
    max_epochs: int,
    eval_every: int,
    eval_seed: int,
    out_dir: Path,
    lr: float = 1e-3,
    dagger_rounds: int = 5,
    dagger_eps: int = 10,
) -> Tuple[dict, np.ndarray, np.ndarray]:
    """BC + DAgger until deterministic eval clear >= target."""
    best: dict = {"clear": -1.0, "epoch": 0}
    total_epochs = 0
    eval_seeds = [eval_seed + i for i in range(5)]
    pre = min(25, max_epochs)
    behavioral_clone(
        net, demo_obs, demo_act, device, epochs=pre, lr=lr, clear_weight=1.5
    )
    total_epochs += pre
    dump = eval_clear_suite(
        env,
        net,
        eval_seeds,
        record_path=str(out_dir / "game_bc_eval_e000.nfmst"),
    )
    print(
        f"[bc] after warm-start clear={dump['clear']:.0f} frames={dump['frames']} "
        f"seed={dump.get('seed')}",
        flush=True,
    )
    best = {
        "clear": float(dump["clear"]),
        "epoch": total_epochs,
        "frames": dump["frames"],
        "return": dump["return"],
    }
    torch.save(
        {"model": net.state_dict(), "bc": True, "clear": best["clear"]},
        out_dir / "policy_bc.pt",
    )

    for rnd in range(dagger_rounds):
        if best["clear"] >= target_clear or total_epochs >= max_epochs:
            break
        # Always DAgger / fine-tune from the best policy so we don't dig from a hole.
        _load_best_bc(net, out_dir)
        beta = max(0.15, 0.85 - 0.12 * rnd)
        round_lr = lr * (0.7 ** rnd)
        print(
            f"[dagger] round {rnd+1}/{dagger_rounds} beta={beta:.2f} "
            f"eps={dagger_eps} lr={round_lr:.2e} best_clear={best['clear']:.0f}",
            flush=True,
        )
        new_o, new_a = dagger_aggregate(
            env,
            net,
            n_eps=dagger_eps,
            seed0=50_000 + rnd * 100,
            beta=beta,
            max_steps=env.max_steps,
        )
        demo_obs = np.concatenate([demo_obs, new_o], axis=0)
        demo_act = np.concatenate([demo_act, new_a], axis=0)
        print(f"[dagger] dataset size={len(demo_obs)} (+{len(new_o)})", flush=True)

        rounds_budget = max(
            eval_every, (max_epochs - total_epochs) // max(dagger_rounds - rnd, 1)
        )
        done = 0
        while done < rounds_budget and total_epochs < max_epochs:
            n = min(eval_every, rounds_budget - done, max_epochs - total_epochs)
            if n <= 0:
                break
            behavioral_clone(
                net,
                demo_obs,
                demo_act,
                device,
                epochs=n,
                lr=round_lr,
                clear_weight=2.0,
            )
            done += n
            total_epochs += n
            path = out_dir / f"game_bc_eval_e{total_epochs:03d}.nfmst"
            dump = eval_clear_suite(env, net, eval_seeds, record_path=str(path))
            print(
                f"[bc] eval @{total_epochs} clear={dump['clear']:.0f} "
                f"frames={dump['frames']} ret={dump['return']:.1f} "
                f"seed={dump.get('seed')} -> {path.name}",
                flush=True,
            )
            if dump["clear"] > best["clear"]:
                best = {
                    "clear": float(dump["clear"]),
                    "epoch": total_epochs,
                    "frames": dump["frames"],
                    "return": dump["return"],
                }
                torch.save(
                    {
                        "model": net.state_dict(),
                        "bc": True,
                        "clear": best["clear"],
                    },
                    out_dir / "policy_bc.pt",
                )
            elif dump["clear"] < best["clear"]:
                # Regressed — snap back before next clone block.
                _load_best_bc(net, out_dir)
            if dump["clear"] >= target_clear:
                print(
                    f"[bc] hit target clear>={target_clear} at epoch {total_epochs}",
                    flush=True,
                )
                _load_best_bc(net, out_dir)
                return best, demo_obs, demo_act

    _load_best_bc(net, out_dir)
    print(
        f"[bc] finished; best clear={best['clear']:.0f} epochs={total_epochs}",
        flush=True,
    )
    return best, demo_obs, demo_act


def ppo_update(net, opt, batch, device, clip=0.2, epochs=4, batch_size=256,
               vf_coef=0.5, ent_coef=0.01, max_grad_norm=0.5,
               demo_obs=None, demo_act=None, bc_coef=0.0,
               ref_net=None, kl_coef=0.0):
    obs = torch.as_tensor(batch["obs"], device=device)
    actions = torch.as_tensor(batch["actions"], device=device)
    old_logp = torch.as_tensor(batch["logp"], device=device)
    returns = torch.as_tensor(batch["returns"], device=device)
    advantages = torch.as_tensor(batch["advantages"], device=device)
    advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

    demo_x = demo_y = None
    if bc_coef > 0 and demo_obs is not None and demo_act is not None and len(demo_obs) > 0:
        demo_x = torch.as_tensor(demo_obs, device=device)
        demo_y = torch.as_tensor(demo_act, device=device)

    n = obs.shape[0]
    idx = np.arange(n)
    last = {}
    for _ in range(epochs):
        np.random.shuffle(idx)
        for start in range(0, n, batch_size):
            mb = idx[start : start + batch_size]
            logits, values = net(obs[mb])
            dist = Bernoulli(logits=logits)
            logp = dist.log_prob(actions[mb]).sum(-1)
            ratio = torch.exp(logp - old_logp[mb])
            adv = advantages[mb]
            surr1 = ratio * adv
            surr2 = torch.clamp(ratio, 1.0 - clip, 1.0 + clip) * adv
            policy_loss = -torch.min(surr1, surr2).mean()
            value_loss = ((values - returns[mb]) ** 2).mean()
            entropy = dist.entropy().sum(-1).mean()
            loss = policy_loss + vf_coef * value_loss - ent_coef * entropy
            bc_loss = None
            if demo_x is not None:
                di = torch.randint(0, demo_x.shape[0], (min(batch_size, demo_x.shape[0]),),
                                   device=device)
                d_logits, _ = net(demo_x[di])
                bc_loss = F.binary_cross_entropy_with_logits(d_logits, demo_y[di])
                loss = loss + bc_coef * bc_loss
            kl_loss = None
            if ref_net is not None and kl_coef > 0:
                with torch.no_grad():
                    ref_logits, _ = ref_net(obs[mb])
                p = torch.sigmoid(logits).clamp(1e-6, 1 - 1e-6)
                q = torch.sigmoid(ref_logits).clamp(1e-6, 1 - 1e-6)
                kl_loss = (
                    p * (torch.log(p) - torch.log(q))
                    + (1 - p) * (torch.log(1 - p) - torch.log(1 - q))
                ).sum(-1).mean()
                loss = loss + kl_coef * kl_loss
            opt.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(net.parameters(), max_grad_norm)
            opt.step()
            last = {
                "policy_loss": float(policy_loss.item()),
                "value_loss": float(value_loss.item()),
                "entropy": float(entropy.item()),
            }
            if bc_loss is not None:
                last["bc_loss"] = float(bc_loss.item())
            if kl_loss is not None:
                last["kl"] = float(kl_loss.item())
    return last


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--games", type=int, default=500, help="training episodes")
    ap.add_argument("--stage", type=int, default=11)
    ap.add_argument("--car", type=int, default=0)
    ap.add_argument("--max-steps", type=int, default=2500,
                    help="episode length (stage 11 needs >>800 for 2+ checkpoints)")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--lr", type=float, default=1e-4,
                    help="PPO lr (keep low after BC so clone is not erased)")
    ap.add_argument("--gamma", type=float, default=0.99)
    ap.add_argument("--update-every", type=int, default=8,
                    help="episodes per PPO update")
    ap.add_argument("--num-envs", type=int, default=32,
                    help="parallel sims for rollouts")
    ap.add_argument("--dump-every", type=int, default=100)
    ap.add_argument("--out-dir", type=Path,
                    default=REPO / "OBJ" / "ppo_runs")
    ap.add_argument(
        "--device",
        default="auto",
        help="train device: cpu | mps | cuda | auto (rollouts always CPU)",
    )
    ap.add_argument(
        "--bc-demos",
        type=int,
        default=40,
        help="AI (Control::preform) episodes for BC warm-start (0=skip)",
    )
    ap.add_argument("--bc-epochs", type=int, default=80,
                    help="max BC epochs (stops early on --bc-target-clear)")
    ap.add_argument("--bc-target-clear", type=int, default=2,
                    help="stop BC when deterministic eval reaches this clear")
    ap.add_argument("--bc-eval-every", type=int, default=10)
    ap.add_argument("--bc-coef", type=float, default=0.5,
                    help="BC loss weight mixed into each PPO update (0=off)")
    ap.add_argument("--bc-only", action="store_true",
                    help="stop after BC (no PPO fine-tune)")
    ap.add_argument("--load", type=Path, default=None,
                    help="optional policy .pt to load before BC/PPO")
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    train_device = resolve_train_device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    net = ActorCritic()  # CPU — rollouts always use this copy
    if args.load and args.load.exists():
        ckpt = torch.load(args.load, map_location="cpu", weights_only=True)
        net.load_state_dict(ckpt["model"] if isinstance(ckpt, dict) and "model" in ckpt else ckpt)
        print(f"[ppo] loaded {args.load}", flush=True)

    dump_env = NfmEnv(
        stage=args.stage,
        car=args.car,
        nplayers=1,
        max_steps=max(args.max_steps, 4000),
    )

    demo_obs = demo_act = None
    if args.bc_demos > 0:
        demo_path = args.out_dir / "bc_ai_demo.nfmst"
        print(
            f"[bc] collecting {args.bc_demos} AI demos "
            f"(max_steps={dump_env.max_steps})…",
            flush=True,
        )
        demo_obs, demo_act, demo_stats = collect_ai_demos(
            dump_env,
            args.bc_demos,
            args.seed + 10_000,
            record_first=demo_path,
        )
        clears = [s["clear"] for s in demo_stats]
        print(
            f"[bc] demos frames={len(demo_obs)} clear "
            f"min/avg/max={min(clears)}/{np.mean(clears):.1f}/{max(clears)} "
            f"saved {demo_path.name}",
            flush=True,
        )
        _best, demo_obs, demo_act = bc_until_clear(
            net,
            dump_env,
            demo_obs,
            demo_act,
            train_device,
            target_clear=args.bc_target_clear,
            max_epochs=args.bc_epochs,
            eval_every=args.bc_eval_every,
            eval_seed=99_001,
            out_dir=args.out_dir,
        )
        eval_path = args.out_dir / "game_bc_eval.nfmst"
        dump = eval_clear(dump_env, net, 99_001, record_path=str(eval_path))
        print(
            f"[bc] final eval {eval_path.name} clear={dump['clear']:.0f} "
            f"frames={dump['frames']} ret={dump['return']:.2f}",
            flush=True,
        )
        if args.bc_only:
            dump_env.close()
            print("[bc] done (--bc-only)", flush=True)
            return
        dump_env.max_steps = args.max_steps

    vec = VecNfm(
        args.num_envs,
        stage=args.stage,
        car=args.car,
        nplayers=1,
        max_steps=args.max_steps,
    )

    seed_counter = args.seed + 1
    vec.reset_all([seed_counter + i for i in range(args.num_envs)])
    seed_counter += args.num_envs

    # Per-env trajectory buffers (list of steps)
    traj_obs: List[List[np.ndarray]] = [[] for _ in range(args.num_envs)]
    traj_act: List[List[np.ndarray]] = [[] for _ in range(args.num_envs)]
    traj_logp: List[List[float]] = [[] for _ in range(args.num_envs)]
    traj_rew: List[List[float]] = [[] for _ in range(args.num_envs)]
    traj_val: List[List[float]] = [[] for _ in range(args.num_envs)]
    traj_done: List[List[bool]] = [[] for _ in range(args.num_envs)]

    print(
        f"[ppo] solo stage={args.stage} car={args.car} games={args.games} "
        f"max_steps={args.max_steps} num_envs={args.num_envs} "
        f"dump_every={args.dump_every} rollout=cpu train={train_device} "
        f"lr={args.lr} bc_coef={args.bc_coef}",
        flush=True,
    )

    buf_eps = []
    rets = []
    game = 0
    t0 = time.time()

    # CPU net for rollouts; train_device clone for PPO updates (synced each update).
    train_net = ActorCritic().to(train_device)
    train_net.load_state_dict(net.state_dict())
    train_opt = torch.optim.Adam(train_net.parameters(), lr=args.lr)

    def do_update():
        if not buf_eps:
            return {}
        obs = np.concatenate([e["obs"] for e in buf_eps])
        actions = np.concatenate([e["actions"] for e in buf_eps])
        logp = np.concatenate([e["logp"] for e in buf_eps])
        returns_list, adv_list = [], []
        for e in buf_eps:
            R = discounted_returns(e["rewards"], e["dones"], args.gamma)
            returns_list.append(R)
            adv_list.append(R - e["values"])
        batch = {
            "obs": obs,
            "actions": actions,
            "logp": logp,
            "returns": np.concatenate(returns_list),
            "advantages": np.concatenate(adv_list),
        }
        train_net.load_state_dict(net.state_dict())
        stats = ppo_update(
            train_net,
            train_opt,
            batch,
            train_device,
            demo_obs=demo_obs,
            demo_act=demo_act,
            bc_coef=args.bc_coef if demo_obs is not None else 0.0,
        )
        net.load_state_dict(
            {k: v.detach().cpu() for k, v in train_net.state_dict().items()}
        )
        buf_eps.clear()
        return stats

    while game < args.games:
        actions, logps, values = act_batch(net, vec.obs, deterministic=False)
        # store obs before step
        for i in range(vec.n):
            traj_obs[i].append(vec.obs[i].copy())
            traj_act[i].append(actions[i].astype(np.float32))
            traj_logp[i].append(float(logps[i]))
            traj_val[i].append(float(values[i]))

        next_obs, rewards, dones, infos = vec.step(actions)

        finished_this_step = []
        for i in range(vec.n):
            traj_rew[i].append(float(rewards[i]))
            traj_done[i].append(bool(dones[i]))
            if not dones[i]:
                continue
            ep = {
                "obs": np.asarray(traj_obs[i], dtype=np.float32),
                "actions": np.asarray(traj_act[i], dtype=np.float32),
                "logp": np.asarray(traj_logp[i], dtype=np.float32),
                "rewards": np.asarray(traj_rew[i], dtype=np.float32),
                "values": np.asarray(traj_val[i], dtype=np.float32),
                "dones": np.asarray(traj_done[i], dtype=np.bool_),
                "return": float(vec.ep_ret[i]),
                "frames": int(infos[i].get("frame", vec.ep_len[i])),
                "clear": float(next_obs[i][14]),
                "dest": float(next_obs[i][15]),
            }
            traj_obs[i].clear()
            traj_act[i].clear()
            traj_logp[i].clear()
            traj_rew[i].clear()
            traj_val[i].clear()
            traj_done[i].clear()

            game += 1
            rets.append(ep["return"])
            buf_eps.append(ep)
            last_ep = ep
            finished_this_step.append(ep)

            vec.reset_one(i, seed_counter)
            seed_counter += 1

            if game % args.update_every == 0:
                stats = do_update()
                avg = float(np.mean(rets[-args.update_every :]))
                print(
                    f"[ppo] game={game}/{args.games} ret_avg={avg:.2f} "
                    f"last_ret={ep['return']:.2f} clear={ep['clear']:.0f} "
                    f"frames={ep['frames']} "
                    f"pi={stats.get('policy_loss', 0):.3f} "
                    f"v={stats.get('value_loss', 0):.3f} "
                    f"H={stats.get('entropy', 0):.3f}"
                    + (
                        f" bc={stats.get('bc_loss', 0):.3f}"
                        if "bc_loss" in stats
                        else ""
                    ),
                    flush=True,
                )

            if game % args.dump_every == 0 or game == args.games:
                # sync train weights already on net (cpu)
                path = args.out_dir / f"game_{game:04d}.nfmst"
                dump = run_episode_cpu(
                    dump_env,
                    net,
                    deterministic=True,
                    record_path=str(path),
                    seed=10_000 + game,
                )
                ckpt = args.out_dir / f"policy_game_{game:04d}.pt"
                torch.save({"model": net.state_dict(), "game": game}, ckpt)
                print(
                    f"[ppo] dump {path.name} frames={dump['frames']} "
                    f"ret={dump['return']:.2f} clear={dump['clear']:.0f} "
                    f"ckpt={ckpt.name}",
                    flush=True,
                )

            if game >= args.games:
                break

    if buf_eps:
        do_update()

    vec.close()
    dump_env.close()
    elapsed = time.time() - t0
    print(
        f"[ppo] done games={args.games} elapsed={elapsed:.1f}s "
        f"out={args.out_dir}",
        flush=True,
    )


if __name__ == "__main__":
    main()
