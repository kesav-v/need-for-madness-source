#ifndef NFM_SIM_H
#define NFM_SIM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Compact observation for RL / Gym (next-CP + full traffic). Layout (52):
 *   0-19 ego pose / vitals / next-CP
 *  20    clear / need
 *  21    place / (nplayers-1)
 *  22-51 up to 6 nearest opponents × 5
 */
enum {
  NFM_SIM_OBS_DIM = 52,
  NFM_SIM_MAX_OPP = 6,
  NFM_SIM_ACTION_DIM = 5
};

typedef struct NfmSimObs {
  float v[NFM_SIM_OBS_DIM];
} NfmSimObs;

typedef struct NfmSimAction {
  bool left;
  bool right;
  bool up;
  bool down;
  bool handb;
} NfmSimAction;

typedef struct NfmSimStepResult {
  NfmSimObs obs;
  float reward;
  bool terminated;
  bool truncated;
  int frame;
  NfmSimAction action;
  int place;
  int clear;
} NfmSimStepResult;

struct NfmSim;

/** Create session; loads models from assets_dir/models. NULL on alloc/load fail. */
struct NfmSim* nfm_sim_create(const char* repo_root, const char* assets_dir);
void nfm_sim_destroy(struct NfmSim* s);

/**
 * Reset race. Writes obs_out[NFM_SIM_OBS_DIM] when non-NULL.
 * Returns 0 on success, nonzero on failure.
 */
int nfm_sim_reset(struct NfmSim* s, int stage, int car, int seed, int nplayers,
                  int max_steps, float* obs_out);

/**
 * Reset with optional NFMS recording (record_path NULL/empty = none).
 * Fills *obs_out when non-NULL. Returns 0 on success.
 */
int nfm_sim_reset_record(struct NfmSim* s, int stage, int car, int seed,
                         int nplayers, int max_steps, const char* record_path,
                         float* obs_out);

/**
 * Step with MultiDiscrete actions[5] = {left,right,up,down,handb} (nonzero=true).
 * Optional outs may be NULL except when needed by caller.
 * Returns 0 on success.
 */
int nfm_sim_step(struct NfmSim* s, const int actions[NFM_SIM_ACTION_DIM],
                 float* obs_out, float* reward, int* terminated, int* truncated,
                 int* place, int* clear, int* need);

void nfm_sim_set_stall_cut(struct NfmSim* s, int enabled);

/** Richer API (Gym / demos). */
NfmSimStepResult nfm_sim_step_action(struct NfmSim* s, NfmSimAction action);
NfmSimStepResult nfm_sim_step_multi(struct NfmSim* s, const NfmSimAction* actions,
                                    int n_actions);
NfmSimStepResult nfm_sim_step_ai(struct NfmSim* s);
NfmSimAction nfm_sim_query_ai_action(struct NfmSim* s);

void nfm_sim_fill_obs(const struct NfmSim* s, NfmSimObs* out, int player);
int nfm_sim_finish_recording(struct NfmSim* s);

bool nfm_sim_ready(const struct NfmSim* s);
const char* nfm_sim_last_error(const struct NfmSim* s);
int nfm_sim_nplayers(const struct NfmSim* s);
int nfm_sim_frame(const struct NfmSim* s);
int nfm_sim_need_clear(const struct NfmSim* s);
int nfm_sim_player_clear(const struct NfmSim* s, int player);
int nfm_sim_player_place(const struct NfmSim* s, int player);
bool nfm_sim_stall_cut(const struct NfmSim* s);
bool nfm_sim_recording(const struct NfmSim* s);
int nfm_sim_recorded_frames(const struct NfmSim* s);

#ifdef __cplusplus
}
#endif

#endif /* NFM_SIM_H */
