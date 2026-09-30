#!/usr/bin/env python3
"""Superhuman curriculum: BC/DAgger floor → vs-AI PPO → optional self-play.

Metric: win rate / mean place vs built-in Control::preform on fixed seeds.
Keeps a strong BC regularizer so PPO does not erase driving skill.
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np
import torch

import train_ppo as tp
from nfm_gym import NfmEnv

REPO = Path(__file__).resolve().parents[1]


def field_dagger_until_win(
    net: tp.ActorCritic,
    *,
    stage: int,
    car: int,
    nplayers: int,
    max_steps: int,
    device: torch.device,
    out_dir: Path,
    solo_obs: np.ndarray,
    solo_act: np.ndarray,
    rounds: int = 10,
    eps_per_round: int = 12,
    target_win_rate: float = 0.5,
    eval_races: int = 8,
    seed0: int = 40_000,
    lr: float = 3e-4,
) -> dict:
    """DAgger in multi-car races: student states, AI labels; protect solo clear."""
    race = NfmEnv(stage=stage, car=car, nplayers=nplayers, max_steps=max_steps)
    race.set_stall_cut(False)
    solo = NfmEnv(stage=stage, car=car, nplayers=1, max_steps=max_steps)
    solo.set_stall_cut(False)

    baseline = eval_vs_ai(
        net,
        stage=stage,
        car=car,
        nplayers=nplayers,
        max_steps=max_steps,
        n_races=eval_races,
        seed0=12_345,
    )
    solo0 = tp.eval_clear(solo, net, 99_001)
    best = {
        "win_rate": baseline["win_rate"],
        "mean_place": baseline["mean_place"],
        "mean_clear": baseline["mean_clear"],
        "solo_clear": float(solo0["clear"]),
    }
    best_state = {k: v.detach().cpu().clone() for k, v in net.state_dict().items()}
    torch.save(
        {"model": best_state, "eval": best, "field_dagger": True},
        out_dir / "policy_field_best.pt",
    )
    print(
        f"[field-dag] baseline win={best['win_rate']:.2f} place={best['mean_place']:.2f} "
        f"clear={best['mean_clear']:.1f} solo={best['solo_clear']:.0f}",
        flush=True,
    )
    if best["win_rate"] >= target_win_rate and best["mean_place"] <= 2.5:
        race.close()
        solo.close()
        return best

    obs_bank = solo_obs.copy()
    act_bank = solo_act.copy()

    for rnd in range(rounds):
        beta = max(0.35, 0.9 - 0.06 * rnd)
        round_lr = lr * (0.85 ** rnd)
        print(
            f"[field-dag] round {rnd+1}/{rounds} beta={beta:.2f} lr={round_lr:.2e} "
            f"best_win={best['win_rate']:.2f}",
            flush=True,
        )
        new_o, new_a = [], []
        for e in range(eps_per_round):
            obs, _ = race.reset(seed=seed0 + rnd * 100 + e)
            while True:
                a_exp = race.query_ai().astype(np.float32)
                new_o.append(obs.copy())
                new_a.append(a_exp)
                if np.random.random() < beta:
                    a = a_exp.astype(np.int8)
                else:
                    a, _, _ = tp.act_batch(net, obs[None, :], deterministic=False)
                    a = a[0].astype(np.int8)
                obs, _, term, trunc, _ = race.step(a)
                if term or trunc:
                    break
        add_o = np.asarray(new_o, dtype=np.float32)
        add_a = np.asarray(new_a, dtype=np.float32)
        obs_bank = np.concatenate([obs_bank, add_o], axis=0)
        act_bank = np.concatenate([act_bank, add_a], axis=0)
        # Cap bank growth; keep all solo + recent field.
        if len(obs_bank) > 400_000:
            keep_solo = min(len(solo_obs), 80_000)
            keep_field = 320_000
            obs_bank = np.concatenate(
                [solo_obs[:keep_solo], obs_bank[-keep_field:]], axis=0
            )
            act_bank = np.concatenate(
                [solo_act[:keep_solo], act_bank[-keep_field:]], axis=0
            )
        print(f"[field-dag] bank={len(obs_bank)} (+{len(add_o)})", flush=True)

        tp.behavioral_clone(
            net,
            obs_bank,
            act_bank,
            device,
            epochs=6,
            lr=round_lr,
            clear_weight=0.0,  # obs[14] is dest now, not clear
        )
        probe = eval_vs_ai(
            net,
            stage=stage,
            car=car,
            nplayers=nplayers,
            max_steps=max_steps,
            n_races=eval_races,
            seed0=12_345,
        )
        solo_p = tp.eval_clear(solo, net, 99_001)
        print(
            f"[field-dag] probe win={probe['win_rate']:.2f} place={probe['mean_place']:.2f} "
            f"clear={probe['mean_clear']:.1f} solo={solo_p['clear']:.0f}",
            flush=True,
        )
        # Never accept solo-clear regression below floor.
        if float(solo_p["clear"]) < max(3.0, 0.5 * best["solo_clear"]):
            net.load_state_dict(best_state)
            print("[field-dag] revert (solo clear collapsed)", flush=True)
            continue
        key = (probe["win_rate"], -probe["mean_place"], probe["mean_clear"])
        best_key = (best["win_rate"], -best["mean_place"], best["mean_clear"])
        if key > best_key:
            best = {
                "win_rate": probe["win_rate"],
                "mean_place": probe["mean_place"],
                "mean_clear": probe["mean_clear"],
                "solo_clear": float(solo_p["clear"]),
            }
            best_state = {
                k: v.detach().cpu().clone() for k, v in net.state_dict().items()
            }
            torch.save(
                {"model": best_state, "eval": best, "field_dagger": True},
                out_dir / "policy_field_best.pt",
            )
            print(
                f"[field-dag] new best win={best['win_rate']:.2f} "
                f"place={best['mean_place']:.2f}",
                flush=True,
            )
        else:
            # Soft hold: if win dropped hard, snap back
            if probe["win_rate"] + 0.2 < best["win_rate"]:
                net.load_state_dict(best_state)
                print("[field-dag] revert (win rate drop)", flush=True)

        if best["win_rate"] >= target_win_rate and best["mean_place"] <= 2.5:
            print(
                f"[field-dag] hit target win>={target_win_rate}",
                flush=True,
            )
            break

    net.load_state_dict(best_state)
    race.close()
    solo.close()
    return best


def eval_vs_ai_multi(
    net: tp.ActorCritic,
    *,
    stage: int,
    car: int,
    nplayers: int,
    max_steps: int,
    seed0s: list[int],
    n_races_each: int = 8,
) -> dict:
    """Average win/place/clear across several seed banks (anti-overfit)."""
    parts = [
        eval_vs_ai(
            net,
            stage=stage,
            car=car,
            nplayers=nplayers,
            max_steps=max_steps,
            n_races=n_races_each,
            seed0=s,
        )
        for s in seed0s
    ]
    places = [p for part in parts for p in part["places"]]
    clears = [c for part in parts for c in part["clears"]]
    wins = sum(part["wins"] for part in parts)
    n = sum(part["n"] for part in parts)
    return {
        "wins": wins,
        "n": n,
        "win_rate": wins / max(n, 1),
        "mean_place": float(np.mean(places)),
        "mean_clear": float(np.mean(clears)),
        "places": places,
        "clears": clears,
    }


def eval_vs_ai(
    net: tp.ActorCritic,
    *,
    stage: int,
    car: int,
    nplayers: int,
    max_steps: int,
    n_races: int,
    seed0: int,
    record_path: Path | None = None,
) -> dict:
    """Policy as p0 vs AI field. Returns wins / mean place / mean clear."""
    env = NfmEnv(stage=stage, car=car, nplayers=nplayers, max_steps=max_steps)
    env.set_stall_cut(False)
    places, clears, wins = [], [], 0
    for i in range(n_races):
        rec = str(record_path) if record_path is not None and i == 0 else None
        obs, _ = env.reset(
            seed=seed0 + i, options={"record_path": rec} if rec else {}
        )
        place = 0
        clear = 0
        while True:
            a, _, _ = tp.act_batch(net, obs[None, :], deterministic=True)
            obs, _, term, trunc, info = env.step(a[0].astype(np.int8))
            place = int(info.get("place", place))
            clear = max(clear, int(info.get("clear", clear)))
            if term or trunc:
                break
        if rec:
            env.finish_recording()
        places.append(place)
        clears.append(clear)
        if place == 0 and clear >= env.need_clear():
            wins += 1
        print(
            f"[eval] race {i+1}/{n_races} place={place} clear={clear} "
            f"need={env.need_clear()}",
            flush=True,
        )
    env.close()
    return {
        "wins": wins,
        "n": n_races,
        "win_rate": wins / max(n_races, 1),
        "mean_place": float(np.mean(places)),
        "mean_clear": float(np.mean(clears)),
        "places": places,
        "clears": clears,
    }


def ppo_vs_ai(
    net: tp.ActorCritic,
    demo_obs: np.ndarray | None,
    demo_act: np.ndarray | None,
    *,
    stage: int,
    car: int,
    nplayers: int,
    games: int,
    max_steps: int,
    num_envs: int,
    update_every: int,
    lr: float,
    bc_coef: float,
    gamma: float,
    dump_every: int,
    out_dir: Path,
    train_device: torch.device,
    seed: int,
    kl_coef: float = 0.5,
    min_clear_for_update: float = 2.0,
    probe_seeds: list[int] | None = None,
) -> None:
    vec = tp.VecNfm(
        num_envs,
        stage=stage,
        car=car,
        nplayers=nplayers,
        max_steps=max_steps,
    )
    # Keep stall_cut off so races can finish; idle is already punished in reward.
    for e in vec.envs:
        e.set_stall_cut(False)
    dump_env = NfmEnv(stage=stage, car=car, nplayers=nplayers, max_steps=max_steps)
    dump_env.set_stall_cut(False)

    seed_counter = seed + 1
    vec.reset_all([seed_counter + i for i in range(num_envs)])
    seed_counter += num_envs

    traj_obs = [[] for _ in range(num_envs)]
    traj_act = [[] for _ in range(num_envs)]
    traj_logp = [[] for _ in range(num_envs)]
    traj_rew = [[] for _ in range(num_envs)]
    traj_val = [[] for _ in range(num_envs)]
    traj_done = [[] for _ in range(num_envs)]

    train_net = tp.ActorCritic().to(train_device)
    train_net.load_state_dict(net.state_dict())
    train_opt = torch.optim.Adam(train_net.parameters(), lr=lr)
    ref_net = tp.ActorCritic().to(train_device)
    ref_net.load_state_dict(net.state_dict())
    for p in ref_net.parameters():
        p.requires_grad_(False)

    # Default: 8 seeds × 8 races = 64 — 3-seed probes overfit badly (~42% vs ~28% true).
    if probe_seeds is None:
        probe_seeds = [12_345, 99_991, 55_555, 77_701, 33_333, 88_888, 11_111, 22_222]
    # Seed best from pre-train baseline so we never ship a regression.
    baseline = eval_vs_ai_multi(
        net,
        stage=stage,
        car=car,
        nplayers=nplayers,
        max_steps=max_steps,
        seed0s=probe_seeds,
        n_races_each=8,
    )
    best_probe = {
        "win_rate": baseline["win_rate"],
        "mean_clear": baseline["mean_clear"],
        "mean_place": baseline["mean_place"],
    }
    best_state = {k: v.detach().cpu().clone() for k, v in net.state_dict().items()}
    torch.save(
        {"model": best_state, "probe": best_probe, "game": 0},
        out_dir / "policy_vsai_best.pt",
    )
    print(
        f"[ppo-vs-ai] baseline win={best_probe['win_rate']:.2f} "
        f"place={best_probe['mean_place']:.2f} clear={best_probe['mean_clear']:.1f}",
        flush=True,
    )

    buf_eps = []
    rets = []
    game = 0
    t0 = time.time()
    print(
        f"[ppo-vs-ai] nplayers={nplayers} games={games} lr={lr} "
        f"bc_coef={bc_coef} kl_coef={kl_coef}",
        flush=True,
    )

    def do_update():
        if not buf_eps:
            return {}
        # Drop wrecked rollouts so PPO does not learn "die early".
        good = [e for e in buf_eps if e["clear"] >= min_clear_for_update]
        use = good if good else buf_eps
        obs = np.concatenate([e["obs"] for e in use])
        actions = np.concatenate([e["actions"] for e in use])
        logp = np.concatenate([e["logp"] for e in use])
        returns_list, adv_list = [], []
        for e in use:
            R = tp.discounted_returns(e["rewards"], e["dones"], gamma)
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
        stats = tp.ppo_update(
            train_net,
            train_opt,
            batch,
            train_device,
            demo_obs=demo_obs,
            demo_act=demo_act,
            bc_coef=bc_coef if demo_obs is not None else 0.0,
            ent_coef=0.002,
            ref_net=ref_net,
            kl_coef=kl_coef,
        )
        net.load_state_dict(
            {k: v.detach().cpu() for k, v in train_net.state_dict().items()}
        )
        buf_eps.clear()
        return stats

    while game < games:
        actions, logps, values = tp.act_batch(net, vec.obs, deterministic=False)
        for i in range(vec.n):
            traj_obs[i].append(vec.obs[i].copy())
            traj_act[i].append(actions[i].astype(np.float32))
            traj_logp[i].append(float(logps[i]))
            traj_val[i].append(float(values[i]))
        next_obs, rewards, dones, infos = vec.step(actions)
        for i in range(vec.n):
            r = float(rewards[i])
            # Extra terminal shaping toward beating the field (C++ already has some).
            if dones[i]:
                place_i = int(infos[i].get("place", nplayers - 1))
                clear_i = int(infos[i].get("clear", 0))
                need_i = int(infos[i].get("need", 6))
                if clear_i >= need_i:
                    if place_i == 0:
                        r += 120.0
                    elif place_i <= 2:
                        r += 40.0
                elif place_i == 0 and clear_i >= need_i - 1:
                    r += 20.0  # near-win pressure
            traj_rew[i].append(r)
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
                "return": float(vec.ep_ret[i]) + (r - float(rewards[i])),
                "frames": int(infos[i].get("frame", vec.ep_len[i])),
                "clear": float(infos[i].get("clear", 0)),
                "place": float(infos[i].get("place", nplayers - 1)),
            }
            for lst in (
                traj_obs[i],
                traj_act[i],
                traj_logp[i],
                traj_rew[i],
                traj_val[i],
                traj_done[i],
            ):
                lst.clear()
            game += 1
            rets.append(ep["return"])
            buf_eps.append(ep)
            vec.reset_one(i, seed_counter)
            seed_counter += 1

            if game % update_every == 0:
                stats = do_update()
                avg = float(np.mean(rets[-update_every:]))
                extra = ""
                if "bc_loss" in stats:
                    extra += f" bc={stats['bc_loss']:.3f}"
                if "kl" in stats:
                    extra += f" kl={stats['kl']:.3f}"
                print(
                    f"[ppo-vs-ai] game={game}/{games} ret_avg={avg:.2f} "
                    f"clear={ep['clear']:.0f} place={ep['place']:.0f} "
                    f"pi={stats.get('policy_loss', 0):.3f} "
                    f"H={stats.get('entropy', 0):.3f}{extra}",
                    flush=True,
                )

            if game % dump_every == 0 or game == games:
                path = out_dir / f"vsai_game_{game:04d}.nfmst"
                dump = tp.eval_clear(
                    dump_env, net, 90_000 + game, record_path=str(path)
                )
                ckpt = out_dir / f"policy_vsai_{game:04d}.pt"
                torch.save({"model": net.state_dict(), "game": game}, ckpt)
                print(
                    f"[ppo-vs-ai] dump {path.name} clear={dump['clear']:.0f} "
                    f"frames={dump['frames']} ckpt={ckpt.name}",
                    flush=True,
                )
                # Multi-seed probe so we do not overfit one lucky bank.
                probe = eval_vs_ai_multi(
                    net,
                    stage=stage,
                    car=car,
                    nplayers=nplayers,
                    max_steps=max_steps,
                    seed0s=probe_seeds,
                    n_races_each=8,
                )
                print(
                    f"[ppo-vs-ai] probe win_rate={probe['win_rate']:.2f} "
                    f"mean_place={probe['mean_place']:.2f} "
                    f"mean_clear={probe['mean_clear']:.1f}",
                    flush=True,
                )
                key = (
                    probe["win_rate"],
                    -probe["mean_place"],
                    probe["mean_clear"],
                )
                best_key = (
                    best_probe["win_rate"],
                    -best_probe["mean_place"],
                    best_probe["mean_clear"],
                )
                if key > best_key:
                    best_probe = {
                        "win_rate": probe["win_rate"],
                        "mean_clear": probe["mean_clear"],
                        "mean_place": probe["mean_place"],
                    }
                    best_state = {
                        k: v.detach().cpu().clone() for k, v in net.state_dict().items()
                    }
                    torch.save(
                        {"model": best_state, "probe": best_probe, "game": game},
                        out_dir / "policy_vsai_best.pt",
                    )
                elif (
                    best_probe["mean_clear"] >= 2.0
                    and probe["mean_clear"] < 0.4 * best_probe["mean_clear"]
                ) or (
                    best_probe["win_rate"] >= 0.3
                    and probe["win_rate"] + 0.25 < best_probe["win_rate"]
                ):
                    net.load_state_dict(best_state)
                    print(
                        f"[ppo-vs-ai] snap-back best win={best_probe['win_rate']:.2f} "
                        f"clear={best_probe['mean_clear']:.1f}",
                        flush=True,
                    )

            if game >= games:
                break

    if buf_eps:
        do_update()
    net.load_state_dict(best_state)
    vec.close()
    dump_env.close()
    print(
        f"[ppo-vs-ai] done elapsed={time.time()-t0:.1f}s "
        f"best win={best_probe['win_rate']:.2f} place={best_probe['mean_place']:.2f}",
        flush=True,
    )


def ppo_selfplay(
    net: tp.ActorCritic,
    demo_obs,
    demo_act,
    *,
    stage: int,
    car: int,
    nplayers: int,
    games: int,
    max_steps: int,
    lr: float,
    bc_coef: float,
    gamma: float,
    dump_every: int,
    out_dir: Path,
    train_device: torch.device,
    seed: int,
    update_every: int = 4,
) -> None:
    """One race, all cars share the policy (true self-play)."""
    env = NfmEnv(stage=stage, car=car, nplayers=nplayers, max_steps=max_steps)
    env.set_stall_cut(False)
    train_net = tp.ActorCritic().to(train_device)
    train_net.load_state_dict(net.state_dict())
    train_opt = torch.optim.Adam(train_net.parameters(), lr=lr)

    buf_eps = []
    game = 0
    t0 = time.time()
    print(f"[selfplay] nplayers={nplayers} games={games}", flush=True)

    while game < games:
        env.reset(seed=seed + game + 1)
        # per-car buffers
        obs_b = [[] for _ in range(nplayers)]
        act_b = [[] for _ in range(nplayers)]
        logp_b = [[] for _ in range(nplayers)]
        rew_b = [[] for _ in range(nplayers)]
        val_b = [[] for _ in range(nplayers)]
        done_b = [[] for _ in range(nplayers)]
        ep_ret = np.zeros(nplayers)

        # initial obs for all cars
        car_obs = np.stack([env.obs_player(i) for i in range(nplayers)])
        places: list = []
        clears: list = []
        while True:
            actions, logps, values = tp.act_batch(net, car_obs, deterministic=False)
            for i in range(nplayers):
                obs_b[i].append(car_obs[i].copy())
                act_b[i].append(actions[i].astype(np.float32))
                logp_b[i].append(float(logps[i]))
                val_b[i].append(float(values[i]))
            prev = car_obs
            prev_clears = list(clears) if clears else [0] * nplayers
            prev_places = list(places) if places else list(range(nplayers))
            _, _, term, trunc, info = env.step_multi(actions.astype(np.int8))
            places = info["places"]
            clears = info["clears"]
            car_obs = np.stack([env.obs_player(i) for i in range(nplayers)])
            need = float(env.need_clear())
            done = bool(term or trunc)
            for i in range(nplayers):
                # next-CP obs: [14]=dest, [17]=cp dist; clear/place live in info only
                dclear = float(clears[i]) - float(prev_clears[i])
                dplace = float(prev_places[i]) - float(places[i])  # lower place better
                dprog = float(prev[i, 17]) - float(car_obs[i, 17])
                speed = float(car_obs[i, 8]) * 200.0
                ri = (dprog if speed > 1.0 else min(dprog, 0.0)) + dclear * 20.0
                ri += -0.0015
                if abs(speed) < 5.0:
                    ri -= 0.15
                elif abs(speed) < 20.0:
                    ri -= 0.03
                if speed < -1.0:
                    ri -= 0.05
                if abs(speed) > 15.0 or dclear > 0:
                    ri += dplace * 8.0
                if car_obs[i, 14] > 0.5:
                    ri -= 1.0
                if dclear > 0 and clears[i] >= need:
                    ri += 80.0 + 25.0 * (nplayers - 1 - places[i])
                rew_b[i].append(ri)
                done_b[i].append(done)
                ep_ret[i] += ri
            if done:
                break

        for i in range(nplayers):
            buf_eps.append(
                {
                    "obs": np.asarray(obs_b[i], dtype=np.float32),
                    "actions": np.asarray(act_b[i], dtype=np.float32),
                    "logp": np.asarray(logp_b[i], dtype=np.float32),
                    "rewards": np.asarray(rew_b[i], dtype=np.float32),
                    "values": np.asarray(val_b[i], dtype=np.float32),
                    "dones": np.asarray(done_b[i], dtype=np.bool_),
                }
            )
        game += 1
        if game % update_every == 0 and buf_eps:
            obs = np.concatenate([e["obs"] for e in buf_eps])
            actions = np.concatenate([e["actions"] for e in buf_eps])
            logp = np.concatenate([e["logp"] for e in buf_eps])
            returns_list, adv_list = [], []
            for e in buf_eps:
                R = tp.discounted_returns(e["rewards"], e["dones"], gamma)
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
            stats = tp.ppo_update(
                train_net,
                train_opt,
                batch,
                train_device,
                demo_obs=demo_obs,
                demo_act=demo_act,
                bc_coef=bc_coef if demo_obs is not None else 0.0,
            )
            net.load_state_dict(
                {k: v.detach().cpu() for k, v in train_net.state_dict().items()}
            )
            buf_eps.clear()
            print(
                f"[selfplay] game={game}/{games} places={places} "
                f"clears={clears} H={stats.get('entropy', 0):.3f}",
                flush=True,
            )

        if game % dump_every == 0 or game == games:
            ckpt = out_dir / f"policy_sp_{game:04d}.pt"
            torch.save({"model": net.state_dict(), "game": game}, ckpt)
            probe = eval_vs_ai(
                net,
                stage=stage,
                car=car,
                nplayers=nplayers,
                max_steps=max_steps,
                n_races=3,
                seed0=70_000 + game,
            )
            print(
                f"[selfplay] probe win_rate={probe['win_rate']:.2f} "
                f"mean_place={probe['mean_place']:.2f}",
                flush=True,
            )

    env.close()
    print(f"[selfplay] done elapsed={time.time()-t0:.1f}s", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--stage", type=int, default=11)
    ap.add_argument("--car", type=int, default=0)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--device", default="auto")
    ap.add_argument("--out-dir", type=Path, default=REPO / "OBJ" / "superhuman")
    ap.add_argument("--max-steps", type=int, default=8000,
                    help="episode horizon for vs-AI / self-play / eval")
    ap.add_argument("--bc-max-steps", type=int, default=8000,
                    help="solo BC/demo horizon (needs room to finish race)")
    # BC floor
    ap.add_argument("--bc-demos", type=int, default=30)
    ap.add_argument("--bc-epochs", type=int, default=120)
    ap.add_argument("--bc-target-clear", type=int, default=6,
                    help="solo clear target before vs-AI (stage11 need=6)")
    ap.add_argument("--bc-eval-every", type=int, default=5)
    ap.add_argument("--dagger-rounds", type=int, default=8)
    ap.add_argument("--dagger-eps", type=int, default=12)
    ap.add_argument("--skip-bc", action="store_true")
    ap.add_argument("--load", type=Path, default=None)
    ap.add_argument(
        "--force-vs-ai",
        action="store_true",
        help="run vs-AI/self-play even if BC missed race-complete target",
    )
    # Field DAgger (primary vs-AI adaptation — preserves race-complete skill)
    ap.add_argument("--field-dagger-rounds", type=int, default=12)
    ap.add_argument("--field-dagger-eps", type=int, default=12)
    ap.add_argument("--skip-field-dagger", action="store_true")
    # vs AI PPO (optional; historically collapses clear skill)
    ap.add_argument("--vs-ai-games", type=int, default=0)
    ap.add_argument("--ppo-vs-ai", action="store_true", help="enable PPO vs-AI phase")
    ap.add_argument("--nplayers", type=int, default=7)
    ap.add_argument("--num-envs", type=int, default=32)
    ap.add_argument("--lr", type=float, default=5e-5)
    ap.add_argument("--bc-coef", type=float, default=1.0)
    ap.add_argument("--dump-every", type=int, default=50)
    ap.add_argument("--skip-vs-ai", action="store_true")
    # self-play
    ap.add_argument("--selfplay-games", type=int, default=0)
    ap.add_argument("--skip-selfplay", action="store_true")
    # final eval
    ap.add_argument("--eval-races", type=int, default=16)
    ap.add_argument(
        "--target-win-rate",
        type=float,
        default=0.5,
        help="success gate for final vs-AI eval (report only)",
    )
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    device = tp.resolve_train_device(args.device)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    print(f"[super] device={device} out={args.out_dir}", flush=True)

    net = tp.ActorCritic()
    if args.load and args.load.exists():
        net = tp.load_policy_expand(args.load, obs_dim=52, hidden=512)
        print(f"[super] loaded/expanded {args.load}", flush=True)

    demo_obs = demo_act = None
    bc_clear = -1.0
    solo_bc = NfmEnv(
        stage=args.stage,
        car=args.car,
        nplayers=1,
        max_steps=args.bc_max_steps,
    )
    race_bc = NfmEnv(
        stage=args.stage,
        car=args.car,
        nplayers=args.nplayers,
        max_steps=args.bc_max_steps,
    )
    race_bc.set_stall_cut(False)

    if not args.skip_bc and args.bc_demos > 0:
        print(
            f"[super] === Phase 1: solo race-complete BC, then guarded field mix ===",
            flush=True,
        )
        # 1) Solo demos only for the clear>=need floor (mixing race demos diluted this).
        demo_obs, demo_act, stats_s = tp.collect_ai_demos(
            solo_bc,
            args.bc_demos,
            args.seed + 10_000,
            record_first=args.out_dir / "bc_ai_demo_solo.nfmst",
        )
        print(
            f"[super] solo demos clear avg={np.mean([s['clear'] for s in stats_s]):.1f} "
            f"max={max(s['clear'] for s in stats_s)}",
            flush=True,
        )
        late = demo_obs[:, 14] >= 3.0
        if late.any():
            demo_obs = np.concatenate([demo_obs, demo_obs[late], demo_obs[late]], axis=0)
            demo_act = np.concatenate([demo_act, demo_act[late], demo_act[late]], axis=0)
        best, demo_obs, demo_act = tp.bc_until_clear(
            net,
            solo_bc,
            demo_obs,
            demo_act,
            device,
            target_clear=args.bc_target_clear,
            max_epochs=args.bc_epochs,
            eval_every=args.bc_eval_every,
            eval_seed=99_001,
            out_dir=args.out_dir,
            dagger_rounds=args.dagger_rounds,
            dagger_eps=args.dagger_eps,
        )
        solo_best = float(best["clear"])
        torch.save(
            {"model": net.state_dict(), "bc": True, "clear": solo_best},
            args.out_dir / "policy_bc_solo.pt",
        )

        # 2) Race demos for PPO BC regularizer (obs with place/traffic).
        race_o, race_a, stats_r = tp.collect_ai_demos(
            race_bc,
            max(8, args.bc_demos // 2),
            args.seed + 20_000,
            record_first=args.out_dir / "bc_ai_demo_race.nfmst",
        )
        print(
            f"[super] race demos clear avg={np.mean([s['clear'] for s in stats_r]):.1f}",
            flush=True,
        )
        demo_obs = np.concatenate([demo_obs, race_o], axis=0)
        demo_act = np.concatenate([demo_act, race_a], axis=0)

        # 3) Light field DAgger; revert if solo clear regresses.
        print("[super] guarded field DAgger…", flush=True)
        for rnd in range(2):
            pre = tp.eval_clear(solo_bc, net, 99_001)
            new_o, new_a = tp.dagger_aggregate(
                race_bc,
                net,
                n_eps=6,
                seed0=60_000 + rnd * 50,
                beta=0.6,
                max_steps=min(args.bc_max_steps, 4000),
            )
            mix_o = np.concatenate([demo_obs, new_o], axis=0)
            mix_a = np.concatenate([demo_act, new_a], axis=0)
            tp.behavioral_clone(
                net, mix_o, mix_a, device, epochs=3, lr=2e-4, clear_weight=1.0
            )
            post = tp.eval_clear(solo_bc, net, 99_001)
            print(
                f"[super] field adapt {rnd+1}: solo_clear {pre['clear']:.0f}->{post['clear']:.0f}",
                flush=True,
            )
            if post["clear"] < pre["clear"]:
                ckpt = torch.load(
                    args.out_dir / "policy_bc_solo.pt",
                    map_location="cpu",
                    weights_only=True,
                )
                net.load_state_dict(ckpt["model"])
                print("[super] reverted field adapt (solo clear dropped)", flush=True)
                break
            demo_obs, demo_act = mix_o, mix_a
            torch.save(
                {"model": net.state_dict(), "bc": True, "clear": float(post["clear"])},
                args.out_dir / "policy_bc_solo.pt",
            )

        dump = tp.eval_clear(
            solo_bc, net, 99_001, record_path=str(args.out_dir / "game_bc_eval.nfmst")
        )
        race_dump = tp.eval_clear(
            race_bc,
            net,
            12_345,
            record_path=str(args.out_dir / "game_bc_eval_race.nfmst"),
        )
        bc_clear = float(dump["clear"])
        print(
            f"[super] BC floor solo_clear={bc_clear:.0f} "
            f"race_clear={race_dump['clear']:.0f} race_frames={race_dump['frames']}",
            flush=True,
        )
        torch.save(
            {
                "model": net.state_dict(),
                "bc": True,
                "clear": bc_clear,
                "race_clear": float(race_dump["clear"]),
            },
            args.out_dir / "policy_bc.pt",
        )
    elif args.load:
        bc_clear = float("inf")
        # Solo demos protect race-complete skill during field DAgger / PPO.
        if demo_obs is None:
            print("[super] collecting solo AI demos for skill bank…", flush=True)
            demo_obs, demo_act, stats_s = tp.collect_ai_demos(
                solo_bc,
                max(16, args.bc_demos // 2),
                args.seed + 10_000,
                record_first=args.out_dir / "bc_ai_demo_solo.nfmst",
            )
            print(
                f"[super] solo demos clear avg="
                f"{np.mean([s['clear'] for s in stats_s]):.1f} frames={len(demo_obs)}",
                flush=True,
            )

    race_ready = bc_clear >= args.bc_target_clear or args.force_vs_ai or args.skip_bc
    if not race_ready:
        print(
            f"[super] BC clear={bc_clear:.0f} < target={args.bc_target_clear}; "
            f"skipping vs-AI/self-play (pass --force-vs-ai to override)",
            flush=True,
        )
        args.skip_field_dagger = True
        args.skip_vs_ai = True
        args.skip_selfplay = True

    if (
        not args.skip_field_dagger
        and args.field_dagger_rounds > 0
        and demo_obs is not None
    ):
        print("[super] === Phase 2: field DAgger vs AI ===", flush=True)
        field_dagger_until_win(
            net,
            stage=args.stage,
            car=args.car,
            nplayers=args.nplayers,
            max_steps=args.max_steps,
            device=device,
            out_dir=args.out_dir,
            solo_obs=demo_obs,
            solo_act=demo_act,
            rounds=args.field_dagger_rounds,
            eps_per_round=args.field_dagger_eps,
            target_win_rate=args.target_win_rate,
            eval_races=max(8, args.eval_races // 2),
            seed0=40_000 + args.seed,
            lr=3e-4,
        )

    if args.ppo_vs_ai and not args.skip_vs_ai and args.vs_ai_games > 0:
        print("[super] === Phase 2b: KL-regularized PPO vs AI ===", flush=True)
        ppo_vs_ai(
            net,
            demo_obs,
            demo_act,
            stage=args.stage,
            car=args.car,
            nplayers=args.nplayers,
            games=args.vs_ai_games,
            max_steps=args.max_steps,
            num_envs=args.num_envs,
            update_every=8,
            lr=args.lr,
            bc_coef=args.bc_coef,
            gamma=0.99,
            dump_every=args.dump_every,
            out_dir=args.out_dir,
            train_device=device,
            seed=args.seed,
            kl_coef=0.75,
            min_clear_for_update=2.0,
        )

    solo_bc.close()
    race_bc.close()

    if not args.skip_selfplay and args.selfplay_games > 0:
        print("[super] === Phase 3: self-play ===", flush=True)
        ppo_selfplay(
            net,
            demo_obs,
            demo_act,
            stage=args.stage,
            car=args.car,
            nplayers=args.nplayers,
            games=args.selfplay_games,
            max_steps=args.max_steps,
            lr=args.lr,
            bc_coef=args.bc_coef,
            gamma=0.99,
            dump_every=args.dump_every,
            out_dir=args.out_dir,
            train_device=device,
            seed=args.seed,
        )

    print("[super] === Final eval vs AI (multi-seed) ===", flush=True)
    # One recorded race for playback, then robust multi-seed gate.
    _ = eval_vs_ai(
        net,
        stage=args.stage,
        car=args.car,
        nplayers=args.nplayers,
        max_steps=args.max_steps,
        n_races=1,
        seed0=12345,
        record_path=args.out_dir / "final_vs_ai.nfmst",
    )
    final = eval_vs_ai_multi(
        net,
        stage=args.stage,
        car=args.car,
        nplayers=args.nplayers,
        max_steps=args.max_steps,
        seed0s=[12_345, 99_991, 55_555, 77_701],
        n_races_each=max(8, args.eval_races // 4),
    )
    torch.save({"model": net.state_dict(), "eval": final}, args.out_dir / "policy_final.pt")
    ok = (
        final["win_rate"] >= args.target_win_rate
        and final["mean_place"] <= 2.5
        and final["mean_clear"] >= args.bc_target_clear * 0.75
    )
    print(
        f"[super] FINAL win_rate={final['win_rate']:.2f} "
        f"mean_place={final['mean_place']:.2f} mean_clear={final['mean_clear']:.1f} "
        f"n={final['n']} target_win>={args.target_win_rate} ok={ok}",
        flush=True,
    )


if __name__ == "__main__":
    main()
