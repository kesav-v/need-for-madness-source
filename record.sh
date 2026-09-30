#!/bin/bash
# Record an auto-race to mp4 (Java), or dump per-tick world state with --norender (C by default).
# Usage: ./record.sh <stage> <car> [out.mp4|.nfmst] [--forward] [--policy weights.bin] [--timeout 0] [--fps 30] [--music]
#          [--norender] [--profile] [--seed N] [--n N] [--java] [--disable-ai]
# --policy PATH.bin  (or a trailing *.bin arg) drives player 0 with a PufferLib checkpoint
#                    instead of --forward / built-in AI; implies --norender.
# Default --timeout is 0 (no wall-clock cutoff) unless explicitly passed.
# --n N runs N games and keeps only the last recording; prints total wall-clock time.
# --norender uses c/build/nfm_sim unless --java or --policy. mp4 always Java.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

STAGE="${1:?stage number required (1-27)}"
CAR="${2:?car index required (0-15)}"
shift 2

OUT=""
NORENDER=0
USE_JAVA=0
HASTIMEOUT=0
N_GAMES=1
FORWARD=0
DISABLE_AI=0
POLICY=""
SEED=""
TIMEOUT=0
EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --norender) NORENDER=1; shift ;;
    --java) USE_JAVA=1; shift ;;
    --forward) FORWARD=1; EXTRA+=("$1"); shift ;;
    --policy)
      POLICY="$2"
      if [[ -z "$POLICY" ]]; then
        echo "[record] --policy requires a .bin path" >&2
        exit 1
      fi
      shift 2 ;;
    --disable-ai) DISABLE_AI=1; shift ;;
    --music|--profile|--full-fx) EXTRA+=("$1"); shift ;;
    --timeout)
      HASTIMEOUT=1
      TIMEOUT="$2"
      EXTRA+=("$1" "$2"); shift 2 ;;
    --fps)
      EXTRA+=("$1" "$2"); shift 2 ;;
    --seed)
      SEED="$2"
      EXTRA+=("$1" "$2"); shift 2 ;;
    --n)
      N_GAMES="$2"
      if ! [[ "$N_GAMES" =~ ^[1-9][0-9]*$ ]]; then
        echo "[record] --n requires a positive integer, got: $N_GAMES" >&2
        exit 1
      fi
      shift 2 ;;
    -*) EXTRA+=("$1"); shift ;;
    *.bin)
      POLICY="$1"
      shift ;;
    *) OUT="$1"; shift ;;
  esac
done

if [[ -n "$POLICY" ]]; then
  if [[ "$POLICY" != /* ]]; then
    POLICY="$ROOT/$POLICY"
  fi
  if [[ ! -f "$POLICY" ]]; then
    echo "[record] policy not found: $POLICY" >&2
    exit 1
  fi
  if [[ "$FORWARD" -eq 1 ]]; then
    echo "[record] --policy replaces --forward; ignoring --forward" >&2
    FORWARD=0
  fi
  NORENDER=1
  USE_JAVA=0
fi

if [[ "$HASTIMEOUT" -eq 0 ]]; then
  EXTRA+=(--timeout 0)
  TIMEOUT=0
fi

# Native sim path for --norender (unless --java or --policy); mp4 always Java.
USE_NATIVE=0
USE_POLICY=0
SIM=""
ASSETS=""
if [[ -n "$POLICY" ]]; then
  USE_POLICY=1
elif [[ "$NORENDER" -eq 1 && "$USE_JAVA" -eq 0 ]]; then
  USE_NATIVE=1
fi

if [[ "$USE_NATIVE" -eq 1 ]]; then
  if ! make -C "$ROOT/c" -j >/dev/null 2>&1 || [[ ! -x "$ROOT/c/build/nfm_sim" ]]; then
    echo "[record] c/ nfm_sim build failed:" >&2
    make -C "$ROOT/c" -j >&2 || true
    exit 1
  fi
  SIM="$ROOT/c/build/nfm_sim"
  ASSETS="$ROOT/c/assets"
elif [[ "$USE_POLICY" -eq 0 ]]; then
  if ! ./build.sh -nowarn >/dev/null 2>&1; then
    echo "[record] build failed:" >&2
    ./build.sh -nowarn >&2 || true
    exit 1
  fi
fi

JAVA="$(printf "%s%s%s" "${JREPATH:-}" "$(test -z "${JREPATH:-}" || echo /)" java)"

if [[ "$NORENDER" -eq 1 ]]; then
  if [[ -z "$OUT" ]]; then
    OUT="$ROOT/OBJ/sim_stage${STAGE}_car${CAR}.nfmst"
  elif [[ "$OUT" != /* ]]; then
    OUT="$ROOT/$OUT"
  fi
else
  if [[ -z "$OUT" ]]; then
    OUT="$ROOT/OBJ/record_stage${STAGE}_car${CAR}.mp4"
  elif [[ "$OUT" != /* ]]; then
    OUT="$ROOT/$OUT"
  fi
fi

if [[ "$USE_POLICY" -eq 1 ]]; then
  echo "Sim (puffer) stage=$STAGE car=$CAR policy=$POLICY -> $OUT"
elif [[ "$NORENDER" -eq 1 ]]; then
  if [[ "$USE_NATIVE" -eq 1 ]]; then
    echo "Sim (c) stage=$STAGE car=$CAR n=$N_GAMES -> $OUT"
  else
    echo "Sim (java) stage=$STAGE car=$CAR n=$N_GAMES -> $OUT"
  fi
else
  echo "Recording stage=$STAGE car=$CAR n=$N_GAMES -> $OUT"
fi

now() { python3 -c 'import time; print(time.perf_counter())'; }

# Single in-place progress bar for multi-game Java runs (stderr).
draw_progress() {
  local cur="$1" total="$2"
  local width=40
  local filled=$((cur * width / total))
  local empty=$((width - filled))
  printf "\r[" >&2
  if [[ "$filled" -gt 0 ]]; then
    printf "%*s" "$filled" "" | tr ' ' '#' >&2
  fi
  if [[ "$empty" -gt 0 ]]; then
    printf "%*s" "$empty" "" | tr ' ' '-' >&2
  fi
  printf "] %d/%d" "$cur" "$total" >&2
}

START="$(now)"

if [[ "$USE_POLICY" -eq 1 ]]; then
  if [[ "$N_GAMES" -ne 1 ]]; then
    echo "[record] --policy currently supports --n 1 only" >&2
    exit 1
  fi
  dump_args=("$POLICY" "$OUT" --stage "$STAGE" --car "$CAR")
  if [[ -n "$SEED" ]]; then
    dump_args+=(--seed "$SEED")
  fi
  if [[ "$TIMEOUT" -gt 0 ]]; then
    dump_args+=(--max-steps "$TIMEOUT")
  fi
  "$ROOT/pufferlib_nfm/dump_nfmst.sh" "${dump_args[@]}"
elif [[ "$USE_NATIVE" -eq 1 ]]; then
  # In-process --n inside nfm_sim (load models once; only last game writes --out).
  args=(--root "$ROOT" --assets "$ASSETS"
    --stage "$STAGE" --car "$CAR" --out "$OUT" --timeout "$TIMEOUT" --n "$N_GAMES")
  if [[ -n "$SEED" ]]; then
    args+=(--seed "$SEED")
  fi
  if [[ "$FORWARD" -eq 1 ]]; then
    args+=(--forward)
  fi
  if [[ "$DISABLE_AI" -eq 1 ]]; then
    args+=(--disable-ai)
  fi
  "$SIM" "${args[@]}"
else
  run_one() {
    local run_out="$1"
    if [[ "$NORENDER" -eq 1 ]]; then
      (cd "$ROOT/OBJ" && $JAVA -Djava.awt.headless=true -jar -Xms1G dev_game.jar \
        --stage "$STAGE" --car "$CAR" --out "$run_out" ${EXTRA[@]+"${EXTRA[@]}"} --norender)
    else
      (cd "$ROOT/OBJ" && $JAVA -jar -Xms1G dev_game.jar \
        --stage "$STAGE" --car "$CAR" --out "$run_out" ${EXTRA[@]+"${EXTRA[@]}"})
    fi
  }

  for ((i = 1; i <= N_GAMES; i++)); do
    if [[ "$i" -eq "$N_GAMES" ]]; then
      RUN_OUT="$OUT"
    else
      # BSD mktemp requires XXXXXX at the end of the template.
      RUN_OUT="$(mktemp "${TMPDIR:-/tmp}/nfm-record.XXXXXX")"
    fi

    if [[ "$N_GAMES" -gt 1 ]]; then
      LOG="$(mktemp "${TMPDIR:-/tmp}/nfm-record-log.XXXXXX")"
      if ! run_one "$RUN_OUT" >"$LOG" 2>&1; then
        printf "\n" >&2
        cat "$LOG" >&2
        rm -f "$LOG" "$RUN_OUT"
        exit 1
      fi
      rm -f "$LOG"
      draw_progress "$i" "$N_GAMES"
    else
      run_one "$RUN_OUT"
    fi

    if [[ "$i" -ne "$N_GAMES" ]]; then
      rm -f "$RUN_OUT"
    fi
  done
  if [[ "$N_GAMES" -gt 1 ]]; then
    printf "\n" >&2
  fi
fi

END="$(now)"
ELAPSED="$(python3 -c "print('{:.3f}'.format($END - $START))")"
echo "[record] $N_GAMES game(s) wall clock: ${ELAPSED}s"
