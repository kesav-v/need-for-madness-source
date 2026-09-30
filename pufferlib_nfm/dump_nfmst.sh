#!/usr/bin/env bash
# Dump one .nfmst by rolling out a PufferLib .bin with the CPU eval binary.
# Usage:
#   ./pufferlib_nfm/dump_nfmst.sh checkpoints/nfm/<run>/<step>.bin [out.nfmst]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${PUFFER_DIR:-$ROOT/PufferLib}"
BIN="${1:?usage: $0 path/to/weights.bin [out.nfmst]}"
OUT="${2:-$ROOT/OBJ/puffer_eval.nfmst}"

if [ ! -f "$BIN" ]; then
  echo "missing weights: $BIN" >&2
  exit 1
fi
if [ ! -d "$DEST" ]; then
  echo "missing $DEST — clone + ./pufferlib_nfm/apply.sh first" >&2
  exit 1
fi

"$ROOT/pufferlib_nfm/apply.sh"
make -C "$ROOT/c" models gym -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
( cd "$DEST" && ./build.sh nfm --cpu )

mkdir -p "$(dirname "$OUT")"
# Absolute out path so cwd=PufferLib still writes where we expect.
OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
BIN_ABS="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"

( cd "$DEST" && ./nfm "$BIN_ABS" --headless \
    --eval_episodes=1 \
    --env.record_path="$OUT_ABS" \
    --env.stall_cut=0 \
    --env.max_steps=8000 )

echo "nfmst: $OUT_ABS"
