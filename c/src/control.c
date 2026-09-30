#include "nfm/control.h"

#include "nfm/checkpoints.h"
#include "nfm/conto.h"
#include "nfm/mad.h"
#include "nfm/trackers.h"

#include <math.h>
#include <stdlib.h>

/* Two sequential Medium::random draws compared (preserves RNG order). */
static int nfm_rand_gt_rand(NfmMedium* m) {
    float _a = nfm_medium_random(m);
    float _b = nfm_medium_random(m);
    return _a > _b;
}

_Thread_local bool nfm_control_autodrive = false;

void nfm_control_apply_autodrive(NfmControl* ctrl) {
        if (!nfm_control_autodrive) {
            return;
        }
        ctrl->left = false;
        ctrl->right = false;
        ctrl->up = true;
        ctrl->down = false;
        ctrl->handb = false;

}

void nfm_control_reset(NfmControl* ctrl, NfmCheckPoints* checkPoints, int n) {
        NfmMedium* m = ctrl->m;

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
        if (checkPoints->stage == 16 || checkPoints->stage == 18) {
            ctrl->hold = 50;
        }
        if (checkPoints->stage == 17) {
            ctrl->hold = 10;
        }
        if (checkPoints->stage == 20) {
            ctrl->hold = 30;
        }
        if (checkPoints->stage == 21) {
            if (n != 13) {
                ctrl->hold = 35;
                ctrl->revstart = 25;
            }
            else {
                ctrl->hold = 5;
            }
            ctrl->statusque = 0;
        }
        if (checkPoints->stage == 22) {
            if (n != 13) {
                ctrl->hold = (int)(20.0f + 10.0f * nfm_medium_random(m));
                ctrl->revstart = (int)(10.0f + 10.0f * nfm_medium_random(m));
            }
            else {
                ctrl->hold = 5;
            }
            ctrl->statusque = 0;
        }
        if (checkPoints->stage == 24) {
            ctrl->hold = 30;
            ctrl->statusque = 0;
            if (n != 14) {
                ctrl->revstart = 1;
            }
        }
        if (checkPoints->stage == 25) {
            ctrl->hold = 40;
        }
        if (checkPoints->stage == 26) {
            ctrl->hold = 20;
        }
        if (checkPoints->stage != 19 && checkPoints->stage != 26) {
            for (int i = 0; i < checkPoints->fn; ++i) {
                int pyv = -10;
                for (int j = 0; j < checkPoints->n; ++j) {
                    if (nfm_control_py(checkPoints->fx[i] / 100, checkPoints->x[j] / 100, checkPoints->fz[i] / 100, checkPoints->z[j] / 100) < pyv || pyv == -10) {
                        pyv = nfm_control_py(checkPoints->fx[i] / 100, checkPoints->x[j] / 100, checkPoints->fz[i] / 100, checkPoints->z[j] / 100);
                        ctrl->fpnt[i] = j;
                    }
                }
            }
            for (int k = 0; k < checkPoints->fn; ++k) {
                ctrl->fpnt[k] -= 4;
                if (ctrl->fpnt[k] < 0) {
                    ctrl->fpnt[k] += checkPoints->nsp;
                }
            }
        }
        else {
            if (checkPoints->stage == 19) {
                ctrl->fpnt[0] = 14;
                ctrl->fpnt[1] = 36;
            }
            if (checkPoints->stage == 26) {
                ctrl->fpnt[3] = 39;
            }
        }
        ctrl->left = false;
        ctrl->right = false;
        ctrl->up = false;
        ctrl->down = false;
        ctrl->handb = false;
        ctrl->lookback = 0;
        ctrl->arrace = false;

}

void nfm_control_preform(NfmControl* ctrl, NfmMad* mad, NfmContO* contO,
                           NfmCheckPoints* checkPoints, NfmTrackers* trackers) {
        NfmMedium* m = ctrl->m;

        ctrl->left = false;
        ctrl->right = false;
        ctrl->up = false;
        ctrl->down = false;
        ctrl->handb = false;
        if (!mad->dest) {
            if (mad->mtouch) {
                if (ctrl->stcnt > ctrl->statusque) {
                    int stage = checkPoints->stage;
                    if (stage > 10) {
                        stage -= 10;
                    }
                    ctrl->acuracy = (7 - checkPoints->pos[mad->im]) * checkPoints->pos[0] * (6 - stage * 2);
                    if (ctrl->acuracy < 0 || checkPoints->stage == -1) {
                        ctrl->acuracy = 0;
                    }
                    ctrl->clrnce = 5;
                    if (checkPoints->stage == 16 || checkPoints->stage == 21) {
                        ctrl->clrnce = 2;
                    }
                    if (checkPoints->stage == 22 && (mad->pcleared == 27 || mad->pcleared == 17)) {
                        ctrl->clrnce = 3;
                    }
                    if (checkPoints->stage == 26 && mad->pcleared == 33) {
                        ctrl->clrnce = 3;
                    }
                    float n = 0.0f;
                    if (checkPoints->stage == 1) {
                        n = 2.0f;
                    }
                    if (checkPoints->stage == 2) {
                        n = 1.5f;
                    }
                    if (checkPoints->stage == 3 && mad->cn != 6) {
                        n = 0.5f;
                    }
                    if (checkPoints->stage == 4) {
                        n = 0.5f;
                    }
                    if (checkPoints->stage == 11) {
                        n = 2.0f;
                    }
                    if (checkPoints->stage == 12) {
                        n = 1.5f;
                    }
                    if (checkPoints->stage == 13 && mad->cn != 9) {
                        n = 0.5f;
                    }
                    if (checkPoints->stage == 14) {
                        n = 0.5f;
                    }
                    ctrl->upwait = (int)((checkPoints->pos[0] - checkPoints->pos[mad->im]) * (checkPoints->pos[0] - checkPoints->pos[mad->im]) * (checkPoints->pos[0] - checkPoints->pos[mad->im]) * n);
                    if (ctrl->upwait > 80) {
                        ctrl->upwait = 80;
                    }
                    if ((checkPoints->stage == 11 || checkPoints->stage == 1) && ctrl->upwait < 20) {
                        ctrl->upwait = 20;
                    }
                    float skiplev_cap = 0.0f;
                    if (checkPoints->stage == 1 || checkPoints->stage == 2) {
                        skiplev_cap = 1.0f;
                    }
                    if (checkPoints->stage == 4) {
                        skiplev_cap = 0.5f;
                    }
                    if (checkPoints->stage == 7) {
                        skiplev_cap = 0.5f;
                    }
                    if (checkPoints->stage == 10) {
                        skiplev_cap = 0.5f;
                    }
                    if (checkPoints->stage == 11 || checkPoints->stage == 12) {
                        skiplev_cap = 1.0f;
                    }
                    if (checkPoints->stage == 13) {
                        skiplev_cap = 0.5f;
                    }
                    if (checkPoints->stage == 14) {
                        skiplev_cap = 0.5f;
                    }
                    if (checkPoints->stage == 15) {
                        skiplev_cap = 0.2f;
                    }
                    if (checkPoints->pos[mad->im] - checkPoints->pos[0] >= -1) {
                        ctrl->skiplev -= 0.1;
                        if (ctrl->skiplev < 0.0f) {
                            ctrl->skiplev = 0.0f;
                        }
                    }
                    else {
                        ctrl->skiplev += 0.2;
                        if (ctrl->skiplev > skiplev_cap) {
                            ctrl->skiplev = skiplev_cap;
                        }
                    }
                    if (checkPoints->stage == 18) {
                        if (mad->pcleared >= 10 && mad->pcleared <= 24) {
                            ctrl->skiplev = 1.0f;
                        }
                        else {
                            ctrl->skiplev = 0.0f;
                        }
                    }
                    if (checkPoints->stage == 21) {
                        ctrl->skiplev = 0.0f;
                        if (mad->pcleared == 5) {
                            ctrl->skiplev = 1.0f;
                        }
                        if (mad->pcleared == 28 || mad->pcleared == 35) {
                            ctrl->skiplev = 0.5f;
                        }
                    }
                    if (checkPoints->stage == 23) {
                        ctrl->skiplev = 0.5f;
                    }
                    if (checkPoints->stage == 24 || checkPoints->stage == 22) {
                        ctrl->skiplev = 1.0f;
                    }
                    if (checkPoints->stage == 26 || checkPoints->stage == 25 || checkPoints->stage == 20) {
                        ctrl->skiplev = 0.0f;
                    }
                    ctrl->rampp = (int)(nfm_medium_random(m) * 4.0f - 2.0f);
                    if (mad->power == 98.0f) {
                        ctrl->rampp = -1;
                    }
                    if (mad->power < 75.0f && ctrl->rampp == -1) {
                        ctrl->rampp = 0;
                    }
                    if (mad->power < 60.0f) {
                        ctrl->rampp = 1;
                    }
                    if (checkPoints->stage == 6) {
                        ctrl->rampp = 2;
                    }
                    if (checkPoints->stage == 18 && mad->pcleared >= 45) {
                        ctrl->rampp = 2;
                    }
                    if (checkPoints->stage == 22 && mad->pcleared == 17) {
                        ctrl->rampp = 2;
                    }
                    if (checkPoints->stage == 25 || checkPoints->stage == 26) {
                        ctrl->rampp = 0;
                    }
                    if (ctrl->cntrn == 0) {
                        ctrl->agressed = false;
                        ctrl->turntyp = (int)(nfm_medium_random(m) * 4.0f);
                        if (checkPoints->stage == 3 && mad->cn == 6) {
                            ctrl->turntyp = 1;
                            if (ctrl->attack == 0) {
                                ctrl->agressed = true;
                            }
                        }
                        if (checkPoints->stage == 9 && mad->cn == 15) {
                            ctrl->turntyp = 1;
                            if (ctrl->attack == 0) {
                                ctrl->agressed = true;
                            }
                        }
                        if (checkPoints->stage == 13 && mad->cn == 9) {
                            ctrl->turntyp = 1;
                            if (ctrl->attack == 0) {
                                ctrl->agressed = true;
                            }
                        }
                        if (checkPoints->pos[0] - checkPoints->pos[mad->im] < 0) {
                            ctrl->turntyp = (int)(nfm_medium_random(m) * 2.0f);
                        }
                        if (checkPoints->stage == 10) {
                            ctrl->turntyp = 2;
                        }
                        if (checkPoints->stage == 18) {
                            ctrl->turntyp = 2;
                        }
                        if (checkPoints->stage == 20) {
                            ctrl->turntyp = 0;
                        }
                        if (checkPoints->stage == 23) {
                            ctrl->turntyp = 1;
                        }
                        if (checkPoints->stage == 24) {
                            ctrl->turntyp = 0;
                        }
                        if (ctrl->attack != 0) {
                            ctrl->turntyp = 2;
                            if (checkPoints->stage == 9 || checkPoints->stage == 10 || checkPoints->stage == 19 || checkPoints->stage == 21 || checkPoints->stage == 23 || checkPoints->stage == 27) {
                                ctrl->turntyp = (int)(nfm_medium_random(m) * 3.0f);
                            }
                            if (checkPoints->stage == 26 && checkPoints->clear[mad->im] - checkPoints->clear[0] >= 5) {
                                ctrl->turntyp = 0;
                            }
                        }
                        if (checkPoints->stage == 6) {
                            ctrl->turntyp = 1;
                            ctrl->agressed = true;
                        }
                        if (checkPoints->stage == 7 || checkPoints->stage == 9 || checkPoints->stage == 10 || checkPoints->stage == 16 || checkPoints->stage == 17 || checkPoints->stage == 19 || checkPoints->stage == 20 || checkPoints->stage == 21 || checkPoints->stage == 22 || checkPoints->stage == 24 || checkPoints->stage == 26 || checkPoints->stage == 27) {
                            ctrl->agressed = true;
                        }
                        if (checkPoints->stage == -1) {
                            if (nfm_rand_gt_rand(m)) {
                                ctrl->agressed = true;
                            }
                            else {
                                ctrl->agressed = false;
                            }
                        }
                        ctrl->cntrn = 5;
                    }
                    else {
                        --ctrl->cntrn;
                    }
                    ctrl->saftey = (int)((98.0f - mad->power) / 2.0f * (nfm_medium_random(m) / 2.0f + 0.5));
                    if (ctrl->saftey > 20) {
                        ctrl->saftey = 20;
                    }
                    float n2 = 0.0f;
                    if (checkPoints->stage == 1 || checkPoints->stage == 11) {
                        n2 = 0.9f;
                    }
                    if (checkPoints->stage == 2 || checkPoints->stage == 12) {
                        n2 = 0.7f;
                    }
                    if (checkPoints->stage == 4 || checkPoints->stage == 13) {
                        n2 = 0.4f;
                    }
                    ctrl->mustland = n2 + (float)(nfm_medium_random(m) / 2.0f - 0.25);
                    float n3 = 1.0f;
                    if (checkPoints->stage == 1 || checkPoints->stage == 11) {
                        n3 = 5.0f;
                    }
                    if (checkPoints->stage == 2 || checkPoints->stage == 12) {
                        n3 = 2.0f;
                    }
                    if (checkPoints->stage == 4 || checkPoints->stage == 13) {
                        n3 = 1.5f;
                    }
                    if (mad->power > 50.0f) {
                        if (checkPoints->pos[0] - checkPoints->pos[mad->im] > 0) {
                            ctrl->saftey *= (int)n3;
                        }
                        else {
                            ctrl->mustland = 0.0f;
                        }
                    }
                    else {
                        ctrl->mustland -= 0.5f;
                    }
                    if (checkPoints->stage == 18 || checkPoints->stage == 20 || checkPoints->stage == 22 || checkPoints->stage == 24) {
                        ctrl->mustland = 0.0f;
                    }
                    ctrl->stuntf = 0;
                    if (checkPoints->stage == 8) {
                        ctrl->stuntf = 17;
                    }
                    if (checkPoints->stage == 18 && mad->pcleared == 57) {
                        ctrl->stuntf = 1;
                    }
                    if (checkPoints->stage == 19 && mad->pcleared == 3) {
                        ctrl->stuntf = 2;
                    }
                    if (checkPoints->stage == 20) {
                        if (checkPoints->pos[0] < checkPoints->pos[mad->im] || abs(checkPoints->clear[0] - mad->clear) >= 2 || mad->clear < 2) {
                            ctrl->stuntf = 4;
                            ctrl->saftey = 10;
                        }
                        else {
                            ctrl->stuntf = 3;
                        }
                    }
                    if (checkPoints->stage == 21 && mad->pcleared == 21) {
                        ctrl->stuntf = 1;
                    }
                    if (checkPoints->stage == 24) {
                        ctrl->saftey = 10;
                        if (mad->pcleared >= 4 && mad->pcleared < 70) {
                            ctrl->stuntf = 4;
                        }
                        else if (mad->cn == 12 || mad->cn == 8) {
                            ctrl->stuntf = 2;
                        }
                        if (mad->cn == 14) {
                            ctrl->stuntf = 6;
                        }
                    }
                    if (checkPoints->stage == 26) {
                        ctrl->mustland = 0.0f;
                        ctrl->saftey = 10;
                        if ((mad->pcleared == 15 || mad->pcleared == 51) && (nfm_medium_random(m) > 0.4 || ctrl->trfix != 0)) {
                            ctrl->stuntf = 7;
                        }
                        if (mad->pcleared == 42) {
                            ctrl->stuntf = 1;
                        }
                        if (mad->pcleared == 77) {
                            ctrl->stuntf = 7;
                        }
                        ctrl->avoidnlev = (int)(2700.0f * nfm_medium_random(m));
                    }
                    ctrl->trickprf = (mad->power - 38.0f) / 50.0f - nfm_medium_random(m) / 2.0f;
                    if (mad->power < 60.0f) {
                        ctrl->trickprf = -1.0f;
                    }
                    if (checkPoints->stage == 6 && ctrl->trickprf > 0.5) {
                        ctrl->trickprf = 0.5f;
                    }
                    if (checkPoints->stage == 3 && mad->cn == 6 && ctrl->trickprf > 0.7) {
                        ctrl->trickprf = 0.7f;
                    }
                    if (checkPoints->stage == 13 && mad->cn == 9 && ctrl->trickprf > 0.7) {
                        ctrl->trickprf = 0.7f;
                    }
                    if (checkPoints->stage == 16 && ctrl->trickprf > 0.3) {
                        ctrl->trickprf = 0.3f;
                    }
                    if (checkPoints->stage == 18 && ctrl->trickprf > 0.2) {
                        ctrl->trickprf = 0.2f;
                    }
                    if (checkPoints->stage == 19) {
                        if (ctrl->trickprf > 0.5) {
                            ctrl->trickprf = 0.5f;
                        }
                        if ((mad->im == 6 || mad->im == 5) && ctrl->trickprf > 0.3) {
                            ctrl->trickprf = 0.3f;
                        }
                    }
                    if (checkPoints->stage == 21 && ctrl->trickprf != -1.0f) {
                        ctrl->trickprf *= 0.75f;
                    }
                    if (checkPoints->stage == 22 && (mad->pcleared == 55 || mad->pcleared == 7)) {
                        ctrl->trickprf = -1.0f;
                        ctrl->stuntf = 5;
                    }
                    if (checkPoints->stage == 23 && ctrl->trickprf > 0.4) {
                        ctrl->trickprf = 0.4f;
                    }
                    if (checkPoints->stage == 24 && ctrl->trickprf > 0.5) {
                        ctrl->trickprf = 0.5f;
                    }
                    if (checkPoints->stage == 27) {
                        ctrl->trickprf = -1.0f;
                    }
                    if (nfm_medium_random(m) > mad->power / 100.0f) {
                        ctrl->usebounce = true;
                    }
                    else {
                        ctrl->usebounce = false;
                    }
                    if (checkPoints->stage == 9) {
                        ctrl->usebounce = false;
                    }
                    if (checkPoints->stage == 14 || checkPoints->stage == 16) {
                        ctrl->usebounce = true;
                    }
                    if (checkPoints->stage == 20 || checkPoints->stage == 24) {
                        ctrl->usebounce = false;
                    }
                    if (nfm_medium_random(m) > (float)mad->hitmag / mad->cd->maxmag[mad->cn]) {
                        ctrl->perfection = false;
                    }
                    else {
                        ctrl->perfection = true;
                    }
                    if (100.0f * mad->hitmag / mad->cd->maxmag[mad->cn] > 60.0f) {
                        ctrl->perfection = true;
                    }
                    if (checkPoints->stage == 3 && mad->cn == 6) {
                        ctrl->perfection = true;
                    }
                    if (checkPoints->stage == 6 || checkPoints->stage == 8 || checkPoints->stage == 9 || checkPoints->stage == 10 || checkPoints->stage == 16 || checkPoints->stage == 18 || checkPoints->stage == 19 || checkPoints->stage == 20 || checkPoints->stage == 21 || checkPoints->stage == 22 || checkPoints->stage == 24 || checkPoints->stage == 26 || checkPoints->stage == 27) {
                        ctrl->perfection = true;
                    }
                    if (ctrl->attack == 0) {
                        bool afta_loc = true;
                        if (checkPoints->stage == 3 || checkPoints->stage == 1 || checkPoints->stage == 4 || checkPoints->stage == 9 || checkPoints->stage == 13 || checkPoints->stage == 11 || checkPoints->stage == 14 || checkPoints->stage == 19 || checkPoints->stage == 23 || checkPoints->stage == 26) {
                            afta_loc = ctrl->afta;
                        }
                        if (checkPoints->stage == 8 || checkPoints->stage == 6 || checkPoints->stage == 18 || checkPoints->stage == 16 || checkPoints->stage == 20 || checkPoints->stage == 24) {
                            afta_loc = false;
                        }
                        if (checkPoints->stage == 3 && mad->cn == 6) {
                            afta_loc = false;
                        }
                        if (checkPoints->stage == -1 && nfm_rand_gt_rand(m)) {
                            afta_loc = false;
                        }
                        bool b = false;
                        if (checkPoints->stage == 13 && mad->cn == 9) {
                            b = true;
                        }
                        if (checkPoints->stage == 18 && mad->cn == 11) {
                            b = true;
                        }
                        if (checkPoints->stage == 19 && checkPoints->clear[0] >= 20) {
                            b = true;
                        }
                        if (checkPoints->stage == 4 || checkPoints->stage == 10 || checkPoints->stage == 21 || checkPoints->stage == 22 || checkPoints->stage == 23 || checkPoints->stage == 25 || checkPoints->stage == 26) {
                            b = true;
                        }
                        if (checkPoints->stage == 3 && mad->cn == 6) {
                            b = true;
                        }
                        int n4 = 60;
                        if (checkPoints->stage == 5) {
                            n4 = 40;
                        }
                        if (checkPoints->stage == 6 && ctrl->bulistc) {
                            n4 = 40;
                        }
                        if (checkPoints->stage == 9 && ctrl->bulistc) {
                            n4 = 30;
                        }
                        if (checkPoints->stage == 3 || checkPoints->stage == 13 || checkPoints->stage == 21 || checkPoints->stage == 27 || checkPoints->stage == 20 || checkPoints->stage == 18) {
                            n4 = 30;
                        }
                        if ((checkPoints->stage == 12 || checkPoints->stage == 23) && mad->cn == 13) {
                            n4 = 50;
                        }
                        if (checkPoints->stage == 14) {
                            n4 = 20;
                        }
                        if (checkPoints->stage == 15 && mad->im != 6) {
                            n4 = 40;
                        }
                        if (checkPoints->stage == 17) {
                            n4 = 40;
                        }
                        if (checkPoints->stage == 18 && mad->cn == 11) {
                            n4 = 40;
                        }
                        if (checkPoints->stage == 19 && b) {
                            n4 = 30;
                        }
                        if (checkPoints->stage == 21 && ctrl->bulistc) {
                            n4 = 30;
                        }
                        if (checkPoints->stage == 22) {
                            n4 = 50;
                        }
                        if (checkPoints->stage == 25 && ctrl->bulistc) {
                            n4 = 40;
                        }
                        if (checkPoints->stage == 26) {
                            if (mad->cn == 11 && checkPoints->clear[0] == 27) {
                                n4 = 0;
                            }
                            if (mad->cn == 15 || mad->cn == 9) {
                                n4 = 50;
                            }
                            if (mad->cn == 11) {
                                n4 = 40;
                            }
                            if (checkPoints->pos[0] > checkPoints->pos[mad->im]) {
                                n4 = 80;
                            }
                        }
                        for (int i = 0; i < 7; ++i) {
                            if (i != mad->im && checkPoints->clear[i] != -1) {
                                int j = contO->xz;
                                if (ctrl->zyinv) {
                                    j += 180;
                                }
                                while (j < 0) {
                                    j += 360;
                                }
                                while (j > 180) {
                                    j -= 360;
                                }
                                int n5 = 0;
                                if (checkPoints->opx[i] - contO->x >= 0) {
                                    n5 = 180;
                                }
                                int k;
                                float divisor = checkPoints->opx[i] - contO->x;
                                if (divisor == 0.0f) {
                                  divisor = 1;
                                }
                                for (
                                  k = (int)(90 + n5 + atan((checkPoints->opz[i] - contO->z) / (divisor)) / 0.017453292519943295);
                                  k < 0;
                                  k += 360
                                ) {}
                                while (k > 180) {
                                    k -= 360;
                                }
                                int n6 = abs(j - k);
                                if (n6 > 180) {
                                    n6 = abs(n6 - 360);
                                }
                                int n7 = 2000 * (abs(checkPoints->clear[i] - mad->clear) + 1);
                                if ((checkPoints->stage == 6 || checkPoints->stage == 9) && ctrl->bulistc) {
                                    n7 = 6000;
                                }
                                if (checkPoints->stage == 3 && mad->cn == 6 && checkPoints->wasted < 2 && n7 > 4000) {
                                    n7 = 4000;
                                }
                                if (checkPoints->stage == 13 && mad->cn == 9 && n7 < 12000) {
                                    n7 = 12000;
                                }
                                if (checkPoints->stage == 14 && n7 < 4000) {
                                    n7 = 4000;
                                }
                                if (checkPoints->stage == 18 && mad->cn == 11) {
                                    if (n7 < 12000) {
                                        n7 = 12000;
                                    }
                                    n6 = 10;
                                }
                                if (checkPoints->stage == 19 && (mad->pcleared == 13 || mad->pcleared == 33 || b) && n7 < 12000) {
                                    n7 = 12000;
                                }
                                if (checkPoints->stage == 21) {
                                    if (ctrl->bulistc) {
                                        n7 = 8000;
                                        n6 = 10;
                                        ctrl->afta = true;
                                    }
                                    else if (n7 < 6000) {
                                        n7 = 6000;
                                    }
                                }
                                if (checkPoints->stage == 22 && ctrl->bulistc) {
                                    n7 = 6000;
                                    n6 = 10;
                                }
                                if (checkPoints->stage == 23) {
                                    n7 = 21000;
                                }
                                if (checkPoints->stage == 25) {
                                    n7 *= abs(checkPoints->clear[i] - mad->clear) + 1;
                                    if (ctrl->bulistc) {
                                        n7 = 4000 * (abs(checkPoints->clear[i] - mad->clear) + 1);
                                        n6 = 10;
                                    }
                                }
                                if (checkPoints->stage == 20) {
                                    n7 = 16000;
                                }
                                if (checkPoints->stage == 26) {
                                    if (mad->cn == 13 && ctrl->bulistc) {
                                        if (ctrl->oupnt == 33) {
                                            n7 = 17000;
                                        }
                                        if (ctrl->oupnt == 51) {
                                            n7 = 30000;
                                        }
                                        if (ctrl->oupnt == 15 && checkPoints->clear[0] >= 14) {
                                            n7 = 60000;
                                        }
                                        n6 = 10;
                                    }
                                    if (mad->cn == 15 || mad->cn == 9) {
                                        n7 *= abs(checkPoints->clear[i] - mad->clear) + 1;
                                    }
                                    if (mad->cn == 11) {
                                        n7 = 4000 * (abs(checkPoints->clear[i] - mad->clear) + 1);
                                    }
                                }
                                int n8 = 85 + 15 * (abs(checkPoints->clear[i] - mad->clear) + 1);
                                if (checkPoints->stage == 23) {
                                    n8 = 45;
                                }
                                if (checkPoints->stage == 26 && (mad->cn == 15 || mad->cn == 9 || mad->cn == 11 || mad->cn == 14)) {
                                    n8 = 50 + 70 * abs(checkPoints->clear[i] - mad->clear);
                                }
                                if (n6 < n8 && nfm_control_py(contO->x / 100, checkPoints->opx[i] / 100, contO->z / 100, checkPoints->opz[i] / 100) < n7 && ctrl->afta && mad->power > n4) {
                                    float n9 = 35 - abs(checkPoints->clear[i] - mad->clear) * 10;
                                    if (n9 < 1.0f) {
                                        n9 = 1.0f;
                                    }
                                    float n10 = (checkPoints->pos[mad->im] + 1) * (5 - checkPoints->pos[i]) / n9;
                                    if (checkPoints->stage != 27 && n10 > 0.7) {
                                        n10 = 0.7f;
                                    }
                                    if (i != 0 && checkPoints->pos[0] < checkPoints->pos[mad->im]) {
                                        n10 = 0.0f;
                                    }
                                    if (i != 0 && b) {
                                        n10 = 0.0f;
                                    }
                                    if (b && checkPoints->stage == 3 && i == 0) {
                                        if (checkPoints->wasted >= 2) {
                                            n10 *= 0.5f;
                                        }
                                        else {
                                            n10 = 0.0f;
                                        }
                                    }
                                    if ((checkPoints->stage == 3 || checkPoints->stage == 9) && i == 4) {
                                        n10 = 0.0f;
                                    }
                                    if (checkPoints->stage == 6) {
                                        n10 = 0.0f;
                                        if (ctrl->bulistc && i == 0) {
                                            n10 = 1.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 8) {
                                        n10 = 0.0f;
                                        if (ctrl->bulistc && mad->cn != 11 && mad->cn != 13) {
                                            n10 = 1.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 9 && mad->cn == 15) {
                                        n10 = 0.0f;
                                    }
                                    if (checkPoints->stage == 9 && ctrl->bulistc) {
                                        if (i == 0) {
                                            n10 = 1.0f;
                                        }
                                        else {
                                            n10 = 0.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 9 && (checkPoints->pos[i] == 4 || checkPoints->pos[i] == 3)) {
                                        n10 = 0.0f;
                                    }
                                    if (checkPoints->stage == 13) {
                                        if (mad->cn == 9 || (mad->cn == 13 && ctrl->bulistc)) {
                                            n10 *= 2.0f;
                                        }
                                        else {
                                            n10 *= 0.5f;
                                        }
                                    }
                                    if (checkPoints->stage == 16) {
                                        n10 = 0.0f;
                                    }
                                    if (checkPoints->stage == 17 && mad->im == 6 && i == 0) {
                                        n10 *= 1.5;
                                    }
                                    if (checkPoints->stage == 18) {
                                        if (mad->cn == 11 || (mad->cn == 13 && ctrl->bulistc)) {
                                            n10 *= 1.5f;
                                        }
                                        else {
                                            n10 = 0.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 19) {
                                        if (i != 0) {
                                            n10 *= 0.5;
                                        }
                                        if (mad->pcleared != 13 && mad->pcleared != 33 && !b) {
                                            n10 *= 0.5f;
                                        }
                                        if ((mad->im == 6 || mad->im == 5) && i != 0) {
                                            n10 = 0.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 20) {
                                        n10 = 0.0f;
                                        if (ctrl->bulistc && mad->cn != 11 && mad->cn != 13) {
                                            n10 = 1.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 21 && ctrl->bulistc && i == 0) {
                                        n10 = 1.0f;
                                    }
                                    if (checkPoints->stage == 22) {
                                        if (mad->cn != 11 && mad->cn != 13) {
                                            n10 = 0.0f;
                                        }
                                        if (mad->cn == 13 && i == 0) {
                                            n10 = 1.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 24) {
                                        n10 = 0.0f;
                                    }
                                    if (checkPoints->stage == 25) {
                                        if (checkPoints->pos[mad->im] == 0) {
                                            n10 *= 0.5;
                                        }
                                        if (checkPoints->pos[0] < checkPoints->pos[mad->im]) {
                                            n10 *= 2.0f;
                                        }
                                        if (ctrl->bulistc && i == 0) {
                                            n10 = 1.0f;
                                        }
                                    }
                                    if (checkPoints->stage == 26) {
                                        if (mad->cn != 14) {
                                            if (checkPoints->pos[0] < checkPoints->pos[mad->im] && checkPoints->clear[0] - checkPoints->clear[mad->im] != 1) {
                                                n10 *= 2.0f;
                                            }
                                        }
                                        else {
                                            n10 *= 0.5;
                                        }
                                        if (mad->cn == 13 && i == 0) {
                                            n10 = 1.0f;
                                        }
                                        if (checkPoints->pos[mad->im] == 0 || (checkPoints->pos[mad->im] == 1 && checkPoints->pos[0] == 0)) {
                                            n10 = 0.0f;
                                        }
                                        if (checkPoints->clear[mad->im] - checkPoints->clear[0] >= 5 && i == 0) {
                                            n10 = 1.0f;
                                        }
                                        if (mad->cn == 10 || mad->cn == 12) {
                                            n10 = 0.0f;
                                        }
                                    }
                                    if (nfm_medium_random(m) < n10) {
                                        ctrl->attack = 40 * (abs(checkPoints->clear[i] - mad->clear) + 1);
                                        if (ctrl->attack > 500) {
                                            ctrl->attack = 500;
                                        }
                                        ctrl->aim = 0.0f;
                                        if (checkPoints->stage == 13 && mad->cn == 9 && nfm_rand_gt_rand(m)) {
                                            ctrl->aim = 1.0f;
                                        }
                                        if (checkPoints->stage == 14) {
                                            if (i == 0 && checkPoints->pos[0] < checkPoints->pos[mad->im]) {
                                                ctrl->aim = 1.5f;
                                            }
                                            else {
                                                ctrl->aim = nfm_medium_random(m);
                                            }
                                        }
                                        if (checkPoints->stage == 15) {
                                            ctrl->aim = nfm_medium_random(m) * 1.5f;
                                        }
                                        if (checkPoints->stage == 17 && mad->im != 6 && (nfm_rand_gt_rand(m) || checkPoints->pos[0] < checkPoints->pos[mad->im])) {
                                            ctrl->aim = 1.0f;
                                        }
                                        if (checkPoints->stage == 18 && mad->cn == 11 && nfm_rand_gt_rand(m)) {
                                            ctrl->aim = 0.76f + nfm_medium_random(m) * 0.76f;
                                        }
                                        if (checkPoints->stage == 19 && (mad->pcleared == 13 || mad->pcleared == 33)) {
                                            ctrl->aim = 1.0f;
                                        }
                                        if (checkPoints->stage == 21) {
                                            if (ctrl->bulistc) {
                                                ctrl->aim = 0.7f;
                                                if (ctrl->attack > 150) {
                                                    ctrl->attack = 150;
                                                }
                                            }
                                            else {
                                                ctrl->aim = nfm_medium_random(m);
                                            }
                                        }
                                        if (checkPoints->stage == 22) {
                                            if (nfm_rand_gt_rand(m)) {
                                                ctrl->aim = 0.7f;
                                            }
                                            if (ctrl->bulistc && ctrl->attack > 150) {
                                                ctrl->attack = 150;
                                            }
                                        }
                                        if (checkPoints->stage == 23 && ctrl->attack > 60) {
                                            ctrl->attack = 60;
                                        }
                                        if (checkPoints->stage == 25) {
                                            ctrl->aim = nfm_medium_random(m) * 1.5f;
                                            ctrl->attack /= 2;
                                            if (nfm_rand_gt_rand(m)) {
                                                ctrl->exitattack = true;
                                            }
                                            else {
                                                ctrl->exitattack = false;
                                            }
                                        }
                                        if (checkPoints->stage == 26) {
                                            if (mad->cn == 13) {
                                                ctrl->aim = 0.76f;
                                                ctrl->attack = 150;
                                            }
                                            else {
                                                ctrl->aim = nfm_medium_random(m) * 1.5f;
                                                if (abs(checkPoints->clear[i] - mad->clear) <= 2 || mad->cn == 14) {
                                                    ctrl->attack /= 3;
                                                }
                                            }
                                        }
                                        if (checkPoints->stage == -1 && nfm_rand_gt_rand(m)) {
                                            ctrl->aim = nfm_medium_random(m) * 1.5f;
                                        }
                                        ctrl->acr = i;
                                        ctrl->turntyp = (int)(1.0f + nfm_medium_random(m) * 2.0f);
                                    }
                                }
                                if (afta_loc && n6 > 100 && nfm_control_py(contO->x / 100, checkPoints->opx[i] / 100, contO->z / 100, checkPoints->opz[i] / 100) < 300 && nfm_medium_random(m) > 0.6 - checkPoints->pos[mad->im] / 10.0f) {
                                    ctrl->clrnce = 0;
                                    ctrl->acuracy = 0;
                                }
                            }
                        }
                    }
                    bool b2 = false;
                    if (checkPoints->stage == 6 || checkPoints->stage == 8) {
                        b2 = true;
                    }
                    if (checkPoints->stage == 9 && mad->cn == 15) {
                        b2 = true;
                    }
                    if (checkPoints->stage == 16 || checkPoints->stage == 20 || checkPoints->stage == 21 || checkPoints->stage == 27) {
                        b2 = true;
                    }
                    if (checkPoints->stage == 18 && mad->pcleared != 73) {
                        b2 = true;
                    }
                    if (checkPoints->stage == -1 && nfm_rand_gt_rand(m)) {
                        b2 = true;
                    }
                    if (ctrl->trfix != 3) {
                        ctrl->trfix = 0;
                        int n11 = 50;
                        if (checkPoints->stage == 26) {
                            n11 = 40;
                        }
                        if (100.0f * mad->hitmag / mad->cd->maxmag[mad->cn] > n11) {
                            ctrl->trfix = 1;
                        }
                        if (!b2) {
                            int n12 = 80;
                            if (checkPoints->stage == 18 && mad->cn != 11) {
                                n12 = 50;
                            }
                            if (checkPoints->stage == 19) {
                                n12 = 70;
                            }
                            if (checkPoints->stage == 25 && mad->pcleared == 91) {
                                n12 = 50;
                            }
                            if (checkPoints->stage == 26 && checkPoints->clear[mad->im] - checkPoints->clear[0] >= 5 && mad->cn != 10 && mad->cn != 12) {
                                n12 = 50;
                            }
                            if (100.0f * mad->hitmag / mad->cd->maxmag[mad->cn] > n12) {
                                ctrl->trfix = 2;
                            }
                        }
                    }
                    else {
                        ctrl->upwait = 0;
                        ctrl->acuracy = 0;
                        ctrl->skiplev = 1.0f;
                        ctrl->clrnce = 2;
                    }
                    if (!ctrl->bulistc) {
                        if (checkPoints->stage == 18 && mad->cn == 11 && mad->pcleared == 35) {
                            mad->pcleared = 73;
                            mad->clear = 0;
                            ctrl->bulistc = true;
                            ctrl->runbul = (int)(100.0f * nfm_medium_random(m));
                        }
                        if (checkPoints->stage == 21 && mad->cn == 13) {
                            ctrl->bulistc = true;
                        }
                        if (checkPoints->stage == 22 && mad->cn == 13) {
                            ctrl->bulistc = true;
                        }
                        if (checkPoints->stage == 25 && checkPoints->clear[0] - mad->clear >= 3 && ctrl->trfix == 0) {
                            ctrl->bulistc = true;
                            ctrl->oupnt = -1;
                        }
                        if (checkPoints->stage == 26) {
                            if (mad->cn == 13 && checkPoints->pcleared == 8) {
                                ctrl->bulistc = true;
                                ctrl->attack = 0;
                            }
                            if (mad->cn == 11 && checkPoints->clear[0] - mad->clear >= 2 && ctrl->trfix == 0) {
                                ctrl->bulistc = true;
                                ctrl->oupnt = -1;
                            }
                        }
                        if ((checkPoints->stage == 6 || checkPoints->stage == 8 || checkPoints->stage == 12 || checkPoints->stage == 13 || checkPoints->stage == 14 || checkPoints->stage == 15 || checkPoints->stage == 18 || checkPoints->stage == 20 || checkPoints->stage == 23) && mad->cn == 13 && abs(checkPoints->clear[0] - mad->clear) >= 2) {
                            ctrl->bulistc = true;
                        }
                        if ((checkPoints->stage == 8 || checkPoints->stage == 20) && mad->cn == 11 && abs(checkPoints->clear[0] - mad->clear) >= 1) {
                            ctrl->bulistc = true;
                        }
                        if (checkPoints->stage == 6 && mad->cn == 11) {
                            ctrl->bulistc = true;
                        }
                        if (checkPoints->stage == 9 && ctrl->afta && (checkPoints->pos[mad->im] == 4 || checkPoints->pos[mad->im] == 3) && mad->cn != 15 && ctrl->trfix != 0) {
                            ctrl->bulistc = true;
                        }
                    }
                    else if (checkPoints->stage == 18) {
                        --ctrl->runbul;
                        if (mad->pcleared == 10) {
                            ctrl->runbul = 0;
                        }
                        if (ctrl->runbul <= 0) {
                            ctrl->bulistc = false;
                        }
                    }
                    ctrl->stcnt = 0;
                    ctrl->statusque = (int)(20.0f * nfm_medium_random(m));
                }
                else {
                    ++ctrl->stcnt;
                }
            }
            bool b3;
            if (ctrl->usebounce) {
                b3 = mad->wtouch;
            }
            else {
                b3 = mad->mtouch;
            }
            if (b3) {
                if (ctrl->trickfase != 0) {
                    ctrl->trickfase = 0;
                }
                if (ctrl->trfix == 2 || ctrl->trfix == 3) {
                    ctrl->attack = 0;
                }
                if (ctrl->attack == 0) {
                    if (ctrl->upcnt < 30) {
                        if (ctrl->revstart <= 0) {
                            ctrl->up = true;
                        }
                        else {
                            ctrl->down = true;
                            --ctrl->revstart;
                        }
                    }
                    if (ctrl->upcnt < 25 + ctrl->actwait) {
                        ++ctrl->upcnt;
                    }
                    else {
                        ctrl->upcnt = 0;
                        ctrl->actwait = ctrl->upwait;
                    }
                    int n13 = mad->point;
                    int n14 = 50;
                    if (checkPoints->stage == 9) {
                        n14 = 20;
                    }
                    if (checkPoints->stage == 18) {
                        n14 = 20;
                    }
                    if (checkPoints->stage == 25) {
                        n14 = 40;
                    }
                    if (checkPoints->stage == 26) {
                        n14 = 20;
                    }
                    if (!ctrl->bulistc || ctrl->trfix == 2 || ctrl->trfix == 3 || ctrl->trfix == 4 || mad->power < n14) {
                        if (ctrl->rampp == 1 && checkPoints->typ[n13] <= 0) {
                            int n15 = n13 + 1;
                            if (n15 >= checkPoints->n) {
                                n15 = 0;
                            }
                            if (checkPoints->typ[n15] == -2) {
                                n13 = n15;
                            }
                        }
                        if (ctrl->rampp == -1 && checkPoints->typ[n13] == -2 && ++n13 >= checkPoints->n) {
                            n13 = 0;
                        }
                        if (nfm_medium_random(m) > ctrl->skiplev) {
                            int n16 = n13;
                            int n17 = 0;
                            if (checkPoints->typ[n16] > 0) {
                                int n18 = 0;
                                for (int l = 0; l < checkPoints->n; ++l) {
                                    if (checkPoints->typ[l] > 0 && l < n16) {
                                        ++n18;
                                    }
                                }
                                n17 = ((mad->clear != n18 + mad->nlaps * checkPoints->nsp) ? 1 : 0);
                            }
                            while (checkPoints->typ[n16] == 0 || checkPoints->typ[n16] == -1 || checkPoints->typ[n16] == -3 || n17 != 0) {
                                n13 = n16;
                                if (++n16 >= checkPoints->n) {
                                    n16 = 0;
                                }
                                n17 = 0;
                                if (checkPoints->typ[n16] > 0) {
                                    int n19 = 0;
                                    for (int n20 = 0; n20 < checkPoints->n; ++n20) {
                                        if (checkPoints->typ[n20] > 0 && n20 < n16) {
                                            ++n19;
                                        }
                                    }
                                    n17 = ((mad->clear != n19 + mad->nlaps * checkPoints->nsp) ? 1 : 0);
                                }
                            }
                        }
                        else if (nfm_medium_random(m) > ctrl->skiplev) {
                            while (checkPoints->typ[n13] == -1) {
                                if (++n13 >= checkPoints->n) {
                                    n13 = 0;
                                }
                            }
                        }
                        if (checkPoints->stage == 18 && mad->pcleared == 73 && ctrl->trfix == 0 && mad->clear != 0) {
                            n13 = 10;
                        }
                        if (checkPoints->stage == 19 && mad->pcleared == 18 && ctrl->trfix == 0) {
                            n13 = 27;
                        }
                        if (checkPoints->stage == 21) {
                            if (mad->pcleared == 5 && ctrl->trfix == 0 && mad->power < 70.0f) {
                                if (n13 <= 16) {
                                    n13 = 16;
                                }
                                else {
                                    n13 = 21;
                                }
                            }
                            if (mad->pcleared == 50) {
                                n13 = 57;
                            }
                        }
                        if (checkPoints->stage == 22 && (mad->pcleared == 27 || mad->pcleared == 37)) {
                            while (checkPoints->typ[n13] == -1) {
                                if (++n13 >= checkPoints->n) {
                                    n13 = 0;
                                }
                            }
                        }
                        if (checkPoints->stage == 23) {
                            while (checkPoints->typ[n13] == -1) {
                                if (++n13 >= checkPoints->n) {
                                    n13 = 0;
                                }
                            }
                        }
                        if (checkPoints->stage == 24) {
                            while (checkPoints->typ[n13] == -1) {
                                if (++n13 >= checkPoints->n) {
                                    n13 = 0;
                                }
                            }
                            if (!mad->gtouch) {
                                while (checkPoints->typ[n13] == -2) {
                                    if (++n13 >= checkPoints->n) {
                                        n13 = 0;
                                    }
                                }
                            }
                            if (ctrl->oupnt >= 68) {
                                n13 = 70;
                            }
                            else {
                                ctrl->oupnt = n13;
                            }
                        }
                        if (checkPoints->stage == 25) {
                            if ((mad->pcleared != 91 && checkPoints->pos[0] < checkPoints->pos[mad->im] && mad->cn != 13) || (checkPoints->pos[mad->im] == 0 && (mad->clear == 12 || mad->clear == 20))) {
                                while (checkPoints->typ[n13] == -4) {
                                    if (++n13 >= checkPoints->n) {
                                        n13 = 0;
                                    }
                                }
                            }
                            if (mad->pcleared == 9) {
                                if (nfm_control_py(contO->x / 100, 297, contO->z / 100, 347) < 400) {
                                    ctrl->oupnt = 1;
                                }
                                if (ctrl->oupnt == 1 && n13 < 22) {
                                    n13 = 22;
                                }
                            }
                            if (mad->pcleared == 67) {
                                if (nfm_control_py(contO->x / 100, 28, contO->z / 100, 494) < 4000) {
                                    ctrl->oupnt = 2;
                                }
                                if (ctrl->oupnt == 2) {
                                    n13 = 76;
                                }
                            }
                            if (mad->pcleared == 76) {
                                if (nfm_control_py(contO->x / 100, -50, contO->z / 100, 0) < 2000) {
                                    ctrl->oupnt = 3;
                                }
                                if (ctrl->oupnt == 3) {
                                    n13 = 91;
                                }
                                else {
                                    n13 = 89;
                                }
                            }
                        }
                        if (checkPoints->stage == 26) {
                            if (mad->pcleared == 128) {
                                if (nfm_control_py(contO->x / 100, 0, contO->z / 100, 229) < 1500 || contO->z > 23000) {
                                    ctrl->oupnt = 128;
                                }
                                if (ctrl->oupnt != 128) {
                                    n13 = 3;
                                }
                            }
                            if (mad->pcleared == 8) {
                                if (nfm_control_py(contO->x / 100, -207, contO->z / 100, 549) < 1500 || contO->x < -20700) {
                                    ctrl->oupnt = 8;
                                }
                                if (ctrl->oupnt != 8) {
                                    n13 = 12;
                                }
                            }
                            if (mad->pcleared == 33) {
                                if (nfm_control_py(contO->x / 100, -60, contO->z / 100, 168) < 250 || contO->z > 17000) {
                                    ctrl->oupnt = 331;
                                }
                                if (nfm_control_py(contO->x / 100, -112, contO->z / 100, 414) < 10000 || contO->z > 40000) {
                                    ctrl->oupnt = 332;
                                }
                                if (ctrl->oupnt != 331 && ctrl->oupnt != 332) {
                                    if (ctrl->trfix != 1) {
                                        n13 = 38;
                                    }
                                    else {
                                        n13 = 39;
                                    }
                                }
                                if (ctrl->oupnt == 331) {
                                    n13 = 71;
                                }
                            }
                            if (mad->pcleared == 42) {
                                if (nfm_control_py(contO->x / 100, -269, contO->z / 100, 493) < 100 || contO->x < -27000) {
                                    ctrl->oupnt = 142;
                                }
                                if (ctrl->oupnt != 142) {
                                    n13 = 47;
                                }
                            }
                            if (mad->pcleared == 51) {
                                if (nfm_control_py(contO->x / 100, -352, contO->z / 100, 260) < 100 || contO->z < 25000) {
                                    ctrl->oupnt = 511;
                                }
                                if (nfm_control_py(contO->x / 100, -325, contO->z / 100, 10) < 2000 || contO->x > -32000) {
                                    ctrl->oupnt = 512;
                                }
                                if (ctrl->oupnt != 511 && ctrl->oupnt != 512) {
                                    n13 = 80;
                                }
                                if (ctrl->oupnt == 511) {
                                    n13 = 61;
                                }
                            }
                            if (mad->pcleared == 77) {
                                if (nfm_control_py(contO->x / 100, -371, contO->z / 100, 319) < 100 || contO->z < 31000) {
                                    ctrl->oupnt = 77;
                                }
                                if (ctrl->oupnt != 77) {
                                    n13 = 78;
                                    mad->nofocus = true;
                                }
                            }
                            if (mad->pcleared == 105) {
                                if (nfm_control_py(contO->x / 100, -179, contO->z / 100, 10) < 2300 || contO->z < 1050) {
                                    ctrl->oupnt = 105;
                                }
                                if (ctrl->oupnt != 105) {
                                    n13 = 65;
                                }
                                else {
                                    n13 = 125;
                                }
                            }
                            if (ctrl->trfix == 3) {
                                if (nfm_control_py(contO->x / 100, -52, contO->z / 100, 448) < 100 || contO->z > 45000) {
                                    ctrl->oupnt = 176;
                                }
                                if (ctrl->oupnt != 176) {
                                    n13 = 41;
                                }
                                else {
                                    n13 = 43;
                                }
                            }
                            if (checkPoints->clear[mad->im] - checkPoints->clear[0] >= 2 && nfm_control_py(contO->x / 100, checkPoints->opx[0] / 100, contO->z / 100, checkPoints->opz[0] / 100) < 1000 + ctrl->avoidnlev) {
                                int xz = contO->xz;
                                if (ctrl->zyinv) {
                                    xz += 180;
                                }
                                while (xz < 0) {
                                    xz += 360;
                                }
                                while (xz > 180) {
                                    xz -= 360;
                                }
                                int n21 = 0;
                                if (checkPoints->opx[0] - contO->x >= 0) {
                                    n21 = 180;
                                }
                                int n22;
                                for (n22 = (int)(90 + n21 + atan((checkPoints->opz[0] - contO->z) / (checkPoints->opx[0] - contO->x)) / 0.017453292519943295); n22 < 0; n22 += 360) {}
                                while (n22 > 180) {
                                    n22 -= 360;
                                }
                                int n23 = abs(xz - n22);
                                if (n23 > 180) {
                                    n23 = abs(n23 - 360);
                                }
                                if (n23 < 90) {
                                    ctrl->wall = 0;
                                }
                            }
                        }
                        if (ctrl->rampp == 2) {
                            int n24 = n13 + 1;
                            if (n24 >= checkPoints->n) {
                                n24 = 0;
                            }
                            if (checkPoints->typ[n24] == -2 && n13 != mad->point && --n13 < 0) {
                                n13 += checkPoints->n;
                            }
                        }
                        if (ctrl->bulistc) {
                            mad->nofocus = true;
                            if (ctrl->gowait) {
                                ctrl->gowait = false;
                            }
                        }
                    }
                    else {
                        if ((checkPoints->stage != 25 && checkPoints->stage != 26) || ctrl->runbul == 0) {
                            n13 -= 2;
                            if (n13 < 0) {
                                n13 += checkPoints->n;
                            }
                            if (checkPoints->stage == 9 && n13 > 76) {
                                n13 = 76;
                            }
                            while (checkPoints->typ[n13] == -4) {
                                if (--n13 < 0) {
                                    n13 += checkPoints->n;
                                }
                            }
                        }
                        if (checkPoints->stage == 21) {
                            if (n13 >= 14 && n13 <= 19) {
                                n13 = 13;
                            }
                            if (ctrl->oupnt == 72 && n13 != 56) {
                                n13 = 57;
                            }
                            else if (ctrl->oupnt == 54 && n13 != 52) {
                                n13 = 53;
                            }
                            else if (ctrl->oupnt == 39 && n13 != 37) {
                                n13 = 38;
                            }
                            else {
                                ctrl->oupnt = n13;
                            }
                        }
                        if (checkPoints->stage == 22) {
                            if (!ctrl->gowait) {
                                if (checkPoints->clear[0] == 0) {
                                    ctrl->wtx = -3500;
                                    ctrl->wtz = 19000;
                                    ctrl->frx = -3500;
                                    ctrl->frz = 39000;
                                    ctrl->frad = 12000;
                                    ctrl->oupnt = 37;
                                    ctrl->gowait = true;
                                    ctrl->afta = false;
                                }
                                if (checkPoints->clear[0] == 7) {
                                    ctrl->wtx = -44800;
                                    ctrl->wtz = 40320;
                                    ctrl->frx = -44800;
                                    ctrl->frz = 34720;
                                    ctrl->frad = 30000;
                                    ctrl->oupnt = 27;
                                    ctrl->gowait = true;
                                    ctrl->afta = false;
                                }
                                if (checkPoints->clear[0] == 10) {
                                    ctrl->wtx = 0;
                                    ctrl->wtz = 48739;
                                    ctrl->frx = 0;
                                    ctrl->frz = 38589;
                                    ctrl->frad = 90000;
                                    ctrl->oupnt = 55;
                                    ctrl->gowait = true;
                                    ctrl->afta = false;
                                }
                                if (checkPoints->clear[0] == 14) {
                                    ctrl->wtx = -3500;
                                    ctrl->wtz = 19000;
                                    ctrl->frx = -14700;
                                    ctrl->frz = 39000;
                                    ctrl->frad = 45000;
                                    ctrl->oupnt = 37;
                                    ctrl->gowait = true;
                                    ctrl->afta = false;
                                }
                                if (checkPoints->clear[0] == 18) {
                                    ctrl->wtx = -48300;
                                    ctrl->wtz = -4550;
                                    ctrl->frx = -48300;
                                    ctrl->frz = 5600;
                                    ctrl->frad = 90000;
                                    ctrl->oupnt = 17;
                                    ctrl->gowait = true;
                                    ctrl->afta = false;
                                }
                            }
                            if (ctrl->gowait) {
                                if (nfm_control_py(contO->x / 100, ctrl->wtx / 100, contO->z / 100, ctrl->wtz / 100) < 10000 && mad->speed > 50.0f) {
                                    ctrl->up = false;
                                }
                                if (nfm_control_py(contO->x / 100, ctrl->wtx / 100, contO->z / 100, ctrl->wtz / 100) < 200) {
                                    ctrl->up = false;
                                    ctrl->handb = true;
                                }
                                if (checkPoints->pcleared == ctrl->oupnt && nfm_control_py(checkPoints->opx[0] / 100, ctrl->frx / 100, checkPoints->opz[0] / 100, ctrl->frz / 100) < ctrl->frad) {
                                    ctrl->afta = true;
                                    ctrl->gowait = false;
                                }
                                if (nfm_control_py(contO->x / 100, checkPoints->opx[0] / 100, contO->z / 100, checkPoints->opz[0] / 100) < 25) {
                                    ctrl->afta = true;
                                    ctrl->gowait = false;
                                    ctrl->attack = 200;
                                    ctrl->acr = 0;
                                }
                            }
                        }
                        if (checkPoints->stage == 25) {
                            if (ctrl->oupnt == -1) {
                                int pyv = -10;
                                for (int oupnt_i = 0; oupnt_i < checkPoints->n; ++oupnt_i) {
                                    if ((checkPoints->typ[oupnt_i] == -2 || checkPoints->typ[oupnt_i] == -4) && (oupnt_i < 50 || oupnt_i > 54) && (nfm_control_py(contO->x / 100, checkPoints->x[oupnt_i] / 100, contO->z / 100, checkPoints->z[oupnt_i] / 100) < pyv || pyv == -10)) {
                                        pyv = nfm_control_py(contO->x / 100, checkPoints->x[oupnt_i] / 100, contO->z / 100, checkPoints->z[oupnt_i] / 100);
                                        ctrl->oupnt = oupnt_i;
                                    }
                                }
                                --ctrl->oupnt;
                                if (n13 < 0) {
                                    ctrl->oupnt += checkPoints->n;
                                }
                            }
                            if (ctrl->oupnt >= 0 && ctrl->oupnt < checkPoints->n) {
                                n13 = ctrl->oupnt;
                                if (nfm_control_py(contO->x / 100, checkPoints->x[n13] / 100, contO->z / 100, checkPoints->z[n13] / 100) < 800) {
                                    ctrl->oupnt = -(int)(75.0f + nfm_medium_random(m) * 200.0f);
                                    ctrl->runbul = (int)(50.0f + nfm_medium_random(m) * 100.0f);
                                }
                            }
                            if (ctrl->oupnt < -1) {
                                ++ctrl->oupnt;
                            }
                            if (ctrl->runbul != 0) {
                                --ctrl->runbul;
                            }
                        }
                        if (checkPoints->stage == 26) {
                            bool b4 = false;
                            if (mad->cn == 13) {
                                if (!ctrl->gowait) {
                                    if (checkPoints->clear[0] == 1) {
                                        if (nfm_medium_random(m) > 0.5) {
                                            ctrl->wtx = -14000;
                                            ctrl->wtz = 48000;
                                            ctrl->frx = -5600;
                                            ctrl->frz = 47600;
                                            ctrl->frad = 88000;
                                            ctrl->oupnt = 33;
                                        }
                                        else {
                                            ctrl->wtx = -5600;
                                            ctrl->wtz = 8000;
                                            ctrl->frx = -7350;
                                            ctrl->frz = -4550;
                                            ctrl->frad = 22000;
                                            ctrl->oupnt = 15;
                                        }
                                        ctrl->gowait = true;
                                        ctrl->afta = false;
                                    }
                                    if (checkPoints->clear[0] == 4) {
                                        ctrl->wtx = -12700;
                                        ctrl->wtz = 14000;
                                        ctrl->frx = -31000;
                                        ctrl->frz = 1050;
                                        ctrl->frad = 11000;
                                        ctrl->oupnt = 51;
                                        ctrl->gowait = true;
                                        ctrl->afta = false;
                                    }
                                    if (checkPoints->clear[0] == 14) {
                                        ctrl->wtx = -35350;
                                        ctrl->wtz = 6650;
                                        ctrl->frx = -48300;
                                        ctrl->frz = 54950;
                                        ctrl->frad = 11000;
                                        ctrl->oupnt = 15;
                                        ctrl->gowait = true;
                                        ctrl->afta = false;
                                    }
                                    if (checkPoints->clear[0] == 17) {
                                        ctrl->wtx = -42700;
                                        ctrl->wtz = 41000;
                                        ctrl->frx = -40950;
                                        ctrl->frz = 49350;
                                        ctrl->frad = 7000;
                                        ctrl->oupnt = 42;
                                        ctrl->gowait = true;
                                        ctrl->afta = false;
                                    }
                                    if (checkPoints->clear[0] == 21) {
                                        ctrl->wtx = -1750;
                                        ctrl->wtz = -15750;
                                        ctrl->frx = -25900;
                                        ctrl->frz = -14000;
                                        ctrl->frad = 11000;
                                        ctrl->oupnt = 125;
                                        ctrl->gowait = true;
                                        ctrl->afta = false;
                                    }
                                }
                                if (ctrl->gowait) {
                                    if (nfm_control_py(contO->x / 100, ctrl->wtx / 100, contO->z / 100, ctrl->wtz / 100) < 10000 && mad->speed > 50.0f) {
                                        ctrl->up = false;
                                    }
                                    if (nfm_control_py(contO->x / 100, ctrl->wtx / 100, contO->z / 100, ctrl->wtz / 100) < 200) {
                                        ctrl->up = false;
                                        ctrl->handb = true;
                                    }
                                    if (checkPoints->pcleared == ctrl->oupnt && nfm_control_py(checkPoints->opx[0] / 100, ctrl->frx / 100, checkPoints->opz[0] / 100, ctrl->frz / 100) < ctrl->frad) {
                                        ctrl->runbul = 0;
                                        ctrl->afta = true;
                                        ctrl->gowait = false;
                                    }
                                    if (nfm_control_py(contO->x / 100, checkPoints->opx[0] / 100, contO->z / 100, checkPoints->opz[0] / 100) < 25) {
                                        ctrl->afta = true;
                                        ctrl->gowait = false;
                                        ctrl->attack = 200;
                                        ctrl->acr = 0;
                                    }
                                    if (checkPoints->clear[0] == 21 && ctrl->oupnt != 125) {
                                        ctrl->gowait = false;
                                    }
                                }
                                if ((checkPoints->clear[0] >= 11 && !ctrl->gowait) || (mad->power < 60.0f && checkPoints->clear[0] < 21)) {
                                    b4 = true;
                                    if (!ctrl->exitattack) {
                                        ctrl->oupnt = -1;
                                        ctrl->exitattack = true;
                                    }
                                }
                                else if (ctrl->exitattack) {
                                    ctrl->exitattack = false;
                                }
                            }
                            if (mad->cn == 11) {
                                b4 = true;
                            }
                            if (b4) {
                                if (ctrl->oupnt == -1) {
                                    int pyv2 = -10;
                                    for (int oupnt2_i = 0; oupnt2_i < checkPoints->n; ++oupnt2_i) {
                                        if (checkPoints->typ[oupnt2_i] == -4 && ((nfm_control_py(contO->x / 100, checkPoints->x[oupnt2_i] / 100, contO->z / 100, checkPoints->z[oupnt2_i] / 100) < pyv2 && nfm_medium_random(m) > 0.6) || pyv2 == -10)) {
                                            pyv2 = nfm_control_py(contO->x / 100, checkPoints->x[oupnt2_i] / 100, contO->z / 100, checkPoints->z[oupnt2_i] / 100);
                                            ctrl->oupnt = oupnt2_i;
                                        }
                                    }
                                    --ctrl->oupnt;
                                    if (n13 < 0) {
                                        ctrl->oupnt += checkPoints->n;
                                    }
                                }
                                if (ctrl->oupnt >= 0 && ctrl->oupnt < checkPoints->n) {
                                    n13 = ctrl->oupnt;
                                    if (nfm_control_py(contO->x / 100, checkPoints->x[n13] / 100, contO->z / 100, checkPoints->z[n13] / 100) < 800) {
                                        ctrl->oupnt = -(int)(75.0f + nfm_medium_random(m) * 200.0f);
                                        ctrl->runbul = (int)(50.0f + nfm_medium_random(m) * 100.0f);
                                    }
                                }
                                if (ctrl->oupnt < -1) {
                                    ++ctrl->oupnt;
                                }
                                if (ctrl->runbul != 0) {
                                    --ctrl->runbul;
                                }
                            }
                        }
                        mad->nofocus = true;
                    }
                    if (checkPoints->stage != 27) {
                        if (checkPoints->stage == 10 || checkPoints->stage == 19 || (checkPoints->stage == 18 && mad->pcleared == 73) || checkPoints->stage == 26) {
                            ctrl->forget = true;
                        }
                        if ((mad->missedcp == 0 || ctrl->forget || ctrl->trfix == 4) && ctrl->trfix != 0) {
                            int n25 = 0;
                            if (checkPoints->stage == 25 || checkPoints->stage == 26) {
                                n25 = 3;
                            }
                            if (ctrl->trfix == 2) {
                                int pyv3 = -10;
                                int n26 = 0;
                                for (int n27 = n25; n27 < checkPoints->fn; ++n27) {
                                    if (nfm_control_py(contO->x / 100, checkPoints->x[ctrl->fpnt[n27]] / 100, contO->z / 100, checkPoints->z[ctrl->fpnt[n27]] / 100) < pyv3 || pyv3 == -10) {
                                        pyv3 = nfm_control_py(contO->x / 100, checkPoints->x[ctrl->fpnt[n27]] / 100, contO->z / 100, checkPoints->z[ctrl->fpnt[n27]] / 100);
                                        n26 = n27;
                                    }
                                }
                                if (checkPoints->stage == 18 || checkPoints->stage == 22) {
                                    n26 = 1;
                                }
                                n13 = ctrl->fpnt[n26];
                                if (checkPoints->special[n26]) {
                                    ctrl->forget = true;
                                }
                                else {
                                    ctrl->forget = false;
                                }
                            }
                            for (int n28 = n25; n28 < checkPoints->fn; ++n28) {
                                if (nfm_control_py(contO->x / 100, checkPoints->x[ctrl->fpnt[n28]] / 100, contO->z / 100, checkPoints->z[ctrl->fpnt[n28]] / 100) < 2000) {
                                    ctrl->forget = false;
                                    ctrl->actwait = 0;
                                    ctrl->upwait = 0;
                                    ctrl->turntyp = 2;
                                    ctrl->randtcnt = -1;
                                    ctrl->acuracy = 0;
                                    ctrl->rampp = 0;
                                    ctrl->trfix = 3;
                                }
                            }
                            if (ctrl->trfix == 3) {
                                mad->nofocus = true;
                            }
                        }
                    }
                    if (ctrl->turncnt > ctrl->randtcnt) {
                        if (!ctrl->gowait) {
                            int n29 = 0;
                            if (checkPoints->x[n13] - contO->x >= 0) {
                                n29 = 180;
                            }
                            float divisor = checkPoints->x[n13] - contO->x;
                            if (divisor == 0.0f) {
                              divisor = 1;
                            }
                            ctrl->pan = (int)(90 + n29 + atan((checkPoints->z[n13] - contO->z) / (divisor)) / 0.017453292519943295);
                        }
                        else {
                            int n30 = 0;
                            if (ctrl->wtx - contO->x >= 0) {
                                n30 = 180;
                            }
                            ctrl->pan = (int)(90 + n30 + atan((ctrl->wtz - contO->z) / (ctrl->wtx - contO->x)) / 0.017453292519943295);
                        }
                        ctrl->turncnt = 0;
                        ctrl->randtcnt = (int)(ctrl->acuracy * nfm_medium_random(m));
                    }
                    else {
                        ++ctrl->turncnt;
                    }
                }
                else {
                    ctrl->up = true;
                    int n31 = 0;
                     int n32 = (int)(nfm_control_pys(contO->x, checkPoints->opx[ctrl->acr], contO->z, checkPoints->opz[ctrl->acr]) / 2.0f * ctrl->aim);
                     int n33 = (int)(checkPoints->opx[ctrl->acr] - n32 * nfm_medium_sin(m, checkPoints->omxz[ctrl->acr]));
                     int n34 = (int)(checkPoints->opz[ctrl->acr] + n32 * nfm_medium_cos(m, checkPoints->omxz[ctrl->acr]));
                    if (n33 - contO->x >= 0) {
                        n31 = 180;
                    }
                    float divisor = n33 - contO->x;
                    if (divisor == 0.0f) {
                      divisor = 1;
                    }
                    ctrl->pan = (int)(90 + n31 + atan((n34 - contO->z) / (divisor)) / 0.017453292519943295);
                    --ctrl->attack;
                    if (ctrl->attack <= 0) {
                        ctrl->attack = 0;
                    }
                    if (checkPoints->stage == 25 && ctrl->exitattack && !ctrl->bulistc && mad->missedcp != 0) {
                        ctrl->attack = 0;
                    }
                    if (checkPoints->stage == 26 && mad->cn == 13 && (checkPoints->clear[0] == 4 || checkPoints->clear[0] == 13 || checkPoints->clear[0] == 21)) {
                        ctrl->attack = 0;
                    }
                    if (checkPoints->stage == 26 && mad->missedcp != 0 && (checkPoints->pos[mad->im] == 0 || (checkPoints->pos[mad->im] == 1 && checkPoints->pos[0] == 0))) {
                        ctrl->attack = 0;
                    }
                    if (checkPoints->stage == 26 && checkPoints->pos[0] > checkPoints->pos[mad->im] && mad->power < 80.0f) {
                        ctrl->attack = 0;
                    }
                }
                int xz2 = contO->xz;
                if (ctrl->zyinv) {
                    xz2 += 180;
                }
                while (xz2 < 0) {
                    xz2 += 360;
                }
                while (xz2 > 180) {
                    xz2 -= 360;
                }
                while (ctrl->pan < 0) {
                    ctrl->pan += 360;
                }
                while (ctrl->pan > 180) {
                    ctrl->pan -= 360;
                }
                if (ctrl->wall != -1 && ctrl->hold == 0) {
                    ctrl->clrnce = 0;
                }
                if (ctrl->hold == 0) {
                    if (abs(xz2 - ctrl->pan) < 180) {
                        if (abs(xz2 - ctrl->pan) > ctrl->clrnce) {
                            if (xz2 < ctrl->pan) {
                                ctrl->left = true;
                                ctrl->lastl = true;
                            }
                            else {
                                ctrl->right = true;
                                ctrl->lastl = false;
                            }
                            if (abs(xz2 - ctrl->pan) > 50 && mad->speed > mad->cd->swits[mad->cn][0] && ctrl->turntyp != 0) {
                                if (ctrl->turntyp == 1) {
                                    ctrl->down = true;
                                }
                                if (ctrl->turntyp == 2) {
                                    ctrl->handb = true;
                                }
                                if (!ctrl->agressed) {
                                    ctrl->up = false;
                                }
                            }
                        }
                    }
                    else if (abs(xz2 - ctrl->pan) < 360 - ctrl->clrnce) {
                        if (xz2 < ctrl->pan) {
                            ctrl->right = true;
                            ctrl->lastl = false;
                        }
                        else {
                            ctrl->left = true;
                            ctrl->lastl = true;
                        }
                        if (abs(xz2 - ctrl->pan) < 310 && mad->speed > mad->cd->swits[mad->cn][0] && ctrl->turntyp != 0) {
                            if (ctrl->turntyp == 1) {
                                ctrl->down = true;
                            }
                            if (ctrl->turntyp == 2) {
                                ctrl->handb = true;
                            }
                            if (!ctrl->agressed) {
                                ctrl->up = false;
                            }
                        }
                    }
                }
                if (checkPoints->stage == 24 && ctrl->wall != -1) {
                    if (trackers->dam[ctrl->wall] == 0 || mad->pcleared == 45) {
                        ctrl->wall = -1;
                    }
                    if (mad->pcleared == 58 && checkPoints->opz[mad->im] < 36700) {
                        ctrl->wall = -1;
                        ctrl->hold = 0;
                    }
                }
                if (ctrl->wall != -1) {
                    if (ctrl->lwall != ctrl->wall) {
                        if (ctrl->lastl) {
                            ctrl->left = true;
                        }
                        else {
                            ctrl->right = true;
                        }
                        ctrl->wlastl = ctrl->lastl;
                        ctrl->lwall = ctrl->wall;
                    }
                    else if (ctrl->wlastl) {
                        ctrl->left = true;
                    }
                    else {
                        ctrl->right = true;
                    }
                    if (trackers->dam[ctrl->wall] != 0) {
                        int n35 = 1;
                        if (trackers->skd[ctrl->wall] == 1) {
                            n35 = 3;
                        }
                        ctrl->hold += n35;
                        if (ctrl->hold > 10 * n35) {
                            ctrl->hold = 10 * n35;
                        }
                    }
                    else {
                        ctrl->hold = 1;
                    }
                    ctrl->wall = -1;
                }
                else if (ctrl->hold != 0) {
                    --ctrl->hold;
                }
            }
            else {
                if (ctrl->trickfase == 0) {
                     int n36 = (int)((mad->scy[0] + mad->scy[1] + mad->scy[2] + mad->scy[3]) * (contO->y - 300) / 4000.0f);
                    int n37 = 3;
                    if (checkPoints->stage == 25) {
                        n37 = 10;
                    }
                    if (n36 > 7 && (nfm_medium_random(m) > ctrl->trickprf / n37 || ctrl->stuntf == 4 || ctrl->stuntf == 3 || ctrl->stuntf == 5 || ctrl->stuntf == 6 || checkPoints->stage == 26)) {
                        ctrl->oxy = mad->pxy;
                        ctrl->ozy = mad->pzy;
                        ctrl->flycnt = 0;
                        ctrl->uddirect = 0;
                        ctrl->lrdirect = 0;
                        ctrl->udswt = false;
                        ctrl->lrswt = false;
                        ctrl->trickfase = 1;
                        if (n36 < 16) {
                            if (ctrl->stuntf != 6) {
                                ctrl->uddirect = -1;
                                ctrl->udstart = 0;
                                ctrl->udswt = false;
                            }
                            else if (ctrl->oupnt != 70) {
                                ctrl->uddirect = 1;
                                ctrl->udstart = 0;
                                ctrl->udswt = false;
                            }
                        }
                        else if ((nfm_rand_gt_rand(m) && ctrl->stuntf != 1) || ctrl->stuntf == 4 || ctrl->stuntf == 6 || ctrl->stuntf == 7 || ctrl->stuntf == 17) {
                            if ((nfm_rand_gt_rand(m) || ctrl->stuntf == 2 || ctrl->stuntf == 7) && ctrl->stuntf != 4 && ctrl->stuntf != 6) {
                                ctrl->uddirect = -1;
                            }
                            else {
                                ctrl->uddirect = 1;
                            }
                            ctrl->udstart = (int)(10.0f * nfm_medium_random(m) * ctrl->trickprf);
                            if (ctrl->stuntf == 6) {
                                ctrl->udstart = 0;
                            }
                            if (checkPoints->stage == 26) {
                                ctrl->udstart = 0;
                            }
                            if (checkPoints->stage == 24 && (ctrl->oupnt == 68 || ctrl->oupnt == 69)) {
                                ctrl->apunch = 20;
                                ctrl->oupnt = 70;
                            }
                            if (nfm_medium_random(m) > 0.85 && ctrl->stuntf != 4 && ctrl->stuntf != 3 && ctrl->stuntf != 6 && ctrl->stuntf != 17 && checkPoints->stage != 26) {
                                ctrl->udswt = true;
                            }
                            if (nfm_medium_random(m) > ctrl->trickprf + 0.3f && ctrl->stuntf != 4 && ctrl->stuntf != 6) {
                                if (nfm_rand_gt_rand(m)) {
                                    ctrl->lrdirect = -1;
                                }
                                else {
                                    ctrl->lrdirect = 1;
                                }
                                ctrl->lrstart = (int)(30.0f * nfm_medium_random(m));
                                if (nfm_medium_random(m) > 0.75) {
                                    ctrl->lrswt = true;
                                }
                            }
                        }
                        else {
                            if (nfm_rand_gt_rand(m)) {
                                ctrl->lrdirect = -1;
                            }
                            else {
                                ctrl->lrdirect = 1;
                            }
                            ctrl->lrstart = (int)(10.0f * nfm_medium_random(m) * ctrl->trickprf);
                            if (nfm_medium_random(m) > 0.75 && checkPoints->stage != 26) {
                                ctrl->lrswt = true;
                            }
                            if (nfm_medium_random(m) > ctrl->trickprf + 0.3f) {
                                if (nfm_rand_gt_rand(m)) {
                                    ctrl->uddirect = -1;
                                }
                                else {
                                    ctrl->uddirect = 1;
                                }
                                ctrl->udstart = (int)(30.0f * nfm_medium_random(m));
                                if (nfm_medium_random(m) > 0.85) {
                                    ctrl->udswt = true;
                                }
                            }
                        }
                        if (ctrl->trfix == 3 || ctrl->trfix == 4) {
                            if (checkPoints->stage != 18 && checkPoints->stage != 8) {
                                if (checkPoints->stage != 25 && ctrl->lrdirect == -1) {
                                    if (checkPoints->stage != 19) {
                                        ctrl->uddirect = -1;
                                    }
                                    else {
                                        ctrl->uddirect = 1;
                                    }
                                }
                                ctrl->lrdirect = 0;
                                if ((checkPoints->stage == 19 || checkPoints->stage == 25) && ctrl->uddirect == -1) {
                                    ctrl->uddirect = 1;
                                }
                                if (mad->power < 60.0f) {
                                    ctrl->uddirect = -1;
                                }
                            }
                            else {
                                if (ctrl->uddirect != 0) {
                                    ctrl->uddirect = -1;
                                }
                                ctrl->lrdirect = 0;
                            }
                            if (checkPoints->stage == 20) {
                                ctrl->uddirect = 1;
                                ctrl->lrdirect = 0;
                            }
                            if (checkPoints->stage == 26) {
                                ctrl->uddirect = -1;
                                ctrl->lrdirect = 0;
                                if (mad->cn != 11 && mad->cn != 13) {
                                    ctrl->udstart = 7;
                                    if (mad->cn == 14 && mad->power > 30.0f) {
                                        ctrl->udstart = 14;
                                    }
                                }
                                else {
                                    ctrl->udstart = 0;
                                }
                                if (mad->cn == 11) {
                                    ctrl->lrdirect = -1;
                                    ctrl->lrstart = 0;
                                }
                            }
                        }
                    }
                    else {
                        ctrl->trickfase = -1;
                    }
                    if (!ctrl->afta) {
                        ctrl->afta = true;
                    }
                    if (ctrl->trfix == 3) {
                        ctrl->trfix = 4;
                        ctrl->statusque += 30;
                    }
                }
                if (ctrl->trickfase == 1) {
                    ++ctrl->flycnt;
                    if (ctrl->lrdirect != 0 && ctrl->flycnt > ctrl->lrstart) {
                        if (ctrl->lrswt && abs(mad->pxy - ctrl->oxy) > 180) {
                            if (ctrl->lrdirect == -1) {
                                ctrl->lrdirect = 1;
                            }
                            else {
                                ctrl->lrdirect = -1;
                            }
                            ctrl->lrswt = false;
                        }
                        if (ctrl->lrdirect == -1) {
                            ctrl->handb = true;
                            ctrl->left = true;
                        }
                        else {
                            ctrl->handb = true;
                            ctrl->right = true;
                        }
                    }
                    if (ctrl->uddirect != 0 && ctrl->flycnt > ctrl->udstart) {
                        if (ctrl->udswt && abs(mad->pzy - ctrl->ozy) > 180) {
                            if (ctrl->uddirect == -1) {
                                ctrl->uddirect = 1;
                            }
                            else {
                                ctrl->uddirect = -1;
                            }
                            ctrl->udswt = false;
                        }
                        if (ctrl->uddirect == -1) {
                            ctrl->handb = true;
                            ctrl->down = true;
                        }
                        else {
                            ctrl->handb = true;
                            ctrl->up = true;
                            if (ctrl->apunch > 0) {
                                ctrl->down = true;
                                --ctrl->apunch;
                            }
                        }
                    }
                    if ((mad->scy[0] + mad->scy[1] + mad->scy[2] + mad->scy[3]) * 100.0f / (contO->y - 300) < -ctrl->saftey) {
                        ctrl->onceu = false;
                        ctrl->onced = false;
                        ctrl->oncel = false;
                        ctrl->oncer = false;
                        ctrl->lrcomp = false;
                        ctrl->udcomp = false;
                        ctrl->udbare = false;
                        ctrl->lrbare = false;
                        ctrl->trickfase = 2;
                        ctrl->swat = 0;
                    }
                }
                if (ctrl->trickfase == 2) {
                    if (ctrl->swat == 0) {
                        if (mad->dcomp != 0.0f || mad->ucomp != 0.0f) {
                            ctrl->udbare = true;
                        }
                        if (mad->lcomp != 0.0f || mad->rcomp != 0.0f) {
                            ctrl->lrbare = true;
                        }
                        ctrl->swat = 1;
                    }
                    if (mad->wtouch) {
                        if (ctrl->swat == 1) {
                            ctrl->swat = 2;
                        }
                    }
                    else if (ctrl->swat == 2) {
                        if (mad->capsized && nfm_medium_random(m) > ctrl->mustland) {
                            if (ctrl->udbare) {
                                ctrl->lrbare = true;
                                ctrl->udbare = false;
                            }
                            else if (ctrl->lrbare) {
                                ctrl->udbare = true;
                                ctrl->lrbare = false;
                            }
                        }
                        ctrl->swat = 3;
                    }
                    if (ctrl->udbare) {
                        int n38;
                        for (n38 = mad->pzy + 90; n38 < 0; n38 += 360) {}
                        while (n38 > 180) {
                            n38 -= 360;
                        }
                         int abs_v = abs(n38);
                        if (mad->lcomp - mad->rcomp < 5.0f && (ctrl->onced || ctrl->onceu)) {
                            ctrl->udcomp = true;
                        }
                        if (mad->dcomp > mad->ucomp) {
                            if (mad->capsized) {
                                if (ctrl->udcomp) {
                                    if (abs_v > 90) {
                                        ctrl->up = true;
                                    }
                                    else {
                                        ctrl->down = true;
                                    }
                                }
                                else if (!ctrl->onced) {
                                    ctrl->down = true;
                                }
                            }
                            else {
                                if (ctrl->udcomp) {
                                    if (ctrl->perfection && abs(abs_v - 90) > 30) {
                                        if (abs_v > 90) {
                                            ctrl->up = true;
                                        }
                                        else {
                                            ctrl->down = true;
                                        }
                                    }
                                }
                                else if (nfm_medium_random(m) > ctrl->mustland) {
                                    ctrl->up = true;
                                }
                                ctrl->onced = true;
                            }
                        }
                        else if (mad->capsized) {
                            if (ctrl->udcomp) {
                                if (abs_v > 90) {
                                    ctrl->up = true;
                                }
                                else {
                                    ctrl->down = true;
                                }
                            }
                            else if (!ctrl->onceu) {
                                ctrl->up = true;
                            }
                        }
                        else {
                            if (ctrl->udcomp) {
                                if (ctrl->perfection && abs(abs_v - 90) > 30) {
                                    if (abs_v > 90) {
                                        ctrl->up = true;
                                    }
                                    else {
                                        ctrl->down = true;
                                    }
                                }
                            }
                            else if (nfm_medium_random(m) > ctrl->mustland) {
                                ctrl->down = true;
                            }
                            ctrl->onceu = true;
                        }
                    }
                    if (ctrl->lrbare) {
                        int n39 = mad->pxy + 90;
                        if (ctrl->zyinv) {
                            n39 += 180;
                        }
                        while (n39 < 0) {
                            n39 += 360;
                        }
                        while (n39 > 180) {
                            n39 -= 360;
                        }
                         int abs2 = abs(n39);
                        if (mad->lcomp - mad->rcomp < 10.0f && (ctrl->oncel || ctrl->oncer)) {
                            ctrl->lrcomp = true;
                        }
                        if (mad->lcomp > mad->rcomp) {
                            if (mad->capsized) {
                                if (ctrl->lrcomp) {
                                    if (abs2 > 90) {
                                        ctrl->left = true;
                                    }
                                    else {
                                        ctrl->right = true;
                                    }
                                }
                                else if (!ctrl->oncel) {
                                    ctrl->left = true;
                                }
                            }
                            else {
                                if (ctrl->lrcomp) {
                                    if (ctrl->perfection && abs(abs2 - 90) > 30) {
                                        if (abs2 > 90) {
                                            ctrl->left = true;
                                        }
                                        else {
                                            ctrl->right = true;
                                        }
                                    }
                                }
                                else if (nfm_medium_random(m) > ctrl->mustland) {
                                    ctrl->right = true;
                                }
                                ctrl->oncel = true;
                            }
                        }
                        else if (mad->capsized) {
                            if (ctrl->lrcomp) {
                                if (abs2 > 90) {
                                    ctrl->left = true;
                                }
                                else {
                                    ctrl->right = true;
                                }
                            }
                            else if (!ctrl->oncer) {
                                ctrl->right = true;
                            }
                        }
                        else {
                            if (ctrl->lrcomp) {
                                if (ctrl->perfection && abs(abs2 - 90) > 30) {
                                    if (abs2 > 90) {
                                        ctrl->left = true;
                                    }
                                    else {
                                        ctrl->right = true;
                                    }
                                }
                            }
                            else if (nfm_medium_random(m) > ctrl->mustland) {
                                ctrl->left = true;
                            }
                            ctrl->oncer = true;
                        }
                    }
                }
            }
        }

}

