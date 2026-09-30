#ifndef NFM_MAD_H
#define NFM_MAD_H

#include <stdbool.h>
#include <stdint.h>

#include "nfm/car_define.h"
#include "nfm/checkpoints.h"
#include "nfm/conto.h"
#include "nfm/control.h"
#include "nfm/medium.h"
#include "nfm/trackers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfmMad {
  NfmMedium* m;
  NfmCarDefine* cd;
  int cn;
  int im;
  int mxz;
  int cxz;
  bool dominate[8];
  bool caught[8];
  int pzy;
  int pxy;
  float speed;
  float forca;
  float scy[4];
  float scz[4];
  float scx[4];
  float drag;
  bool mtouch;
  bool wtouch;
  int cntouch;
  bool capsized;
  int txz;
  int fxz;
  int pmlt;
  int nmlt;
  int dcnt;
  int skid;
  bool pushed;
  bool gtouch;
  bool pl, pr, pd, pu;
  int loop;
  float ucomp, dcomp, lcomp, rcomp;
  int lxz;
  int travxy, travzy, travxz;
  int trcnt, capcnt, srfcnt;
  bool rtab, ftab, btab, surfer;
  float powerup;
  int xtpower;
  float tilt;
  int squash;
  int nbsq;
  int hitmag;
  int cntdest;
  bool dest;
  bool newcar;
  int pan;
  int pcleared;
  int clear;
  int nlaps;
  int focus;
  float power;
  int missedcp;
  int lastcolido;
  int point;
  bool nofocus;
  int rpdcatch;
  int newedcar;
  int fixes;
  int shakedam;
  int outshakedam;
  bool colidim;
  int crank[4][4];
  int lcrank[4][4];

  /* stubs for xtGraphics coupling (norender: no FX, but fields exist) */
  int xt_im;
  int xt_multion;
} NfmMad;

/** Zero + defaults matching Mad.hpp in-class initializers; set cd/m/im. */
void nfm_mad_init(NfmMad* self, NfmCarDefine* cd, NfmMedium* m, int im);

/** Clear xtGraphics.scrape statics (call at race start for in-process --n). */
void nfm_mad_reset_scrape_rng(void);

void nfm_mad_reseto(NfmMad* self, int cn, NfmContO* contO,
                    NfmCheckPoints* checkPoints);
void nfm_mad_drive(NfmMad* self, NfmControl* control, NfmContO* contO,
                   NfmTrackers* trackers, NfmCheckPoints* checkPoints);
void nfm_mad_colide(NfmMad* self, NfmContO* contO, NfmMad* mad,
                    NfmContO* contO2);
void nfm_mad_distruct(NfmMad* self, NfmContO* contO);
int nfm_mad_regy(NfmMad* self, int n, float n2, NfmContO* contO);
int nfm_mad_regx(NfmMad* self, int n, float n2, NfmContO* contO);
int nfm_mad_regz(NfmMad* self, int n, float n2, NfmContO* contO);
void nfm_mad_rot(NfmMad* self, float* array, float* array2, int n, int n2,
                 int n3, int n4);
int nfm_mad_rpy(NfmMad* self, float n, float n2, float n3, float n4, float n5,
                float n6);

static inline int nfm_mad_py(int a, int b, int c, int d) {
  return (a - b) * (a - b) + (c - d) * (c - d);
}

#ifdef __cplusplus
}
#endif

#endif /* NFM_MAD_H */
