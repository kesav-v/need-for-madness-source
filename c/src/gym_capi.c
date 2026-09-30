#include "nfm/gym_capi.h"

#include "nfm/sim.h"

#include <stdlib.h>
#include <string.h>

struct NfmGymHandle {
  struct NfmSim* sim;
};

NfmGymHandle* nfm_gym_create(const char* repo_root, const char* assets_dir) {
  NfmGymHandle* h = (NfmGymHandle*)calloc(1, sizeof(NfmGymHandle));
  if (!h) return NULL;
  h->sim = nfm_sim_create(repo_root ? repo_root : ".",
                          assets_dir ? assets_dir : "c/assets");
  if (!h->sim) {
    free(h);
    return NULL;
  }
  return h;
}

void nfm_gym_destroy(NfmGymHandle* h) {
  if (!h) return;
  nfm_sim_destroy(h->sim);
  free(h);
}

const char* nfm_gym_last_error(NfmGymHandle* h) {
  if (!h || !h->sim) return "null handle";
  return nfm_sim_last_error(h->sim);
}

int nfm_gym_obs_dim(void) { return NFM_SIM_OBS_DIM; }

int nfm_gym_reset(NfmGymHandle* h, int stage, int car, int seed, int nplayers,
                  int max_steps, const char* record_path, float* obs_out) {
  if (!h || !h->sim || !obs_out) return -1;
  if (nfm_sim_reset_record(h->sim, stage, car, seed, nplayers, max_steps,
                           record_path, obs_out) != 0)
    return -2;
  if (!nfm_sim_ready(h->sim)) return -2;
  return 0;
}

int nfm_gym_step(NfmGymHandle* h, int left, int right, int up, int down,
                 int handb, float* obs_out, float* reward, int* terminated,
                 int* truncated, int* frame) {
  return nfm_gym_step_ex(h, left, right, up, down, handb, obs_out, reward,
                         terminated, truncated, frame, NULL, NULL);
}

int nfm_gym_step_ex(NfmGymHandle* h, int left, int right, int up, int down,
                    int handb, float* obs_out, float* reward, int* terminated,
                    int* truncated, int* frame, int* place_out, int* clear_out) {
  NfmSimAction a;
  NfmSimStepResult r;
  if (!h || !h->sim || !obs_out || !reward || !terminated || !truncated || !frame)
    return -1;
  memset(&a, 0, sizeof(a));
  a.left = left != 0;
  a.right = right != 0;
  a.up = up != 0;
  a.down = down != 0;
  a.handb = handb != 0;
  r = nfm_sim_step_action(h->sim, a);
  memcpy(obs_out, r.obs.v, sizeof(float) * NFM_SIM_OBS_DIM);
  *reward = r.reward;
  *terminated = r.terminated ? 1 : 0;
  *truncated = r.truncated ? 1 : 0;
  *frame = r.frame;
  if (place_out) *place_out = r.place;
  if (clear_out) *clear_out = r.clear;
  return 0;
}

int nfm_gym_step_multi(NfmGymHandle* h, const int* actions5n, int nplayers,
                       float* obs_out, float* reward, int* terminated,
                       int* truncated, int* frame, int* places, int* clears) {
  NfmSimAction acts[8];
  NfmSimStepResult r;
  int i;
  if (!h || !h->sim || !actions5n || !obs_out || !reward || !terminated ||
      !truncated || !frame)
    return -1;
  if (nplayers != nfm_sim_nplayers(h->sim)) return -3;
  for (i = 0; i < nplayers; ++i) {
    const int* a = actions5n + i * 5;
    acts[i].left = a[0] != 0;
    acts[i].right = a[1] != 0;
    acts[i].up = a[2] != 0;
    acts[i].down = a[3] != 0;
    acts[i].handb = a[4] != 0;
  }
  r = nfm_sim_step_multi(h->sim, acts, nplayers);
  memcpy(obs_out, r.obs.v, sizeof(float) * NFM_SIM_OBS_DIM);
  *reward = r.reward;
  *terminated = r.terminated ? 1 : 0;
  *truncated = r.truncated ? 1 : 0;
  *frame = r.frame;
  if (places || clears) {
    for (i = 0; i < nplayers; ++i) {
      if (places) places[i] = nfm_sim_player_place(h->sim, i);
      if (clears) clears[i] = nfm_sim_player_clear(h->sim, i);
    }
  }
  return 0;
}

int nfm_gym_obs_player(NfmGymHandle* h, int player, float* obs_out) {
  NfmSimObs o;
  if (!h || !h->sim || !obs_out) return -1;
  nfm_sim_fill_obs(h->sim, &o, player);
  memcpy(obs_out, o.v, sizeof(float) * NFM_SIM_OBS_DIM);
  return 0;
}

int nfm_gym_need_clear(NfmGymHandle* h) {
  return h && h->sim ? nfm_sim_need_clear(h->sim) : 0;
}

int nfm_gym_nplayers(NfmGymHandle* h) {
  return h && h->sim ? nfm_sim_nplayers(h->sim) : 0;
}

int nfm_gym_step_ai(NfmGymHandle* h, float* obs_out, float* reward,
                    int* terminated, int* truncated, int* frame, int* left,
                    int* right, int* up, int* down, int* handb, int* place_out,
                    int* clear_out) {
  NfmSimStepResult r;
  if (!h || !h->sim || !obs_out || !reward || !terminated || !truncated ||
      !frame || !left || !right || !up || !down || !handb)
    return -1;
  r = nfm_sim_step_ai(h->sim);
  memcpy(obs_out, r.obs.v, sizeof(float) * NFM_SIM_OBS_DIM);
  *reward = r.reward;
  *terminated = r.terminated ? 1 : 0;
  *truncated = r.truncated ? 1 : 0;
  *frame = r.frame;
  *left = r.action.left ? 1 : 0;
  *right = r.action.right ? 1 : 0;
  *up = r.action.up ? 1 : 0;
  *down = r.action.down ? 1 : 0;
  *handb = r.action.handb ? 1 : 0;
  if (place_out) *place_out = r.place;
  if (clear_out) *clear_out = r.clear;
  return 0;
}

int nfm_gym_query_ai(NfmGymHandle* h, int* left, int* right, int* up, int* down,
                     int* handb) {
  NfmSimAction a;
  if (!h || !h->sim || !left || !right || !up || !down || !handb) return -1;
  a = nfm_sim_query_ai_action(h->sim);
  *left = a.left ? 1 : 0;
  *right = a.right ? 1 : 0;
  *up = a.up ? 1 : 0;
  *down = a.down ? 1 : 0;
  *handb = a.handb ? 1 : 0;
  return 0;
}

int nfm_gym_finish_recording(NfmGymHandle* h) {
  if (!h || !h->sim) return 0;
  return nfm_sim_finish_recording(h->sim);
}

void nfm_gym_set_stall_cut(NfmGymHandle* h, int enabled) {
  if (!h || !h->sim) return;
  nfm_sim_set_stall_cut(h->sim, enabled);
}
