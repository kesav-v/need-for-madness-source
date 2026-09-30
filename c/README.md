# C NFM engine

Headless sim targeting bit-identical `.nfmst` vs Java `./record.sh N M --norender --seed`.

Legacy Ocean stubs: `ocean/nfm/` (`c_reset` / `c_step`).  
Current PufferLib wiring: `../pufferlib_nfm/` (apply onto a PufferLib clone).

## Build

```bash
cd c && make -j
```

## Bit-identical gate

```bash
make -C c test   # stage 11 / car 0 / seed 1 / --forward
```

## Gym (ctypes)

```bash
make -C c gym
# python: nfm_gym loads c/build/libnfm_gym.{dylib,so}
```

## PufferLib train (CUDA)

```bash
git clone https://github.com/PufferAI/PufferLib.git
./pufferlib_nfm/apply.sh
make -C c models gym -j
cd PufferLib && ./build.sh nfm && ./puffer train
```
