#ifndef NFM_GYM_CAPI_H
#define NFM_GYM_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque sim handle for ctypes / Gym. */
typedef struct NfmGymHandle NfmGymHandle;

NfmGymHandle* nfm_gym_create(const char* repo_root, const char* assets_dir);
void nfm_gym_destroy(NfmGymHandle* h);
const char* nfm_gym_last_error(NfmGymHandle* h);

/** Observation length (float32). */
int nfm_gym_obs_dim(void);

/**
 * Reset. nplayers=1 for solo. max_steps<=0 → 100000.
 * record_path: optional NFMS dump path (NULL/empty = no recording).
 * Writes obs_out[obs_dim]. Returns 0 on success.
 */
int nfm_gym_reset(NfmGymHandle* h, int stage, int car, int seed, int nplayers,
                  int max_steps, const char* record_path, float* obs_out);

/**
 * Step with 5 bools: left, right, up, down, handb (nonzero = true).
 * Writes obs_out, reward, terminated, truncated, frame.
 * Optional place_out/clear_out may be NULL.
 */
int nfm_gym_step(NfmGymHandle* h, int left, int right, int up, int down,
                 int handb, float* obs_out, float* reward, int* terminated,
                 int* truncated, int* frame);

int nfm_gym_step_ex(NfmGymHandle* h, int left, int right, int up, int down,
                    int handb, float* obs_out, float* reward, int* terminated,
                    int* truncated, int* frame, int* place_out, int* clear_out);

/**
 * Self-play step: actions length = 5 * nplayers (packed).
 * Writes p0 obs/reward/flags. Optional places[nplayers], clears[nplayers].
 */
int nfm_gym_step_multi(NfmGymHandle* h, const int* actions5n, int nplayers,
                       float* obs_out, float* reward, int* terminated,
                       int* truncated, int* frame, int* places, int* clears);

/** Fill observation for player index. */
int nfm_gym_obs_player(NfmGymHandle* h, int player, float* obs_out);

int nfm_gym_need_clear(NfmGymHandle* h);
int nfm_gym_nplayers(NfmGymHandle* h);

/**
 * One tick with built-in AI driving player 0. Writes obs/reward/flags and the
 * 5 action bits the AI applied (for behavioral cloning).
 * Optional place_out/clear_out may be NULL.
 */
int nfm_gym_step_ai(NfmGymHandle* h, float* obs_out, float* reward,
                    int* terminated, int* truncated, int* frame, int* left,
                    int* right, int* up, int* down, int* handb, int* place_out,
                    int* clear_out);

/** Query Control::preform for p0 without stepping. Writes 5 action bits. */
int nfm_gym_query_ai(NfmGymHandle* h, int* left, int* right, int* up, int* down,
                     int* handb);

/** Finish current .nfmst early. Returns frames written (0 if not recording). */
int nfm_gym_finish_recording(NfmGymHandle* h);

/** Enable/disable stall penalty+truncate (1/0). Disabled for long BC evals. */
void nfm_gym_set_stall_cut(NfmGymHandle* h, int enabled);

#ifdef __cplusplus
}
#endif

#endif /* NFM_GYM_CAPI_H */
