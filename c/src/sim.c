#include "nfm/sim.h"
#include "nfm/env_flags.h"

#include "nfm/car_define.h"
#include "nfm/checkpoints.h"
#include "nfm/conto.h"
#include "nfm/control.h"
#include "nfm/mad.h"
#include "nfm/medium.h"
#include "nfm/sort_cars.h"
#include "nfm/stage_load.h"
#include "nfm/state_recorder.h"
#include "nfm/trackers.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct NfmSim {
  char root[512];
  char assets[512];
  char err[512];
  bool ready;

  int stage;
  int car;
  int nplayers;
  int max_steps;
  int frame;
  bool holdit;
  int holdcnt;
  int p0_wasted_ticks;
  int prev_clear;
  int prev_pos;
  float prev_cp_dist;
  int route_i;
  int prev_route_i;
  float best_route_dist;
  int stall_ticks;
  bool stall_cut_enabled;
  bool all_external;

  NfmMedium medium;
  NfmTrackers trackers;
  NfmCarDefine cd;
  NfmContO bco[124];
  NfmContO world[610];
  NfmCheckPoints cp;
  NfmMad mad[8];
  NfmControl u[8];
  int sc[8];
  bool models_ok;
  bool bco_shared; /* shallow view of process-global templates */

  NfmStateRecorder rec;
  bool rec_open;
};

/* Process-wide model templates — planes are immutable after load; place() deep-copies. */
static pthread_mutex_t g_bco_mu = PTHREAD_MUTEX_INITIALIZER;
static NfmContO g_bco[124];
static NfmMedium g_bco_medium;
static NfmTrackers g_bco_trackers;
static int g_bco_ok;
static char g_bco_assets[512];

static int ensure_shared_bco(const char* assets_dir, char* errbuf, size_t errlen) {
  int i;
  char models_path[640];
  pthread_mutex_lock(&g_bco_mu);
  if (g_bco_ok && strcmp(g_bco_assets, assets_dir ? assets_dir : "") == 0) {
    pthread_mutex_unlock(&g_bco_mu);
    return 1;
  }
  if (g_bco_ok) {
    for (i = 0; i < 124; ++i) nfm_conto_free(&g_bco[i]);
    nfm_trackers_free_sect(&g_bco_trackers);
    g_bco_ok = 0;
  }
  nfm_medium_init(&g_bco_medium, true, 0);
  nfm_trackers_init(&g_bco_trackers);
  for (i = 0; i < 124; ++i) nfm_conto_init_empty(&g_bco[i]);
  snprintf(models_path, sizeof(models_path), "%s/models",
           assets_dir ? assets_dir : "c/assets");
  if (errbuf && errlen) errbuf[0] = '\0';
  if (!nfm_load_models_zip(models_path, g_bco, &g_bco_medium, &g_bco_trackers,
                           errbuf, errlen)) {
    for (i = 0; i < 124; ++i) nfm_conto_free(&g_bco[i]);
    nfm_trackers_free_sect(&g_bco_trackers);
    pthread_mutex_unlock(&g_bco_mu);
    return 0;
  }
  snprintf(g_bco_assets, sizeof(g_bco_assets), "%s",
           assets_dir ? assets_dir : "");
  g_bco_ok = 1;
  pthread_mutex_unlock(&g_bco_mu);
  return 1;
}

static void attach_shared_bco(struct NfmSim* s) {
  int i;
  memcpy(s->bco, g_bco, sizeof(g_bco));
  for (i = 0; i < 124; ++i) {
    s->bco[i].m = &s->medium;
    s->bco[i].t = &s->trackers;
  }
  s->bco_shared = true;
}

typedef struct NfmSim NfmSim;

static void set_err(NfmSim* s, const char* msg) {
  if (!s) return;
  if (!msg) {
    s->err[0] = '\0';
    return;
  }
  snprintf(s->err, sizeof(s->err), "%s", msg);
}

static void clear_world(NfmSim* s) {
  int i;
  for (i = 0; i < 610; ++i) nfm_conto_free(&s->world[i]);
}

static void reset_trackers(NfmSim* s) {
  nfm_trackers_free_sect(&s->trackers);
  s->trackers.nt = 0;
  s->trackers.sx = 0;
  s->trackers.sz = 0;
  s->trackers.ncx = 0;
  s->trackers.ncz = 0;
}

static int next_checkpoint_index(const NfmSim* s, int player) {
  int nxt, start;
  if (s->cp.n <= 0 || player < 0 || player >= s->nplayers) return 0;
  nxt = s->mad[player].pcleared + 1;
  if (nxt >= s->cp.n) nxt = 0;
  start = nxt;
  while (s->cp.typ[nxt] <= 0) {
    if (++nxt >= s->cp.n) nxt = 0;
    if (nxt == start) break;
  }
  return nxt;
}

static float checkpoint_dist(const NfmSim* s, int player) {
  int nxt;
  float dx, dz;
  const NfmContO* c;
  if (!s->ready || s->cp.n <= 0) return 0.f;
  nxt = next_checkpoint_index(s, player);
  c = &s->world[player];
  dx = (s->cp.x[nxt] - c->x) / 5000.f;
  dz = (s->cp.z[nxt] - c->z) / 5000.f;
  return sqrtf(dx * dx + dz * dz);
}

static float route_dist(const NfmSim* s, int player) {
  int i;
  float dx, dz;
  const NfmContO* c;
  if (!s->ready || s->cp.n <= 0) return 0.f;
  if (player != 0) return checkpoint_dist(s, player);
  i = s->route_i;
  if (i < 0 || i >= s->cp.n) i = next_checkpoint_index(s, 0);
  c = &s->world[0];
  dx = (s->cp.x[i] - c->x) / 5000.f;
  dz = (s->cp.z[i] - c->z) / 5000.f;
  return sqrtf(dx * dx + dz * dz);
}

static void sync_route_target(NfmSim* s) {
  int guard;
  const float kAdvance = 2500.f / 5000.f;
  if (!s->ready || s->cp.n <= 0) return;
  if (s->mad[0].clear != s->prev_clear) {
    s->route_i = s->mad[0].pcleared + 1;
    if (s->route_i >= s->cp.n) s->route_i = 0;
  }
  for (guard = 0; guard < s->cp.n; ++guard) {
    if (route_dist(s, 0) > kAdvance) break;
    if (s->cp.typ[s->route_i] > 0 && s->route_i == next_checkpoint_index(s, 0) &&
        s->mad[0].clear == s->prev_clear) {
      break;
    }
    if (++s->route_i >= s->cp.n) s->route_i = 0;
  }
}

static void rebuild_newcars(NfmSim* s) {
  int i;
  for (i = 0; i < s->nplayers; ++i) {
    int xz, xy, zy;
    if (!s->mad[i].newcar) continue;
    xz = s->world[i].xz;
    xy = s->world[i].xy;
    zy = s->world[i].zy;
    /* Match C++ NfmSim::rebuild_newcars (register_tracks default true). */
    nfm_conto_place(&s->world[i], &s->bco[s->mad[i].cn], s->world[i].x,
                    s->world[i].y, s->world[i].z, 0, &s->medium, &s->trackers,
                    true);
    s->world[i].xz = xz;
    s->world[i].xy = xy;
    s->world[i].zy = zy;
    s->mad[i].newcar = false;
  }
}

static void tick_physics(NfmSim* s, int mode, const NfmSimAction* actions,
                         int n_actions) {
  int i, j;
  rebuild_newcars(s);
  for (i = 0; i < s->nplayers; ++i) {
    for (j = 0; j < s->nplayers; ++j) {
      if (i == j) continue;
      nfm_mad_colide(&s->mad[i], &s->world[i], &s->mad[j], &s->world[j]);
    }
  }
  if (mode == 2 && actions && n_actions >= s->nplayers) {
    for (i = 0; i < s->nplayers; ++i) {
      s->u[i].left = actions[i].left;
      s->u[i].right = actions[i].right;
      s->u[i].up = actions[i].up;
      s->u[i].down = actions[i].down;
      s->u[i].handb = actions[i].handb;
    }
  } else if (mode == 1) {
    nfm_control_preform(&s->u[0], &s->mad[0], &s->world[0], &s->cp,
                        &s->trackers);
  } else if (actions && n_actions >= 1) {
    s->u[0].left = actions[0].left;
    s->u[0].right = actions[0].right;
    s->u[0].up = actions[0].up;
    s->u[0].down = actions[0].down;
    s->u[0].handb = actions[0].handb;
  } else {
    s->u[0].left = s->u[0].right = s->u[0].up = s->u[0].down =
        s->u[0].handb = false;
  }
  for (i = 0; i < s->nplayers; ++i)
    nfm_mad_drive(&s->mad[i], &s->u[i], &s->world[i], &s->trackers, &s->cp);
  nfm_checkpoints_checkstat(&s->cp, s->mad, s->world, s->nplayers, 0, 0, 0);
  if (s->holdit) {
    ++s->holdcnt;
  } else {
    s->holdcnt = 0;
    if (s->cp.wasted == s->nplayers - 1 && s->nplayers != 1) {
      s->cp.haltall = true;
      s->holdit = true;
    }
    if (!s->holdit) {
      for (i = 0; i < s->nplayers; ++i) {
        if (s->cp.clear[i] == s->cp.nlaps * s->cp.nsp && s->cp.pos[i] == 0) {
          s->cp.haltall = true;
          s->holdit = true;
          break;
        }
      }
    }
  }
  if (mode != 2) {
    for (i = 1; i < s->nplayers; ++i)
      nfm_control_preform(&s->u[i], &s->mad[i], &s->world[i], &s->cp,
                          &s->trackers);
  }
}

static void finish_step(NfmSim* s, NfmSimStepResult* r, bool stall_cut) {
  const int clear = s->mad[0].clear;
  const int dclear = clear - s->prev_clear;
  const int pos = s->cp.pos[0];
  float dist;
  float speed;
  float shaped;
  int need;
  const float kProgEps = 0.002f;
  bool progressed = false;
  const int kStallGrace = 15;
  const int kStallTruncateSolo = 90;
  const int kStallTruncateRace = 120;

  if (s->mad[0].dest) {
    if (s->p0_wasted_ticks < 0)
      s->p0_wasted_ticks = 0;
    else
      ++s->p0_wasted_ticks;
  }

  sync_route_target(s);
  dist = route_dist(s, 0);
  speed = s->mad[0].speed;
  shaped = 0.f;
  if (dclear == 0) {
    const float dprog = s->prev_cp_dist - dist;
    shaped = (speed > 1.f) ? dprog : fminf(dprog, 0.f);
  }
  r->reward = shaped + (float)dclear * 20.f;
  r->reward -= 0.001f;
  if (fabsf(speed) < 5.f) {
    r->reward -= 0.15f;
  } else if (fabsf(speed) < 20.f) {
    r->reward -= 0.03f;
  }
  if (speed < -1.f) r->reward -= 0.05f;
  if (s->mad[0].dest) r->reward -= 1.f;

  need = s->cp.nlaps * s->cp.nsp;
  if (s->nplayers > 1) {
    if (fabsf(speed) > 15.f || dclear > 0) {
      r->reward += (float)(s->prev_pos - pos) * 8.f;
    }
    r->reward -= 0.001f;
  }
  if (dclear > 0 && clear >= need) {
    r->reward += 80.f + 25.f * (float)(s->nplayers - 1 - pos);
    if (pos == 0) r->reward += 150.f;
  } else if (dclear > 0 && pos <= 2) {
    r->reward += 12.f;
  }

  if (dclear > 0 || s->route_i != s->prev_route_i ||
      (s->nplayers > 1 && pos < s->prev_pos && fabsf(speed) > 15.f)) {
    progressed = true;
    s->best_route_dist = dist;
  } else if (dist < s->best_route_dist - kProgEps && fabsf(speed) > 1.f) {
    progressed = true;
    s->best_route_dist = dist;
  }
  if (progressed) {
    s->stall_ticks = 0;
  } else {
    ++s->stall_ticks;
  }
  if (s->stall_ticks > kStallGrace) {
    const float t =
        fminf((float)(s->stall_ticks - kStallGrace) / 20.f, 8.f);
    r->reward -= 0.05f * t;
  }

  s->prev_clear = clear;
  s->prev_pos = pos;
  s->prev_cp_dist = dist;
  s->prev_route_i = s->route_i;

  nfm_sim_fill_obs(s, &r->obs, 0);
  r->frame = s->frame;
  r->place = pos;
  r->clear = clear;

  if (s->max_steps > 0 && s->frame >= s->max_steps) {
    r->truncated = true;
  }
  if (stall_cut &&
      s->stall_ticks >=
          (s->nplayers > 1 ? kStallTruncateRace : kStallTruncateSolo)) {
    r->truncated = true;
  }
  if (s->holdit && s->holdcnt > 60) {
    r->terminated = true;
  }
  if (s->mad[0].dest && s->p0_wasted_ticks >= 90) {
    r->terminated = true;
  }
  if (r->terminated || r->truncated) nfm_sim_finish_recording(s);
}

void nfm_sim_fill_obs(const struct NfmSim* s, NfmSimObs* out, int player) {
  const NfmContO* c;
  const NfmMad* m;
  int nxt, need, n_opp, i, k;
  float dx, dz, sn, co, ego_f, ego_r, dist;
  typedef struct Opp {
    float d, f, r, ds, dc;
  } Opp;
  Opp opps[NFM_SIM_MAX_OPP];

  memset(out->v, 0, sizeof(out->v));
  if (!s || !s->ready || player < 0 || player >= s->nplayers) return;
  c = &s->world[player];
  m = &s->mad[player];
  out->v[0] = c->x / 5000.f;
  out->v[1] = c->y / 500.f;
  out->v[2] = c->z / 5000.f;
  out->v[3] = c->xz / 360.f;
  out->v[4] = c->xy / 360.f;
  out->v[5] = c->zy / 360.f;
  out->v[6] = c->wxz / 36.f;
  out->v[7] = c->wzy / 30.f;
  out->v[8] = m->speed / 200.f;
  out->v[9] = m->power / 100.f;
  out->v[10] = m->hitmag / 10000.f;
  out->v[11] = (float)m->squash;
  out->v[12] = m->mxz / 360.f;
  out->v[13] = m->cxz / 360.f;
  out->v[14] = m->dest ? 1.f : 0.f;

  if (s->cp.n <= 0) return;
  nxt = next_checkpoint_index(s, player);
  dx = (s->cp.x[nxt] - c->x) / 5000.f;
  dz = (s->cp.z[nxt] - c->z) / 5000.f;
  sn = nfm_medium_sin(&s->medium, c->xz);
  co = nfm_medium_cos(&s->medium, c->xz);
  ego_f = -dx * sn + dz * co;
  ego_r = dx * co + dz * sn;
  dist = sqrtf(dx * dx + dz * dz);
  out->v[15] = ego_f;
  out->v[16] = ego_r;
  out->v[17] = dist;
  if (dist > 1e-5f) {
    out->v[18] = ego_r / dist;
    out->v[19] = ego_f / dist;
  } else {
    out->v[18] = 0.f;
    out->v[19] = 1.f;
  }

  need = s->cp.nlaps * s->cp.nsp;
  if (need < 1) need = 1;
  out->v[20] = (float)m->clear / (float)need;
  if (s->nplayers > 1) {
    int pl = s->cp.pos[player];
    if (pl < 0) pl = 0;
    if (pl > s->nplayers - 1) pl = s->nplayers - 1;
    out->v[21] = (float)pl / (float)(s->nplayers - 1);
  } else {
    out->v[21] = 0.f;
  }

  memset(opps, 0, sizeof(opps));
  n_opp = 0;
  for (i = 0; i < s->nplayers; ++i) {
    float odx, odz, od, of, orr, dspd, dclr;
    int slot;
    if (i == player) continue;
    odx = (s->world[i].x - c->x) / 5000.f;
    odz = (s->world[i].z - c->z) / 5000.f;
    od = sqrtf(odx * odx + odz * odz);
    of = -odx * sn + odz * co;
    orr = odx * co + odz * sn;
    dspd = (s->mad[i].speed - m->speed) / 200.f;
    dclr = (float)(s->mad[i].clear - m->clear) / (float)need;
    slot = n_opp;
    if (n_opp < NFM_SIM_MAX_OPP) {
      ++n_opp;
    } else if (od >= opps[NFM_SIM_MAX_OPP - 1].d) {
      continue;
    } else {
      slot = NFM_SIM_MAX_OPP - 1;
    }
    while (slot > 0 && od < opps[slot - 1].d) {
      opps[slot] = opps[slot - 1];
      --slot;
    }
    opps[slot].d = od;
    opps[slot].f = of;
    opps[slot].r = orr;
    opps[slot].ds = dspd;
    opps[slot].dc = dclr;
  }
  for (k = 0; k < n_opp; ++k) {
    const int b = 22 + k * 5;
    out->v[b + 0] = opps[k].f;
    out->v[b + 1] = opps[k].r;
    out->v[b + 2] = opps[k].d;
    out->v[b + 3] = opps[k].ds;
    out->v[b + 4] = opps[k].dc;
  }
}

struct NfmSim* nfm_sim_create(const char* repo_root, const char* assets_dir) {
  nfm_env_flags_init();
  NfmSim* s;
  char errbuf[512];
  int i;

  s = (NfmSim*)calloc(1, sizeof(NfmSim));
  if (!s) return NULL;

  snprintf(s->root, sizeof(s->root), "%s", repo_root ? repo_root : ".");
  snprintf(s->assets, sizeof(s->assets), "%s",
           assets_dir ? assets_dir : "c/assets");
  s->stall_cut_enabled = true;
  nfm_medium_init(&s->medium, true, 0);
  nfm_trackers_init(&s->trackers);
  nfm_car_define_init(&s->cd);
  for (i = 0; i < 124; ++i) nfm_conto_init_empty(&s->bco[i]);
  for (i = 0; i < 610; ++i) nfm_conto_init_empty(&s->world[i]);
  nfm_checkpoints_init(&s->cp);

  errbuf[0] = '\0';
  s->models_ok = ensure_shared_bco(s->assets, errbuf, sizeof(errbuf));
  if (s->models_ok) {
    attach_shared_bco(s);
  } else {
    set_err(s, errbuf[0] ? errbuf : "models not loaded");
  }
  return s;
}

void nfm_sim_destroy(struct NfmSim* s) {
  int i;
  if (!s) return;
  nfm_sim_finish_recording(s);
  clear_world(s);
  if (!s->bco_shared) {
    for (i = 0; i < 124; ++i) nfm_conto_free(&s->bco[i]);
  }
  nfm_trackers_free_sect(&s->trackers);
  free(s);
}

int nfm_sim_finish_recording(struct NfmSim* s) {
  int n;
  if (!s || !s->rec_open) return 0;
  n = s->rec.frames;
  nfm_state_recorder_finish(&s->rec);
  s->rec_open = false;
  return n;
}

int nfm_sim_reset_record(struct NfmSim* s, int stage, int car, int seed,
                         int nplayers, int max_steps, const char* record_path,
                         float* obs_out) {
  char stage_path[640];
  char errbuf[512];
  NfmStageLoadResult slr;
  NfmSimObs obs;
  int i;

  if (!s) return -1;
  nfm_sim_finish_recording(s);
  set_err(s, NULL);
  s->ready = false;
  if (!s->models_ok) {
    if (!s->err[0]) set_err(s, "models not loaded");
    if (obs_out) memset(obs_out, 0, sizeof(float) * NFM_SIM_OBS_DIM);
    return -2;
  }
  if (nplayers < 1) nplayers = 1;
  if (nplayers > 8) nplayers = 8;

  s->stage = stage;
  s->car = car;
  s->nplayers = nplayers;
  s->max_steps = max_steps > 0 ? max_steps : 100000;
  s->frame = 0;
  s->holdit = false;
  s->holdcnt = 0;
  s->p0_wasted_ticks = -1;
  s->prev_clear = 0;
  s->prev_pos = 0;
  s->prev_cp_dist = 0.f;
  s->route_i = 0;
  s->prev_route_i = 0;
  s->best_route_dist = 0.f;
  s->stall_ticks = 0;

  nfm_medium_reseed(&s->medium, seed);
  nfm_mad_reset_scrape_rng();
  reset_trackers(s);
  clear_world(s);

  for (i = 0; i < 8; ++i) s->sc[i] = 0;
  s->sc[0] = s->car;
  nfm_sort_cars(&s->medium, s->sc, s->stage, s->nplayers);

  nfm_checkpoints_init(&s->cp);
  memset(&slr, 0, sizeof(slr));
  errbuf[0] = '\0';
  snprintf(stage_path, sizeof(stage_path), "%s/OBJ/stages/%d.txt", s->root,
           s->stage);
  if (!nfm_load_stage(stage_path, s->stage, s->bco, s->world, s->nplayers, s->sc,
                      &s->medium, &s->trackers, &s->cp, &slr, errbuf,
                      sizeof(errbuf))) {
    set_err(s, errbuf[0] ? errbuf : "stage load failed");
    if (obs_out) memset(obs_out, 0, sizeof(float) * NFM_SIM_OBS_DIM);
    return -3;
  }

  for (i = 0; i < s->nplayers; ++i) {
    nfm_mad_init(&s->mad[i], &s->cd, &s->medium, i);
    nfm_control_init(&s->u[i], &s->medium);
    nfm_control_reset(&s->u[i], &s->cp, s->sc[i]);
    nfm_mad_reseto(&s->mad[i], s->sc[i], &s->world[i], &s->cp);
  }
  nfm_control_autodrive = false;
  s->ready = true;
  s->all_external = false;
  s->prev_clear = s->mad[0].clear;
  s->prev_pos = s->cp.pos[0];
  s->route_i = next_checkpoint_index(s, 0);
  s->prev_route_i = s->route_i;
  s->prev_cp_dist = route_dist(s, 0);
  s->best_route_dist = s->prev_cp_dist;
  s->stall_ticks = 0;

  if (record_path && record_path[0]) {
    if (nfm_state_recorder_open(&s->rec, record_path, s->stage, s->car,
                                s->nplayers, s->cp.nlaps, s->cp.nsp,
                                s->sc) == 0) {
      s->rec_open = true;
    } else {
      set_err(s, "failed to open record path");
    }
  }

  nfm_sim_fill_obs(s, &obs, 0);
  if (obs_out) memcpy(obs_out, obs.v, sizeof(float) * NFM_SIM_OBS_DIM);
  return s->ready ? 0 : -4;
}

int nfm_sim_reset(struct NfmSim* s, int stage, int car, int seed, int nplayers,
                  int max_steps, float* obs_out) {
  return nfm_sim_reset_record(s, stage, car, seed, nplayers, max_steps, NULL,
                              obs_out);
}

NfmSimStepResult nfm_sim_step_action(struct NfmSim* s, NfmSimAction action) {
  NfmSimStepResult r;
  memset(&r, 0, sizeof(r));
  if (!s || !s->ready) {
    r.terminated = true;
    if (s) set_err(s, "sim not ready; call reset()");
    return r;
  }
  s->all_external = false;
  r.action = action;
  tick_physics(s, 0, &action, 1);
  ++s->frame;
  if (s->rec_open) nfm_state_recorder_capture(&s->rec, s->world, s->mad);
  finish_step(s, &r, s->stall_cut_enabled);
  return r;
}

NfmSimStepResult nfm_sim_step_multi(struct NfmSim* s, const NfmSimAction* actions,
                                    int n_actions) {
  NfmSimStepResult r;
  memset(&r, 0, sizeof(r));
  if (!s || !s->ready) {
    r.terminated = true;
    if (s) set_err(s, "sim not ready; call reset()");
    return r;
  }
  if (!actions || n_actions < s->nplayers) {
    set_err(s, "step_multi needs nplayers actions");
    r.terminated = true;
    return r;
  }
  s->all_external = true;
  r.action = actions[0];
  tick_physics(s, 2, actions, n_actions);
  ++s->frame;
  if (s->rec_open) nfm_state_recorder_capture(&s->rec, s->world, s->mad);
  finish_step(s, &r, false);
  return r;
}

NfmSimStepResult nfm_sim_step_ai(struct NfmSim* s) {
  NfmSimStepResult r;
  memset(&r, 0, sizeof(r));
  if (!s || !s->ready) {
    r.terminated = true;
    if (s) set_err(s, "sim not ready; call reset()");
    return r;
  }
  s->all_external = false;
  tick_physics(s, 1, NULL, 0);
  r.action.left = s->u[0].left;
  r.action.right = s->u[0].right;
  r.action.up = s->u[0].up;
  r.action.down = s->u[0].down;
  r.action.handb = s->u[0].handb;
  ++s->frame;
  if (s->rec_open) nfm_state_recorder_capture(&s->rec, s->world, s->mad);
  finish_step(s, &r, false);
  return r;
}

NfmSimAction nfm_sim_query_ai_action(struct NfmSim* s) {
  NfmSimAction a;
  memset(&a, 0, sizeof(a));
  if (!s || !s->ready) return a;
  nfm_control_preform(&s->u[0], &s->mad[0], &s->world[0], &s->cp, &s->trackers);
  a.left = s->u[0].left;
  a.right = s->u[0].right;
  a.up = s->u[0].up;
  a.down = s->u[0].down;
  a.handb = s->u[0].handb;
  return a;
}

int nfm_sim_step(struct NfmSim* s, const int actions[NFM_SIM_ACTION_DIM],
                 float* obs_out, float* reward, int* terminated, int* truncated,
                 int* place, int* clear, int* need) {
  NfmSimAction a;
  NfmSimStepResult r;
  if (!s || !actions) return -1;
  memset(&a, 0, sizeof(a));
  a.left = actions[0] != 0;
  a.right = actions[1] != 0;
  a.up = actions[2] != 0;
  a.down = actions[3] != 0;
  a.handb = actions[4] != 0;
  r = nfm_sim_step_action(s, a);
  if (obs_out) memcpy(obs_out, r.obs.v, sizeof(float) * NFM_SIM_OBS_DIM);
  if (reward) *reward = r.reward;
  if (terminated) *terminated = r.terminated ? 1 : 0;
  if (truncated) *truncated = r.truncated ? 1 : 0;
  if (place) *place = r.place;
  if (clear) *clear = r.clear;
  if (need) *need = s->cp.nlaps * s->cp.nsp;
  return 0;
}

void nfm_sim_set_stall_cut(struct NfmSim* s, int enabled) {
  if (!s) return;
  s->stall_cut_enabled = enabled != 0;
}

bool nfm_sim_ready(const struct NfmSim* s) { return s && s->ready; }

const char* nfm_sim_last_error(const struct NfmSim* s) {
  if (!s) return "null handle";
  return s->err;
}

int nfm_sim_nplayers(const struct NfmSim* s) { return s ? s->nplayers : 0; }

int nfm_sim_frame(const struct NfmSim* s) { return s ? s->frame : 0; }

int nfm_sim_need_clear(const struct NfmSim* s) {
  return s ? s->cp.nlaps * s->cp.nsp : 0;
}

int nfm_sim_player_clear(const struct NfmSim* s, int player) {
  if (!s || player < 0 || player >= s->nplayers) return 0;
  return s->mad[player].clear;
}

int nfm_sim_player_place(const struct NfmSim* s, int player) {
  if (!s || player < 0 || player >= s->nplayers) return 0;
  return s->cp.pos[player];
}

bool nfm_sim_stall_cut(const struct NfmSim* s) {
  return s && s->stall_cut_enabled;
}

bool nfm_sim_recording(const struct NfmSim* s) { return s && s->rec_open; }

int nfm_sim_recorded_frames(const struct NfmSim* s) {
  return (s && s->rec_open) ? s->rec.frames : 0;
}
