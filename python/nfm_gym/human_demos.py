"""Build policy obs/actions from Java human .nfmst + .nfmac demos."""
from __future__ import annotations

import math
import re
import struct
from pathlib import Path
from typing import Any, Iterable

import numpy as np

from nfm_gym.nfmac import load_demo, sidecar_path

PLAYER_FRAME_BYTES = 65
_REPO = Path(__file__).resolve().parents[2]


def parse_stage_checkpoints(stage_path: Path) -> dict[str, Any]:
    """Mirror C++ load_stage CP extraction (x/z/typ/nsp/pcs/nlaps)."""
    text = stage_path.read_text()
    cp_x: list[int] = []
    cp_z: list[int] = []
    cp_typ: list[int] = []
    nsp = 0
    pcs = 0
    nlaps = 0

    def getint(key: str, line: str, i: int) -> int:
        m = re.search(rf"{re.escape(key)}\(([^)]*)\)", line)
        if not m:
            raise ValueError(f"no {key}(...) in {line!r}")
        return int(m.group(1).split(",")[i])

    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("nlaps"):
            nlaps = getint("nlaps", line, 0)
            continue
        if line.startswith("set"):
            paren = line.find(")")
            if paren < 0 or not line[paren:].startswith(")p"):
                continue
            cp_x.append(getint("set", line, 1))
            cp_z.append(getint("set", line, 2))
            suf = line[paren:]
            if suf.startswith(")pt"):
                cp_typ.append(-1)
            elif suf.startswith(")pr"):
                cp_typ.append(-2)
            elif suf.startswith(")po"):
                cp_typ.append(-3)
            elif suf.startswith(")ph"):
                cp_typ.append(-4)
            else:
                cp_typ.append(0)
            continue
        if line.startswith("chk"):
            cp_x.append(getint("chk", line, 1))
            cp_z.append(getint("chk", line, 2))
            rot = getint("chk", line, 3)
            cp_typ.append(1 if rot == 0 else 2)
            pcs = len(cp_x) - 1
            nsp += 1
            continue
    return {
        "x": cp_x,
        "z": cp_z,
        "typ": cp_typ,
        "n": len(cp_x),
        "nsp": nsp,
        "pcs": pcs,
        "nlaps": nlaps,
        "need": max(1, nlaps * nsp),
    }


def _infer_pcleared(cp: dict[str, Any], clear: int) -> int:
    if clear <= 0 or cp["nsp"] <= 0:
        return int(cp["pcs"])
    want = ((clear - 1) % cp["nsp"]) + 1
    n119 = 0
    for i, t in enumerate(cp["typ"]):
        if t > 0:
            n119 += 1
            if n119 == want:
                return i
    return int(cp["pcs"])


def _next_checkpoint_index(cp: dict[str, Any], pcleared: int) -> int:
    n = cp["n"]
    if n <= 0:
        return 0
    nxt = pcleared + 1
    if nxt >= n:
        nxt = 0
    start = nxt
    typ = cp["typ"]
    while typ[nxt] <= 0:
        nxt += 1
        if nxt >= n:
            nxt = 0
        if nxt == start:
            break
    return nxt


def _deg_sin(deg: int) -> float:
    return math.sin(math.radians(deg % 360))


def _deg_cos(deg: int) -> float:
    return math.cos(math.radians(deg % 360))


def frame_to_obs(frame: bytes, cp: dict[str, Any]) -> np.ndarray:
    """NFMS per-player payload → SimObs float32[52] (solo; opp slots 0)."""
    (
        x,
        y,
        z,
        xz,
        xy,
        zy,
        wxz,
        wzy,
    ) = struct.unpack(">8i", frame[0:32])
    speed, power = struct.unpack(">2f", frame[32:40])
    hitmag, squash, _cn, mxz, cxz, clear = struct.unpack(">6i", frame[40:64])
    dest = frame[64]

    obs = np.zeros(52, dtype=np.float32)
    obs[0] = x / 5000.0
    obs[1] = y / 500.0
    obs[2] = z / 5000.0
    obs[3] = xz / 360.0
    obs[4] = xy / 360.0
    obs[5] = zy / 360.0
    obs[6] = wxz / 36.0
    obs[7] = wzy / 30.0
    obs[8] = speed / 200.0
    obs[9] = power / 100.0
    obs[10] = hitmag / 10000.0
    obs[11] = float(squash)
    obs[12] = mxz / 360.0
    obs[13] = cxz / 360.0
    obs[14] = 1.0 if dest else 0.0

    pcleared = _infer_pcleared(cp, clear)
    nxt = _next_checkpoint_index(cp, pcleared)
    dx = (cp["x"][nxt] - x) / 5000.0
    dz = (cp["z"][nxt] - z) / 5000.0
    s = _deg_sin(xz)
    co = _deg_cos(xz)
    ego_f = -dx * s + dz * co
    ego_r = dx * co + dz * s
    dist = math.sqrt(dx * dx + dz * dz)
    obs[15] = ego_f
    obs[16] = ego_r
    obs[17] = dist
    if dist > 1e-5:
        obs[18] = ego_r / dist
        obs[19] = ego_f / dist
    else:
        obs[18] = 0.0
        obs[19] = 1.0

    need = float(cp["need"])
    obs[20] = clear / need
    obs[21] = 0.0
    return obs


def iter_human_nfmst(roots: Iterable[Path]) -> list[Path]:
    out: list[Path] = []
    for root in roots:
        root = Path(root)
        if not root.exists():
            continue
        for p in sorted(root.rglob("*.nfmst")):
            if p.stat().st_size < 64:
                continue
            if not sidecar_path(p).exists() or sidecar_path(p).stat().st_size < 20:
                continue
            out.append(p)
    return out


def load_human_dataset(
    demo_globs: list[str] | None = None,
    *,
    stage: int = 11,
    require_finish: bool = True,
    repo: Path | None = None,
) -> dict[str, Any]:
    """Load all usable human demos under OBJ/human_demos*.

    Returns obs [N,52], actions [N,5], plus per-episode stats.
    """
    repo = Path(repo) if repo else _REPO
    if demo_globs is None:
        roots = sorted(repo.glob("OBJ/human_demos*"))
    else:
        roots = []
        for g in demo_globs:
            roots.extend(sorted(repo.glob(g)))
    stage_path = repo / "OBJ" / "stages" / f"{stage}.txt"
    cp = parse_stage_checkpoints(stage_path)
    paths = iter_human_nfmst(roots)

    obs_list: list[np.ndarray] = []
    act_list: list[np.ndarray] = []
    stats: list[dict[str, Any]] = []
    for path in paths:
        demo = load_demo(path)
        if demo["stage"] != stage:
            print(f"[human] skip {path.name}: stage {demo['stage']}")
            continue
        n = demo["n_frames"]
        npl = demo["nplayers"]
        raw = demo["state_raw"]
        acts = demo["actions"]
        max_clear = 0
        ep_obs = []
        ep_act = []
        for t in range(n):
            fr = raw[t * PLAYER_FRAME_BYTES * npl : t * PLAYER_FRAME_BYTES * npl + PLAYER_FRAME_BYTES]
            clear = struct.unpack(">i", fr[40 + 20 : 40 + 24])[0]
            max_clear = max(max_clear, clear)
            ep_obs.append(frame_to_obs(fr, cp))
            ep_act.append(acts[t].astype(np.float32))
        finished = max_clear >= cp["need"]
        if require_finish and not finished:
            print(f"[human] skip DNF {path} clear={max_clear}/{cp['need']}")
            continue
        obs_list.extend(ep_obs)
        act_list.extend(ep_act)
        stats.append(
            {
                "path": str(path),
                "frames": n,
                "max_clear": max_clear,
                "finished": finished,
                "secs": n / 30.0,
                "car": demo["car"],
            }
        )
        print(
            f"[human] {path.relative_to(repo)} frames={n} clear={max_clear} "
            f"~{n/30:.0f}s {'FINISH' if finished else 'DNF'}",
            flush=True,
        )

    if not obs_list:
        raise RuntimeError("no human demos loaded")
    obs = np.asarray(obs_list, dtype=np.float32)
    actions = np.asarray(act_list, dtype=np.float32)
    finishes = [s for s in stats if s["finished"]]
    print(
        f"[human] dataset episodes={len(stats)} finishes={len(finishes)} "
        f"frames={len(obs)} mean_finish_t="
        f"{np.mean([s['frames'] for s in finishes]):.0f}"
        if finishes
        else f"[human] dataset episodes={len(stats)} finishes=0 frames={len(obs)}",
        flush=True,
    )
    return {"obs": obs, "actions": actions, "stats": stats, "cp": cp}
