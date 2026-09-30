#ifndef NFM_TRACKERS_H
#define NFM_TRACKERS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { NFM_TRACKERS_MAX = 6700 };

typedef struct NfmSectList {
  int* idx;
  int n;
} NfmSectList;

typedef struct NfmTrackers {
  int x[NFM_TRACKERS_MAX];
  int y[NFM_TRACKERS_MAX];
  int z[NFM_TRACKERS_MAX];
  int xy[NFM_TRACKERS_MAX];
  int zy[NFM_TRACKERS_MAX];
  int skd[NFM_TRACKERS_MAX];
  int dam[NFM_TRACKERS_MAX];
  bool notwall[NFM_TRACKERS_MAX];
  bool decor[NFM_TRACKERS_MAX];
  int c[NFM_TRACKERS_MAX][3];
  int radx[NFM_TRACKERS_MAX];
  int radz[NFM_TRACKERS_MAX];
  int rady[NFM_TRACKERS_MAX];
  int nt;
  int sx;
  int sz;
  int ncx;
  int ncz;
  /* sect[i][j] after devidetrackers: width=sect_w, height=sect_h (pre --ncx/--ncz). */
  NfmSectList* sect;
  int sect_w;
  int sect_h;
} NfmTrackers;

void nfm_trackers_init(NfmTrackers* t);
void nfm_trackers_free_sect(NfmTrackers* t);
void nfm_trackers_devidetrackers(NfmTrackers* t, int sx, int n, int sz, int n2);

static inline int nfm_trackers_py(int a, int b, int c, int d) {
  return (a - b) * (a - b) + (c - d) * (c - d);
}

/** Flat sect[i][j] → sect[i * sect_h + j]. */
static inline NfmSectList* nfm_trackers_sect(NfmTrackers* t, int i, int j) {
  return &t->sect[i * t->sect_h + j];
}

#ifdef __cplusplus
}
#endif

#endif /* NFM_TRACKERS_H */
