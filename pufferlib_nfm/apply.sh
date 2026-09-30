#!/usr/bin/env bash
# Apply NFM Ocean overlay into ./PufferLib (clone first).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${PUFFER_DIR:-$ROOT/PufferLib}"
OVERLAY="$(cd "$(dirname "$0")" && pwd)"

if [ ! -d "$DEST" ]; then
  echo "Missing $DEST — clone PufferLib there first:" >&2
  echo "  git clone https://github.com/PufferAI/PufferLib.git \"$DEST\"" >&2
  exit 1
fi

mkdir -p "$DEST/ocean/nfm" "$DEST/config"
cp "$OVERLAY/ocean/nfm/nfm.h" "$DEST/ocean/nfm/nfm.h"
cp "$OVERLAY/config/nfm.ini" "$DEST/config/nfm.ini"

python3 - <<'PY' "$DEST/build.sh" "$OVERLAY/build.sh.patch"
import pathlib, re, subprocess, sys

build = pathlib.Path(sys.argv[1])
patch = pathlib.Path(sys.argv[2])
text = build.read_text()

# 1) nfm env block
if 'ENV" = "nfm"' not in text and "[ \"$ENV\" = \"nfm\" ]" not in text:
    needle = 'elif [ -d "ocean/$ENV" ]; then'
    block = '''elif [ "$ENV" = "nfm" ]; then
    SRC_DIR="ocean/$ENV"
    NFM_ROOT="$(cd .. && pwd)"
    echo "Building NFM C engine at $NFM_ROOT/c ..."
    make -C "$NFM_ROOT/c" models gym -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
    INCLUDES+=(-I"$NFM_ROOT/c/include")
    EXTRA_CFLAGS+=(-I"$NFM_ROOT/c/include")
    # Link engine objects (exclude gym/ocean glue).
    for _nfm_o in medium trackers state_recorder env_flags control mad \\
                  car_define conto stage_load checkpoints sort_cars sim; do
        EXTRA_LDFLAGS+=("$NFM_ROOT/c/build/${_nfm_o}.o")
    done
'''
    if needle not in text:
        # try official patch file as last resort
        r = subprocess.run(["patch", "-p1", "--dry-run"], cwd=build.parent,
                           input=patch.read_bytes(), capture_output=True)
        if r.returncode == 0:
            subprocess.run(["patch", "-p1"], cwd=build.parent, input=patch.read_bytes(), check=True)
            text = build.read_text()
        else:
            raise SystemExit(f"cannot find insertion point in {build}")
    else:
        text = text.replace(needle, block + needle, 1)

# 2) ccache optional (Colab often lacks it)
old_nvcc = 'NVCC="ccache $CUDA_HOME/bin/nvcc"'
new_nvcc = (
    'NVCC="${NVCC:-$(command -v ccache >/dev/null && '
    'echo "ccache $CUDA_HOME/bin/nvcc" || echo "$CUDA_HOME/bin/nvcc")}"'
)
if old_nvcc in text:
    text = text.replace(old_nvcc, new_nvcc, 1)

# 2b) Linux OpenMP: upstream -lomp5 is missing on Colab/Ubuntu.
# nvcc host-links with g++, so use -lgomp (libgomp1), not LLVM -lomp.
text = text.replace('OMP_LIB=-lomp5', 'OMP_LIB=-lgomp', 1)
text = text.replace('OMP_LIB=-lomp\n', 'OMP_LIB=-lgomp\n', 1)

# 3) bash3-safe PUFFER_$ENV
text2, n = re.subn(
    r"EXTRA_CFLAGS\+=\(-DPUFFER_\$\{ENV\^\^\}\)",
    'ENV_UPPER=$(printf \'%s\' "$ENV" | tr \'[:lower:]\' \'[:upper:]\')\n'
    "EXTRA_CFLAGS+=(-DPUFFER_${ENV_UPPER})",
    text,
    count=1,
)
text = text2

build.write_text(text)
print("patched", build)
PY

if [ ! -f "$DEST/ocean/nfm/nfm.h" ] || ! grep -q 'ENV" = "nfm"' "$DEST/build.sh"; then
  echo "ERROR: overlay incomplete (missing ocean/nfm or build.sh nfm block)" >&2
  exit 1
fi

echo "Applied NFM overlay -> $DEST"
echo "Next: make -C \"$ROOT/c\" models gym && (cd \"$DEST\" && ./build.sh nfm && ./puffer train)"
