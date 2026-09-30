#include "nfm/trackers.h"

#include <stdlib.h>
#include <string.h>

void nfm_trackers_init(NfmTrackers* t) {
  memset(t, 0, sizeof(*t));
}

void nfm_trackers_free_sect(NfmTrackers* t) {
  int i, n;
  if (!t->sect) return;
  n = t->sect_w * t->sect_h;
  for (i = 0; i < n; ++i) {
    free(t->sect[i].idx);
    t->sect[i].idx = NULL;
    t->sect[i].n = 0;
  }
  free(t->sect);
  t->sect = NULL;
  t->sect_w = 0;
  t->sect_h = 0;
}

void nfm_trackers_devidetrackers(NfmTrackers* t, int sx_in, int n, int sz_in,
                                 int n2) {
  int i, j, k, l;
  nfm_trackers_free_sect(t);
  t->sx = sx_in;
  t->sz = sz_in;
  t->ncx = n / 3000;
  if (t->ncx <= 0) t->ncx = 1;
  t->ncz = n2 / 3000;
  if (t->ncz <= 0) t->ncz = 1;
  t->sect_w = t->ncx;
  t->sect_h = t->ncz;
  t->sect = (NfmSectList*)calloc((size_t)t->sect_w * (size_t)t->sect_h,
                                 sizeof(NfmSectList));
  if (!t->sect) return;

  for (i = 0; i < t->ncx; ++i) {
    for (j = 0; j < t->ncz; ++j) {
      const int n3 = t->sx + i * 3000 + 1500;
      const int n4 = t->sz + j * 3000 + 1500;
      int array[NFM_TRACKERS_MAX];
      int n5 = 0;
      NfmSectList* cell = nfm_trackers_sect(t, i, j);
      for (k = 0; k < t->nt; ++k) {
        const int p = nfm_trackers_py(n3, t->x[k], n4, t->z[k]);
        if (p < 20250000 && p > 0 && t->dam[k] != 167) {
          array[n5++] = k;
        }
      }
      if (i == 0 || j == 0 || i == t->ncx - 1 || j == t->ncz - 1) {
        for (l = 0; l < t->nt; ++l) {
          if (t->dam[l] == 167) array[n5++] = l;
        }
      }
      if (n5 == 0) array[n5++] = 0;
      cell->n = n5;
      cell->idx = (int*)malloc((size_t)n5 * sizeof(int));
      if (cell->idx) memcpy(cell->idx, array, (size_t)n5 * sizeof(int));
    }
  }
  for (k = 0; k < t->nt; ++k) {
    if (t->dam[k] == 167) t->dam[k] = 1;
  }
  --t->ncx;
  --t->ncz;
}
