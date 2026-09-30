#ifndef NFM_STAGE_LOAD_H
#define NFM_STAGE_LOAD_H

#include <stddef.h>

#include "nfm/checkpoints.h"
#include "nfm/conto.h"
#include "nfm/medium.h"
#include "nfm/trackers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfmStageLoadResult {
  int nob; /* world ContO count after players */
  int nlaps;
  int nsp;
  int nfix;
  int sx, sz, ex, ez; /* bounds for devidetrackers */
} NfmStageLoadResult;

/**
 * Port of GameSparker.loadstage collision/world path (no render).
 * bco[0..123] = loaded models; world ContOs written starting at index
 * nplayers (cars placed at 0..nplayers-1).
 * err may be NULL; otherwise written as a C string (truncated to errlen).
 * Returns 1 on success, 0 on failure.
 */
int nfm_load_stage(const char* stage_path, int stage_id, NfmContO* bco,
                   NfmContO* world, int nplayers, const int* sc, NfmMedium* m,
                   NfmTrackers* t, NfmCheckPoints* cp, NfmStageLoadResult* out,
                   char* err, size_t errlen);

/** Immutable snapshot of stage + starting cars + trackers + checkpoints. */
typedef struct NfmStageCache {
  int nplayers;
  int nob;
  int sc[8];
  NfmStageLoadResult slr;
  NfmCheckPoints cp;
  NfmTrackers trackers;
  NfmContO* world; /* length world_cap; owns deep-copied ContOs */
  int world_cap;
} NfmStageCache;

void nfm_stage_cache_init(NfmStageCache* cache);
void nfm_stage_cache_free(NfmStageCache* cache);
int nfm_stage_cache_empty(const NfmStageCache* cache);

/** Capture a just-loaded stage for fast restore (deep-copies ContOs). */
void nfm_stage_cache_capture(NfmStageCache* cache, const NfmContO* world,
                             int nplayers, const int* sc, const NfmTrackers* t,
                             const NfmCheckPoints* cp,
                             const NfmStageLoadResult* slr);

/**
 * Restore cached *car* ContOs + checkpoints. Sets ContO m/t to live pointers.
 * Stage ContOs (indices >= nplayers) are not copied — trackers already hold
 * collision geometry and the race loop only mutates cars.
 * Does NOT copy Trackers — pass &cache->trackers for shared read-only use.
 */
int nfm_stage_cache_apply(const NfmStageCache* cache, NfmContO* world,
                          const int* sc, NfmMedium* m, NfmTrackers* t,
                          NfmCheckPoints* cp);

#ifdef __cplusplus
}
#endif

#endif /* NFM_STAGE_LOAD_H */
