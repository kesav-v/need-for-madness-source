/*
 * PufferLib v4 Ocean binding for Need for Madness.
 * Build inside a PufferLib checkout (or with PUFFER_ROOT pointing at one):
 *   #define Env Nfm
 *   #include "vecenv.h"
 *
 * Spaces (must match Python / ini):
 *   obs:  Box(float32, shape=(52,))
 *   act:  MultiDiscrete([2, 2, 2, 2, 2])  # left right up down handb
 */

#include "nfm.h"

#define Env Nfm

/* When compiling under PufferLib, vecenv.h provides the Python module glue.
 * Standalone / repo builds skip it so `make` does not need Puffer headers. */
#ifdef NFM_PUFFER_BINDING
#include "vecenv.h"

static void read_int(Dict* kwargs, const char* key, int* dst) {
  DictItem* it = dict_get_unsafe(kwargs, key);
  if (it != NULL) *dst = (int)it->value;
}

static void read_str(Dict* kwargs, const char* key, char* dst, size_t n) {
  DictItem* it = dict_get_unsafe(kwargs, key);
  if (it != NULL && it->str_value != NULL) {
    snprintf(dst, n, "%s", it->str_value);
  }
}

void my_init(Env* env, Dict* kwargs) {
  env->stage = 11;
  env->car = 0;
  env->seed = 0;
  env->nplayers = 7;
  env->max_steps = 100000;
  env->stall_cut = 1;
  env->repo_root[0] = '\0';
  env->assets_dir[0] = '\0';
  env->episode_id = 0;
  env->sim = NULL;

  read_int(kwargs, "stage", &env->stage);
  read_int(kwargs, "car", &env->car);
  read_int(kwargs, "seed", &env->seed);
  read_int(kwargs, "nplayers", &env->nplayers);
  read_int(kwargs, "max_steps", &env->max_steps);
  read_int(kwargs, "stall_cut", &env->stall_cut);
  read_str(kwargs, "repo_root", env->repo_root, sizeof(env->repo_root));
  read_str(kwargs, "assets_dir", env->assets_dir, sizeof(env->assets_dir));

  nfm_init(env);
}

void my_log(Log* log, Dict* out) {
  dict_set(out, "perf", log->perf);
  dict_set(out, "score", log->score);
  dict_set(out, "episode_return", log->episode_return);
  dict_set(out, "episode_length", log->episode_length);
  dict_set(out, "place", log->place);
  dict_set(out, "clear_frac", log->clear_frac);
}
#endif /* NFM_PUFFER_BINDING */
