#ifndef NFM_CONTO_H
#define NFM_CONTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nfm/medium.h"
#include "nfm/trackers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfmPlane {
  int* ox;
  int* oy;
  int* oz;
  int n;
  int wz; /* 0 = body (damageable) */
  int glass;
  int gr;
  bool nocol;
  int chip;
  int embos;
  float ctmag;
  int bfase;
  float hsb[3];
} NfmPlane;

/** Physics ContO: pose + mesh planes + track templates (no render). */
typedef struct NfmContO {
  NfmMedium* m;
  NfmTrackers* t;

  int x, y, z;
  int xz, xy, zy;
  int wxz, wzy;
  int grat;
  int keyx[4];
  int keyz[4];
  int maxR;
  int npl;
  float grounded;
  bool decor;
  bool shadow;
  int dist;
  int fix;
  int fcnt;
  int checkpoint;
  bool elec;
  bool roted;
  int ust; /* dust particle ring index (RNG parity) */

  NfmPlane* p; /* length npl; each plane owns ox/oy/oz */
  int* ox;
  int* oy;
  int* oz;
  int npts;

  int tnt;
  int* txy;
  int* tzy;
  int* tradx;
  int* tradz;
  int* trady;
  int* tx;
  int* ty;
  int* tz;
  int* skd;
  int* dam;
  bool* notwall;
  int* tc0;
  int* tc1;
  int* tc2;
} NfmContO;

/** Zero all fields; no heap allocations. Safe before first init. */
void nfm_conto_init_empty(NfmContO* c);

/**
 * Parse .rad bytes into *out (caller-owned struct).
 * Allocates internal arrays; call nfm_conto_free when done.
 * Frees any prior heap contents of *out first.
 * Returns 0 on success, -1 on failure.
 */
int nfm_conto_from_rad(NfmContO* out, const uint8_t* data, size_t len,
                       NfmMedium* med, NfmTrackers* trk);

/**
 * Place a deep copy of base at (px,py,pz) with yaw into *out.
 * If register_tracks, appends base track templates into trk.
 * Frees any prior heap contents of *out first.
 * Returns 0 on success, -1 on failure.
 */
int nfm_conto_place(NfmContO* out, const NfmContO* base, int px, int py, int pz,
                    int yaw, NfmMedium* med, NfmTrackers* trk,
                    bool register_tracks);

/**
 * Procedural pile ContO — registers 5 trackers (Java ContO(seed,w,h,m,t,x,z,y)).
 * Frees any prior heap contents of *out first.
 * Returns 0 on success, -1 on failure.
 */
int nfm_conto_pile(NfmContO* out, int seed, int width, int height,
                   NfmMedium* med, NfmTrackers* trk, int px, int pz, int py);

/** Free all malloc'd arrays owned by ContO; leaves struct zeroed. Does not free c itself. */
void nfm_conto_free(NfmContO* c);

/** Alias: free internals + zero (same as nfm_conto_free). */
void nfm_conto_clear(NfmContO* c);

/** Match ContO.clear_tracks_template — drops track template txy / tnt. */
void nfm_conto_clear_tracks_template(NfmContO* c);

/**
 * Deep copy src into *dst (frees dst first). m/t pointers are copied as-is.
 * Returns 0 on success, -1 on alloc failure.
 */
int nfm_conto_copy(NfmContO* dst, const NfmContO* src);

/**
 * Heap-allocate a ContO and deep-copy src into it. Caller frees with
 * nfm_conto_free then free(). Returns NULL on failure.
 */
NfmContO* nfm_conto_clone(const NfmContO* src);

/** Match ContO.dust RNG side-effects (visual buffers optional). */
void nfm_conto_dust(NfmContO* c, int n, float n2, float n3, float n4, int n5,
                    int n6, float n7, int n8, bool b);

/**
 * Load cars[0..15] + pieces[56..123] from models_dir/NAME.rad into out124[124].
 * Each slot is initialized (prior contents freed). Returns true on success.
 */
bool nfm_load_models_zip(const char* models_dir, NfmContO* out124, NfmMedium* m,
                         NfmTrackers* t, char* err, size_t errlen);

#ifdef __cplusplus
}
#endif

#endif /* NFM_CONTO_H */
