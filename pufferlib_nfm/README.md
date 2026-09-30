# PufferLib NFM overlay

Does **not** vendor PufferLib (large upstream). Apply onto a checkout at:

`6ffa5b10dbbbe4d1e8288367c7d9d3acd3bad4a2` (or nearby `master`).

## Colab / Linux GPU

```bash
# from repo root
./pufferlib_nfm/colab_deps.sh          # libomp, mesa GL, X11 (once per runtime)
git clone --depth 1 https://github.com/PufferAI/PufferLib.git
./pufferlib_nfm/apply.sh
cd c && make models gym -j && cd ..
cd PufferLib && ./build.sh nfm && ./puffer train
```

## Dump `.nfmst` from a `.bin` checkpoint

```bash
# via record.sh (preferred; replaces --forward)
./record.sh 11 0 out.nfmst --policy PufferLib/checkpoints/nfm/<run>/<step>.bin
./record.sh 11 0 out.nfmst path/to/weights.bin   # trailing *.bin also works

# helper
./pufferlib_nfm/dump_nfmst.sh path/to/weights.bin out.nfmst --stage 11 --car 0
```

## Files

| Path | Role |
|------|------|
| `ocean/nfm/nfm.h` | Ocean env (`puf_*`, obs 52, MultiDiscrete 5×2) |
| `config/nfm.ini` | Train defaults (paths assume `PufferLib/` next to `c/`) |
| `build.sh.patch` | Links `../c` engine objects; bash3-safe `PUFFER_*` define |
