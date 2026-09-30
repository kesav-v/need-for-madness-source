"""Gymnasium environment for Need for Madness (C headless sim).

Player 0 is driven by the action passed to ``step``; other cars use built-in AI
unless ``nplayers=1`` (solo / ``disable_ai``).

Pass ``record_path`` (ctor or ``reset(options=...)``) to dump an NFMS ``.nfmst``
usable with ``./generate_video.sh``.
"""
from __future__ import annotations

import ctypes
import os
from pathlib import Path
from typing import Any, Optional, SupportsFloat, Tuple

import numpy as np

try:
    import gymnasium as gym
    from gymnasium import spaces
except ImportError as e:  # pragma: no cover
    raise ImportError(
        "nfm_gym requires gymnasium: pip install gymnasium"
    ) from e


_REPO_ROOT = Path(__file__).resolve().parents[2]
_C = _REPO_ROOT / "c"
_OBS_DIM = 52


def _lib_path() -> Path:
    if os.name == "nt":
        name = "nfm_gym.dll"
    elif (_C / "build" / "libnfm_gym.dylib").exists() or os.uname().sysname == "Darwin":
        name = "libnfm_gym.dylib"
    else:
        name = "libnfm_gym.so"
    return _C / "build" / name


def _load_lib() -> ctypes.CDLL:
    path = _lib_path()
    if not path.exists():
        raise FileNotFoundError(
            f"Missing {path}; build with: cd c && make gym"
        )
    lib = ctypes.CDLL(str(path))
    lib.nfm_gym_create.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    lib.nfm_gym_create.restype = ctypes.c_void_p
    lib.nfm_gym_destroy.argtypes = [ctypes.c_void_p]
    lib.nfm_gym_destroy.restype = None
    lib.nfm_gym_last_error.argtypes = [ctypes.c_void_p]
    lib.nfm_gym_last_error.restype = ctypes.c_char_p
    lib.nfm_gym_obs_dim.argtypes = []
    lib.nfm_gym_obs_dim.restype = ctypes.c_int
    lib.nfm_gym_reset.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.nfm_gym_reset.restype = ctypes.c_int
    lib.nfm_gym_step.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.nfm_gym_step.restype = ctypes.c_int
    lib.nfm_gym_step_ex.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.nfm_gym_step_ex.restype = ctypes.c_int
    lib.nfm_gym_step_multi.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.nfm_gym_step_multi.restype = ctypes.c_int
    lib.nfm_gym_obs_player.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.nfm_gym_obs_player.restype = ctypes.c_int
    lib.nfm_gym_need_clear.argtypes = [ctypes.c_void_p]
    lib.nfm_gym_need_clear.restype = ctypes.c_int
    lib.nfm_gym_nplayers.argtypes = [ctypes.c_void_p]
    lib.nfm_gym_nplayers.restype = ctypes.c_int
    lib.nfm_gym_step_ai.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.nfm_gym_step_ai.restype = ctypes.c_int
    lib.nfm_gym_query_ai.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.nfm_gym_query_ai.restype = ctypes.c_int
    lib.nfm_gym_finish_recording.argtypes = [ctypes.c_void_p]
    lib.nfm_gym_finish_recording.restype = ctypes.c_int
    lib.nfm_gym_set_stall_cut.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.nfm_gym_set_stall_cut.restype = None
    return lib


class NfmEnv(gym.Env):
    """Need for Madness physics env — external control of player 0.

    Action: MultiBinary(5) = [left, right, up, down, handb]
    Observation: float32[52] pose / next-CP / up to 6 opponents (see C++ SimObs).

    Recording: set ``record_path`` on the ctor or via
    ``reset(..., options={"record_path": "out.nfmst"})``. The dump closes on
    episode end, ``finish_recording()``, next reset, or ``close()``.
    """

    metadata = {"render_modes": []}

    def __init__(
        self,
        stage: int = 11,
        car: int = 0,
        nplayers: int = 1,
        max_steps: int = 5000,
        record_path: Optional[str] = None,
        repo_root: Optional[str] = None,
        assets_dir: Optional[str] = None,
    ) -> None:
        super().__init__()
        self.stage = int(stage)
        self.car = int(car)
        self.nplayers = int(nplayers)
        self.max_steps = int(max_steps)
        self.record_path = record_path
        root = Path(repo_root) if repo_root else _REPO_ROOT
        assets = Path(assets_dir) if assets_dir else (_C / "assets")
        self._lib = _load_lib()
        self._h = self._lib.nfm_gym_create(
            str(root).encode(), str(assets).encode()
        )
        if not self._h:
            raise RuntimeError("nfm_gym_create failed")
        dim = int(self._lib.nfm_gym_obs_dim())
        assert dim == _OBS_DIM
        self.observation_space = spaces.Box(
            low=-np.inf, high=np.inf, shape=(dim,), dtype=np.float32
        )
        self.action_space = spaces.MultiBinary(5)
        self._obs_buf = (ctypes.c_float * dim)()
        self._last_record_path: Optional[str] = None

    def close(self) -> None:
        if getattr(self, "_h", None):
            self._lib.nfm_gym_finish_recording(self._h)
            self._lib.nfm_gym_destroy(self._h)
            self._h = None

    def _err(self) -> str:
        e = self._lib.nfm_gym_last_error(self._h)
        return e.decode() if e else ""

    def finish_recording(self) -> int:
        """Close the current .nfmst. Returns frames written."""
        return int(self._lib.nfm_gym_finish_recording(self._h))

    def set_stall_cut(self, enabled: bool) -> None:
        self._lib.nfm_gym_set_stall_cut(self._h, 1 if enabled else 0)

    def reset(
        self,
        *,
        seed: Optional[int] = None,
        options: Optional[dict[str, Any]] = None,
    ) -> Tuple[np.ndarray, dict[str, Any]]:
        super().reset(seed=seed)
        options = options or {}
        stage = int(options.get("stage", self.stage))
        car = int(options.get("car", self.car))
        nplayers = int(options.get("nplayers", self.nplayers))
        max_steps = int(options.get("max_steps", self.max_steps))
        record_path = options.get("record_path", self.record_path)
        if seed is None:
            seed = int(self.np_random.integers(0, 2**31 - 1))
        path_arg = None
        if record_path:
            path_arg = str(record_path).encode()
            self._last_record_path = str(record_path)
        else:
            self._last_record_path = None
        rc = self._lib.nfm_gym_reset(
            self._h,
            stage,
            car,
            int(seed),
            nplayers,
            max_steps,
            path_arg,
            self._obs_buf,
        )
        if rc != 0:
            raise RuntimeError(f"nfm_gym_reset failed ({rc}): {self._err()}")
        self.stage = stage
        self.car = car
        self.nplayers = nplayers
        self.max_steps = max_steps
        obs = np.ctypeslib.as_array(self._obs_buf).copy()
        info = {"stage": stage, "car": car, "seed": int(seed)}
        if self._last_record_path:
            info["record_path"] = self._last_record_path
        return obs, info

    def step(
        self, action: Any
    ) -> Tuple[np.ndarray, SupportsFloat, bool, bool, dict[str, Any]]:
        a = np.asarray(action, dtype=np.int8).reshape(5)
        reward = ctypes.c_float(0.0)
        terminated = ctypes.c_int(0)
        truncated = ctypes.c_int(0)
        frame = ctypes.c_int(0)
        place = ctypes.c_int(0)
        clear = ctypes.c_int(0)
        rc = self._lib.nfm_gym_step_ex(
            self._h,
            int(a[0]),
            int(a[1]),
            int(a[2]),
            int(a[3]),
            int(a[4]),
            self._obs_buf,
            ctypes.byref(reward),
            ctypes.byref(terminated),
            ctypes.byref(truncated),
            ctypes.byref(frame),
            ctypes.byref(place),
            ctypes.byref(clear),
        )
        if rc != 0:
            raise RuntimeError(f"nfm_gym_step failed ({rc}): {self._err()}")
        obs = np.ctypeslib.as_array(self._obs_buf).copy()
        info: dict[str, Any] = {
            "frame": int(frame.value),
            "place": int(place.value),
            "clear": int(clear.value),
        }
        if self._last_record_path:
            info["record_path"] = self._last_record_path
        return (
            obs,
            float(reward.value),
            bool(terminated.value),
            bool(truncated.value),
            info,
        )

    def obs_player(self, player: int) -> np.ndarray:
        buf = (ctypes.c_float * _OBS_DIM)()
        rc = self._lib.nfm_gym_obs_player(self._h, int(player), buf)
        if rc != 0:
            raise RuntimeError(f"nfm_gym_obs_player failed ({rc}): {self._err()}")
        return np.ctypeslib.as_array(buf).copy()

    def need_clear(self) -> int:
        return int(self._lib.nfm_gym_need_clear(self._h))

    def step_multi(
        self, actions: Any
    ) -> Tuple[np.ndarray, SupportsFloat, bool, bool, dict[str, Any]]:
        """Self-play: actions shape (nplayers, 5). Returns p0 obs + per-car places."""
        n = int(self._lib.nfm_gym_nplayers(self._h))
        acts = np.asarray(actions, dtype=np.int32).reshape(n, 5)
        flat = (ctypes.c_int * (n * 5))(*acts.reshape(-1).tolist())
        reward = ctypes.c_float(0.0)
        terminated = ctypes.c_int(0)
        truncated = ctypes.c_int(0)
        frame = ctypes.c_int(0)
        places = (ctypes.c_int * n)()
        clears = (ctypes.c_int * n)()
        rc = self._lib.nfm_gym_step_multi(
            self._h,
            flat,
            n,
            self._obs_buf,
            ctypes.byref(reward),
            ctypes.byref(terminated),
            ctypes.byref(truncated),
            ctypes.byref(frame),
            places,
            clears,
        )
        if rc != 0:
            raise RuntimeError(f"nfm_gym_step_multi failed ({rc}): {self._err()}")
        obs = np.ctypeslib.as_array(self._obs_buf).copy()
        info = {
            "frame": int(frame.value),
            "places": [int(places[i]) for i in range(n)],
            "clears": [int(clears[i]) for i in range(n)],
            "place": int(places[0]),
            "clear": int(clears[0]),
        }
        return (
            obs,
            float(reward.value),
            bool(terminated.value),
            bool(truncated.value),
            info,
        )

    def query_ai(self) -> np.ndarray:
        """Return MultiBinary(5) from Control::preform without stepping."""
        left = ctypes.c_int(0)
        right = ctypes.c_int(0)
        up = ctypes.c_int(0)
        down = ctypes.c_int(0)
        handb = ctypes.c_int(0)
        rc = self._lib.nfm_gym_query_ai(
            self._h,
            ctypes.byref(left),
            ctypes.byref(right),
            ctypes.byref(up),
            ctypes.byref(down),
            ctypes.byref(handb),
        )
        if rc != 0:
            raise RuntimeError(f"nfm_gym_query_ai failed ({rc}): {self._err()}")
        return np.array(
            [left.value, right.value, up.value, down.value, handb.value],
            dtype=np.int8,
        )

    def step_ai(
        self,
    ) -> Tuple[np.ndarray, SupportsFloat, bool, bool, dict[str, Any]]:
        """Advance one tick with built-in AI; info['action'] is MultiBinary(5)."""
        reward = ctypes.c_float(0.0)
        terminated = ctypes.c_int(0)
        truncated = ctypes.c_int(0)
        frame = ctypes.c_int(0)
        left = ctypes.c_int(0)
        right = ctypes.c_int(0)
        up = ctypes.c_int(0)
        down = ctypes.c_int(0)
        handb = ctypes.c_int(0)
        place = ctypes.c_int(0)
        clear = ctypes.c_int(0)
        rc = self._lib.nfm_gym_step_ai(
            self._h,
            self._obs_buf,
            ctypes.byref(reward),
            ctypes.byref(terminated),
            ctypes.byref(truncated),
            ctypes.byref(frame),
            ctypes.byref(left),
            ctypes.byref(right),
            ctypes.byref(up),
            ctypes.byref(down),
            ctypes.byref(handb),
            ctypes.byref(place),
            ctypes.byref(clear),
        )
        if rc != 0:
            raise RuntimeError(f"nfm_gym_step_ai failed ({rc}): {self._err()}")
        obs = np.ctypeslib.as_array(self._obs_buf).copy()
        action = np.array(
            [left.value, right.value, up.value, down.value, handb.value],
            dtype=np.int8,
        )
        info: dict[str, Any] = {
            "frame": int(frame.value),
            "action": action,
            "place": int(place.value),
            "clear": int(clear.value),
        }
        if self._last_record_path:
            info["record_path"] = self._last_record_path
        return (
            obs,
            float(reward.value),
            bool(terminated.value),
            bool(truncated.value),
            info,
        )


def make_nfm_env(**kwargs: Any) -> NfmEnv:
    return NfmEnv(**kwargs)


# Gymnasium registration (optional import of this module)
try:
    gym.register(
        id="NFM-v0",
        entry_point="nfm_gym.env:NfmEnv",
        kwargs={"stage": 11, "car": 0, "nplayers": 1, "max_steps": 5000},
    )
except Exception:
    pass
