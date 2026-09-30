#ifndef NFM_OCEAN_H
#define NFM_OCEAN_H

/*
 * PufferLib Ocean-facing env for Need for Madness.
 * Template: https://github.com/PufferAI/PufferLib (Squared / Target / Nethack).
 *
 * Contract (must hold for `puffer build` / vecenv):
 *  - Env struct has Log log first-ish, then observations/actions/rewards/terminals
 *  - Log fields are floats only; last field is n
 *  - c_reset / c_step / c_render / c_close
 *  - c_step auto-resets on terminal (Puffer has no truncations yet)
 *  - NEVER free observations/actions/rewards/terminals (Python owns them)
 *  - Keep C simple; heavy physics lives behind NfmSim*
 *
 * Obs: float32[NFM_OBS_DIM] (see fill_obs layout in sim).
 * Act: MultiDiscrete([2,2,2,2,2]) → int actions[5] = {left,right,up,down,handb}.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  NFM_OBS_DIM = 52,
  NFM_ACTION_DIM = 5,
  NFM_MAX_PLAYERS = 8
};

/** Required. Only floats; n must be last. */
typedef struct Log {
  float perf;            /* 1 if finished 1st (or clear goal), else 0 */
  float score;           /* clear / need at episode end */
  float episode_return;
  float episode_length;
  float place;           /* 0..1 normalized finishing place */
  float clear_frac;      /* clear / need */
  float n;               /* required aggregator count */
} Log;

struct NfmSim; /* opaque full race session (internal engine) */

/**
 * Required Ocean env struct. Name matches env file convention (Nfm).
 * Buffer pointers are set by PufferLib vecenv — do not allocate/free them here.
 */
typedef struct Nfm {
  Log log;

  float* observations;       /* [NFM_OBS_DIM] float32 */
  int* actions;              /* [NFM_ACTION_DIM] each in {0,1} */
  float* rewards;            /* [1] */
  unsigned char* terminals;  /* [1] */

  /* kwargs / ini */
  int stage;
  int car;
  int seed;
  int nplayers;
  int max_steps;
  int stall_cut; /* 1 = stall truncate+penalty (default), 0 = BC-friendly */

  char repo_root[512];
  char assets_dir[512];

  /* episode */
  int tick;
  float ep_return;
  int episode_id; /* bump each reset for staggered seeds across vec envs */

  struct NfmSim* sim;
} Nfm;

/** Optional one-time setup after buffers + kwargs applied (loads models). */
int nfm_init(Nfm* env);

void c_reset(Nfm* env);
void c_step(Nfm* env);
void c_render(Nfm* env);
void c_close(Nfm* env);

#ifdef __cplusplus
}
#endif

#endif /* NFM_OCEAN_H */
