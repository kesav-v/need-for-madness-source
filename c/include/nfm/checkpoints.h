#ifndef NFM_CHECKPOINTS_H
#define NFM_CHECKPOINTS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decls avoid include cycle with mad.h (which includes this header). */
struct NfmMad;
struct NfmContO;

typedef struct NfmCheckPoints {
  int x[140];
  int z[140];
  int y[140];
  int typ[140];
  int pcs;
  int nsp;
  int n;
  int fx[5];
  int fz[5];
  int fy[5];
  bool roted[5];
  bool special[5];
  int fn;
  int stage;
  int nlaps;
  int nfix;
  bool notb;
  int pos[8];
  int clear[8];
  int dested[8];
  float magperc[8];
  int wasted;
  bool haltall;
  int pcleared;
  int opx[8];
  int opz[8];
  int onscreen[8];
  int omxz[8];
  int catchfin;
  int postwo;
  float prox;
} NfmCheckPoints;

void nfm_checkpoints_init(NfmCheckPoints* cp);
void nfm_checkpoints_calprox(NfmCheckPoints* cp);
void nfm_checkpoints_checkstat(NfmCheckPoints* cp, struct NfmMad* mad,
                               struct NfmContO* cars, int nplayers, int focus,
                               int unused, int n3);
int nfm_checkpoints_py(const NfmCheckPoints* cp, int a, int b, int c, int d);

#ifdef __cplusplus
}
#endif

#endif /* NFM_CHECKPOINTS_H */
