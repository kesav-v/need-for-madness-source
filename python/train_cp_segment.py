#!/usr/bin/env python3
"""CP k → CP k+1 segment behavioral cloning (goal-conditioned next-CP obs).

Obs is ego-relative next checkpoint (no clear/place skill IDs). Demos are still
sliced by true clear for balanced sampling. Stage 11 / Tornado Shark default.
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np
import torch

import train_ppo as tp
from nfm_gym import NfmEnv

REPO = Path(__file__).resolve().parents[1]


def collect_segmented_demos(
    env: NfmEnv,
    n_demos: int,
    seed0: int,
    *,
    record_first: Path | None = None,
    stage: int | None = None,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, List[dict]]:
    """AI rollouts; each frame tagged with segment k = clear at decision time.

    Returns obs, act, segs, stages (per-frame stage id).
    """
    obs_list: List[np.ndarray] = []
    act_list: List[np.ndarray] = []
    seg_list: List[int] = []
    stage_list: List[int] = []
    stats: List[dict] = []
    for d in range(n_demos):
        rec = str(record_first) if record_first is not None and d == 0 else None
        opts: dict = {}
        if rec:
            opts["record_path"] = rec
        if stage is not None:
            opts["stage"] = int(stage)
        obs, _ = env.reset(seed=seed0 + d, options=opts or None)
        st = int(stage if stage is not None else env.stage)
        ep_ret = 0.0
        frames = 0
        max_clear = 0
        clear_now = 0
        seg_counts: Dict[int, int] = {}
        while True:
            k = clear_now
            next_obs, reward, term, trunc, info = env.step_ai()
            obs_list.append(obs.copy())
            act_list.append(info["action"].astype(np.float32))
            seg_list.append(k)
            stage_list.append(st)
            seg_counts[k] = seg_counts.get(k, 0) + 1
            ep_ret += float(reward)
            frames += 1
            clear_now = int(info.get("clear", clear_now))
            max_clear = max(max_clear, clear_now)
            obs = next_obs
            if term or trunc:
                break
        if rec:
            env.finish_recording()
        stats.append(
            {
                "stage": st,
                "return": ep_ret,
                "frames": frames,
                "clear": max_clear,
                "need": env.need_clear(),
                "segments": dict(sorted(seg_counts.items())),
            }
        )
        print(
            f"[seg] stage={st} demo {d+1}/{n_demos} frames={frames} "
            f"clear={max_clear}/{env.need_clear()} segs={stats[-1]['segments']}",
            flush=True,
        )
    return (
        np.asarray(obs_list, dtype=np.float32),
        np.asarray(act_list, dtype=np.float32),
        np.asarray(seg_list, dtype=np.int32),
        np.asarray(stage_list, dtype=np.int32),
        stats,
    )


def collect_multistage_demos(
    env: NfmEnv,
    stages: List[int],
    demos_per_stage: int,
    seed0: int,
    *,
    out_dir: Path,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, List[dict]]:
    """Roll AI on each stage; concatenate into one bank."""
    all_o, all_a, all_s, all_st = [], [], [], []
    all_stats: List[dict] = []
    for i, st in enumerate(stages):
        rec = out_dir / f"ai_demo_stage{st}.nfmst" if i == 0 else None
        o, a, s, stg, stats = collect_segmented_demos(
            env,
            demos_per_stage,
            seed0 + i * 10_000,
            record_first=rec,
            stage=st,
        )
        all_o.append(o)
        all_a.append(a)
        all_s.append(s)
        all_st.append(stg)
        all_stats.extend(stats)
    return (
        np.concatenate(all_o, axis=0),
        np.concatenate(all_a, axis=0),
        np.concatenate(all_s, axis=0),
        np.concatenate(all_st, axis=0),
        all_stats,
    )


def segment_weights(
    segs: np.ndarray,
    stages: np.ndarray | None = None,
    *,
    power: float = 1.0,
) -> np.ndarray:
    """Inverse-frequency weights; optionally balance (stage, segment) jointly."""
    w = np.ones(len(segs), dtype=np.float64)
    if stages is None:
        keys = [(int(k),) for k in np.unique(segs)]
        for (k,) in keys:
            m = segs == k
            n_k = int(m.sum())
            if n_k > 0:
                w[m] = (1.0 / n_k) ** power
    else:
        for st in np.unique(stages):
            for k in np.unique(segs[stages == st]):
                m = (stages == st) & (segs == k)
                n_k = int(m.sum())
                if n_k > 0:
                    w[m] = (1.0 / n_k) ** power
    w /= w.mean()
    return w


def behavioral_clone_balanced(
    net: tp.ActorCritic,
    obs: np.ndarray,
    actions: np.ndarray,
    segs: np.ndarray,
    device: torch.device,
    *,
    stages: np.ndarray | None = None,
    epochs: int = 40,
    batch_size: int = 512,
    lr: float = 1e-3,
) -> dict:
    """BCE with replacement sampling balanced across CP segments (and stages)."""
    net.to(device)
    opt = torch.optim.Adam(net.parameters(), lr=lr)
    x = torch.as_tensor(obs, device=device)
    y = torch.as_tensor(actions, device=device)
    w = torch.as_tensor(
        segment_weights(segs, stages), device=device, dtype=torch.float32
    )
    n = x.shape[0]
    counts = {int(k): int((segs == k).sum()) for k in np.unique(segs)}
    print(f"[seg-bc] frames={n} per-segment={counts}", flush=True)
    if stages is not None:
        sc = {int(s): int((stages == s).sum()) for s in np.unique(stages)}
        print(f"[seg-bc] per-stage={sc}", flush=True)
    last = {}
    for ep in range(epochs):
        perm = torch.multinomial(w, n, replacement=True)
        total_loss = 0.0
        n_batches = 0
        for start in range(0, n, batch_size):
            mb = perm[start : start + batch_size]
            logits, _ = net(x[mb])
            loss = torch.nn.functional.binary_cross_entropy_with_logits(logits, y[mb])
            opt.zero_grad()
            loss.backward()
            opt.step()
            total_loss += float(loss.item())
            n_batches += 1
        last = {"bc_loss": total_loss / max(n_batches, 1), "epoch": ep + 1}
        if (ep + 1) % 5 == 0 or ep == 0:
            print(
                f"[seg-bc] epoch {ep+1}/{epochs} loss={last['bc_loss']:.4f}",
                flush=True,
            )
    net.to("cpu")
    return last


def dagger_segment_round(
    env: NfmEnv,
    net: tp.ActorCritic,
    *,
    n_eps: int,
    seed0: int,
    beta: float,
    max_steps: int,
    stage: int | None = None,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Mix expert/student rollout; label with query_ai; tag segment k + stage."""
    obs_list: List[np.ndarray] = []
    act_list: List[np.ndarray] = []
    seg_list: List[int] = []
    stage_list: List[int] = []
    env.set_stall_cut(False)
    env.max_steps = max_steps
    st = int(stage if stage is not None else env.stage)
    for d in range(n_eps):
        obs, _ = env.reset(seed=seed0 + d, options={"stage": st})
        clear_now = 0
        while True:
            k = clear_now
            a_exp = env.query_ai().astype(np.float32)
            obs_list.append(obs.copy())
            act_list.append(a_exp)
            seg_list.append(k)
            stage_list.append(st)
            if np.random.random() < beta:
                a = a_exp.astype(np.int8)
            else:
                a, _, _ = tp.act_batch(net, obs[None, :], deterministic=False)
                a = a[0].astype(np.int8)
            obs, _, term, trunc, info = env.step(a)
            clear_now = int(info.get("clear", clear_now))
            if term or trunc:
                break
    return (
        np.asarray(obs_list, dtype=np.float32),
        np.asarray(act_list, dtype=np.float32),
        np.asarray(seg_list, dtype=np.int32),
        np.asarray(stage_list, dtype=np.int32),
    )


def _spot_score(spot: dict) -> Tuple[int, float]:
    """Rank by (# full clears, sum clear/need)."""
    oks = sum(1 for v in spot.values() if v["ok"])
    frac = sum(v["clear"] / max(v["need"], 1) for v in spot.values())
    return oks, frac


def train_until_clear(
    net: tp.ActorCritic,
    env: NfmEnv,
    obs: np.ndarray,
    act: np.ndarray,
    segs: np.ndarray,
    device: torch.device,
    *,
    target_clear: int,
    max_epochs: int,
    eval_every: int,
    eval_seed: int,
    out_dir: Path,
    dagger_rounds: int,
    dagger_eps: int,
    lr: float,
    stages: np.ndarray | None = None,
    stage_list: List[int] | None = None,
    gate_all: bool = False,
    car: int = 0,
    skip_warm_start: bool = False,
) -> Tuple[dict, np.ndarray, np.ndarray, np.ndarray, np.ndarray | None]:
    best = {"clear": -1.0, "epoch": 0, "oks": -1, "frac": -1.0}
    total = 0
    eval_stage = int(env.stage)
    stages_for_dagger = list(stage_list) if stage_list else [eval_stage]

    if not skip_warm_start:
        pre = min(25, max_epochs)
        behavioral_clone_balanced(
            net, obs, act, segs, device, stages=stages, epochs=pre, lr=lr
        )
        total += pre
    else:
        print("[seg] skip warm-start BC (resume)", flush=True)

    env.reset(seed=eval_seed, options={"stage": eval_stage})
    dump = tp.eval_clear_suite(
        env,
        net,
        [eval_seed + i for i in range(3)],
        record_path=str(out_dir / "game_seg_eval_e000.nfmst"),
    )
    spot = (
        eval_stages(
            net,
            stages=stages_for_dagger,
            car=car,
            max_steps=env.max_steps,
            seed0=eval_seed + 500,
            out_dir=out_dir,
            quiet=False,
        )
        if gate_all
        else {
            eval_stage: {
                "clear": float(dump["clear"]),
                "need": target_clear,
                "frames": dump["frames"],
                "ok": float(dump["clear"]) >= target_clear,
            }
        }
    )
    oks, frac = _spot_score(spot)
    print(
        f"[seg] after warm-start stage={eval_stage} clear={dump['clear']:.0f} "
        f"frames={dump['frames']} spot_ok={oks}/{len(stages_for_dagger)}",
        flush=True,
    )
    best = {
        "clear": float(dump["clear"]),
        "epoch": total,
        "frames": dump["frames"],
        "return": dump.get("return", 0.0),
        "oks": oks,
        "frac": frac,
        "spot": {int(k): dict(v) for k, v in spot.items()},
    }
    torch.save(
        {
            "model": net.state_dict(),
            "bc": True,
            "clear": best["clear"],
            "seg": True,
            "oks": oks,
            "spot": best["spot"],
        },
        out_dir / "policy_seg.pt",
    )
    if gate_all and oks >= len(stages_for_dagger):
        print("[seg] all stages already clear", flush=True)
        return best, obs, act, segs, stages
    if (not gate_all) and best["clear"] >= target_clear:
        print(f"[seg] hit target clear>={target_clear}", flush=True)
        return best, obs, act, segs, stages

    for rnd in range(dagger_rounds):
        if total >= max_epochs:
            break
        if gate_all and best["oks"] >= len(stages_for_dagger):
            break
        if (not gate_all) and best["clear"] >= target_clear:
            break

        ckpt = torch.load(out_dir / "policy_seg.pt", map_location="cpu", weights_only=True)
        net.load_state_dict(ckpt["model"])
        beta = max(0.2, 0.85 - 0.12 * rnd)
        round_lr = lr * (0.75 ** rnd)

        # Prefer stages that are not yet full-clear; fall back to all.
        weak = [
            st
            for st in stages_for_dagger
            if not best.get("spot", {}).get(st, {}).get("ok", False)
        ]
        if not weak:
            weak = list(stages_for_dagger)
        # Rotate focus across rounds so late stages (12/13/15) get DAgger too.
        start = (rnd * 3) % len(weak)
        rotated = weak[start:] + weak[:start]
        focus = rotated[:3]
        if gate_all and eval_stage not in focus:
            focus.append(eval_stage)
        focus = focus[:4]
        eps_each = max(2, dagger_eps // len(focus))
        print(
            f"[seg-dagger] round {rnd+1}/{dagger_rounds} beta={beta:.2f} "
            f"lr={round_lr:.2e} best_ok={best['oks']}/{len(stages_for_dagger)} "
            f"weak={weak} focus={focus} eps/stage={eps_each}",
            flush=True,
        )

        added = 0
        for fi, st in enumerate(focus):
            new_o, new_a, new_s, new_st = dagger_segment_round(
                env,
                net,
                n_eps=eps_each,
                seed0=50_000 + rnd * 1000 + fi * 50,
                beta=beta,
                max_steps=env.max_steps,
                stage=st,
            )
            obs = np.concatenate([obs, new_o], axis=0)
            act = np.concatenate([act, new_a], axis=0)
            segs = np.concatenate([segs, new_s], axis=0)
            if stages is None:
                stages = new_st.copy()
            else:
                stages = np.concatenate([stages, new_st], axis=0)
            added += len(new_o)
        print(f"[seg-dagger] dataset={len(obs)} (+{added})", flush=True)

        budget = max(
            eval_every, (max_epochs - total) // max(dagger_rounds - rnd, 1)
        )
        done = 0
        while done < budget and total < max_epochs:
            n = min(eval_every, budget - done, max_epochs - total)
            if n <= 0:
                break
            behavioral_clone_balanced(
                net, obs, act, segs, device, stages=stages, epochs=n, lr=round_lr
            )
            done += n
            total += n

            env.reset(seed=eval_seed, options={"stage": eval_stage})
            path = out_dir / f"game_seg_eval_e{total:03d}.nfmst"
            dump = tp.eval_clear_suite(
                env, net, [eval_seed + i for i in range(3)], record_path=str(path)
            )
            if gate_all:
                # Fixed seeds so the gate is stable across rounds.
                spot = eval_stages(
                    net,
                    stages=stages_for_dagger,
                    car=car,
                    max_steps=env.max_steps,
                    seed0=12_000,
                    out_dir=out_dir,
                    quiet=True,
                )
            else:
                spot = {
                    eval_stage: {
                        "clear": float(dump["clear"]),
                        "need": target_clear,
                        "frames": dump["frames"],
                        "ok": float(dump["clear"]) >= target_clear,
                    }
                }
            oks, frac = _spot_score(spot)
            print(
                f"[seg] eval @{total} stage={eval_stage} clear={dump['clear']:.0f} "
                f"frames={dump['frames']} ret={dump['return']:.1f} "
                f"spot_ok={oks}/{len(stages_for_dagger)} frac={frac:.2f}",
                flush=True,
            )
            improved = (oks, frac, float(dump["clear"])) > (
                best["oks"],
                best["frac"],
                best["clear"],
            )
            if improved:
                best = {
                    "clear": float(dump["clear"]),
                    "epoch": total,
                    "frames": dump["frames"],
                    "return": dump["return"],
                    "oks": oks,
                    "frac": frac,
                    "spot": {int(k): dict(v) for k, v in spot.items()},
                }
                torch.save(
                    {
                        "model": net.state_dict(),
                        "bc": True,
                        "clear": best["clear"],
                        "seg": True,
                        "oks": oks,
                        "spot": best["spot"],
                    },
                    out_dir / "policy_seg.pt",
                )
            else:
                # Soft rollback only if Shark regresses below prior best clear
                # when gating single-stage, or if oks dropped when gating all.
                regress = (
                    (not gate_all and dump["clear"] < best["clear"])
                    or (gate_all and oks < best["oks"])
                )
                if regress:
                    best_ckpt = torch.load(
                        out_dir / "policy_seg.pt", map_location="cpu", weights_only=True
                    )
                    net.load_state_dict(best_ckpt["model"])

            hit = (
                (gate_all and oks >= len(stages_for_dagger))
                or ((not gate_all) and dump["clear"] >= target_clear)
            )
            if hit:
                print(
                    f"[seg] hit target at epoch {total} "
                    f"(ok={oks}/{len(stages_for_dagger)} clear={dump['clear']:.0f})",
                    flush=True,
                )
                best_ckpt = torch.load(
                    out_dir / "policy_seg.pt", map_location="cpu", weights_only=True
                )
                net.load_state_dict(best_ckpt["model"])
                return best, obs, act, segs, stages

    best_ckpt = torch.load(out_dir / "policy_seg.pt", map_location="cpu", weights_only=True)
    net.load_state_dict(best_ckpt["model"])
    print(
        f"[seg] finished; best ok={best['oks']}/{len(stages_for_dagger)} "
        f"clear={best['clear']:.0f} epochs={total}",
        flush=True,
    )
    return best, obs, act, segs, stages


def eval_stages(
    net: tp.ActorCritic,
    *,
    stages: List[int],
    car: int,
    max_steps: int,
    seed0: int,
    out_dir: Path,
    quiet: bool = False,
) -> dict:
    """Deterministic clear on each stage (one seed each)."""
    results = {}
    env = NfmEnv(stage=stages[0], car=car, nplayers=1, max_steps=max_steps)
    env.set_stall_cut(False)
    for st in stages:
        env.stage = st
        rec = str(out_dir / f"eval_stage{st}.nfmst") if st == 11 else None
        dump = tp.eval_clear(env, net, seed0 + st, record_path=rec)
        need = env.need_clear()
        results[st] = {
            "clear": float(dump["clear"]),
            "need": need,
            "frames": dump["frames"],
            "ok": float(dump["clear"]) >= need,
        }
        if not quiet:
            print(
                f"[seg] spot stage={st} clear={dump['clear']:.0f}/{need} "
                f"frames={dump['frames']} ok={results[st]['ok']}",
                flush=True,
            )
    env.close()
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--stage", type=int, default=11, help="primary eval stage")
    ap.add_argument(
        "--stages",
        type=str,
        default="",
        help="comma list for multi-stage demos (default: AI-complete set incl. 11)",
    )
    ap.add_argument("--car", type=int, default=0)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--device", default="auto")
    ap.add_argument("--out-dir", type=Path, default=REPO / "OBJ" / "cp_segment")
    ap.add_argument("--nplayers", type=int, default=1)
    ap.add_argument("--max-steps", type=int, default=8000)
    ap.add_argument("--demos", type=int, default=30, help="demos if single-stage")
    ap.add_argument("--demos-per-stage", type=int, default=8)
    ap.add_argument("--epochs", type=int, default=100)
    ap.add_argument("--target-clear", type=int, default=6,
                    help="gate on --stage (stage 11 need=6)")
    ap.add_argument("--eval-every", type=int, default=5)
    ap.add_argument("--dagger-rounds", type=int, default=8)
    ap.add_argument("--dagger-eps", type=int, default=12)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--load", type=Path, default=None)
    ap.add_argument("--bank", type=Path, default=None, help="load existing segment_bank.npz")
    ap.add_argument(
        "--gate-all",
        action="store_true",
        help="DAgger on weak stages; stop when all --stages full-clear",
    )
    ap.add_argument(
        "--skip-warm-start",
        action="store_true",
        help="skip initial BC (use with --load of a trained policy)",
    )
    args = ap.parse_args()

    if args.stages.strip():
        stage_list = [int(x) for x in args.stages.split(",") if x.strip()]
    else:
        # Stages where Control::preform finishes with car 0 within 5k steps.
        stage_list = [1, 2, 4, 8, 11, 12, 13, 15]

    gate_all = bool(args.gate_all or len(stage_list) > 1)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    device = tp.resolve_train_device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    print(
        f"[seg] device={device} out={args.out_dir} stages={stage_list} "
        f"eval_stage={args.stage} gate_all={gate_all}",
        flush=True,
    )

    net = tp.ActorCritic()
    if args.load and args.load.exists():
        ckpt = torch.load(args.load, map_location="cpu", weights_only=True)
        net.load_state_dict(
            ckpt["model"] if isinstance(ckpt, dict) and "model" in ckpt else ckpt
        )
        print(f"[seg] loaded {args.load}", flush=True)

    env = NfmEnv(
        stage=args.stage,
        car=args.car,
        nplayers=args.nplayers,
        max_steps=args.max_steps,
    )
    env.set_stall_cut(False)

    t0 = time.time()
    stages_arr: np.ndarray | None = None
    if args.bank and args.bank.exists():
        z = np.load(args.bank)
        obs, act, segs = z["obs"], z["act"], z["segs"]
        stages_arr = z["stages"] if "stages" in z.files else None
        print(f"[seg] loaded bank {args.bank} frames={len(obs)}", flush=True)
    elif len(stage_list) > 1:
        print("[seg] === collect multi-stage AI demos ===", flush=True)
        obs, act, segs, stages_arr, stats = collect_multistage_demos(
            env,
            stage_list,
            args.demos_per_stage,
            args.seed + 10_000,
            out_dir=args.out_dir,
        )
        print(
            f"[seg] bank frames={len(obs)} stages={sorted(set(int(s) for s in stages_arr))} "
            f"clear avg={np.mean([s['clear'] for s in stats]):.1f}",
            flush=True,
        )
    else:
        print("[seg] === collect AI demos & slice by CP ===", flush=True)
        obs, act, segs, stages_arr, stats = collect_segmented_demos(
            env,
            args.demos,
            args.seed + 10_000,
            record_first=args.out_dir / "ai_demo.nfmst",
            stage=args.stage,
        )
        print(
            f"[seg] demos clear avg={np.mean([s['clear'] for s in stats]):.1f} "
            f"max={max(s['clear'] for s in stats)} frames={len(obs)}",
            flush=True,
        )

    np.savez_compressed(
        args.out_dir / "segment_bank.npz",
        obs=obs,
        act=act,
        segs=segs,
        stages=stages_arr if stages_arr is not None else np.full(len(segs), args.stage),
    )

    # Ensure eval env is primary stage.
    env.reset(seed=args.seed, options={"stage": args.stage})
    env.stage = args.stage

    mode = "all stages" if gate_all else f"stage {args.stage}"
    print(f"[seg] === balanced BC + DAgger (gate on {mode}) ===", flush=True)
    best, obs, act, segs, stages_arr = train_until_clear(
        net,
        env,
        obs,
        act,
        segs,
        device,
        target_clear=args.target_clear,
        max_epochs=args.epochs,
        eval_every=args.eval_every,
        eval_seed=99_001,
        out_dir=args.out_dir,
        dagger_rounds=args.dagger_rounds,
        dagger_eps=args.dagger_eps,
        lr=args.lr,
        stages=stages_arr,
        stage_list=stage_list,
        gate_all=gate_all,
        car=args.car,
        skip_warm_start=bool(args.skip_warm_start),
    )
    # Persist expanded bank after DAgger.
    np.savez_compressed(
        args.out_dir / "segment_bank.npz",
        obs=obs,
        act=act,
        segs=segs,
        stages=stages_arr if stages_arr is not None else np.full(len(segs), args.stage),
    )

    env.reset(seed=99_001, options={"stage": args.stage})
    dump = tp.eval_clear(
        env, net, 99_001, record_path=str(args.out_dir / "final_seg_eval.nfmst")
    )
    print(
        f"[seg] FINAL stage={args.stage} clear={dump['clear']:.0f} "
        f"frames={dump['frames']} best_ok={best.get('oks', '?')} "
        f"best_clear={best['clear']:.0f} elapsed={time.time()-t0:.1f}s",
        flush=True,
    )
    spot = eval_stages(
        net,
        stages=stage_list,
        car=args.car,
        max_steps=args.max_steps,
        seed0=12_000,
        out_dir=args.out_dir,
    )
    torch.save(
        {
            "model": net.state_dict(),
            "bc": True,
            "seg": True,
            "clear": float(dump["clear"]),
            "best": best,
            "spot": spot,
            "stages": stage_list,
        },
        args.out_dir / "policy_seg_final.pt",
    )
    env.close()


if __name__ == "__main__":
    main()
