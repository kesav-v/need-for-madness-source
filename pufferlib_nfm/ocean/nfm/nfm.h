/* Need for Madness — PufferLib Ocean env (CPU).
 * Obs: float32[52]. Act: MultiDiscrete([2,2,2,2,2]) left/right/up/down/handb.
 * Physics: ../c NfmSim (shared model templates across envs).
 *
 * Optional NFMS dump: set [env] record_path = out.nfmst (or NFM_RECORD_PATH).
 * Records the first episode only, then clears the path.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "raylib.h"
typedef float obs_t;
#include "pufferenv.h"
#include "nfm/sim.h"

#define ACT_SIZES {2, 2, 2, 2, 2}
#define OBS_SIZE 52
#define NUM_ATNS 5

struct Log {
    float perf;
    float score;
    float episode_return;
    float episode_length;
    float place;
    float clear_frac;
    float n;
};

struct Env {
    Log log;
    Agent agents[1];
    int tag;
    int boundary_reached;
    int num_agents;

    int stage;
    int car;
    int seed;
    int nplayers;
    int max_steps;
    int stall_cut;
    int tick;
    float episode_return;
    int episode_id;
    unsigned int rng;

    char repo_root[512];
    char assets_dir[512];
    char record_path[512];

    struct NfmSim* sim;
};
typedef Env Nfm;

static void nfm_add_log(Nfm* env, float reward, int place, int clear, int need) {
    const float need_f = need > 0 ? (float)need : 1.f;
    const float clear_frac = (float)clear / need_f;
    const float place_n =
        (env->nplayers > 1) ? (float)place / (float)(env->nplayers - 1) : 0.f;
    env->log.perf += (place == 0 && clear >= need) ? 1.f : 0.f;
    env->log.score += clear_frac;
    env->log.episode_return += env->episode_return + reward;
    env->log.episode_length += (float)env->tick;
    env->log.place += place_n;
    env->log.clear_frac += clear_frac;
    env->log.n += 1.f;
}

static int nfm_ensure_sim(Nfm* env) {
    if (env->sim) return 0;
    env->sim = nfm_sim_create(env->repo_root, env->assets_dir);
    if (!env->sim) return -1;
    nfm_sim_set_stall_cut(env->sim, env->stall_cut);
    return 0;
}

void puf_reset(Nfm* env) {
    const int seed = env->seed + env->episode_id;
    const char* rec = NULL;
    ++env->episode_id;
    env->tick = 0;
    env->episode_return = 0.f;
    env->agents[0].rewards[0] = 0.f;
    env->agents[0].terminals[0] = 0.f;
    if (nfm_ensure_sim(env) != 0) {
        memset(env->agents[0].observations, 0, sizeof(float) * OBS_SIZE);
        return;
    }
    nfm_sim_set_stall_cut(env->sim, env->stall_cut);
    if (env->record_path[0]) rec = env->record_path;
    if (nfm_sim_reset_record(env->sim, env->stage, env->car, seed, env->nplayers,
                             env->max_steps, rec,
                             env->agents[0].observations) != 0) {
        memset(env->agents[0].observations, 0, sizeof(float) * OBS_SIZE);
    }
}

void puf_step(Nfm* env) {
    int terminated = 0;
    int truncated = 0;
    int place = 0;
    int clear = 0;
    int need = 1;
    float reward = 0.f;
    int acts[NUM_ATNS];
    int i;

    env->agents[0].terminals[0] = 0.f;
    env->agents[0].rewards[0] = 0.f;
    if (nfm_ensure_sim(env) != 0) return;

    for (i = 0; i < NUM_ATNS; ++i)
        acts[i] = ((int)env->agents[0].actions[i]) ? 1 : 0;

    ++env->tick;
    if (nfm_sim_step(env->sim, acts, env->agents[0].observations, &reward,
                     &terminated, &truncated, &place, &clear, &need) != 0) {
        return;
    }

    env->agents[0].rewards[0] = reward;
    env->episode_return += reward;

    if (terminated || truncated) {
        env->agents[0].terminals[0] = 1.f;
        if (env->record_path[0]) {
            int frames = nfm_sim_finish_recording(env->sim);
            fprintf(stderr, "[nfm] wrote %s frames=%d clear=%d ret=%.2f\n",
                    env->record_path, frames, clear, env->episode_return + reward);
            /* One-shot: do not overwrite on auto-reset. */
            env->record_path[0] = '\0';
        }
        nfm_add_log(env, reward, place, clear, need);
        puf_reset(env);
    }
}

void puf_render(Nfm* env) {
    (void)env;
}

void puf_close(Nfm* env) {
    if (!env) return;
    if (env->sim) {
        if (env->record_path[0]) nfm_sim_finish_recording(env->sim);
        nfm_sim_destroy(env->sim);
        env->sim = NULL;
    }
}

void puf_log(Log* log, Dict* out) {
    dict_set(out, "perf", log->perf);
    dict_set(out, "score", log->score);
    dict_set(out, "episode_return", log->episode_return);
    dict_set(out, "episode_length", log->episode_length);
    dict_set(out, "place", log->place);
    dict_set(out, "clear_frac", log->clear_frac);
    dict_set(out, "n", log->n);
}

void puf_init(Env* env, Dict* kwargs) {
    const char* root;
    const char* assets;
    const char* rec;
    const char* env_rec;

    env->num_agents = 1;
    env->stage = (int)dict_get(kwargs, "stage");
    env->car = (int)dict_get(kwargs, "car");
    env->seed = (int)dict_get(kwargs, "seed");
    env->nplayers = (int)dict_get(kwargs, "nplayers");
    env->max_steps = (int)dict_get(kwargs, "max_steps");
    env->stall_cut = (int)dict_get(kwargs, "stall_cut");
    env->tick = 0;
    env->episode_return = 0.f;
    env->episode_id = 0;
    env->sim = NULL;
    env->agents[0].action_mask = NULL;
    env->agents[0].policy = 0;
    memset(&env->log, 0, sizeof(Log));
    env->record_path[0] = '\0';

    root = dict_get_str(kwargs, "repo_root");
    assets = dict_get_str(kwargs, "assets_dir");
    snprintf(env->repo_root, sizeof(env->repo_root), "%s", root);
    snprintf(env->assets_dir, sizeof(env->assets_dir), "%s", assets);

    rec = dict_get_str(kwargs, "record_path");
    if (rec && rec[0] && strcmp(rec, "None") != 0 && strcmp(rec, "''") != 0 &&
        strcmp(rec, "\"\"") != 0) {
        snprintf(env->record_path, sizeof(env->record_path), "%s", rec);
    }
    env_rec = getenv("NFM_RECORD_PATH");
    if ((!env->record_path[0]) && env_rec && env_rec[0]) {
        snprintf(env->record_path, sizeof(env->record_path), "%s", env_rec);
    }

    if (env->stage <= 0) env->stage = 11;
    if (env->nplayers <= 0) env->nplayers = 1;
    if (env->max_steps <= 0) env->max_steps = 2500;
}
