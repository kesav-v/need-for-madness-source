#include "nfm/checkpoints.h"

#include "nfm/conto.h"
#include "nfm/mad.h"

#include <stdlib.h>
#include <string.h>

void nfm_checkpoints_init(NfmCheckPoints* cp) {
  int i;
  if (!cp) return;
  memset(cp, 0, sizeof(*cp));
  cp->stage = 1;
  for (i = 0; i < 8; ++i) cp->pos[i] = 7;
}

int nfm_checkpoints_py(const NfmCheckPoints* cp, int a, int b, int c, int d) {
  (void)cp;
  return (a - b) * (a - b) + (c - d) * (c - d);
}

void nfm_checkpoints_calprox(NfmCheckPoints* cp) {
  int span = 0;
  int i, j;
  if (!cp) return;
  for (i = 0; i < cp->n - 1; ++i) {
    for (j = i + 1; j < cp->n; ++j) {
      int dx = abs(cp->x[i] - cp->x[j]);
      int dz = abs(cp->z[i] - cp->z[j]);
      if (dx > span) span = dx;
      if (dz > span) span = dz;
    }
  }
  cp->prox = span / 90.0f;
}

void nfm_checkpoints_checkstat(NfmCheckPoints* cp, struct NfmMad* mad,
                               struct NfmContO* cars, int nplayers, int focus,
                               int unused, int n3) {
  const int n2 = focus;
  int i, j, k, l, n9;
  (void)unused;
  if (!cp || !mad || !cars) return;

  if (!cp->haltall) {
    cp->pcleared = mad[n2].pcleared;
    for (i = 0; i < nplayers; ++i) {
      cp->magperc[i] =
          (float)mad[i].hitmag / mad[i].cd->maxmag[mad[i].cn];
      if (cp->magperc[i] > 1.0f) cp->magperc[i] = 1.0f;
      cp->pos[i] = 0;
      cp->onscreen[i] = cars[i].dist;
      cp->opx[i] = cars[i].x;
      cp->opz[i] = cars[i].z;
      cp->omxz[i] = mad[i].mxz;
      if (cp->dested[i] == 0)
        cp->clear[i] = mad[i].clear;
      else
        cp->clear[i] = -1;
      mad[i].outshakedam = mad[i].shakedam;
      mad[i].shakedam = 0;
    }
    for (j = 0; j < nplayers; ++j) {
      for (k = j + 1; k < nplayers; ++k) {
        if (cp->clear[j] != cp->clear[k]) {
          if (cp->clear[j] < cp->clear[k])
            ++cp->pos[j];
          else
            ++cp->pos[k];
        } else {
          int n6 = mad[j].pcleared + 1;
          if (n6 >= cp->n) n6 = 0;
          while (cp->typ[n6] <= 0) {
            if (++n6 >= cp->n) n6 = 0;
          }
          if (nfm_checkpoints_py(cp, cars[j].x / 100, cp->x[n6] / 100,
                                 cars[j].z / 100, cp->z[n6] / 100) >
              nfm_checkpoints_py(cp, cars[k].x / 100, cp->x[n6] / 100,
                                 cars[k].z / 100, cp->z[n6] / 100))
            ++cp->pos[j];
          else
            ++cp->pos[k];
        }
      }
    }
    if (cp->stage > 2) {
      for (l = 0; l < nplayers; ++l) {
        if (cp->clear[l] == cp->nlaps * cp->nsp && cp->pos[l] == 0) {
          if (l == n2) {
            int postwo_i;
            for (postwo_i = 0; postwo_i < nplayers; ++postwo_i) {
              if (cp->pos[postwo_i] == 1) cp->postwo = postwo_i;
            }
            if (nfm_checkpoints_py(cp, cp->opx[n2] / 100, cp->opx[cp->postwo] / 100,
                                   cp->opz[n2] / 100, cp->opz[cp->postwo] / 100) <
                    14000 &&
                cp->clear[n2] - cp->clear[cp->postwo] == 1) {
              cp->catchfin = 30;
            }
          } else if (cp->pos[n2] == 1 &&
                     nfm_checkpoints_py(cp, cp->opx[n2] / 100, cp->opx[l] / 100,
                                        cp->opz[n2] / 100, cp->opz[l] / 100) <
                         14000 &&
                     cp->clear[l] - cp->clear[n2] == 1) {
            cp->catchfin = 30;
            cp->postwo = l;
          }
        }
      }
    }
  }
  cp->wasted = 0;
  for (n9 = 0; n9 < nplayers; ++n9) {
    if ((n2 != n9 || n3 >= 2) && mad[n9].dest) ++cp->wasted;
  }
  if (cp->catchfin != 0 && n3 < 2) {
    --cp->catchfin;
  }
}
