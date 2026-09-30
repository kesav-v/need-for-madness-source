#ifndef NFM_CONTROL_H
#define NFM_CONTROL_H

#include <math.h>
#include <stdbool.h>

#include "nfm/medium.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decls — mad.h includes control.h; full types available in .c units. */
struct NfmMad;
struct NfmContO;
struct NfmCheckPoints;
struct NfmTrackers;

/** thread_local Control::autodrive — single-threaded tests may treat as global. */
extern _Thread_local bool nfm_control_autodrive;

typedef struct NfmControl {
  NfmMedium* m;
  bool left, right, up, down, handb;
  int lookback;
  bool enter, exit, arrace;
  bool zyinv;
  int pan;
  int attack;
  int acr;
  bool afta;
  int trfix;
  int acuracy;
  int upwait;
  bool forget;
  bool bulistc;
  int runbul;
  int revstart;
  int oupnt;
  bool gowait;
  int apunch;
  bool exitattack;
  int hold;
  int statusque;
  int fpnt[5];
  bool agressed;
  float skiplev;
  int clrnce;
  int rampp;
  int turntyp;
  float aim;
  int saftey;
  bool perfection;
  float mustland;
  bool usebounce;
  float trickprf;
  int stuntf;
  bool lastl;
  bool wlastl;
  int wall;
  int lwall;
  int stcnt;
  int turncnt;
  int randtcnt;
  int upcnt;
  int trickfase;
  int swat;
  bool udcomp;
  bool lrcomp;
  bool udbare;
  bool lrbare;
  bool onceu;
  bool onced;
  bool oncel;
  bool oncer;
  int lrdirect;
  int uddirect;
  int lrstart;
  int udstart;
  int oxy;
  int ozy;
  int flycnt;
  bool lrswt;
  bool udswt;
  int actwait;
  int cntrn;
  int wtz;
  int wtx;
  int frx;
  int frz;
  int frad;
  int avoidnlev;
} NfmControl;

static inline void nfm_control_init(NfmControl* ctrl, NfmMedium* med) {
  int i;
  ctrl->m = med;
  ctrl->left = false;
  ctrl->right = false;
  ctrl->up = false;
  ctrl->down = false;
  ctrl->handb = false;
  ctrl->lookback = 0;
  ctrl->enter = false;
  ctrl->exit = false;
  ctrl->arrace = false;
  ctrl->zyinv = false;
  ctrl->pan = 0;
  ctrl->attack = 0;
  ctrl->acr = 0;
  ctrl->afta = false;
  ctrl->trfix = 0;
  ctrl->acuracy = 0;
  ctrl->upwait = 0;
  ctrl->forget = false;
  ctrl->bulistc = false;
  ctrl->runbul = 0;
  ctrl->revstart = 0;
  ctrl->oupnt = 0;
  ctrl->gowait = false;
  ctrl->apunch = 0;
  ctrl->exitattack = false;
  ctrl->hold = 0;
  ctrl->statusque = 0;
  for (i = 0; i < 5; ++i) ctrl->fpnt[i] = 0;
  ctrl->agressed = false;
  ctrl->skiplev = 1.0f;
  ctrl->clrnce = 5;
  ctrl->rampp = 0;
  ctrl->turntyp = 0;
  ctrl->aim = 0.0f;
  ctrl->saftey = 30;
  ctrl->perfection = false;
  ctrl->mustland = 0.5f;
  ctrl->usebounce = false;
  ctrl->trickprf = 0.5f;
  ctrl->stuntf = 0;
  ctrl->lastl = false;
  ctrl->wlastl = false;
  ctrl->wall = -1;
  ctrl->lwall = -1;
  ctrl->stcnt = 0;
  ctrl->turncnt = 0;
  ctrl->randtcnt = 0;
  ctrl->upcnt = 0;
  ctrl->trickfase = 0;
  ctrl->swat = 0;
  ctrl->udcomp = false;
  ctrl->lrcomp = false;
  ctrl->udbare = false;
  ctrl->lrbare = false;
  ctrl->onceu = false;
  ctrl->onced = false;
  ctrl->oncel = false;
  ctrl->oncer = false;
  ctrl->lrdirect = 0;
  ctrl->uddirect = 0;
  ctrl->lrstart = 0;
  ctrl->udstart = 0;
  ctrl->oxy = 0;
  ctrl->ozy = 0;
  ctrl->flycnt = 0;
  ctrl->lrswt = false;
  ctrl->udswt = false;
  ctrl->actwait = 0;
  ctrl->cntrn = 0;
  ctrl->wtz = 0;
  ctrl->wtx = 0;
  ctrl->frx = 0;
  ctrl->frz = 0;
  ctrl->frad = 0;
  ctrl->avoidnlev = 0;
}

void nfm_control_apply_autodrive(NfmControl* ctrl);
void nfm_control_reset(NfmControl* ctrl, struct NfmCheckPoints* checkPoints,
                       int car_index);
void nfm_control_preform(NfmControl* ctrl, struct NfmMad* mad,
                         struct NfmContO* contO,
                         struct NfmCheckPoints* checkPoints,
                         struct NfmTrackers* trackers);

static inline int nfm_control_py(int a, int b, int c, int d) {
  return (a - b) * (a - b) + (c - d) * (c - d);
}

static inline int nfm_control_pys(int a, int b, int c, int d) {
  return (int)sqrt((double)((a - b) * (a - b) + (c - d) * (c - d)));
}

#ifdef __cplusplus
}
#endif

#endif /* NFM_CONTROL_H */
