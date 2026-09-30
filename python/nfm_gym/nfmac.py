"""Load NFMA (.nfmac) action sidecars and paired NFMS (.nfmst) demos."""
from __future__ import annotations

import struct
from pathlib import Path
from typing import Any

import numpy as np

# Matches ActionRecorder / Control: left, right, up, down, handb
ACTION_NAMES = ("left", "right", "up", "down", "handb")
BYTES_PER_FRAME = 5
PLAYER_FRAME_BYTES = 65  # StateRecorder per-player payload


def sidecar_path(nfmst: str | Path) -> Path:
    p = Path(nfmst)
    if p.suffix == ".nfmst":
        return p.with_suffix(".nfmac")
    return Path(str(p) + ".nfmac")


def load_nfmac(path: str | Path) -> dict[str, Any]:
    """Return header + (T, 5) uint8 actions."""
    path = Path(path)
    with path.open("rb") as f:
        magic = f.read(4)
        if magic != b"NFMA":
            raise ValueError(f"{path}: bad magic {magic!r}, expected NFMA")
        ver, stage, car, nplayers = struct.unpack(">4i", f.read(16))
        raw = f.read()
    if len(raw) % BYTES_PER_FRAME != 0:
        raise ValueError(f"{path}: truncated action stream ({len(raw)} bytes)")
    actions = np.frombuffer(raw, dtype=np.uint8).reshape(-1, BYTES_PER_FRAME).copy()
    return {
        "version": ver,
        "stage": stage,
        "car": car,
        "nplayers": nplayers,
        "actions": actions,
        "path": str(path),
    }


def load_nfmst(path: str | Path) -> dict[str, Any]:
    """Minimal NFMS loader (header + raw per-player frames)."""
    path = Path(path)
    with path.open("rb") as f:
        magic = f.read(4)
        if magic != b"NFMS":
            raise ValueError(f"{path}: bad magic {magic!r}, expected NFMS")
        ver, stage, focus, npl, nlaps, nsp = struct.unpack(">6i", f.read(24))
        sc = struct.unpack(f">{npl}i", f.read(4 * npl))
        blob = f.read()
    frame_size = PLAYER_FRAME_BYTES * npl
    n_frames = len(blob) // frame_size if frame_size else 0
    blob = blob[: n_frames * frame_size]
    return {
        "version": ver,
        "stage": stage,
        "focus": focus,
        "nplayers": npl,
        "nlaps": nlaps,
        "nsp": nsp,
        "sc": sc,
        "raw": blob,
        "n_frames": n_frames,
        "path": str(path),
    }


def load_demo(nfmst: str | Path, nfmac: str | Path | None = None) -> dict[str, Any]:
    """Load paired state + actions; trims to min frame count."""
    state = load_nfmst(nfmst)
    act_path = Path(nfmac) if nfmac is not None else sidecar_path(nfmst)
    actions = load_nfmac(act_path)
    n = min(state["n_frames"], len(actions["actions"]))
    if state["n_frames"] != len(actions["actions"]):
        print(
            f"[nfmac] trim frames state={state['n_frames']} actions={len(actions['actions'])} -> {n}"
        )
    return {
        "stage": state["stage"],
        "car": actions["car"],
        "nplayers": state["nplayers"],
        "nlaps": state["nlaps"],
        "nsp": state["nsp"],
        "sc": state["sc"],
        "n_frames": n,
        "actions": actions["actions"][:n],
        "state_raw": state["raw"][: n * PLAYER_FRAME_BYTES * state["nplayers"]],
        "nfmst": state["path"],
        "nfmac": actions["path"],
    }


def actions_to_control_dict(row: np.ndarray) -> dict[str, bool]:
    return {name: bool(row[i]) for i, name in enumerate(ACTION_NAMES)}
