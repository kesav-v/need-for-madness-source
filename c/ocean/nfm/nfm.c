#include "nfm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ocean entrypoints. Physics lives in NfmSim (ported from cpp/); until the C
 * sim is wired, c_step is a no-op stub so the binding shape can be reviewed.
 * Replace stub body once c/src/sim.c exists — do not change the buffer contract.
 */

/* Forward decls — implemented by c/src/sim.c once ported. */
struct NfmSim;
struct NfmSim* nfm_sim_create(const char* repo_root, const char* assets_dir);
void nfm_sim_destroy(struct NfmSim* s);
int nfm_sim_reset(struct NfmSim* s, int stage, int car, int seed, int nplayers,
                  int max_steps, float* obs_out);
int nfm_sim_step(struct NfmSim* s, const int actions[NFM_ACTION_DIM],
                 float* obs_out, float* reward, int* terminated, int* truncated,
                 int* place, int* clear, int* need);
void nfm_sim_set_stall_cut(struct NfmSim* s, int enabled);

static void nfm_add_log(Nfm* env, float reward, int place, int clear, int need) {
  const float need_f = need > 0 ? (float)need : 1.f;
  const float clear_frac = (float)clear / need_f;
  const float place_n =
      (env->nplayers > 1)
          ? (float)place / (float)(env->nplayers - 1)
          : 0.f;
  env->log.perf += (place == 0 && clear >= need) ? 1.f : 0.f;
  env->log.score += clear_frac;
  env->log.episode_return += env->ep_return + reward;
  env->log.episode_length += (float)env->tick;
  env->log.place += place_n;
  env->log.clear_frac += clear_frac;
  env->log.n += 1.f;
}

int nfm_init(Nfm* env) {
  if (!env) return -1;
  if (env->stage <= 0) env->stage = 11;
  if (env->car < 0) env->car = 0;
  if (env->nplayers <= 0) env->nplayers = 7;
  if (env->max_steps <= 0) env->max_steps = 100000;
  if (env->repo_root[0] == '\0')
    snprintf(env->repo_root, sizeof(env->repo_root), ".");
  if (env->assets_dir[0] == '\0')
    snprintf(env->assets_dir, sizeof(env->assets_dir), "c/assets");
  if (!env->sim) {
    env->sim = nfm_sim_create(env->repo_root, env->assets_dir);
    if (!env->sim) return -2;
  }
  nfm_sim_set_stall_cut(env->sim, env->stall_cut);
  return 0;
}

void c_reset(Nfm* env) {
  if (!env || !env->observations) return;
  if (!env->sim && nfm_init(env) != 0) {
    memset(env->observations, 0, sizeof(float) * NFM_OBS_DIM);
    return;
  }
  const int seed = env->seed + env->episode_id;
  ++env->episode_id;
  env->tick = 0;
  env->ep_return = 0.f;
  if (env->rewards) env->rewards[0] = 0.f;
  if (env->terminals) env->terminals[0] = 0;
  nfm_sim_set_stall_cut(env->sim, env->stall_cut);
  if (nfm_sim_reset(env->sim, env->stage, env->car, seed, env->nplayers,
                    env->max_steps, env->observations) != 0) {
    memset(env->observations, 0, sizeof(float) * NFM_OBS_DIM);
  }
}

void c_step(Nfm* env) {
  int terminated = 0;
  int truncated = 0;
  int place = 0;
  int clear = 0;
  int need = 1;
  float reward = 0.f;
  int acts[NFM_ACTION_DIM];
  int i;

  if (!env || !env->observations || !env->actions || !env->rewards ||
      !env->terminals)
    return;

  env->terminals[0] = 0;
  env->rewards[0] = 0.f;

  if (!env->sim && nfm_init(env) != 0) return;

  for (i = 0; i < NFM_ACTION_DIM; ++i)
    acts[i] = env->actions[i] ? 1 : 0;

  ++env->tick;
  if (nfm_sim_step(env->sim, acts, env->observations, &reward, &terminated,
                   &truncated, &place, &clear, &need) != 0) {
    return;
  }

  env->rewards[0] = reward;
  env->ep_return += reward;

  /* Puffer: treat truncate as terminal; auto-reset inside c_step. */
  if (terminated || truncated) {
    env->terminals[0] = 1;
    nfm_add_log(env, reward, place, clear, need);
    c_reset(env);
  }
}

void c_render(Nfm* env) {
  (void)env;
  /* Headless by default. Optional Raylib viewer can mirror Squared later. */
}

void c_close(Nfm* env) {
  if (!env) return;
  if (env->sim) {
    nfm_sim_destroy(env->sim);
    env->sim = NULL;
  }
  /* Do not free observations / actions / rewards / terminals. */
}
