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

if ! grep -q 'ENV" = "nfm"' "$DEST/build.sh" 2>/dev/null; then
  patch -d "$DEST" -p1 < "$OVERLAY/build.sh.patch"
else
  echo "build.sh already has nfm block; skipping patch"
fi

echo "Applied NFM overlay -> $DEST"
echo "Next: make -C \"$ROOT/c\" models gym && (cd \"$DEST\" && ./build.sh nfm && ./puffer train)"
