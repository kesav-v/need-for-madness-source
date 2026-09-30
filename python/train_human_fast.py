#!/usr/bin/env python3
"""Train a faster solo F7 policy from human .nfmst/.nfmac demos.

Finish-gated PPO with time pressure (−0.05/frame, +500 finish, −1000 DNF).
Train-only stall AI-takeover (Control::preform) with engage/tick penalties.
Eval/probe never uses takeover.
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path
from typing import List

import numpy as np
import torch
from torch.distributions import Bernoulli

import train_ppo as tp
from nfm_gym import NfmEnv
from nfm_gym.human_demos import load_human_dataset

REPO = Path(__file__).resolve().parents[1]
NEED = 6  # stage 11: 3 laps × 2 cps


def reshape_finish_time_rewards(
    n: int,
    *,
    finished: bool,
    takeover: np.ndarray | None = None,
    engage: np.ndarray | None = None,
    time_pen: float = 0.05,
    finish_bonus: float = 500.0,
    dnf_pen: float = 1000.0,
    takeover_engage_pen: float = 10.0,
    takeover_tick_pen: float = 0.02,
) -> np.ndarray:
    """Pure finish/time signal + optional stall-takeover penalties."""
    out = np.full(n, -time_pen, dtype=np.float32)
    if n == 0:
        return out
    if takeover is not None and len(takeover) == n:
        out -= takeover.astype(np.float32) * takeover_tick_pen
    if engage is not None and len(engage) == n:
        out -= engage.astype(np.float32) * takeover_engage_pen
    if finished:
        out[-1] += finish_bonus
    else:
        out[-1] = -dnf_pen
    return out


@torch.inference_mode()
def logp_and_value(net: tp.ActorCritic, obs: np.ndarray, actions: np.ndarray):
    """Log-prob of given actions under policy + values. obs/actions [N,...]."""
    o = torch.as_tensor(obs, dtype=torch.float32)
    a = torch.as_tensor(actions, dtype=torch.float32)
    logits, values = net(o)
    dist = Bernoulli(logits=logits)
    logp = dist.log_prob(a).sum(-1)
    return (
        logp.numpy().astype(np.float32),
        values.numpy().astype(np.float32),
    )


def probe_finish(
    env: NfmEnv,
    net: tp.ActorCritic,
    seeds: List[int],
) -> dict:
    env.set_stall_cut(False)
    finishes = 0
    times: List[int] = []
    fails: List[int] = []
    clears: List[float] = []
    for seed in seeds:
        dump = tp.run_episode_cpu(env, net, deterministic=True, seed=seed)
        c = float(dump["clear"])
        clears.append(c)
        if c >= NEED:
            finishes += 1
            times.append(int(dump["frames"]))
        else:
            fails.append(seed)
    env.set_stall_cut(True)
    n = len(seeds)
    return {
        "finish": finishes / n,
        "mean_t": float(np.mean(times)) if times else float("inf"),
        "mean_clear": float(np.mean(clears)),
        "fails": fails,
        "n_fin": finishes,
        "n": n,
    }


def better(a: dict, b: dict, *, min_finish: float) -> bool:
    """Prefer high finish rate, then lower mean finish time."""
    if a["finish"] < min_finish and b["finish"] >= min_finish:
        return False
    if a["finish"] >= min_finish and b["finish"] < min_finish:
        return True
    if abs(a["finish"] - b["finish"]) > 0.02:
        return a["finish"] > b["finish"]
    return a["mean_t"] < b["mean_t"]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--stage", type=int, default=11)
    ap.add_argument("--car", type=int, default=1, help="F7")
    ap.add_argument("--out-dir", type=Path, default=REPO / "OBJ" / "human_fast_f7")
    ap.add_argument("--device", default="auto")
    ap.add_argument("--bc-epochs", type=int, default=20,
                    help="human BC fine-tune epochs (0=skip; use with --load)")
    ap.add_argument(
        "--skip-bc",
        action="store_true",
        help="skip human BC; only PPO+bc_coef regularizer",
    )
    ap.add_argument("--bc-lr", type=float, default=1e-3)
    ap.add_argument("--games", type=int, default=800)
    ap.add_argument("--num-envs", type=int, default=32)
    ap.add_argument("--max-steps", type=int, default=8000)
    ap.add_argument("--lr", type=float, default=2e-5)
    ap.add_argument("--bc-coef", type=float, default=0.35)
    ap.add_argument("--gamma", type=float, default=0.99)
    ap.add_argument("--probe-every", type=int, default=64)
    ap.add_argument("--min-finish", type=float, default=0.85)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--load", type=Path, default=None)
    ap.add_argument("--bc-only", action="store_true")
    ap.add_argument(
        "--takeover",
        action="store_true",
        default=True,
        help="train-only: stall → Control::preform takeover (default on)",
    )
    ap.add_argument("--no-takeover", action="store_true",
                    help="disable stall AI takeover")
    ap.add_argument("--takeover-speed", type=float, default=8.0,
                    help="|speed| below this counts as stall (game units)")
    ap.add_argument("--takeover-grace", type=int, default=20,
                    help="stall ticks before AI takeover engages")
    ap.add_argument("--takeover-engage-pen", type=float, default=10.0)
    ap.add_argument("--takeover-tick-pen", type=float, default=0.02)
    args = ap.parse_args()
    if args.no_takeover:
        args.takeover = False

    args.out_dir.mkdir(parents=True, exist_ok=True)
    train_device = tp.resolve_train_device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    def log(msg: str) -> None:
        print(msg, flush=True)

    data = load_human_dataset(stage=args.stage, require_finish=True, repo=REPO)
    demo_obs, demo_act = data["obs"], data["actions"]
    fin_stats = [s for s in data["stats"] if s["finished"]]
    human_mean_t = float(np.mean([s["frames"] for s in fin_stats]))
    log(
        f"[human-fast] demos={len(fin_stats)} frames={len(demo_obs)} "
        f"human_mean_t={human_mean_t:.0f} (~{human_mean_t/30:.1f}s) "
        f"device={train_device} takeover={args.takeover}"
    )

    if args.load and args.load.exists():
        net = tp.load_policy_expand(args.load)
        log(f"[human-fast] loaded {args.load}")
    else:
        net = tp.ActorCritic()

    env = NfmEnv(
        stage=args.stage,
        car=args.car,
        nplayers=1,
        max_steps=args.max_steps,
    )
    env.set_stall_cut(False)

    if not args.skip_bc and args.bc_epochs > 0:
        log(f"[human-fast] BC epochs={args.bc_epochs}…")
        obs_w = demo_obs.copy()
        obs_w[:, 14] = demo_obs[:, 20]
        tp.behavioral_clone(
            net,
            obs_w,
            demo_act,
            train_device,
            epochs=args.bc_epochs,
            batch_size=1024,
            lr=args.bc_lr,
            clear_weight=1.5,
        )
    else:
        log("[human-fast] skip BC (PPO + human bc_coef only)")

    probe_seeds = list(range(1000, 1032))
    best = probe_finish(env, net, probe_seeds)
    log(
        f"[human-fast] start finish={best['finish']:.3f} "
        f"mean_t={best['mean_t']:.0f} mean_clear={best['mean_clear']:.2f} "
        f"fails={best['fails'][:8]}"
    )
    torch.save(
        {"model": net.state_dict(), "probe": best, "human_mean_t": human_mean_t},
        args.out_dir / "policy_bc.pt",
    )
    best_state = {k: v.detach().cpu().clone() for k, v in net.state_dict().items()}
    torch.save(
        {"model": best_state, "probe": best},
        args.out_dir / "policy_best.pt",
    )
    if args.bc_only:
        env.close()
        log("[human-fast] done (--bc-only)")
        return

    vec = tp.VecNfm(
        args.num_envs,
        stage=args.stage,
        car=args.car,
        nplayers=1,
        max_steps=args.max_steps,
    )
    for i in range(vec.n):
        vec.envs[i].set_stall_cut(False)

    seed_counter = args.seed + 1
    vec.reset_all([seed_counter + i for i in range(args.num_envs)])
    seed_counter += args.num_envs

    traj_obs: List[List[np.ndarray]] = [[] for _ in range(args.num_envs)]
    traj_act: List[List[np.ndarray]] = [[] for _ in range(args.num_envs)]
    traj_logp: List[List[float]] = [[] for _ in range(args.num_envs)]
    traj_rew: List[List[float]] = [[] for _ in range(args.num_envs)]
    traj_val: List[List[float]] = [[] for _ in range(args.num_envs)]
    traj_done: List[List[bool]] = [[] for _ in range(args.num_envs)]
    traj_clear: List[List[int]] = [[] for _ in range(args.num_envs)]
    traj_takeover: List[List[bool]] = [[] for _ in range(args.num_envs)]
    traj_engage: List[List[bool]] = [[] for _ in range(args.num_envs)]

    stall_ticks = np.zeros(args.num_envs, dtype=np.int32)
    in_takeover = np.zeros(args.num_envs, dtype=np.bool_)
    prev_clear = np.zeros(args.num_envs, dtype=np.int32)

    train_net = tp.ActorCritic().to(train_device)
    train_net.load_state_dict(net.state_dict())
    train_opt = torch.optim.Adam(train_net.parameters(), lr=args.lr)

    log(
        f"[human-fast] PPO games={args.games} envs={args.num_envs} "
        f"lr={args.lr} bc_coef={args.bc_coef} min_finish={args.min_finish} "
        f"takeover={args.takeover} speed<{args.takeover_speed} "
        f"grace={args.takeover_grace} eng_pen={args.takeover_engage_pen} "
        f"tick_pen={args.takeover_tick_pen}"
    )

    buf_eps: list = []
    game = 0
    t0 = time.time()
    takeover_frames_total = 0
    engage_total = 0

    while game < args.games:
        actions, logps, values = tp.act_batch(net, vec.obs, deterministic=False)
        engage_step = np.zeros(vec.n, dtype=np.bool_)
        take_step = np.zeros(vec.n, dtype=np.bool_)

        if args.takeover:
            for i in range(vec.n):
                speed = abs(float(vec.obs[i, 8]) * 200.0)
                clr = int(round(float(vec.obs[i, 20]) * NEED))
                if clr > prev_clear[i] or speed >= args.takeover_speed:
                    stall_ticks[i] = 0
                    if speed >= args.takeover_speed * 1.5:
                        in_takeover[i] = False
                else:
                    stall_ticks[i] += 1
                if (not in_takeover[i]) and stall_ticks[i] >= args.takeover_grace:
                    in_takeover[i] = True
                    engage_step[i] = True
                    engage_total += 1
                if in_takeover[i]:
                    take_step[i] = True
                    actions[i] = vec.envs[i].query_ai().astype(np.float32)
                    takeover_frames_total += 1
                prev_clear[i] = clr

            if take_step.any():
                lp, val = logp_and_value(net, vec.obs, actions)
                for i in range(vec.n):
                    if take_step[i]:
                        logps[i] = lp[i]
                        values[i] = val[i]

        for i in range(vec.n):
            traj_obs[i].append(vec.obs[i].copy())
            traj_act[i].append(actions[i].astype(np.float32))
            traj_logp[i].append(float(logps[i]))
            traj_val[i].append(float(values[i]))
            traj_takeover[i].append(bool(take_step[i]))
            traj_engage[i].append(bool(engage_step[i]))

        _next_obs, rewards, dones, infos = vec.step(actions)

        for i in range(vec.n):
            traj_rew[i].append(float(rewards[i]))
            traj_done[i].append(bool(dones[i]))
            traj_clear[i].append(int(infos[i].get("clear", 0)))
            if not dones[i]:
                continue
            max_clear = max(traj_clear[i]) if traj_clear[i] else 0
            finished = max_clear >= NEED
            take_arr = np.asarray(traj_takeover[i], dtype=np.bool_)
            eng_arr = np.asarray(traj_engage[i], dtype=np.bool_)
            rew = reshape_finish_time_rewards(
                len(traj_rew[i]),
                finished=finished,
                takeover=take_arr,
                engage=eng_arr,
                takeover_engage_pen=args.takeover_engage_pen,
                takeover_tick_pen=args.takeover_tick_pen,
            )
            # PPO only on policy-controlled frames (exclude AI takeover).
            policy_mask = ~take_arr
            if not policy_mask.any():
                policy_mask = np.ones_like(take_arr)
            obs_e = np.asarray(traj_obs[i], dtype=np.float32)[policy_mask]
            act_e = np.asarray(traj_act[i], dtype=np.float32)[policy_mask]
            logp_e = np.asarray(traj_logp[i], dtype=np.float32)[policy_mask]
            val_e = np.asarray(traj_val[i], dtype=np.float32)[policy_mask]
            rew_e = rew[policy_mask]
            done_e = np.asarray(traj_done[i], dtype=np.bool_)[policy_mask].copy()
            if len(done_e):
                done_e[-1] = True
            ep = {
                "obs": obs_e,
                "actions": act_e,
                "logp": logp_e,
                "rewards": rew_e,
                "values": val_e,
                "dones": done_e,
                "return": float(rew.sum()),
                "frames": len(traj_rew[i]),
                "clear": float(max_clear),
                "finished": finished,
                "takeover_frac": float(take_arr.mean()) if len(take_arr) else 0.0,
                "engages": int(eng_arr.sum()),
            }
            if finished:
                buf_eps.append(ep)
            game += 1
            if game % 16 == 0:
                log(
                    f"[human-fast] game={game}/{args.games} "
                    f"clear={max_clear} frames={ep['frames']} "
                    f"fin={int(finished)} take={ep['takeover_frac']:.2f} "
                    f"eng={ep['engages']} buf={len(buf_eps)} "
                    f"ret={ep['return']:.1f}"
                )

            traj_obs[i].clear()
            traj_act[i].clear()
            traj_logp[i].clear()
            traj_rew[i].clear()
            traj_val[i].clear()
            traj_done[i].clear()
            traj_clear[i].clear()
            traj_takeover[i].clear()
            traj_engage[i].clear()
            stall_ticks[i] = 0
            in_takeover[i] = False
            prev_clear[i] = 0
            vec.reset_one(i, seed_counter)
            seed_counter += 1

            if len(buf_eps) >= 8:
                obs = np.concatenate([e["obs"] for e in buf_eps])
                actions_b = np.concatenate([e["actions"] for e in buf_eps])
                logp = np.concatenate([e["logp"] for e in buf_eps])
                returns_list, adv_list = [], []
                for e in buf_eps:
                    R = tp.discounted_returns(e["rewards"], e["dones"], args.gamma)
                    returns_list.append(R)
                    adv_list.append(R - e["values"])
                batch = {
                    "obs": obs,
                    "actions": actions_b,
                    "logp": logp,
                    "returns": np.concatenate(returns_list),
                    "advantages": np.concatenate(adv_list),
                }
                train_net.load_state_dict(net.state_dict())
                tp.ppo_update(
                    train_net,
                    train_opt,
                    batch,
                    train_device,
                    demo_obs=demo_obs if args.bc_coef > 0 else None,
                    demo_act=demo_act if args.bc_coef > 0 else None,
                    bc_coef=args.bc_coef,
                    ent_coef=0.002,
                )
                net.load_state_dict(
                    {k: v.detach().cpu() for k, v in train_net.state_dict().items()}
                )
                buf_eps.clear()

            if game % args.probe_every == 0 and game > 0:
                probe = probe_finish(env, net, probe_seeds)
                log(
                    f"[human-fast] probe finish={probe['finish']:.3f} "
                    f"mean_t={probe['mean_t']:.0f} fails={probe['fails'][:8]} "
                    f"take_frames={takeover_frames_total} engages={engage_total}"
                )
                if better(probe, best, min_finish=args.min_finish):
                    best = probe
                    best_state = {
                        k: v.detach().cpu().clone() for k, v in net.state_dict().items()
                    }
                    torch.save(
                        {"model": best_state, "probe": best},
                        args.out_dir / "policy_best.pt",
                    )
                    log(
                        f"[human-fast] NEW BEST finish={best['finish']:.3f} "
                        f"mean_t={best['mean_t']:.0f}"
                    )
                    if (
                        best["finish"] >= 0.999
                        and best["mean_t"] <= human_mean_t * 1.08
                    ):
                        log("[human-fast] HIT near-human pace at 100% finish")
                        game = args.games
                        break
                elif probe["finish"] < max(0.5, best["finish"] - 0.10):
                    net.load_state_dict(best_state)
                    train_net.load_state_dict(best_state)
                    buf_eps.clear()
                    log("[human-fast] snap-back")

        if game >= args.games:
            break

    net.load_state_dict(best_state)
    final = probe_finish(env, net, probe_seeds)
    verify = probe_finish(env, net, list(range(2000, 2032)))
    torch.save(
        {"model": best_state, "probe": final, "verify": verify},
        args.out_dir / "policy_final.pt",
    )
    rec = str(args.out_dir / "human_fast_eval.nfmst")
    env.set_stall_cut(False)
    dump = tp.run_episode_cpu(
        env, net, deterministic=True, seed=99_001, record_path=rec
    )
    env.finish_recording()
    elapsed = time.time() - t0
    log(
        f"[human-fast] FINAL probe={final['finish']:.3f} t={final['mean_t']:.0f} "
        f"verify={verify['finish']:.3f} t={verify['mean_t']:.0f} "
        f"eval_clear={dump['clear']:.0f} frames={dump['frames']} "
        f"take_frames={takeover_frames_total} engages={engage_total} "
        f"elapsed={elapsed:.0f}s"
    )
    env.close()
    vec.close()


if __name__ == "__main__":
    main()
