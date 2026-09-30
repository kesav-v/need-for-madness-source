#!/usr/bin/env bash
# Dump one .nfmst by rolling out a PufferLib .bin with the CPU eval binary.
# Usage:
#   ./pufferlib_nfm/dump_nfmst.sh path/to/weights.bin [out.nfmst] \
#       [--stage N] [--car M] [--seed S] [--max-steps T]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${PUFFER_DIR:-$ROOT/PufferLib}"
BIN="${1:?usage: $0 path/to/weights.bin [out.nfmst] [--stage N] [--car M] [--seed S]}"
shift
OUT="$ROOT/OBJ/puffer_eval.nfmst"
STAGE=11
CAR=0
SEED=0
MAX_STEPS=8000

if [[ $# -gt 0 && "$1" != -* ]]; then
  OUT="$1"
  shift
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stage) STAGE="$2"; shift 2 ;;
    --car) CAR="$2"; shift 2 ;;
    --seed) SEED="$2"; shift 2 ;;
    --max-steps) MAX_STEPS="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

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
OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
BIN_ABS="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"

echo "[dump] stage=$STAGE car=$CAR seed=$SEED bin=$BIN_ABS -> $OUT_ABS"
( cd "$DEST" && ./nfm "$BIN_ABS" --headless \
    --eval_episodes=1 \
    --env.record_path="$OUT_ABS" \
    --env.stage="$STAGE" \
    --env.car="$CAR" \
    --env.seed="$SEED" \
    --env.nplayers=1 \
    --env.stall_cut=0 \
    --env.max_steps="$MAX_STEPS" )

echo "nfmst: $OUT_ABS"
