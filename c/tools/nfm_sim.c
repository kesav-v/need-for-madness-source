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
#include "nfm/env_flags.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct RaceConfig {
  int stage;
  int car;
  int nplayers;
  int forward;
  int timeout;
  char repo_root[512];
} RaceConfig;

static void usage(void) {
  fprintf(stderr,
          "nfm_sim --stage N --car M --out path.nfmst [--seed S] [--forward]\n"
          "        [--timeout T] [--nplayers 7] [--disable-ai] [--assets DIR]\n"
          "        [--n N] [--jobs J] [--root DIR]\n"
          "  --disable-ai   solo race: player 0 only (no AI cars)\n"
          "  --n N          run N games in-process; only the last writes --out\n"
          "  --jobs J       parallel races (default 1; each job has its own Medium/world)\n");
}

static void draw_progress(int cur, int total) {
  const int width = 40;
  const int filled = (cur * width) / total;
  const int empty = width - filled;
  int i;
  fprintf(stderr, "\r[");
  for (i = 0; i < filled; ++i) fputc('#', stderr);
  for (i = 0; i < empty; ++i) fputc('-', stderr);
  fprintf(stderr, "] %d/%d", cur, total);
  fflush(stderr);
}

static int fresh_seed(void) {
  return (int)time(NULL) ^ (int)clock();
}

/** Run one race. Uses cache when non-null (skips load_stage; shares trackers).
 *  cars must hold at least cfg->nplayers ContOs (caller-owned, reused across races).
 *  Full stage ContOs are not required when the stage cache hits. */
static int run_race(NfmMedium* medium, NfmContO* bco, NfmCarDefine* cd,
                    const RaceConfig* cfg, int seed, const NfmStageCache* cache,
                    NfmContO* cars, const char* out_path, int quiet) {
  int sc[8];
  NfmCheckPoints cp;
  NfmStageLoadResult slr;
  char err[512];
  NfmTrackers owned_trackers;
  NfmTrackers* trackers = NULL;
  int own_trackers = 0;
  NfmMad mad[8];
  NfmControl u[8];
  NfmStateRecorder rec;
  int rec_open = 0;
  int max_frames;
  int holdit = 0;
  int holdcnt = 0;
  int p0_wasted_ticks = -1;
  const int p0_wasted_hold = 90;
  int frame;
  int i, j;
  char stage_path[640];

  (void)seed;
  if (!cars) return 1;
  NfmContO* world = cars;
  NfmContO* scratch = NULL;
  int scratch_n = 0;
  nfm_control_autodrive = cfg->forward != 0;
  nfm_mad_reset_scrape_rng();

  memset(sc, 0, sizeof(sc));
  sc[0] = cfg->car;
  nfm_sort_cars(medium, sc, cfg->stage, cfg->nplayers);

  nfm_checkpoints_init(&cp);
  memset(&slr, 0, sizeof(slr));
  err[0] = '\0';
  nfm_trackers_init(&owned_trackers);

  if (cache && !nfm_stage_cache_empty(cache)) {
    trackers = (NfmTrackers*)&cache->trackers;
    if (!nfm_stage_cache_apply(cache, world, sc, medium, trackers, &cp)) {
      int k;
      trackers = &owned_trackers;
      own_trackers = 1;
      scratch = (NfmContO*)calloc(610, sizeof(NfmContO));
      if (!scratch) {
        fprintf(stderr, "[sim] out of memory\n");
        if (own_trackers) nfm_trackers_free_sect(&owned_trackers);
        return 1;
      }
      for (k = 0; k < 610; ++k) nfm_conto_init_empty(&scratch[k]);
      scratch_n = 610;
      world = scratch;
      snprintf(stage_path, sizeof(stage_path), "%s/OBJ/stages/%d.txt",
               cfg->repo_root, cfg->stage);
      if (!nfm_load_stage(stage_path, cfg->stage, bco, world, cfg->nplayers, sc,
                          medium, trackers, &cp, &slr, err, sizeof(err))) {
        fprintf(stderr, "[sim] stage load failed: %s\n", err);
        if (scratch) {
          int k;
          for (k = 0; k < scratch_n; ++k) nfm_conto_free(&scratch[k]);
          free(scratch);
        }
        if (own_trackers) nfm_trackers_free_sect(&owned_trackers);
        return 1;
      }
    } else {
      slr = cache->slr;
    }
  } else {
    trackers = &owned_trackers;
    own_trackers = 1;
    {
      int k;
      scratch = (NfmContO*)calloc(610, sizeof(NfmContO));
      if (!scratch) {
        fprintf(stderr, "[sim] out of memory\n");
        if (own_trackers) nfm_trackers_free_sect(&owned_trackers);
        return 1;
      }
      for (k = 0; k < 610; ++k) nfm_conto_init_empty(&scratch[k]);
      scratch_n = 610;
      world = scratch;
    }
    snprintf(stage_path, sizeof(stage_path), "%s/OBJ/stages/%d.txt",
             cfg->repo_root, cfg->stage);
    if (!nfm_load_stage(stage_path, cfg->stage, bco, world, cfg->nplayers, sc,
                        medium, trackers, &cp, &slr, err, sizeof(err))) {
      fprintf(stderr, "[sim] stage load failed: %s\n", err);
      if (own_trackers) nfm_trackers_free_sect(&owned_trackers);
      return 1;
    }
  }

  for (i = 0; i < cfg->nplayers; ++i) {
    nfm_mad_init(&mad[i], cd, medium, i);
    nfm_control_init(&u[i], medium);
    nfm_control_reset(&u[i], &cp, sc[i]);
    nfm_mad_reseto(&mad[i], sc[i], &world[i], &cp);
  }

  memset(&rec, 0, sizeof(rec));
  if (out_path && out_path[0]) {
    if (nfm_state_recorder_open(&rec, out_path, cfg->stage, cfg->car,
                                cfg->nplayers, cp.nlaps, cp.nsp, sc) == 0) {
      rec_open = 1;
    }
  }
  if (!quiet) {
    printf("[sim] race started stage=%d car=%d seed=%d forward=%d nplayers=%d\n",
           cfg->stage, cfg->car, seed, cfg->forward ? 1 : 0, cfg->nplayers);
  }

  max_frames = (cfg->timeout > 0) ? cfg->timeout : 100000;
  for (frame = 0; frame < max_frames; ++frame) {
    for (i = 0; i < cfg->nplayers; ++i) {
      int xz, xy, zy;
      if (!mad[i].newcar) continue;
      xz = world[i].xz;
      xy = world[i].xy;
      zy = world[i].zy;
      nfm_conto_place(&world[i], &bco[mad[i].cn], world[i].x, world[i].y,
                      world[i].z, 0, medium, trackers, false);
      world[i].xz = xz;
      world[i].xy = xy;
      world[i].zy = zy;
      mad[i].newcar = false;
    }
    for (i = 0; i < cfg->nplayers; ++i) {
      for (j = 0; j < cfg->nplayers; ++j) {
        if (i == j) continue;
        nfm_mad_colide(&mad[i], &world[i], &mad[j], &world[j]);
      }
    }

    if (cfg->forward)
      nfm_control_apply_autodrive(&u[0]);
    else
      nfm_control_preform(&u[0], &mad[0], &world[0], &cp, trackers);

    for (i = 0; i < cfg->nplayers; ++i)
      nfm_mad_drive(&mad[i], &u[i], &world[i], trackers, &cp);

    nfm_checkpoints_checkstat(&cp, mad, world, cfg->nplayers, 0, 0, 0);

    if (holdit) {
      ++holdcnt;
    } else {
      holdcnt = 0;
      if (cp.wasted == cfg->nplayers - 1 && cfg->nplayers != 1) {
        cp.haltall = true;
        holdit = 1;
      }
      if (!holdit) {
        for (i = 0; i < cfg->nplayers; ++i) {
          if (cp.clear[i] == cp.nlaps * cp.nsp && cp.pos[i] == 0) {
            cp.haltall = true;
            holdit = 1;
            break;
          }
        }
      }
    }
    for (i = 1; i < cfg->nplayers; ++i)
      nfm_control_preform(&u[i], &mad[i], &world[i], &cp, trackers);

    if (rec_open) nfm_state_recorder_capture(&rec, world, mad);

    if (holdit && holdcnt > 60) break;
    if (mad[0].dest) {
      if (p0_wasted_ticks < 0)
        p0_wasted_ticks = 0;
      else
        ++p0_wasted_ticks;
      if (p0_wasted_ticks >= p0_wasted_hold) break;
    }
    if (cfg->timeout > 0 && frame + 1 >= cfg->timeout) break;
  }
  if (rec_open) nfm_state_recorder_finish(&rec);

  if (scratch) {
    int k;
    /* Preserve car ContOs into caller slots for arena reuse next race. */
    for (k = 0; k < cfg->nplayers; ++k) {
      nfm_conto_free(&cars[k]);
      cars[k] = world[k];
      nfm_conto_init_empty(&world[k]);
    }
    for (k = 0; k < scratch_n; ++k) nfm_conto_free(&scratch[k]);
    free(scratch);
  }
  if (own_trackers) nfm_trackers_free_sect(&owned_trackers);
  return 0;
}


typedef struct JobPool {
  RaceConfig cfg;
  NfmContO* bco;
  NfmCarDefine* cd;
  const NfmStageCache* cache;
  int use_cache;
  int seed;
  int seed_set;
  int n_games;
  const char* out_path;
  int quiet;
  atomic_int next; /* 1-based index of next race to claim */
  atomic_int done;
  atomic_int fail;
  pthread_mutex_t progress_mu;
} JobPool;

typedef struct JobWorkerArg {
  JobPool* pool;
  NfmContO* world; /* nplayers car ContOs (arena) owned by this worker */
} JobWorkerArg;

static void* job_worker(void* arg) {
  JobWorkerArg* wa = (JobWorkerArg*)arg;
  JobPool* pool = wa->pool;
  while (1) {
    int g = atomic_fetch_add_explicit(&pool->next, 1, memory_order_relaxed);
    int run_seed;
    NfmMedium medium;
    const char* dump;
    int d;
    if (g > pool->n_games) break;
    if (pool->seed_set)
      run_seed = pool->seed;
    else if (g == 1)
      run_seed = pool->seed;
    else
      run_seed = fresh_seed() ^ (g * 0x9e3779b9);
    nfm_medium_init(&medium, true, run_seed);
    dump = (g == pool->n_games) ? pool->out_path : "";
    if (run_race(&medium, pool->bco, pool->cd, &pool->cfg, run_seed,
                 pool->use_cache ? pool->cache : NULL, wa->world, dump,
                 pool->quiet) != 0) {
      atomic_store_explicit(&pool->fail, 1, memory_order_relaxed);
    }
    d = atomic_fetch_add_explicit(&pool->done, 1, memory_order_relaxed) + 1;
    if (pool->quiet) {
      pthread_mutex_lock(&pool->progress_mu);
      draw_progress(d, pool->n_games);
      pthread_mutex_unlock(&pool->progress_mu);
    }
  }
  return NULL;
}

int main(int argc, char** argv) {
  RaceConfig cfg;
  int seed = 0;
  int seed_set = 0;
  int disable_ai = 0;
  int n_games = 1;
  int jobs = 1;
  char out_path[1024];
  char assets[512];
  NfmMedium boot_medium;
  NfmTrackers boot_trackers;
  NfmCarDefine cd;
  NfmContO bco[124];
  char err[512];
  NfmStageCache cache;
  int use_cache;
  int quiet;
  int g, i;
  int fail = 0;

  memset(&cfg, 0, sizeof(cfg));
  cfg.stage = 1;
  cfg.car = 0;
  cfg.nplayers = 7;
  snprintf(cfg.repo_root, sizeof(cfg.repo_root), ".");
  out_path[0] = '\0';
  snprintf(assets, sizeof(assets), "c/assets");

  for (i = 1; i < argc; ++i) {
    const char* a = argv[i];
    const char* val;
    if (i + 1 < argc)
      val = argv[i + 1];
    else
      val = NULL;

#define NEED(flag)                                                             \
  do {                                                                         \
    if (!val) {                                                                \
      fprintf(stderr, "missing value for %s\n", flag);                         \
      return 2;                                                                \
    }                                                                          \
    ++i;                                                                       \
  } while (0)

    if (strcmp(a, "--stage") == 0) {
      NEED("--stage");
      cfg.stage = atoi(val);
    } else if (strcmp(a, "--car") == 0) {
      NEED("--car");
      cfg.car = atoi(val);
    } else if (strcmp(a, "--out") == 0) {
      NEED("--out");
      snprintf(out_path, sizeof(out_path), "%s", val);
    } else if (strcmp(a, "--seed") == 0) {
      NEED("--seed");
      seed = atoi(val);
      seed_set = 1;
    } else if (strcmp(a, "--forward") == 0) {
      cfg.forward = 1;
    } else if (strcmp(a, "--disable-ai") == 0) {
      disable_ai = 1;
    } else if (strcmp(a, "--timeout") == 0) {
      NEED("--timeout");
      cfg.timeout = atoi(val);
    } else if (strcmp(a, "--nplayers") == 0) {
      NEED("--nplayers");
      cfg.nplayers = atoi(val);
    } else if (strcmp(a, "--n") == 0) {
      NEED("--n");
      n_games = atoi(val);
    } else if (strcmp(a, "--jobs") == 0) {
      NEED("--jobs");
      jobs = atoi(val);
    } else if (strcmp(a, "--assets") == 0) {
      NEED("--assets");
      snprintf(assets, sizeof(assets), "%s", val);
    } else if (strcmp(a, "--root") == 0) {
      NEED("--root");
      snprintf(cfg.repo_root, sizeof(cfg.repo_root), "%s", val);
    } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
      usage();
      return 0;
    } else {
      fprintf(stderr, "unknown arg %s\n", a);
      usage();
      return 2;
    }
#undef NEED
  }

  nfm_env_flags_init();

  if (!out_path[0]) {
    usage();
    return 2;
  }
  if (n_games < 1) n_games = 1;
  if (disable_ai) cfg.nplayers = 1;
  if (cfg.nplayers < 1) cfg.nplayers = 1;
  if (cfg.nplayers > 8) cfg.nplayers = 8;

  if (!seed_set) {
    seed = fresh_seed();
    fprintf(stderr, "[sim] no --seed; using seed=%d\n", seed);
  }

  nfm_medium_init(&boot_medium, true, seed);
  nfm_trackers_init(&boot_trackers);
  nfm_car_define_init(&cd);
  for (i = 0; i < 124; ++i) nfm_conto_init_empty(&bco[i]);
  err[0] = '\0';
  {
    char models_path[640];
    snprintf(models_path, sizeof(models_path), "%s/models", assets);
    if (!nfm_load_models_zip(models_path, bco, &boot_medium, &boot_trackers,
                             err, sizeof(err))) {
      fprintf(stderr, "[sim] model load failed: %s\n", err);
      return 1;
    }
  }

  nfm_stage_cache_init(&cache);
  use_cache = getenv("NFM_NO_CACHE") == NULL;
  if (use_cache) {
    NfmMedium m;
    NfmTrackers* t = (NfmTrackers*)calloc(1, sizeof(NfmTrackers));
    int sc[8];
    NfmContO* world;
    NfmCheckPoints cp;
    NfmStageLoadResult slr;
    char stage_path[640];

    if (!t) {
      fprintf(stderr, "[sim] out of memory\n");
      return 1;
    }
    nfm_medium_init(&m, true, seed);
    nfm_trackers_init(t);
    memset(sc, 0, sizeof(sc));
    sc[0] = cfg.car;
    nfm_sort_cars(&m, sc, cfg.stage, cfg.nplayers);
    world = (NfmContO*)calloc(610, sizeof(NfmContO));
    if (!world) {
      free(t);
      fprintf(stderr, "[sim] out of memory\n");
      return 1;
    }
    for (i = 0; i < 610; ++i) nfm_conto_init_empty(&world[i]);
    nfm_checkpoints_init(&cp);
    memset(&slr, 0, sizeof(slr));
    err[0] = '\0';
    snprintf(stage_path, sizeof(stage_path), "%s/OBJ/stages/%d.txt",
             cfg.repo_root, cfg.stage);
    if (!nfm_load_stage(stage_path, cfg.stage, bco, world, cfg.nplayers, sc, &m,
                        t, &cp, &slr, err, sizeof(err))) {
      fprintf(stderr, "[sim] stage load failed: %s\n", err);
      for (i = 0; i < 610; ++i) nfm_conto_free(&world[i]);
      free(world);
      nfm_trackers_free_sect(t);
      free(t);
      return 1;
    }
    nfm_stage_cache_capture(&cache, world, cfg.nplayers, sc, t, &cp, &slr);
    for (i = 0; i < 610; ++i) nfm_conto_free(&world[i]);
    free(world);
    nfm_trackers_free_sect(t);
    free(t);
  }

  quiet = n_games > 1;
  if (jobs < 1) jobs = 1;
  if (jobs > n_games) jobs = n_games;

  if (jobs == 1) {
    NfmContO* world = (NfmContO*)calloc(8, sizeof(NfmContO));
    if (!world) {
      fprintf(stderr, "[sim] out of memory\n");
      fail = 1;
    } else {
      for (i = 0; i < 8; ++i) nfm_conto_init_empty(&world[i]);
      for (g = 1; g <= n_games; ++g) {
        int run_seed = seed_set ? seed : (g == 1 ? seed : fresh_seed());
        NfmMedium medium;
        const char* dump = (g == n_games) ? out_path : "";
        nfm_medium_init(&medium, true, run_seed);
        if (run_race(&medium, bco, &cd, &cfg, run_seed,
                     use_cache ? &cache : NULL, world, dump, quiet) != 0) {
          fail = 1;
          break;
        }
        if (quiet) draw_progress(g, n_games);
      }
      for (i = 0; i < 8; ++i) nfm_conto_free(&world[i]);
      free(world);
    }
  } else {
    JobPool pool;
    pthread_t* threads;
    JobWorkerArg* wargs;
    NfmContO** worlds;
    memset(&pool, 0, sizeof(pool));
    pool.cfg = cfg;
    pool.bco = bco;
    pool.cd = &cd;
    pool.cache = &cache;
    pool.use_cache = use_cache;
    pool.seed = seed;
    pool.seed_set = seed_set;
    pool.n_games = n_games;
    pool.out_path = out_path;
    pool.quiet = quiet;
    atomic_init(&pool.next, 1);
    atomic_init(&pool.done, 0);
    atomic_init(&pool.fail, 0);
    pthread_mutex_init(&pool.progress_mu, NULL);
    threads = (pthread_t*)calloc((size_t)jobs, sizeof(pthread_t));
    wargs = (JobWorkerArg*)calloc((size_t)jobs, sizeof(JobWorkerArg));
    worlds = (NfmContO**)calloc((size_t)jobs, sizeof(NfmContO*));
    if (!threads || !wargs || !worlds) {
      fprintf(stderr, "[sim] out of memory\n");
      fail = 1;
      free(threads);
      free(wargs);
      free(worlds);
    } else {
      fprintf(stderr, "[sim] running %d games with %d jobs\n", n_games, jobs);
      for (i = 0; i < jobs; ++i) {
        int k;
        worlds[i] = (NfmContO*)calloc(8, sizeof(NfmContO));
        if (!worlds[i]) {
          fail = 1;
          break;
        }
        for (k = 0; k < 8; ++k) nfm_conto_init_empty(&worlds[i][k]);
        wargs[i].pool = &pool;
        wargs[i].world = worlds[i];
        if (pthread_create(&threads[i], NULL, job_worker, &wargs[i]) != 0) {
          fprintf(stderr, "[sim] pthread_create failed\n");
          atomic_store(&pool.fail, 1);
          atomic_store(&pool.next, n_games + 1);
          jobs = i;
          fail = 1;
          break;
        }
      }
      {
        int spawned = i;
        for (i = 0; i < spawned; ++i) pthread_join(threads[i], NULL);
        for (i = 0; i < jobs; ++i) {
          if (worlds[i]) {
            int k;
            for (k = 0; k < 8; ++k) nfm_conto_free(&worlds[i][k]);
            free(worlds[i]);
          }
        }
      }
      free(threads);
      free(wargs);
      free(worlds);
      if (!fail) fail = atomic_load(&pool.fail);
    }
    pthread_mutex_destroy(&pool.progress_mu);
  }
  if (quiet) fputc('\n', stderr);

  nfm_stage_cache_free(&cache);
  for (i = 0; i < 124; ++i) nfm_conto_free(&bco[i]);
  nfm_trackers_free_sect(&boot_trackers);
  return fail ? 1 : 0;
}
