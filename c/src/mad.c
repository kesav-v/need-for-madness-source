#include "nfm/mad.h"
#include "nfm/env_flags.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* C11 _Generic abs matching C++ std::abs overloads used in mad.cpp.
 * Use nfm_iabs (not abs) — mad.cpp has locals named abs/abs2 that would shadow. */
static inline int nfm_iabs(int x) { return x < 0 ? -x : x; }
#define nfm_abs(x) _Generic((x), float: fabsf, double: fabs, default: nfm_iabs)(x)

static bool nfm_rand_a_gt_b(NfmMedium* m) {
  float a = nfm_medium_random(m);
  float b = nfm_medium_random(m);
  return a > b;
}

static bool nfm_skd5_roll(NfmMedium* m, int im) {
  m->trace_label = "skd5";
  if (nfm_env.dmglog && im == 0)
    fprintf(stderr, "SKD5 im=0 rng=%llu\n", (unsigned long long)m->rng_calls);
  return nfm_rand_a_gt_b(m);
}

// xtGraphics.scrape RNG side-effects (norender skips audio but still burns random()).
// bfscrape is only decremented in playsounds, which norender never calls — so once
// set, it stays set for the rest of the race (matches Java --norender).
// thread_local so --jobs can run independent races concurrently.
static _Thread_local int xt_bfscrape = 0;
static _Thread_local int xt_sturn0 = 0;
static _Thread_local int xt_sturn1 = 0;

void nfm_mad_reset_scrape_rng(void) {
  xt_bfscrape = 0;
  xt_sturn0 = 0;
  xt_sturn1 = 0;
}

static void xt_scrape(NfmMedium* m, int n, int n2, int n3) {
  if (xt_bfscrape == 0 &&
      sqrt((double)(n) * n + (double)(n2) * n2 +
                (double)(n3) * n3) /
              10.0 >
          10.0) {
    int n4 = 0;
    // Java: m.random() > m.random() evaluates LHS first
    {
      float a = nfm_medium_random(m);
      float b = nfm_medium_random(m);
      if (a > b) n4 = 1;
    }
    if (n4 == 0) {
      xt_sturn1 = 0;
      ++xt_sturn0;
      if (xt_sturn0 == 3) {
        n4 = 1;
        xt_sturn1 = 1;
        xt_sturn0 = 0;
      }
    } else {
      xt_sturn0 = 0;
      ++xt_sturn1;
      if (xt_sturn1 == 3) {
        n4 = 0;
        xt_sturn0 = 1;
        xt_sturn1 = 0;
      }
    }
    (void)n4;
    xt_bfscrape = 5;
  }
}

static void nfm_log_dmg(NfmMedium* m, const char* fn, int im, int cn, int wheel, float n2, int before, int after) {
  if (!nfm_env.dmglog) return;
  if (after == before) return;
  fprintf(stderr, "DMG %s im=%d cn=%d wheel=%d n2=%.8f dhit=%d hitmag=%d->%d rng=%llu\n",
    fn, im, cn, wheel, n2, after - before, before, after, (unsigned long long)m->rng_calls);
}
void nfm_mad_init(NfmMad* self, NfmCarDefine* cd_in, NfmMedium* m_in, int im_in) {
  memset(self, 0, sizeof(*self));
  self->m = m_in;
  self->cd = cd_in;
  self->im = im_in;
  self->drag = 0.5f;
  self->pmlt = 1;
  self->nmlt = 1;
  self->focus = -1;
  self->power = 75.f;
  self->fixes = -1;
}



void nfm_mad_reseto(NfmMad* self, int cn, NfmContO* contO, NfmCheckPoints* checkPoints) {
        self->cn = cn;

        for (int i = 0; i < 8; ++i) {

            self->dominate[i] = false;

            self->caught[i] = false;

        }

        self->mxz = 0;

        self->cxz = 0;

        self->pzy = 0;

        self->pxy = 0;

        self->speed = 0.0f;

        for (int j = 0; j < 4; ++j) {

            self->scy[j] = 0.0f;

            self->scx[j] = 0.0f;

            self->scz[j] = 0.0f;

        }

        self->forca = ((float)sqrt(contO->keyz[0] * contO->keyz[0] + contO->keyx[0] * contO->keyx[0]) + (float)sqrt(contO->keyz[1] * contO->keyz[1] + contO->keyx[1] * contO->keyx[1]) + (float)sqrt(contO->keyz[2] * contO->keyz[2] + contO->keyx[2] * contO->keyx[2]) + (float)sqrt(contO->keyz[3] * contO->keyz[3] + contO->keyx[3] * contO->keyx[3])) / 10000.0f * (float)(self->cd->bounce[cn] - 0.3);

        self->mtouch = false;

        self->wtouch = false;

        self->txz = 0;

        self->fxz = 0;

        self->pmlt = 1;

        self->nmlt = 1;

        self->dcnt = 0;

        self->skid = 0;

        self->pushed = false;

        self->gtouch = false;

        self->pl = false;

        self->pr = false;

        self->pd = false;

        self->pu = false;

        self->loop = 0;

        self->ucomp = 0.0f;

        self->dcomp = 0.0f;

        self->lcomp = 0.0f;

        self->rcomp = 0.0f;

        self->lxz = 0;

        self->travxy = 0;

        self->travzy = 0;

        self->travxz = 0;

        self->rtab = false;

        self->ftab = false;

        self->btab = false;

        self->powerup = 0.0f;

        self->xtpower = 0;

        self->trcnt = 0;

        self->capcnt = 0;

        self->tilt = 0.0f;

        for (int k = 0; k < 4; ++k) {

            for (int l = 0; l < 4; ++l) {

                self->crank[k][l] = 0;

                self->lcrank[k][l] = 0;

            }

        }

        self->pan = 0;

        self->pcleared = checkPoints->pcs;

        self->clear = 0;

        self->nlaps = 0;

        self->focus = -1;

        self->missedcp = 0;

        self->nofocus = false;

        self->power = 98.0f;

        self->lastcolido = 0;

        checkPoints->dested[self->im] = 0;

        self->squash = 0;

        self->nbsq = 0;

        self->hitmag = 0;

        self->cntdest = 0;

        self->dest = false;

        self->newcar = false;

        if (self->im == self->xt_im) {

            self->m->checkpoint = -1;

            self->m->lastcheck = false;

        }

        self->rpdcatch = 0;

        self->newedcar = 0;

        self->fixes = -1;

        if (checkPoints->nfix == 1) {

            self->fixes = 4;

        }

        if (checkPoints->nfix == 2) {

            self->fixes = 3;

        }

        if (checkPoints->nfix == 3) {

            self->fixes = 2;

        }

        if (checkPoints->nfix == 4) {

            self->fixes = 1;

        }

}

void nfm_mad_drive(NfmMad* self, NfmControl* control, NfmContO* contO, NfmTrackers* trackers, NfmCheckPoints* checkPoints) {
  if (!nfm_env.inited) nfm_env_flags_init();
int n = 1;

        int n2 = 1;

        bool zyinv = false;

        bool b = false;

        bool b2 = false;

        self->capsized = false;

        int i;

        for (i = nfm_abs(self->pzy); i > 270; i -= 360) {}

        if (nfm_abs(i) > 90) {

            zyinv = true;

        }

        int n3 = 0;

        int j;

        for (j = nfm_abs(self->pxy); j > 270; j -= 360) {}

        if (nfm_abs(j) > 90) {

            n3 = 1;

            n2 = -1;

        }

        int grat = contO->grat;

        if (zyinv) {

            if (n3 != 0) {

                n3 = 0;

                b = true;

            }

            else {

                n3 = 1;

                self->capsized = true;

            }

            n = -1;

        }

        else if (n3 != 0) {

            self->capsized = true;

        }

        if (self->capsized) {

            grat = self->cd->flipy[self->cn] + self->squash;

        }

        control->zyinv = zyinv;

        float n4 = 0.0f;

        float n5 = 0.0f;

        float n6 = 0.0f;

        if (self->mtouch) {

            self->loop = 0;

        }

        if (self->wtouch) {

            if (self->loop == 2 || self->loop == -1) {

                self->loop = -1;

                if (control->left) {

                    self->pl = true;

                }

                if (control->right) {

                    self->pr = true;

                }

                if (control->up) {

                    self->pu = true;

                }

                if (control->down) {

                    self->pd = true;

                }

            }

            self->ucomp = 0.0f;

            self->dcomp = 0.0f;

            self->lcomp = 0.0f;

            self->rcomp = 0.0f;

        }

        if (control->handb) {

            if (!self->pushed) {

                if (!self->wtouch) {

                    if (self->loop == 0) {

                        self->loop = 1;

                    }

                }

                else if (self->gtouch) {

                    self->pushed = true;

                }

            }

        }

        else {

            self->pushed = false;

        }

        if (self->loop == 1) {

            float n7 = (self->scy[0] + self->scy[1] + self->scy[2] + self->scy[3]) / 4.0f;

            for (int k = 0; k < 4; ++k) {

                self->scy[k] = n7;

            }

            self->loop = 2;

        }

        if (!self->dest) {

            if (self->loop == 2) {

                if (control->up) {

                    if (self->ucomp == 0.0f) {

                        self->ucomp = 10.0f + (self->scy[0] + 50.0f) / 20.0f;

                        if (self->ucomp < 5.0f) {

                            self->ucomp = 5.0f;

                        }

                        if (self->ucomp > 10.0f) {

                            self->ucomp = 10.0f;

                        }

                        self->ucomp *= self->cd->airs[self->cn];

                    }

                    if (self->ucomp < 20.0f) {

                        self->ucomp += (float)(0.5 * self->cd->airs[self->cn]);

                    }

                    n4 = -self->cd->airc[self->cn] * nfm_medium_sin(self->m, contO->xz) * n2;

                    n5 = self->cd->airc[self->cn] * nfm_medium_cos(self->m, contO->xz) * n2;

                }

                else if (self->ucomp != 0.0f && self->ucomp > -2.0f) {

                    self->ucomp -= (float)(0.5 * self->cd->airs[self->cn]);

                }

                if (control->down) {

                    if (self->dcomp == 0.0f) {

                        self->dcomp = 10.0f + (self->scy[0] + 50.0f) / 20.0f;

                        if (self->dcomp < 5.0f) {

                            self->dcomp = 5.0f;

                        }

                        if (self->dcomp > 10.0f) {

                            self->dcomp = 10.0f;

                        }

                        self->dcomp *= self->cd->airs[self->cn];

                    }

                    if (self->dcomp < 20.0f) {

                        self->dcomp += (float)(0.5 * self->cd->airs[self->cn]);

                    }

                    n6 = -self->cd->airc[self->cn];

                }

                else if (self->dcomp != 0.0f && self->ucomp > -2.0f) {

                    self->dcomp -= (float)(0.5 * self->cd->airs[self->cn]);

                }

                if (control->left) {

                    if (self->lcomp == 0.0f) {

                        self->lcomp = 5.0f;

                    }

                    if (self->lcomp < 20.0f) {

                        self->lcomp += 2.0f * self->cd->airs[self->cn];

                    }

                    n4 = -self->cd->airc[self->cn] * nfm_medium_cos(self->m, contO->xz) * n;

                    n5 = -self->cd->airc[self->cn] * nfm_medium_sin(self->m, contO->xz) * n;

                }

                else if (self->lcomp > 0.0f) {

                    self->lcomp -= 2.0f * self->cd->airs[self->cn];

                }

                if (control->right) {

                    if (self->rcomp == 0.0f) {

                        self->rcomp = 5.0f;

                    }

                    if (self->rcomp < 20.0f) {

                        self->rcomp += 2.0f * self->cd->airs[self->cn];

                    }

                    n4 = self->cd->airc[self->cn] * nfm_medium_cos(self->m, contO->xz) * n;

                    n5 = self->cd->airc[self->cn] * nfm_medium_sin(self->m, contO->xz) * n;

                }

                else if (self->rcomp > 0.0f) {

                    self->rcomp -= 2.0f * self->cd->airs[self->cn];

                }

                self->pzy += (int)((self->dcomp - self->ucomp) * nfm_medium_cos(self->m, self->pxy));

                if (zyinv) {

                    contO->xz += (int)((self->dcomp - self->ucomp) * nfm_medium_sin(self->m, self->pxy));

                }

                else {

                    contO->xz -= (int)((self->dcomp - self->ucomp) * nfm_medium_sin(self->m, self->pxy));

                }

                self->pxy += (int)(self->rcomp - self->lcomp);

            }

            else {

                float loc_power = self->power;

                if (loc_power < 40.0f) {

                    loc_power = 40.0f;

                }

                if (control->down) {

                    if (self->speed > 0.0f) {

                        self->speed -= self->cd->handb[self->cn] / 2;

                    }

                    else {

                        int n8 = 0;

                        for (int l = 0; l < 2; ++l) {

                            if (self->speed <= -(self->cd->swits[self->cn][l] / 2 + loc_power * self->cd->swits[self->cn][l] / 196.0f)) {

                                ++n8;

                            }

                        }

                        if (n8 != 2) {

                            self->speed -= self->cd->acelf[self->cn][n8] / 2.0f + loc_power * self->cd->acelf[self->cn][n8] / 196.0f;

                        }

                        else {

                            self->speed = -(self->cd->swits[self->cn][1] / 2 + loc_power * self->cd->swits[self->cn][1] / 196.0f);

                        }

                    }

                }

                if (control->up) {

                    if (self->speed < 0.0f) {

                        self->speed += self->cd->handb[self->cn];

                    }

                    else {

                        int n9 = 0;

                        for (int n10 = 0; n10 < 3; ++n10) {

                            if (self->speed >= self->cd->swits[self->cn][n10] / 2 + loc_power * self->cd->swits[self->cn][n10] / 196.0f) {

                                ++n9;

                            }

                        }

                        if (n9 != 3) {

                            self->speed += self->cd->acelf[self->cn][n9] / 2.0f + loc_power * self->cd->acelf[self->cn][n9] / 196.0f;

                        }

                        else {

                            self->speed = self->cd->swits[self->cn][2] / 2 + loc_power * self->cd->swits[self->cn][2] / 196.0f;

                        }

                    }

                }

                if (control->handb && nfm_abs(self->speed) > self->cd->handb[self->cn]) {

                    if (self->speed < 0.0f) {

                        self->speed += self->cd->handb[self->cn];

                    }

                    else {

                        self->speed -= self->cd->handb[self->cn];

                    }

                }

                if (self->loop == -1 && contO->y < 100) {

                    if (control->left) {

                        if (!self->pl) {

                            if (self->lcomp == 0.0f) {

                                self->lcomp = 5.0f * self->cd->airs[self->cn];

                            }

                            if (self->lcomp < 20.0f) {

                                self->lcomp += 2.0f * self->cd->airs[self->cn];

                            }

                        }

                    }

                    else {

                        if (self->lcomp > 0.0f) {

                            self->lcomp -= 2.0f * self->cd->airs[self->cn];

                        }

                        self->pl = false;

                    }

                    if (control->right) {

                        if (!self->pr) {

                            if (self->rcomp == 0.0f) {

                                self->rcomp = 5.0f * self->cd->airs[self->cn];

                            }

                            if (self->rcomp < 20.0f) {

                                self->rcomp += 2.0f * self->cd->airs[self->cn];

                            }

                        }

                    }

                    else {

                        if (self->rcomp > 0.0f) {

                            self->rcomp -= 2.0f * self->cd->airs[self->cn];

                        }

                        self->pr = false;

                    }

                    if (control->up) {

                        if (!self->pu) {

                            if (self->ucomp == 0.0f) {

                                self->ucomp = 5.0f * self->cd->airs[self->cn];

                            }

                            if (self->ucomp < 20.0f) {

                                self->ucomp += 2.0f * self->cd->airs[self->cn];

                            }

                        }

                    }

                    else {

                        if (self->ucomp > 0.0f) {

                            self->ucomp -= 2.0f * self->cd->airs[self->cn];

                        }

                        self->pu = false;

                    }

                    if (control->down) {

                        if (!self->pd) {

                            if (self->dcomp == 0.0f) {

                                self->dcomp = 5.0f * self->cd->airs[self->cn];

                            }

                            if (self->dcomp < 20.0f) {

                                self->dcomp += 2.0f * self->cd->airs[self->cn];

                            }

                        }

                    }

                    else {

                        if (self->dcomp > 0.0f) {

                            self->dcomp -= 2.0f * self->cd->airs[self->cn];

                        }

                        self->pd = false;

                    }

                    self->pzy += (int)((self->dcomp - self->ucomp) * nfm_medium_cos(self->m, self->pxy));

                    if (zyinv) {

                        contO->xz += (int)((self->dcomp - self->ucomp) * nfm_medium_sin(self->m, self->pxy));

                    }

                    else {

                        contO->xz -= (int)((self->dcomp - self->ucomp) * nfm_medium_sin(self->m, self->pxy));

                    }

                    self->pxy += (int)(self->rcomp - self->lcomp);

                }

            }

        }

        float n11 = 20.0f * self->speed / (154.0f * self->cd->simag[self->cn]);

        if (n11 > 20.0f) {

            n11 = 20.0f;

        }

        contO->wzy -= (int)n11;

        if (contO->wzy < -30) {

            contO->wzy += 30;

        }

        if (contO->wzy > 30) {

            contO->wzy -= 30;

        }

        if (control->right) {

            contO->wxz -= self->cd->turn[self->cn];

            if (contO->wxz < -36) {

                contO->wxz = -36;

            }

        }

        if (control->left) {

            contO->wxz += self->cd->turn[self->cn];

            if (contO->wxz > 36) {

                contO->wxz = 36;

            }

        }

        if (contO->wxz != 0 && !control->left && !control->right) {

            if (nfm_abs(self->speed) < 10.0f) {

                if (nfm_abs(contO->wxz) == 1) {

                    contO->wxz = 0;

                }

                if (contO->wxz > 0) {

                    --contO->wxz;

                }

                if (contO->wxz < 0) {

                    ++contO->wxz;

                }

            }

            else {

                if (nfm_abs(contO->wxz) < self->cd->turn[self->cn] * 2) {

                    contO->wxz = 0;

                }

                if (contO->wxz > 0) {

                    contO->wxz -= self->cd->turn[self->cn] * 2;

                }

                if (contO->wxz < 0) {

                    contO->wxz += self->cd->turn[self->cn] * 2;

                }

            }

        }

        int n12 = (int)(3600.0f / (self->speed * self->speed));

        if (n12 < 5) {

            n12 = 5;

        }

        if (self->speed < 0.0f) {

            n12 = -n12;

        }

        if (self->wtouch) {

            if (!self->capsized) {

                if (!control->handb) {

                    self->fxz = contO->wxz / (n12 * 3);

                }

                else {

                    self->fxz = contO->wxz / n12;

                }

                contO->xz += contO->wxz / n12;

            }

            self->wtouch = false;

            self->gtouch = false;

        }

        else {

            contO->xz += self->fxz;

        }

        if (self->speed > 30.0f || self->speed < -100.0f) {

            while (nfm_abs(self->mxz - self->cxz) > 180) {

                if (self->cxz > self->mxz) {

                    self->cxz -= 360;

                }

                else {

                    if (self->cxz >= self->mxz) {

                        continue;

                    }

                    self->cxz += 360;

                }

            }

            if (nfm_abs(self->mxz - self->cxz) < 30) {

                self->cxz += (int)((self->mxz - self->cxz) / 4.0f);

            }

            else {

                if (self->cxz > self->mxz) {

                    self->cxz -= 10;

                }

                if (self->cxz < self->mxz) {

                    self->cxz += 10;

                }

            }

        }
float array[4] = {0};

        float array2[4] = {0};

        float array3[4] = {0};

        for (int n13 = 0; n13 < 4; ++n13) {

            array[n13] = contO->keyx[n13] + contO->x;

            array3[n13] = grat + contO->y;

            array2[n13] = contO->z + contO->keyz[n13];

            
            int n14 = n13;

            self->scy[n14] += 7.0f;

        }

        nfm_mad_rot(self, array, array3, contO->x, contO->y, self->pxy, 4);

        nfm_mad_rot(self, array3, array2, contO->y, contO->z, self->pzy, 4);

        nfm_mad_rot(self, array, array2, contO->x, contO->z, contO->xz, 4);

        bool b3 = false;

        int n15 = (int)((self->scx[0] + self->scx[1] + self->scx[2] + self->scx[3]) / 4.0f);

        int n16 = (int)((self->scz[0] + self->scz[1] + self->scz[2] + self->scz[3]) / 4.0f);

        for (int n17 = 0; n17 < 4; ++n17) {

            if (self->scx[n17] - n15 > 200.0f) {

                self->scx[n17] = 200 + n15;

            }

            if (self->scx[n17] - n15 < -200.0f) {

                self->scx[n17] = n15 - 200;

            }

            if (self->scz[n17] - n16 > 200.0f) {

                self->scz[n17] = 200 + n16;

            }

            if (self->scz[n17] - n16 < -200.0f) {

                self->scz[n17] = n16 - 200;

            }

        }

        for (int n18 = 0; n18 < 4; ++n18) {
            array3[n18] += self->scy[n18];
            array[n18] += (self->scx[0] + self->scx[1] + self->scx[2] + self->scx[3]) / 4.0f;
            array2[n18] += (self->scz[0] + self->scz[1] + self->scz[2] + self->scz[3]) / 4.0f;
        }
        if (nfm_env.ylog && self->im==5 && contO->z > 33500 && contO->z < 34200) {
          fprintf(stderr, "POSTINT im=5 arr3=%.2f,%.2f,%.2f,%.2f scy=%.3f,%.3f,%.3f,%.3f mt=%d rng=%llu\n",
            array3[0],array3[1],array3[2],array3[3], self->scy[0],self->scy[1],self->scy[2],self->scy[3], (int)self->mtouch,
            (unsigned long long)self->m->rng_calls);
        }
        if (nfm_env.ylog && self->im==0 && self->hitmag >= 900 && self->hitmag <= 2300) {
          fprintf(stderr, "D0 POSTINT hm=%d mt=%d n22=? rng=%llu scy=%.2f,%.2f,%.2f,%.2f\n",
            self->hitmag,(int)self->mtouch,(unsigned long long)self->m->rng_calls,self->scy[0],self->scy[1],self->scy[2],self->scy[3]);
        }
        int ncx = (contO->x - trackers->sx) / 3000;

        if (ncx > trackers->ncx) {

            ncx = trackers->ncx;

        }

        if (ncx < 0) {

            ncx = 0;

        }

        int ncz = (contO->z - trackers->sz) / 3000;

        if (ncz > trackers->ncz) {

            ncz = trackers->ncz;

        }

        if (ncz < 0) {

            ncz = 0;

        }

        int n22 = 1;

        for (int n23 = 0; n23 < nfm_trackers_sect(trackers, ncx, ncz)->n; ++n23) {

            int n24 = nfm_trackers_sect(trackers, ncx, ncz)->idx[n23];

            if (nfm_abs(trackers->zy[n24]) != 90 && nfm_abs(trackers->xy[n24]) != 90 && nfm_abs(contO->x - trackers->x[n24]) < trackers->radx[n24] && nfm_abs(contO->z - trackers->z[n24]) < trackers->radz[n24] && (!trackers->decor[n24] || self->m->resdown != 2 || self->xt_multion != 0)) {

                n22 = trackers->skd[n24];

            }

        }
if (self->mtouch) {

            float n25 = self->cd->grip[self->cn] - nfm_abs(self->txz - contO->xz) * self->speed / 250.0f;

            if (control->handb) {

                n25 -= nfm_abs(self->txz - contO->xz) * 4;

            }

            if (n25 < self->cd->grip[self->cn]) {

                if (self->skid != 2) {

                    self->skid = 1;

                }

                self->speed -= self->speed / 100.0f;

            }

            else if (self->skid == 1) {

                self->skid = 2;

            }

            if (n22 == 1) {

                n25 *= 0.75;

            }

            if (n22 == 2) {

                n25 *= 0.55;

            }

            int n26 = -(int)(self->speed * nfm_medium_sin(self->m, contO->xz) * nfm_medium_cos(self->m, self->pzy));

            int n27 = (int)(self->speed * nfm_medium_cos(self->m, contO->xz) * nfm_medium_cos(self->m, self->pzy));

            int n28 = -(int)(self->speed * nfm_medium_sin(self->m, self->pzy));

            if (self->capsized || self->dest || checkPoints->haltall) {

                n26 = 0;

                n27 = 0;

                n28 = 0;

                n25 = self->cd->grip[self->cn] / 5.0f;

                if (self->speed > 0.0f) {

                    self->speed -= 2.0f;

                }

                else {

                    self->speed += 2.0f;

                }

            }

            if (nfm_abs(self->speed) > self->drag) {

                if (self->speed > 0.0f) {

                    self->speed -= self->drag;

                }

                else {

                    self->speed += self->drag;

                }

            }

            else {

                self->speed = 0.0f;

            }

            if (self->cn == 8 && n25 < 5.0f) {

                n25 = 5.0f;

            }

            if (n25 < 1.0f) {

                n25 = 1.0f;

            }

            float n29 = 0.0f;

            float n30 = 0.0f;

            for (int n31 = 0; n31 < 4; ++n31) {

                if (nfm_abs(self->scx[n31] - n26) > n25) {

                    if (self->scx[n31] < n26) {

                        
                        int n32 = n31;

                        self->scx[n32] += n25;

                    }

                    else {

                        self->scx[n31] -= n25;

                    }

                }

                else {

                    self->scx[n31] = n26;

                }

                if (nfm_abs(self->scz[n31] - n27) > n25) {

                    if (self->scz[n31] < n27) {

                        
                        int n34 = n31;

                        self->scz[n34] += n25;

                    }

                    else {

                        self->scz[n31] -= n25;

                    }

                }

                else {

                    self->scz[n31] = n27;

                }

                if (nfm_abs(self->scy[n31] - n28) > n25) {

                    if (self->scy[n31] < n28) {

                        self->scy[n31] += n25;

                    }

                    else {

                        self->scy[n31] -= n25;

                    }

                }

                else {

                    self->scy[n31] = n28;

                }

                if (n25 < self->cd->grip[self->cn]) {

                    if (self->txz != contO->xz) {

                        ++self->dcnt;

                    }

                    else if (self->dcnt != 0) {

                        self->dcnt = 0;

                    }

                        if (self->dcnt > 40.0f * n25 / self->cd->grip[self->cn] || self->capsized) {

                        float n38 = 1.0f;

                        if (n22 != 0) {

                            n38 = 1.2f;

                        }

                        if (nfm_env.drvrng && self->im == 2 && self->m->rng_calls >= 221290 && self->m->rng_calls <= 221320)
                          fprintf(stderr, "GATE65 im=2 w=%d dcnt=%d n22=%d rng=%llu\n", n31, self->dcnt, n22, (unsigned long long)self->m->rng_calls);
                        if (nfm_medium_random(self->m) > 0.65) {
                            if (nfm_env.skiddust && self->im==2)
                              fprintf(stderr, "SKIDDUST im=2 n=%d dcnt=%d n22=%d cap=%d mt=%d rng=%llu y=%.2f\n",
                                n31, self->dcnt, n22, (int)self->capsized, (int)self->mtouch, (unsigned long long)self->m->rng_calls, array3[n31]);
                            nfm_conto_dust(contO, n31, array[n31], array3[n31], array2[n31],
                                        (int)self->scx[n31], (int)self->scz[n31],
                                        n38 * self->cd->simag[self->cn], (int)self->tilt,
                                        self->capsized && self->mtouch);
                            if (self->im == self->xt_im && !self->capsized) {

                            }

                        }

                    }

                    else {

                        if (nfm_env.drvrng && self->im == 2 && self->m->rng_calls >= 221290 && self->m->rng_calls <= 221320)
                          fprintf(stderr, "GATELOW im=2 w=%d dcnt=%d n22=%d rng=%llu\n", n31, self->dcnt, n22, (unsigned long long)self->m->rng_calls);
                        if (n22 == 1 && nfm_medium_random(self->m) > 0.8) {
                            nfm_conto_dust(contO, n31, array[n31], array3[n31], array2[n31],
                                        (int)self->scx[n31], (int)self->scz[n31],
                                        1.1f * self->cd->simag[self->cn], (int)self->tilt,
                                        self->capsized && self->mtouch);
                        }

                        if ((n22 == 2 || n22 == 3) && nfm_medium_random(self->m) > 0.6) {
                            nfm_conto_dust(contO, n31, array[n31], array3[n31], array2[n31],
                                        (int)self->scx[n31], (int)self->scz[n31],
                                        1.15f * self->cd->simag[self->cn], (int)self->tilt,
                                        self->capsized && self->mtouch);
                        }

                    }

                }

                else if (self->dcnt != 0) {

                    self->dcnt -= 2;

                    if (self->dcnt < 0) {

                        self->dcnt = 0;

                    }

                }

                if (n22 == 3) {
                    // Java evaluates array index before RHS; C++17 does RHS first — sequence explicitly.
                    const int wi = (int)(nfm_medium_random(self->m) * 4.0f);
                    self->scy[wi] = (float)(-100.0f * nfm_medium_random(self->m) * (self->speed / self->cd->swits[self->cn][2]) * (self->cd->bounce[self->cn] - 0.3));
                }

                if (n22 == 4) {
                    const int wi = (int)(nfm_medium_random(self->m) * 4.0f);
                    self->scy[wi] = (float)(-150.0f * nfm_medium_random(self->m) * (self->speed / self->cd->swits[self->cn][2]) * (self->cd->bounce[self->cn] - 0.3));
                }

                n29 += self->scx[n31];

                n30 += self->scz[n31];

            }

            self->txz = contO->xz;

            int n39;

            if (n29 > 0.0f) {

                n39 = -1;

            }

            else {

                n39 = 1;

            }

            // Java: Math.sqrt/acos are double; n29*n29+n30*n30 is float then widened.
            self->mxz = (int)(
                acos(n30 / sqrt((double)(n29 * n29 + n30 * n30))) /
                0.017453292519943295 * n39);
            if (self->skid == 2) {

                if (!self->capsized) {

                    n29 /= 4.0f;

                    n30 /= 4.0f;

                    if (b) {

                        self->speed = -((float)sqrt(n29 * n29 + n30 * n30) * nfm_medium_cos(self->m, self->mxz - contO->xz));

                    }

                    else {

                        self->speed = (float)sqrt(n29 * n29 + n30 * n30) * nfm_medium_cos(self->m, self->mxz - contO->xz);

                    }

                }

                self->skid = 0;

            }

            if (self->capsized && n29 == 0.0f && n30 == 0.0f) {

                n22 = 0;

            }

            self->mtouch = false;

            b3 = true;

        }

        else if (self->skid != 2) {

            self->skid = 2;

        }
        if (nfm_env.ylog && self->im==0 && self->hitmag >= 900 && self->hitmag <= 2300) {
          int pts=0, body=0;
          for (int i=0;i<contO->npl;i++) {
            pts += contO->p[i].n;
            if (contO->p[i].wz==0) body += contO->p[i].n;
          }
          long long sum=0;
          for (int i=0;i<contO->npl;i++) if (contO->p[i].wz==0)
            for (int j=0;j<contO->p[i].n;j++)
              sum += contO->p[i].ox[j]*1315423911LL + contO->p[i].oy[j]*2654435761LL + contO->p[i].oz[j];
          fprintf(stderr, "D0 AFTERGRIP hm=%d mt=%d n22=%d rng=%llu scy=%.2f,%.2f,%.2f,%.2f npl=%d pts=%d body=%d clrad=%d key=%d,%d mesh=%lld xz=%d\n",
            self->hitmag,(int)self->mtouch,n22,(unsigned long long)self->m->rng_calls,self->scy[0],self->scy[1],self->scy[2],self->scy[3],
            contO->npl,pts,body, self->cd->clrad[self->cn], contO->keyx[0], contO->keyz[0], sum, contO->xz);
        }
int n40 = 0;

        bool array7[4] = {0};

        bool array8[4] = {0};

        bool array9[4] = {0};

        float n41 = 0.0f;

        for (int n42 = 0; n42 < 4; ++n42) {

            array9[n42] = (array8[n42] = false);

            if (array3[n42] > 245.0f) {

                ++n40;

                self->wtouch = true;

                self->gtouch = true;

                if (!b3 && self->scy[n42] != 7.0f) {

                    float n43 = self->scy[n42] / 333.33f;

                    if (n43 > 0.3) {

                        n43 = 0.3f;

                    }

                    float n44;

                    if (n22 == 0) {

                        n44 = (float)(n43 + 1.1);

                    }

                    else {

                        n44 = (float)(n43 + 1.2);

                    }
                    nfm_conto_dust(contO, n42, array[n42], array3[n42], array2[n42],
                                (int)self->scx[n42], (int)self->scz[n42],
                                n44 * self->cd->simag[self->cn], 0, self->capsized && self->mtouch);

                }

                array3[n42] = 250.0f;

                array9[n42] = true;

                n41 += array3[n42] - 250.0f;

                float n45 = (nfm_abs(nfm_medium_sin(self->m, self->pxy)) + nfm_abs(nfm_medium_sin(self->m, self->pzy))) / 3.0f;

                if (n45 > 0.4) {

                    n45 = 0.4f;

                }

                float n46 = n45 + self->cd->bounce[self->cn];

                if (n46 < 1.1) {

                    n46 = 1.1f;

                }

                nfm_mad_regy(self, n42, nfm_abs(self->scy[n42] * n46), contO);

                if (self->scy[n42] > 0.0f) {

                    self->scy[n42] -= nfm_abs(self->scy[n42] * n46);

                }

                if (self->capsized) {

                    array8[n42] = true;

                }

            }

            array7[n42] = false;

        }

        if (n40 != 0) {

            float n48 = n41 / n40;

            for (int n49 = 0; n49 < 4; ++n49) {

                if (!array9[n49]) {

                    array3[n49] -= n48;

                }

            }

        }
        if (nfm_env.dmglog && self->im==0 && self->hitmag >= 1600 && self->hitmag <= 2300) {
          fprintf(stderr, "D0 POSTGROUND hm=%d rng=%llu\n", self->hitmag, (unsigned long long)self->m->rng_calls);
        }
int n51 = 0;
        if (nfm_env.ylog && self->im==5 && contO->z > 33500 && contO->z < 34200) {
          fprintf(stderr, "PRETRACK im=5 arr3=%.2f,%.2f,%.2f,%.2f scy=%.3f,%.3f,%.3f,%.3f scx=%.2f,%.2f,%.2f,%.2f n22=%d\n",
            array3[0],array3[1],array3[2],array3[3], self->scy[0],self->scy[1],self->scy[2],self->scy[3],
            self->scx[0],self->scx[1],self->scx[2],self->scx[3], n22);
        }
        if (nfm_env.track && self->im==5) {
          if (contO->z > 33800 && contO->z < 34500 && contO->x > 700 && contO->x < 900) {
            fprintf(stderr, "SECT5 ncx=%d ncz=%d cy=%d cz=%d nt=%d wY=%.1f,%.1f,%.1f,%.1f\n",
              ncx,ncz,contO->y,contO->z,trackers->nt, array3[0],array3[1],array3[2],array3[3]);
            for (int si=0; si<nfm_trackers_sect(trackers, ncx, ncz)->n; ++si) {
              int id=nfm_trackers_sect(trackers, ncx, ncz)->idx[si];
              // near wheels
              if (nfm_abs(trackers->x[id]-801)<800 && nfm_abs(trackers->z[id]-34132)<800) {
                fprintf(stderr, "  T id=%d y=%d x=%d z=%d xy=%d zy=%d rx=%d rz=%d ry=%d skd=%d decor=%d\n",
                  id, trackers->y[id], trackers->x[id], trackers->z[id],
                  trackers->xy[id], trackers->zy[id], trackers->radx[id], trackers->radz[id], trackers->rady[id],
                  trackers->skd[id], (int)trackers->decor[id]);
              }
            }
          }
        }

        for (int n52 = 0; n52 < nfm_trackers_sect(trackers, ncx, ncz)->n; ++n52) {

            int n53 = nfm_trackers_sect(trackers, ncx, ncz)->idx[n52];

            // Car XZ AABB from current wheel points (recomputed: walls can move wheels mid-self->loop).

            float carMinX = array[0];

            float carMaxX = array[0];

            float carMinZ = array2[0];

            float carMaxZ = array2[0];

            for (int wi = 1; wi < 4; ++wi) {

                if (array[wi] < carMinX) {

                    carMinX = array[wi];

                }

                if (array[wi] > carMaxX) {

                    carMaxX = array[wi];

                }

                if (array2[wi] < carMinZ) {

                    carMinZ = array2[wi];

                }

                if (array2[wi] > carMaxZ) {

                    carMaxZ = array2[wi];

                }

            }

            int tMinX = trackers->x[n53] - trackers->radx[n53];

            int tMaxX = trackers->x[n53] + trackers->radx[n53];

            int tMinZ = trackers->z[n53] - trackers->radz[n53];

            int tMaxZ = trackers->z[n53] + trackers->radz[n53];

            // Same open-interval rule as per-wheel: x > tMin && x < tMax.

            if (carMaxX <= tMinX || carMinX >= tMaxX || carMaxZ <= tMinZ || carMinZ >= tMaxZ) {
                continue;
            }
            if (nfm_env.dmglog && self->im==0 && self->hitmag>=1600 && self->hitmag<=2300 && self->m->rng_calls>=12550 && self->m->rng_calls<=12560) {
              fprintf(stderr, "TSECT im=0 idx=%d skd=%d xy=%d zy=%d y=%d rng=%llu\n",
                n53, trackers->skd[n53], trackers->xy[n53], trackers->zy[n53], trackers->y[n53],
                (unsigned long long)self->m->rng_calls);
            }
            int n54 = 0;
            int n55 = 0;

            for (int n56 = 0; n56 < 4; ++n56) {

                if (array8[n56] && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1) && array[n56] > trackers->x[n53] - trackers->radx[n53] && array[n56] < trackers->x[n53] + trackers->radx[n53] && array2[n56] > trackers->z[n53] - trackers->radz[n53] && array2[n56] < trackers->z[n53] + trackers->radz[n53]) {

                    if (self->im == self->xt_im) {

                    }

                }

                if (!array7[n56] && array[n56] > trackers->x[n53] - trackers->radx[n53] && array[n56] < trackers->x[n53] + trackers->radx[n53] && array2[n56] > trackers->z[n53] - trackers->radz[n53] && array2[n56] < trackers->z[n53] + trackers->radz[n53] && array3[n56] > trackers->y[n53] - trackers->rady[n53] && array3[n56] < trackers->y[n53] + trackers->rady[n53] && (!trackers->decor[n53] || self->m->resdown != 2 || self->xt_multion != 0)) {

                    if (trackers->xy[n53] == 0 && trackers->zy[n53] == 0 && trackers->y[n53] != 250 && array3[n56] > trackers->y[n53] - 5) {

                        ++n55;

                        self->wtouch = true;

                        self->gtouch = true;

                        if (!b3 && self->scy[n56] != 7.0f) {

                            float n57 = self->scy[n56] / 333.33f;

                            if (n57 > 0.3) {

                                n57 = 0.3f;

                            }

                            float n58;

                            if (n22 == 0) {

                                n58 = (float)(n57 + 1.1);

                            }

                            else {

                                n58 = (float)(n57 + 1.2);

                            }
                            nfm_conto_dust(contO, n56, array[n56], array3[n56], array2[n56],
                                        (int)self->scx[n56], (int)self->scz[n56],
                                        n58 * self->cd->simag[self->cn], 0, self->capsized && self->mtouch);

                        }

                        array3[n56] = trackers->y[n53];
                        if (nfm_env.track && (self->im==2 || self->im==5)) {
                          fprintf(stderr, "TRACKFLAT im=%d w=%d idx=%d y=%d skd=%d arr=%.2f,%.2f,%.2f scy=%.3f n22=%d\n",
                            self->im, n56, n53, trackers->y[n53], trackers->skd[n53],
                            array[n56], array3[n56], array2[n56], self->scy[n56], n22);
                        }

                        if (self->capsized && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1)) {

                            if (self->im == self->xt_im) {

                            }

                        }

                        float n59 = (nfm_abs(nfm_medium_sin(self->m, self->pxy)) + nfm_abs(nfm_medium_sin(self->m, self->pzy))) / 3.0f;

                        if (n59 > 0.4) {

                            n59 = 0.4f;

                        }

                        float n60 = n59 + self->cd->bounce[self->cn];

                        if (n60 < 1.1) {

                            n60 = 1.1f;

                        }

                        nfm_mad_regy(self, n56, nfm_abs(self->scy[n56] * n60), contO);

                        if (self->scy[n56] > 0.0f) {

                            self->scy[n56] -= nfm_abs(self->scy[n56] * n60);

                        }

                        array7[n56] = true;

                    }

                    if (trackers->zy[n53] == -90 && array2[n56] < trackers->z[n53] + trackers->radz[n53] && (self->scz[n56] < 0.0f || trackers->radz[n53] == 287)) {

                        for (int n62 = 0; n62 < 4; ++n62) {

                            if (n56 != n62 && array2[n62] >= trackers->z[n53] + trackers->radz[n53]) {

                                array2[n62] -= array2[n56] - (trackers->z[n53] + trackers->radz[n53]);

                            }

                        }

                        array2[n56] = trackers->z[n53] + trackers->radz[n53];

                        if (trackers->skd[n53] != 2) {

                            ++self->crank[0][n56];

                        }

                        if (trackers->skd[n53] == 5 && nfm_skd5_roll(self->m, self->im)) {

                            ++self->crank[0][n56];

                        }

                        if (self->crank[0][n56] > 1) {

                            if (self->im == self->xt_im) {
                                xt_scrape(self->m, (int)self->scx[n56], (int)self->scy[n56], (int)self->scz[n56]);
                            }

                        }

                        float n66 = (nfm_abs(nfm_medium_cos(self->m, self->pxy)) + nfm_abs(nfm_medium_cos(self->m, self->pzy))) / 4.0f;

                        if (n66 > 0.3) {

                            n66 = 0.3f;

                        }

                        if (b3) {

                            n66 = 0.0f;

                        }

                        float n67 = (float)(n66 + (self->cd->bounce[self->cn] - 0.2));

                        if (n67 < 1.1) {

                            n67 = 1.1f;

                        }

                        if (nfm_env.dmglog && self->im==0 && self->hitmag>=1600 && self->hitmag<=2300)
                          fprintf(stderr, "WALLREGZ zy-90 skd=%d dam=%d rng=%llu\n", trackers->skd[n53], trackers->dam[n53], (unsigned long long)self->m->rng_calls);
                        nfm_mad_regz(self, n56, nfm_abs(self->scz[n56] * n67 * trackers->dam[n53]), contO);

                        self->scz[n56] += nfm_abs(self->scz[n56] * n67);

                        self->skid = 2;

                        b2 = true;

                        array7[n56] = true;

                        if (!trackers->notwall[n53]) {

                            control->wall = n53;

                        }

                    }

                    if (trackers->zy[n53] == 90 && array2[n56] > trackers->z[n53] - trackers->radz[n53] && (self->scz[n56] > 0.0f || trackers->radz[n53] == 287)) {

                        for (int n69 = 0; n69 < 4; ++n69) {

                            if (n56 != n69 && array2[n69] <= trackers->z[n53] - trackers->radz[n53]) {

                                array2[n69] -= array2[n56] - (trackers->z[n53] - trackers->radz[n53]);

                            }

                        }

                        array2[n56] = trackers->z[n53] - trackers->radz[n53];

                        if (trackers->skd[n53] != 2) {

                            ++self->crank[1][n56];

                        }

                        if (trackers->skd[n53] == 5 && nfm_skd5_roll(self->m, self->im)) {

                            ++self->crank[1][n56];

                        }

                        if (self->crank[1][n56] > 1) {

                            if (self->im == self->xt_im) {
                                xt_scrape(self->m, (int)self->scx[n56], (int)self->scy[n56], (int)self->scz[n56]);
                            }

                        }

                        float n73 = (nfm_abs(nfm_medium_cos(self->m, self->pxy)) + nfm_abs(nfm_medium_cos(self->m, self->pzy))) / 4.0f;

                        if (n73 > 0.3) {

                            n73 = 0.3f;

                        }

                        if (b3) {

                            n73 = 0.0f;

                        }

                        float n74 = (float)(n73 + (self->cd->bounce[self->cn] - 0.2));

                        if (n74 < 1.1) {

                            n74 = 1.1f;

                        }

                        if (nfm_env.dmglog && self->im==0 && self->hitmag>=1600 && self->hitmag<=2300)
                          fprintf(stderr, "WALLREGZ zy90 skd=%d dam=%d rng=%llu\n", trackers->skd[n53], trackers->dam[n53], (unsigned long long)self->m->rng_calls);
                        nfm_mad_regz(self, n56, -nfm_abs(self->scz[n56] * n74 * trackers->dam[n53]), contO);

                        self->scz[n56] -= nfm_abs(self->scz[n56] * n74);

                        self->skid = 2;

                        b2 = true;

                        array7[n56] = true;

                        if (!trackers->notwall[n53]) {

                            control->wall = n53;

                        }

                    }

                    if (trackers->xy[n53] == -90 && array[n56] < trackers->x[n53] + trackers->radx[n53] && (self->scx[n56] < 0.0f || trackers->radx[n53] == 287)) {

                        for (int n76 = 0; n76 < 4; ++n76) {

                            if (n56 != n76 && array[n76] >= trackers->x[n53] + trackers->radx[n53]) {

                                array[n76] -= array[n56] - (trackers->x[n53] + trackers->radx[n53]);

                            }

                        }

                        array[n56] = trackers->x[n53] + trackers->radx[n53];

                        if (trackers->skd[n53] != 2) {

                            ++self->crank[2][n56];

                        }

                        if (trackers->skd[n53] == 5 && nfm_skd5_roll(self->m, self->im)) {

                            ++self->crank[2][n56];

                        }

                        if (self->crank[2][n56] > 1) {

                            if (self->im == self->xt_im) {
                                xt_scrape(self->m, (int)self->scx[n56], (int)self->scy[n56], (int)self->scz[n56]);
                            }

                        }

                        float n80 = (nfm_abs(nfm_medium_cos(self->m, self->pxy)) + nfm_abs(nfm_medium_cos(self->m, self->pzy))) / 4.0f;

                        if (n80 > 0.3) {

                            n80 = 0.3f;

                        }

                        if (b3) {

                            n80 = 0.0f;

                        }

                        float n81 = (float)(n80 + (self->cd->bounce[self->cn] - 0.2));

                        if (n81 < 1.1) {

                            n81 = 1.1f;

                        }

                        nfm_mad_regx(self, n56, nfm_abs(self->scx[n56] * n81 * trackers->dam[n53]), contO);

                        self->scx[n56] += nfm_abs(self->scx[n56] * n81);

                        self->skid = 2;

                        b2 = true;

                        array7[n56] = true;

                        if (!trackers->notwall[n53]) {

                            control->wall = n53;

                        }

                    }

                    if (trackers->xy[n53] == 90 && array[n56] > trackers->x[n53] - trackers->radx[n53] && (self->scx[n56] > 0.0f || trackers->radx[n53] == 287)) {

                        for (int n83 = 0; n83 < 4; ++n83) {

                            if (n56 != n83 && array[n83] <= trackers->x[n53] - trackers->radx[n53]) {

                                array[n83] -= array[n56] - (trackers->x[n53] - trackers->radx[n53]);

                            }

                        }

                        array[n56] = trackers->x[n53] - trackers->radx[n53];

                        if (trackers->skd[n53] != 2) {

                            ++self->crank[3][n56];

                        }

                        if (trackers->skd[n53] == 5 && nfm_skd5_roll(self->m, self->im)) {

                            ++self->crank[3][n56];

                        }

                        if (self->crank[3][n56] > 1) {

                            if (self->im == self->xt_im) {
                                xt_scrape(self->m, (int)self->scx[n56], (int)self->scy[n56], (int)self->scz[n56]);
                            }

                        }

                        float n87 = (nfm_abs(nfm_medium_cos(self->m, self->pxy)) + nfm_abs(nfm_medium_cos(self->m, self->pzy))) / 4.0f;

                        if (n87 > 0.3) {

                            n87 = 0.3f;

                        }

                        if (b3) {

                            n87 = 0.0f;

                        }

                        float n88 = (float)(n87 + (self->cd->bounce[self->cn] - 0.2));

                        if (n88 < 1.1) {

                            n88 = 1.1f;

                        }

                        nfm_mad_regx(self, n56, -nfm_abs(self->scx[n56] * n88 * trackers->dam[n53]), contO);

                        self->scx[n56] -= nfm_abs(self->scx[n56] * n88);

                        self->skid = 2;

                        b2 = true;

                        array7[n56] = true;

                        if (!trackers->notwall[n53]) {

                            control->wall = n53;

                        }

                    }

                    if (trackers->zy[n53] != 0 && trackers->zy[n53] != 90 && trackers->zy[n53] != -90) {

                        int n90 = 90 + trackers->zy[n53];

                        float n91 = 1.0f + (50 - nfm_abs(trackers->zy[n53])) / 30.0f;

                        if (n91 < 1.0f) {

                            n91 = 1.0f;

                        }

                        float n92 = trackers->y[n53] + ((array3[n56] - trackers->y[n53]) * nfm_medium_cos(self->m, n90) - (array2[n56] - trackers->z[n53]) * nfm_medium_sin(self->m, n90));

                        float n93 = trackers->z[n53] + ((array3[n56] - trackers->y[n53]) * nfm_medium_sin(self->m, n90) + (array2[n56] - trackers->z[n53]) * nfm_medium_cos(self->m, n90));

                        if (n93 > trackers->z[n53] && n93 < trackers->z[n53] + 200) {

                            self->scy[n56] -= (n93 - trackers->z[n53]) / n91;

                            n93 = trackers->z[n53];

                        }

                        if (n93 > trackers->z[n53] - 30) {

                            if (trackers->skd[n53] == 2) {

                                ++n54;

                            }

                            else {

                                ++n51;

                            }

                            self->wtouch = true;

                            self->gtouch = false;

                            if (self->capsized && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1)) {

                                if (self->im == self->xt_im) {

                                }

                            }

                            if (!b3 && n22 != 0) {
                                nfm_conto_dust(contO, n56, array[n56], array3[n56], array2[n56],
                                            (int)self->scx[n56], (int)self->scz[n56],
                                            1.4f * self->cd->simag[self->cn], 0,
                                            self->capsized && self->mtouch);
                            }

                        }

                        array3[n56] = trackers->y[n53] + ((n92 - trackers->y[n53]) * nfm_medium_cos(self->m, -n90) - (n93 - trackers->z[n53]) * nfm_medium_sin(self->m, -n90));

                        array2[n56] = trackers->z[n53] + ((n92 - trackers->y[n53]) * nfm_medium_sin(self->m, -n90) + (n93 - trackers->z[n53]) * nfm_medium_cos(self->m, -n90));
                        if (nfm_env.track && self->im==5) {
                          fprintf(stderr, "TRACKZY im=5 w=%d idx=%d ty=%d zy=%d arrY=%.2f\n", n56,n53,trackers->y[n53],trackers->zy[n53],array3[n56]);
                        }

                        array7[n56] = true;

                    }

                    if (trackers->xy[n53] != 0 && trackers->xy[n53] != 90 && trackers->xy[n53] != -90) {

                        int n95 = 90 + trackers->xy[n53];

                        float n96 = 1.0f + (50 - nfm_abs(trackers->xy[n53])) / 30.0f;

                        if (n96 < 1.0f) {

                            n96 = 1.0f;

                        }

                        float n97 = trackers->y[n53] + ((array3[n56] - trackers->y[n53]) * nfm_medium_cos(self->m, n95) - (array[n56] - trackers->x[n53]) * nfm_medium_sin(self->m, n95));

                        float n98 = trackers->x[n53] + ((array3[n56] - trackers->y[n53]) * nfm_medium_sin(self->m, n95) + (array[n56] - trackers->x[n53]) * nfm_medium_cos(self->m, n95));

                        if (n98 > trackers->x[n53] && n98 < trackers->x[n53] + 200) {

                            self->scy[n56] -= (n98 - trackers->x[n53]) / n96;

                            n98 = trackers->x[n53];

                        }

                        if (n98 > trackers->x[n53] - 30) {

                            if (trackers->skd[n53] == 2) {

                                ++n54;

                            }

                            else {

                                ++n51;

                            }

                            self->wtouch = true;

                            self->gtouch = false;

                            if (self->capsized && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1)) {

                                if (self->im == self->xt_im) {

                                }

                            }

                            if (!b3 && n22 != 0) {
                                nfm_conto_dust(contO, n56, array[n56], array3[n56], array2[n56],
                                            (int)self->scx[n56], (int)self->scz[n56],
                                            1.4f * self->cd->simag[self->cn], 0,
                                            self->capsized && self->mtouch);
                            }

                        }

                        array3[n56] = trackers->y[n53] + ((n97 - trackers->y[n53]) * nfm_medium_cos(self->m, -n95) - (n98 - trackers->x[n53]) * nfm_medium_sin(self->m, -n95));

                        array[n56] = trackers->x[n53] + ((n97 - trackers->y[n53]) * nfm_medium_sin(self->m, -n95) + (n98 - trackers->x[n53]) * nfm_medium_cos(self->m, -n95));
                        if (nfm_env.track && self->im==5) {
                          fprintf(stderr, "TRACKXY im=5 w=%d idx=%d ty=%d xy=%d arrY=%.2f\n", n56,n53,trackers->y[n53],trackers->xy[n53],array3[n56]);
                        }

                        array7[n56] = true;

                    }

                }

            }

            if (n54 == 4) {

                self->mtouch = true;

            }

            if (n55 == 4) {

                n40 = 4;

            }

        }
if (n51 == 4) {

            self->mtouch = true;

        }

        for (int n100 = 0; n100 < 4; ++n100) {

            for (int n101 = 0; n101 < 4; ++n101) {

                if (self->crank[n100][n101] == self->lcrank[n100][n101]) {

                    self->crank[n100][n101] = 0;

                }

                self->lcrank[n100][n101] = self->crank[n100][n101];

            }

        }

        int n102 = 0;

        int n103 = 0;

        int n104 = 0;

        int n105 = 0;

        if (self->scy[2] != self->scy[0]) {

            int n106;

            if (self->scy[2] < self->scy[0]) {

                n106 = -1;

            }

            else {

                n106 = 1;

            }

            double n107 = sqrt((array2[0] - array2[2]) * (array2[0] - array2[2]) + (array3[0] - array3[2]) * (array3[0] - array3[2]) + (array[0] - array[2]) * (array[0] - array[2])) / (nfm_abs(contO->keyz[0]) + nfm_abs(contO->keyz[2]));

            if (n107 >= 0.9998) {

                n102 = n106;

            }

            else {

                n102 = (int)(acos(n107) / 0.017453292519943295 * n106);

            }

        }

        if (self->scy[3] != self->scy[1]) {

            int n108;

            if (self->scy[3] < self->scy[1]) {

                n108 = -1;

            }

            else {

                n108 = 1;

            }

            double n109 = sqrt((array2[1] - array2[3]) * (array2[1] - array2[3]) + (array3[1] - array3[3]) * (array3[1] - array3[3]) + (array[1] - array[3]) * (array[1] - array[3])) / (nfm_abs(contO->keyz[1]) + nfm_abs(contO->keyz[3]));

            if (n109 >= 0.9998) {

                n103 = n108;

            }

            else {

                n103 = (int)(acos(n109) / 0.017453292519943295 * n108);

            }

        }

        if (self->scy[1] != self->scy[0]) {

            int n110;

            if (self->scy[1] < self->scy[0]) {

                n110 = -1;

            }

            else {

                n110 = 1;

            }

            double n111 = sqrt((array2[0] - array2[1]) * (array2[0] - array2[1]) + (array3[0] - array3[1]) * (array3[0] - array3[1]) + (array[0] - array[1]) * (array[0] - array[1])) / (nfm_abs(contO->keyx[0]) + nfm_abs(contO->keyx[1]));

            if (n111 >= 0.9998) {

                n104 = n110;

            }

            else {

                n104 = (int)(acos(n111) / 0.017453292519943295 * n110);

            }

        }

        if (self->scy[3] != self->scy[2]) {

            int n112;

            if (self->scy[3] < self->scy[2]) {

                n112 = -1;

            }

            else {

                n112 = 1;

            }

            double n113 = sqrt((array2[2] - array2[3]) * (array2[2] - array2[3]) + (array3[2] - array3[3]) * (array3[2] - array3[3]) + (array[2] - array[3]) * (array[2] - array[3])) / (nfm_abs(contO->keyx[2]) + nfm_abs(contO->keyx[3]));

            if (n113 >= 0.9998) {

                n105 = n112;

            }

            else {

                n105 = (int)(acos(n113) / 0.017453292519943295 * n112);

            }

        }

        if (nfm_env.acos && (self->im==2 || self->im==5)) {
          const double d02 = sqrt((array2[0]-array2[2])*(array2[0]-array2[2])+(array3[0]-array3[2])*(array3[0]-array3[2])+(array[0]-array[2])*(array[0]-array[2]));
          const double n107b = d02 / (nfm_abs(contO->keyz[0])+nfm_abs(contO->keyz[2]));
          const double d13 = sqrt((array2[1]-array2[3])*(array2[1]-array2[3])+(array3[1]-array3[3])*(array3[1]-array3[3])+(array[1]-array[3])*(array[1]-array[3]));
          const double n109b = d13 / (nfm_abs(contO->keyz[1])+nfm_abs(contO->keyz[3]));
          fprintf(stderr, "ACOS im=%d n102=%d n103=%d n107=%.17g n109=%.17g d02=%.17g keyz=%d,%d arr0=%.4f,%.4f,%.4f arr2=%.4f,%.4f,%.4f\n",
            self->im,n102,n103,n107b,n109b,d02, contO->keyz[0],contO->keyz[2],
            array[0],array3[0],array2[0], array[2],array3[2],array2[2]);
        }
        if (b2) {

            int abs;

            for (abs = nfm_abs(contO->xz + 45); abs > 180; abs -= 360) {}

            if (nfm_abs(abs) > 90) {

                self->pmlt = 1;

            }

            else {

                self->pmlt = -1;

            }

            int abs2;

            for (abs2 = nfm_abs(contO->xz - 45); abs2 > 180; abs2 -= 360) {}

            if (nfm_abs(abs2) > 90) {

                self->nmlt = 1;

            }

            else {

                self->nmlt = -1;

            }

        }

        contO->xz += (int)(self->forca * (self->scz[0] * self->nmlt - self->scz[1] * self->pmlt + self->scz[2] * self->pmlt - self->scz[3] * self->nmlt + self->scx[0] * self->pmlt + self->scx[1] * self->nmlt - self->scx[2] * self->nmlt - self->scx[3] * self->pmlt));

        if (nfm_abs(n103) > nfm_abs(n102)) {

            n102 = n103;

        }

        if (nfm_abs(n105) > nfm_abs(n104)) {

            n104 = n105;

        }

        if (!zyinv) {

            self->pzy += n102;

        }

        else {

            self->pzy -= n102;

        }

        if (n3 == 0) {

            self->pxy += n104;

        }

        else {

            self->pxy -= n104;

        }

        if (n40 == 4) {

            int n114 = 0;

            while (self->pzy < 360) {

                self->pzy += 360;

                contO->zy += 360;

            }

            while (self->pzy > 360) {

                self->pzy -= 360;

                contO->zy -= 360;

            }

            if (self->pzy < 190 && self->pzy > 170) {

                self->pzy = 180;

                contO->zy = 180;

                ++n114;

            }

            if (self->pzy > 350 || self->pzy < 10) {

                self->pzy = 0;

                contO->zy = 0;

                ++n114;

            }

            while (self->pxy < 360) {

                self->pxy += 360;

                contO->xy += 360;

            }

            while (self->pxy > 360) {

                self->pxy -= 360;

                contO->xy -= 360;

            }

            if (self->pxy < 190 && self->pxy > 170) {

                self->pxy = 180;

                contO->xy = 180;

                ++n114;

            }

            if (self->pxy > 350 || self->pxy < 10) {

                self->pxy = 0;

                contO->xy = 0;

                ++n114;

            }

            if (n114 == 2) {

                self->mtouch = true;

            }

        }

        if (!self->mtouch && self->wtouch) {

            if (self->cntouch == 10) {

                self->mtouch = true;

            }

            else {

                ++self->cntouch;

            }

        }

        else {

            self->cntouch = 0;

        }

        contO->y = (int)((array3[0] + array3[1] + array3[2] + array3[3]) / 4.0f - grat * nfm_medium_cos(self->m, self->pzy) * nfm_medium_cos(self->m, self->pxy) + n6);
        if (nfm_env.ylog && self->im==5) {
          fprintf(stderr, "YLOG im=5 arr3=%.3f,%.3f,%.3f,%.3f cy=%d pzy=%d pxy=%d hit=%d scy=%.3f,%.3f,%.3f,%.3f\n",
            array3[0],array3[1],array3[2],array3[3], contO->y, self->pzy, self->pxy, self->hitmag,
            self->scy[0],self->scy[1],self->scy[2],self->scy[3]);
        }

        int n115;

        if (zyinv) {

            n115 = -1;

        }

        else {

            n115 = 1;

        }

        contO->x = (int)((array[0] - contO->keyx[0] * nfm_medium_cos(self->m, contO->xz) + n115 * contO->keyz[0] * nfm_medium_sin(self->m, contO->xz) + array[1] - contO->keyx[1] * nfm_medium_cos(self->m, contO->xz) + n115 * contO->keyz[1] * nfm_medium_sin(self->m, contO->xz) + array[2] - contO->keyx[2] * nfm_medium_cos(self->m, contO->xz) + n115 * contO->keyz[2] * nfm_medium_sin(self->m, contO->xz) + array[3] - contO->keyx[3] * nfm_medium_cos(self->m, contO->xz) + n115 * contO->keyz[3] * nfm_medium_sin(self->m, contO->xz)) / 4.0f + grat * nfm_medium_sin(self->m, self->pxy) * nfm_medium_cos(self->m, contO->xz) - grat * nfm_medium_sin(self->m, self->pzy) * nfm_medium_sin(self->m, contO->xz) + n4);

        contO->z = (int)((array2[0] - n115 * contO->keyz[0] * nfm_medium_cos(self->m, contO->xz) - contO->keyx[0] * nfm_medium_sin(self->m, contO->xz) + array2[1] - n115 * contO->keyz[1] * nfm_medium_cos(self->m, contO->xz) - contO->keyx[1] * nfm_medium_sin(self->m, contO->xz) + array2[2] - n115 * contO->keyz[2] * nfm_medium_cos(self->m, contO->xz) - contO->keyx[2] * nfm_medium_sin(self->m, contO->xz) + array2[3] - n115 * contO->keyz[3] * nfm_medium_cos(self->m, contO->xz) - contO->keyx[3] * nfm_medium_sin(self->m, contO->xz)) / 4.0f + grat * nfm_medium_sin(self->m, self->pxy) * nfm_medium_sin(self->m, contO->xz) - grat * nfm_medium_sin(self->m, self->pzy) * nfm_medium_cos(self->m, contO->xz) + n5);

        if (nfm_abs(self->speed) > 10.0f || !self->mtouch) {

            if (nfm_abs(self->pxy - contO->xy) >= 4) {

                if (self->pxy > contO->xy) {

                    contO->xy += 2 + (self->pxy - contO->xy) / 2;

                }

                else {

                    contO->xy -= 2 + (contO->xy - self->pxy) / 2;

                }

            }

            else {

                contO->xy = self->pxy;

            }

            if (nfm_abs(self->pzy - contO->zy) >= 4) {

                if (self->pzy > contO->zy) {

                    contO->zy += 2 + (self->pzy - contO->zy) / 2;

                }

                else {

                    contO->zy -= 2 + (contO->zy - self->pzy) / 2;

                }

            }

            else {

                contO->zy = self->pzy;

            }

        }

        if (self->wtouch && !self->capsized) {

            float n116 = (float)(self->speed / self->cd->swits[self->cn][2] * 14.0f * (self->cd->bounce[self->cn] - 0.4));

            if (control->left && self->tilt < n116 && self->tilt >= 0.0f) {

                self->tilt += 0.4;

            }

            else if (control->right && self->tilt > -n116 && self->tilt <= 0.0f) {

                self->tilt -= 0.4;

            }

            else if (nfm_abs(self->tilt) > 3.0 * (self->cd->bounce[self->cn] - 0.4)) {

                if (self->tilt > 0.0f) {

                    self->tilt -= (float)(3.0 * (self->cd->bounce[self->cn] - 0.3));

                }

                else {

                    self->tilt += (float)(3.0 * (self->cd->bounce[self->cn] - 0.3));

                }

            }

            else {

                self->tilt = 0.0f;

            }

            contO->xy += (int)self->tilt;

            if (self->gtouch) {

                contO->y -= (int)(self->tilt / 1.5);

            }

        }

        else if (self->tilt != 0.0f) {

            self->tilt = 0.0f;

        }

        if (self->wtouch && n22 == 2) {
            if (nfm_env.bnc && self->im==2)
              fprintf(stderr, "BNC2 im=2 rng=%llu zy=%d\n", (unsigned long long)self->m->rng_calls, contO->zy);
            contO->zy += (int)((nfm_medium_random(self->m) * 6.0f * self->speed / self->cd->swits[self->cn][2] - 3.0f * self->speed / self->cd->swits[self->cn][2]) * (self->cd->bounce[self->cn] - 0.3));
            contO->xy += (int)((nfm_medium_random(self->m) * 6.0f * self->speed / self->cd->swits[self->cn][2] - 3.0f * self->speed / self->cd->swits[self->cn][2]) * (self->cd->bounce[self->cn] - 0.3));
        }
        if (self->wtouch && n22 == 1) {
            if (nfm_env.bnc && self->im==2)
              fprintf(stderr, "BNC1 im=2 rng=%llu zy=%d\n", (unsigned long long)self->m->rng_calls, contO->zy);
            contO->zy += (int)((nfm_medium_random(self->m) * 4.0f * self->speed / self->cd->swits[self->cn][2] - 2.0f * self->speed / self->cd->swits[self->cn][2]) * (self->cd->bounce[self->cn] - 0.3));
            contO->xy += (int)((nfm_medium_random(self->m) * 4.0f * self->speed / self->cd->swits[self->cn][2] - 2.0f * self->speed / self->cd->swits[self->cn][2]) * (self->cd->bounce[self->cn] - 0.3));
        }
if (self->hitmag >= self->cd->maxmag[self->cn] && !self->dest) {

            nfm_mad_distruct(self, contO);

            if (self->cntdest == 7) {

                self->dest = true;

            }

            else {

                ++self->cntdest;

            }

            if (self->cntdest == 1) {

            }

        }

        if (contO->dist == 0) {

        }

        int loc_focus = 0;

        int n118 = 0;

        int n119 = 0;

        int n120;

        if (self->nofocus) {

            n120 = 1;

        }

        else {

            n120 = 7;

        }

        for (int n121 = 0; n121 < checkPoints->n; ++n121) {

            if (checkPoints->typ[n121] > 0) {

                ++n119;

                if (checkPoints->typ[n121] == 1) {

                    if (self->clear == n119 + self->nlaps * checkPoints->nsp) {

                        n120 = 1;

                    }

                    if (nfm_abs(contO->z - checkPoints->z[n121]) < 60.0f + nfm_abs(self->scz[0] + self->scz[1] + self->scz[2] + self->scz[3]) / 4.0f && nfm_abs(contO->x - checkPoints->x[n121]) < 700 && nfm_abs(contO->y - checkPoints->y[n121] + 350) < 450 && self->clear == n119 + self->nlaps * checkPoints->nsp - 1) {

                        self->clear = n119 + self->nlaps * checkPoints->nsp;

                        self->pcleared = n121;

                        self->focus = -1;

                    }

                }

                if (checkPoints->typ[n121] == 2) {

                    if (self->clear == n119 + self->nlaps * checkPoints->nsp) {

                        n120 = 1;

                    }

                    if (nfm_abs(contO->x - checkPoints->x[n121]) < 60.0f + nfm_abs(self->scx[0] + self->scx[1] + self->scx[2] + self->scx[3]) / 4.0f && nfm_abs(contO->z - checkPoints->z[n121]) < 700 && nfm_abs(contO->y - checkPoints->y[n121] + 350) < 450 && self->clear == n119 + self->nlaps * checkPoints->nsp - 1) {

                        self->clear = n119 + self->nlaps * checkPoints->nsp;

                        self->pcleared = n121;

                        self->focus = -1;

                    }

                }

            }

            if (nfm_mad_py(contO->x / 100, checkPoints->x[n121] / 100, contO->z / 100, checkPoints->z[n121] / 100) * n120 < n118 || n118 == 0) {

                loc_focus = n121;

                n118 = nfm_mad_py(contO->x / 100, checkPoints->x[n121] / 100, contO->z / 100, checkPoints->z[n121] / 100) * n120;

            }

        }

        if (self->clear == n119 + self->nlaps * checkPoints->nsp) {

            ++self->nlaps;

        }

        if (self->im == self->xt_im) {

            self->m->checkpoint = self->clear;

            while (self->m->checkpoint >= checkPoints->nsp) {
                self->m->checkpoint -= checkPoints->nsp;

            }

            if (self->clear == checkPoints->nlaps * checkPoints->nsp - 1) {

                self->m->lastcheck = true;

            }

            if (checkPoints->haltall) {

                self->m->lastcheck = false;

            }

        }

        if (self->focus == -1) {

            if (self->im == self->xt_im) {

                loc_focus += 2;

            }

            else {

                ++loc_focus;

            }

            if (!self->nofocus) {

                int n122 = self->pcleared + 1;

                if (n122 >= checkPoints->n) {

                    n122 = 0;

                }

                while (checkPoints->typ[n122] <= 0) {

                    if (++n122 >= checkPoints->n) {

                        n122 = 0;

                    }

                }

                if (loc_focus > n122 && (self->clear != self->nlaps * checkPoints->nsp || loc_focus < self->pcleared)) {

                    loc_focus = n122;

                    self->focus = loc_focus;

                }

            }

            if (loc_focus >= checkPoints->n) {

                loc_focus -= checkPoints->n;

            }

            if (checkPoints->typ[loc_focus] == -3) {

                loc_focus = 0;

            }

            if (self->im == self->xt_im) {

                if (self->missedcp != -1) {

                    self->missedcp = -1;

                }

            }

            else if (self->missedcp != 0) {

                self->missedcp = 0;

            }

        }

        else {

            loc_focus = self->focus;

            if (self->im == self->xt_im) {

                if (self->missedcp == 0 && self->mtouch && sqrt(nfm_mad_py(contO->x / 10, checkPoints->x[self->focus] / 10, contO->z / 10, checkPoints->z[self->focus] / 10)) > 800.0) {

                    self->missedcp = 1;

                }

                if (self->missedcp == -2 && sqrt(nfm_mad_py(contO->x / 10, checkPoints->x[self->focus] / 10, contO->z / 10, checkPoints->z[self->focus] / 10)) < 400.0) {

                    self->missedcp = 0;

                }

                if (self->missedcp != 0 && self->mtouch && sqrt(nfm_mad_py(contO->x / 10, checkPoints->x[self->focus] / 10, contO->z / 10, checkPoints->z[self->focus] / 10)) < 250.0) {

                    self->missedcp = 68;

                }

            }

            else {

                self->missedcp = 1;

            }

            if (self->nofocus) {

                self->focus = -1;

                self->missedcp = 0;

            }

        }

        if (self->nofocus) {

            self->nofocus = false;

        }

        self->point = loc_focus;

        if (self->fixes != 0) {

            if (self->m->noelec == 0) {

                for (int n123 = 0; n123 < checkPoints->fn; ++n123) {

                    if (!checkPoints->roted[n123]) {

                        if (nfm_abs(contO->z - checkPoints->fz[n123]) < 200 && nfm_mad_py(contO->x / 100, checkPoints->fx[n123] / 100, contO->y / 100, checkPoints->fy[n123] / 100) < 30) {

                            if (contO->dist == 0) {

                                contO->fcnt = 8;

                            }

                            else {

                                if (self->im == self->xt_im && !contO->fix && !false) {

                                }

                                contO->fix = true;

                            }

                        }

                    }

                    else if (nfm_abs(contO->x - checkPoints->fx[n123]) < 200 && nfm_mad_py(contO->z / 100, checkPoints->fz[n123] / 100, contO->y / 100, checkPoints->fy[n123] / 100) < 30) {

                        if (contO->dist == 0) {

                            contO->fcnt = 8;

                        }

                        else {

                            if (self->im == self->xt_im && !contO->fix && !false) {

                            }

                            contO->fix = true;

                        }

                    }

                }

            }

        }

        else {

            for (int n124 = 0; n124 < checkPoints->fn; ++n124) {

                if (nfm_mad_rpy(self, contO->x / 100, checkPoints->fx[n124] / 100, contO->y / 100, checkPoints->fy[n124] / 100, contO->z / 100, checkPoints->fz[n124] / 100) < 760) {

                    self->m->noelec = 2;

                }

            }

        }

        if (contO->fcnt == 7 || contO->fcnt == 8) {

            self->squash = 0;

            self->nbsq = 0;

            self->hitmag = 0;

            self->cntdest = 0;

            self->dest = false;

            self->newcar = true;

            contO->fcnt = 9;

            if (self->fixes > 0) {

                --self->fixes;

            }

        }

        if (self->newedcar != 0) {

            --self->newedcar;

            if (self->newedcar == 10) {

                self->newcar = false;

            }

        }

        if (!self->mtouch) {

            if (self->trcnt != 1) {

                self->trcnt = 1;

                self->lxz = contO->xz;

            }

            if (self->loop == 2 || self->loop == -1) {

                self->travxy += (int)(self->rcomp - self->lcomp);

                if (nfm_abs(self->travxy) > 135) {

                    self->rtab = true;

                }

                self->travzy += (int)(self->ucomp - self->dcomp);

                if (self->travzy > 135) {

                    self->ftab = true;

                }

                if (self->travzy < -135) {

                    self->btab = true;

                }

            }

            if (self->lxz != contO->xz) {

                self->travxz += self->lxz - contO->xz;

                self->lxz = contO->xz;

            }

            if (self->srfcnt < 10) {

                if (control->wall != -1) {

                    self->surfer = true;

                }

                ++self->srfcnt;

            }

        }

        else if (!self->dest) {

            if (!self->capsized) {

                if (self->capcnt != 0) {

                    self->capcnt = 0;

                }

                if (self->gtouch && self->trcnt != 0) {

                    if (self->trcnt == 9) {

                        self->powerup = 0.0f;

                        if (nfm_abs(self->travxy) > 90) {

                            self->powerup += nfm_abs(self->travxy) / 24.0f;

                        }

                        else if (self->rtab) {

                            self->powerup += 30.0f;

                        }

                        if (nfm_abs(self->travzy) > 90) {

                            self->powerup += nfm_abs(self->travzy) / 18.0f;

                        }

                        else {

                            if (self->ftab) {

                                self->powerup += 40.0f;

                            }

                            if (self->btab) {

                                self->powerup += 40.0f;

                            }

                        }

                        if (nfm_abs(self->travxz) > 90) {

                            self->powerup += nfm_abs(self->travxz) / 18.0f;

                        }

                        if (self->surfer) {

                            self->powerup += 30.0f;

                        }

                        self->power += self->powerup;

                        self->rpdcatch = 30;

                        if (self->power > 98.0f) {

                            self->power = 98.0f;

                            if (self->powerup > 150.0f) {

                                self->xtpower = 200;

                            }

                            else {

                                self->xtpower = 100;

                            }

                        }

                    }

                    if (self->trcnt == 10) {

                        self->travxy = 0;

                        self->travzy = 0;

                        self->travxz = 0;

                        self->ftab = false;

                        self->rtab = false;

                        self->btab = false;

                        self->trcnt = 0;

                        self->srfcnt = 0;

                        self->surfer = false;

                    }

                    else {

                        ++self->trcnt;

                    }

                }

            }

            else {

                if (self->trcnt != 0) {

                    self->travxy = 0;

                    self->travzy = 0;

                    self->travxz = 0;

                    self->ftab = false;

                    self->rtab = false;

                    self->btab = false;

                    self->trcnt = 0;

                    self->srfcnt = 0;

                    self->surfer = false;

                }

                if (self->capcnt == 0) {

                    int n125 = 0;

                    for (int n126 = 0; n126 < 4; ++n126) {

                        if (nfm_abs(self->scz[n126]) < 70.0f && nfm_abs(self->scx[n126]) < 70.0f) {

                            ++n125;

                        }

                    }

                    if (n125 == 4) {

                        self->capcnt = 1;

                    }

                }

                else {

                    ++self->capcnt;

                    if (self->capcnt == 30) {

                        self->speed = 0.0f;

                        contO->y += self->cd->flipy[self->cn];

                        self->pxy += 180;

                        contO->xy += 180;

                        self->capcnt = 0;

                    }

                }

            }

            if (self->trcnt == 0 && self->speed != 0.0f) {

                if (self->xtpower == 0) {

                    if (self->power > 0.0f) {

                        self->power -= self->power * self->power * self->power / self->cd->powerloss[self->cn];

                    }

                    else {

                        self->power = 0.0f;

                    }

                }

                else {

                    --self->xtpower;

                }

            }

        }

        if (self->im == self->xt_im) {

            if (control->wall != -1) {

                control->wall = -1;

            }

        }

        else if (self->lastcolido != 0 && !self->dest) {

            --self->lastcolido;

        }

        if (self->dest) {

            if (checkPoints->dested[self->im] == 0) {

                if (self->lastcolido == 0) {

                    checkPoints->dested[self->im] = 1;

                }

                else {

                    checkPoints->dested[self->im] = 2;

                }

            }

        }

        else if (checkPoints->dested[self->im] != 0 && checkPoints->dested[self->im] != 3) {

            checkPoints->dested[self->im] = 0;

        }

        // Java: if (self->im==xt.im && rpd.wasted==0 && self->rpdcatch!=0) { --self->rpdcatch;
        //   if (self->rpdcatch==0) { rpd.cotchinow; if (rpd.hcaught) random(); } }
        // Norender skips Record.rec → self->caught stays 0 → hcaught never true → no random().
        if (self->im == self->xt_im && self->rpdcatch != 0) {
            --self->rpdcatch;
            // cotchinow / whenwasted omitted (no Record); do not call random() here.
        }
}

void nfm_mad_colide(NfmMad* self, NfmContO* contO, NfmMad* mad, NfmContO* contO2) {
        float array[4] = {0};

        float array2[4] = {0};

        float array3[4] = {0};

        float array4[4] = {0};

        float array5[4] = {0};

        float array6[4] = {0};

        for (int i = 0; i < 4; ++i) {

            array[i] = contO->x + contO->keyx[i];

            if (self->capsized) {

                array2[i] = contO->y + self->cd->flipy[self->cn] + self->squash;

            }

            else {

                array2[i] = contO->y + contO->grat;

            }

            array3[i] = contO->z + contO->keyz[i];

            array4[i] = contO2->x + contO2->keyx[i];

            if (self->capsized) {

                array5[i] = contO2->y + self->cd->flipy[mad->cn] + mad->squash;

            }

            else {

                array5[i] = contO2->y + contO2->grat;

            }

            array6[i] = contO2->z + contO2->keyz[i];

        }

        nfm_mad_rot(self, array, array2, contO->x, contO->y, contO->xy, 4);

        nfm_mad_rot(self, array2, array3, contO->y, contO->z, contO->zy, 4);

        nfm_mad_rot(self, array, array3, contO->x, contO->z, contO->xz, 4);

        nfm_mad_rot(self, array4, array5, contO2->x, contO2->y, contO2->xy, 4);

        nfm_mad_rot(self, array5, array6, contO2->y, contO2->z, contO2->zy, 4);

        nfm_mad_rot(self, array4, array6, contO2->x, contO2->z, contO2->xz, 4);

        if (nfm_mad_rpy(self, contO->x, contO2->x, contO->y, contO2->y, contO->z, contO2->z) < (contO->maxR * contO->maxR + contO2->maxR * contO2->maxR) * 1.5) {

            if (!self->caught[mad->im] && (self->speed != 0.0f || mad->speed != 0.0f)) {

                if (nfm_abs(self->power * self->speed * self->cd->moment[self->cn]) != nfm_abs(mad->power * mad->speed * self->cd->moment[mad->cn])) {

                    if (nfm_abs(self->power * self->speed * self->cd->moment[self->cn]) > nfm_abs(mad->power * mad->speed * self->cd->moment[mad->cn])) {

                        self->dominate[mad->im] = true;

                    }

                    else {

                        self->dominate[mad->im] = false;

                    }

                }

                else if (self->cd->moment[self->cn] > self->cd->moment[mad->cn]) {

                    self->dominate[mad->im] = true;

                }

                else {

                    self->dominate[mad->im] = false;

                }

                self->caught[mad->im] = true;

            }

        }

        else if (self->caught[mad->im]) {

            self->caught[mad->im] = false;

        }

        int n = 0;

        int n2 = 0;

        if (self->dominate[mad->im]) {

            int n3 = (int)(((self->scz[0] - mad->scz[0] + self->scz[1] - mad->scz[1] + self->scz[2] - mad->scz[2] + self->scz[3] - mad->scz[3]) * (self->scz[0] - mad->scz[0] + self->scz[1] - mad->scz[1] + self->scz[2] - mad->scz[2] + self->scz[3] - mad->scz[3]) + (self->scx[0] - mad->scx[0] + self->scx[1] - mad->scx[1] + self->scx[2] - mad->scx[2] + self->scx[3] - mad->scx[3]) * (self->scx[0] - mad->scx[0] + self->scx[1] - mad->scx[1] + self->scx[2] - mad->scx[2] + self->scx[3] - mad->scx[3])) / 16.0f);

            int n4 = 7000;

            float n5 = 1.0f;

            if (self->xt_multion != 0) {

                n4 = 28000;

                n5 = 1.27f;

            }

            for (int j = 0; j < 4; ++j) {

                for (int k = 0; k < 4; ++k) {

                    if (nfm_mad_rpy(self, array[j], array4[k], array2[j], array5[k], array3[j], array6[k]) < (n3 + n4) * (self->cd->comprad[mad->cn] + self->cd->comprad[self->cn])) {

                        if (nfm_abs(self->scx[j] * self->cd->moment[self->cn]) > nfm_abs(mad->scx[k] * self->cd->moment[mad->cn])) {

                            float n6 = mad->scx[k] * self->cd->revpush[self->cn];

                            if (n6 > 300.0f) {

                                n6 = 300.0f;

                            }

                            if (n6 < -300.0f) {

                                n6 = -300.0f;

                            }

                            float n7 = self->scx[j] * self->cd->push[self->cn];

                            if (n7 > 300.0f) {

                                n7 = 300.0f;

                            }

                            if (n7 < -300.0f) {

                                n7 = -300.0f;

                            }

                            mad->scx[k] += n7;

                            if (self->im == self->xt_im) {

                                mad->colidim = true;

                            }

                            int n9 = n + nfm_mad_regx(mad, k, n7 * self->cd->moment[self->cn] * n5, contO2);

                            if (mad->colidim) {

                                mad->colidim = false;

                            }

                            self->scx[j] -= n6;

                            n2 += nfm_mad_regx(self, j, -n6 * self->cd->moment[self->cn] * n5, contO);

                            
                            int n11 = j;

                            self->scy[n11] -= self->cd->revlift[self->cn];

                            if (self->im == self->xt_im) {

                                mad->colidim = true;

                            }

                            n = n9 + nfm_mad_regy(mad, k, self->cd->revlift[self->cn] * 7, contO2);

                            if (mad->colidim) {

                                mad->colidim = false;

                            }

                            if (nfm_rand_a_gt_b(self->m)) {

                            }

                        }

                        if (nfm_abs(self->scz[j] * self->cd->moment[self->cn]) > nfm_abs(mad->scz[k] * self->cd->moment[mad->cn])) {

                            float n12 = mad->scz[k] * self->cd->revpush[self->cn];

                            if (n12 > 300.0f) {

                                n12 = 300.0f;

                            }

                            if (n12 < -300.0f) {

                                n12 = -300.0f;

                            }

                            float n13 = self->scz[j] * self->cd->push[self->cn];

                            if (n13 > 300.0f) {

                                n13 = 300.0f;

                            }

                            if (n13 < -300.0f) {

                                n13 = -300.0f;

                            }

                            mad->scz[k] += n13;

                            if (self->im == self->xt_im) {

                                mad->colidim = true;

                            }

                            int n15 = n + nfm_mad_regz(mad, k, n13 * self->cd->moment[self->cn] * n5, contO2);

                            if (mad->colidim) {

                                mad->colidim = false;

                            }

                            self->scz[j] -= n12;

                            n2 += nfm_mad_regz(self, j, -n12 * self->cd->moment[self->cn] * n5, contO);

                            self->scy[j] -= self->cd->revlift[self->cn];

                            if (self->im == self->xt_im) {

                                mad->colidim = true;

                            }

                            n = n15 + nfm_mad_regy(mad, k, self->cd->revlift[self->cn] * 7, contO2);

                            if (mad->colidim) {

                                mad->colidim = false;

                            }

                            if (nfm_rand_a_gt_b(self->m)) {

                            }

                        }

                        if (self->im == self->xt_im) {

                            mad->lastcolido = 70;

                        }

                        if (mad->im == self->xt_im) {

                            self->lastcolido = 70;

                        }

                        mad->scy[k] -= self->cd->lift[self->cn];

                    }

                }

            }

        }
}

void nfm_mad_distruct(NfmMad* self, NfmContO* contO) {
  (void)self;
  (void)contO;  // TODO: ContO.p[wz==0].embos (render mesh)
}

int nfm_mad_regy(NfmMad* self, int n, float n2, NfmContO* contO) {
  const int _hm0 = self->hitmag;
  int n3 = 0;
  bool b = true;
  if (self->xt_multion == 1 && self->xt_im != self->im) b = false;
  if (self->xt_multion >= 2) b = false;
  n2 *= self->cd->dammult[self->cn];
  if (n2 > 100.0f) {
    n2 -= 100.0f;
    int n4 = 0;
    int n5 = 0;
    int i = contO->zy;
    int j = contO->xy;
    while (i < 360) i += 360;
    while (i > 360) i -= 360;
    if (i < 210 && i > 150) n4 = -1;
    if (i > 330 || i < 30) n4 = 1;
    while (j < 360) j += 360;
    while (j > 360) j -= 360;
    if (j < 210 && j > 150) n5 = -1;
    if (j > 330 || j < 30) n5 = 1;
    if (n5 * n4 == 0) self->shakedam = (int)((nfm_abs(n2) + self->shakedam) / 2.0f);
    if (n5 * n4 == 0 || self->mtouch) {
      for (int k = 0; k < contO->npl; ++k) {
        float ctmag = 0.0f;
        for (int l = 0; l < contO->p[k].n; ++l) {
          if (contO->p[k].wz == 0 &&
              nfm_mad_py(contO->keyx[n], contO->p[k].ox[l], contO->keyz[n],
                 contO->p[k].oz[l]) < self->cd->clrad[self->cn]) {
            { const char* _p=self->m->trace_label; self->m->trace_label="reg"; ctmag = n2 / 20.0f * nfm_medium_random(self->m); self->m->trace_label=_p; }
            contO->p[k].oz[l] += (int)(ctmag * nfm_medium_sin(self->m, i));
            contO->p[k].ox[l] -= (int)(ctmag * nfm_medium_sin(self->m, j));
            if (b) {
              self->hitmag += (int)(nfm_abs(ctmag));
              n3 += (int)(nfm_abs(ctmag));
            }
          }
        }
        if (ctmag != 0.0f) {
          if (nfm_abs(ctmag) >= 1.0f) {
            contO->p[k].chip = 1;
            contO->p[k].ctmag = ctmag;
          }
          if (!contO->p[k].nocol && contO->p[k].glass != 1) {
            contO->p[k].bfase += (int)(ctmag);
          }
          if (contO->p[k].glass == 1)
            contO->p[k].gr += (int)(nfm_abs(ctmag * 1.5f));
        }
      }
    }
    if (n5 * n4 == -1) {
      if (self->nbsq > 0) {
        int n8 = 0;
        int n9 = 1;
        for (int n10 = 0; n10 < contO->npl; ++n10) {
          float ctmag2 = 0.0f;
          for (int n11 = 0; n11 < contO->p[n10].n; ++n11) {
            if (contO->p[n10].wz == 0) {
              { const char* _p=self->m->trace_label; self->m->trace_label="regsquash"; ctmag2 = n2 / 15.0f * nfm_medium_random(self->m); self->m->trace_label=_p; }
              if ((nfm_abs(contO->p[n10].oy[n11] - self->cd->flipy[self->cn] - self->squash) <
                       self->cd->msquash[self->cn] * 3 ||
                   contO->p[n10].oy[n11] < self->cd->flipy[self->cn] + self->squash) &&
                  self->squash < self->cd->msquash[self->cn]) {
                contO->p[n10].oy[n11] += (int)(ctmag2);
                n8 += (int)(ctmag2);
                ++n9;
                if (b) {
                  self->hitmag += (int)(nfm_abs(ctmag2));
                  n3 += (int)(nfm_abs(ctmag2));
                }
              }
            }
          }
          if (contO->p[n10].glass == 1)
            contO->p[n10].gr += 5;
          else if (ctmag2 != 0.0f)
            contO->p[n10].bfase += (int)(ctmag2);
          if (nfm_abs(ctmag2) >= 1.0f) {
            contO->p[n10].chip = 1;
            contO->p[n10].ctmag = ctmag2;
          }
        }
        self->squash += n8 / n9;
        self->nbsq = 0;
      } else {
        ++self->nbsq;
      }
    }
  }
  nfm_log_dmg(self->m, "regy", self->im, self->cn, n, n2, _hm0, self->hitmag);
  if (nfm_env.pzy && self->im==2) fprintf(stderr, "PZY after regy wheel=%d pzy=%d hit=%d rng=%llu cy=%d\n", n, self->pzy, self->hitmag, (unsigned long long)self->m->rng_calls, contO->y);
  return n3;
}

int nfm_mad_regx(NfmMad* self, int n, float n2, NfmContO* contO) {
  const int _hm0 = self->hitmag;
  int n3 = 0;
  bool b = true;
  if (self->xt_multion == 1 && self->xt_im != self->im) b = false;
  if (self->xt_multion >= 2) b = false;
  n2 *= self->cd->dammult[self->cn];
  if (nfm_abs(n2) > 100.0f) {
    if (n2 > 100.0f) n2 -= 100.0f;
    if (n2 < -100.0f) n2 += 100.0f;
    self->shakedam = (int)((nfm_abs(n2) + self->shakedam) / 2.0f);
    for (int i = 0; i < contO->npl; ++i) {
      float ctmag = 0.0f;
      for (int j = 0; j < contO->p[i].n; ++j) {
        if (contO->p[i].wz == 0 &&
            nfm_mad_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n],
               contO->p[i].oz[j]) < self->cd->clrad[self->cn]) {
          { const char* _p=self->m->trace_label; self->m->trace_label="reg"; ctmag = n2 / 20.0f * nfm_medium_random(self->m); self->m->trace_label=_p; }
          contO->p[i].oz[j] -=
              (int)(ctmag * nfm_medium_sin(self->m, contO->xz) * nfm_medium_cos(self->m, contO->zy));
          contO->p[i].ox[j] +=
              (int)(ctmag * nfm_medium_cos(self->m, contO->xz) * nfm_medium_cos(self->m, contO->xy));
          if (b) {
            self->hitmag += (int)(nfm_abs(ctmag));
            n3 += (int)(nfm_abs(ctmag));
          }
        }
      }
      if (ctmag != 0.0f) {
        if (nfm_abs(ctmag) >= 1.0f) {
          contO->p[i].chip = 1;
          contO->p[i].ctmag = ctmag;
        }
        if (!contO->p[i].nocol && contO->p[i].glass != 1)
          contO->p[i].bfase += (int)(nfm_abs(ctmag));
        if (contO->p[i].glass == 1)
          contO->p[i].gr += (int)(nfm_abs(ctmag * 1.5f));
      }
    }
  }
  nfm_log_dmg(self->m, "regx", self->im, self->cn, n, n2, _hm0, self->hitmag);
  return n3;
}

int nfm_mad_regz(NfmMad* self, int n, float n2, NfmContO* contO) {
  const int _hm0 = self->hitmag;
  int n3 = 0;
  int _nhit = 0;
  const unsigned long long _rng_enter = self->m->rng_calls;
  bool b = true;
  if (self->xt_multion == 1 && self->xt_im != self->im) b = false;
  if (self->xt_multion >= 2) b = false;
  n2 *= self->cd->dammult[self->cn];
  if (nfm_abs(n2) > 100.0f) {
    if (n2 > 100.0f) n2 -= 100.0f;
    if (n2 < -100.0f) n2 += 100.0f;
    self->shakedam = (int)((nfm_abs(n2) + self->shakedam) / 2.0f);
    for (int i = 0; i < contO->npl; ++i) {
      float ctmag = 0.0f;
      for (int j = 0; j < contO->p[i].n; ++j) {
        if (contO->p[i].wz == 0 &&
            nfm_mad_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n],
               contO->p[i].oz[j]) < self->cd->clrad[self->cn]) {
          ++_nhit;
          { const char* _p=self->m->trace_label; self->m->trace_label="reg"; ctmag = n2 / 20.0f * nfm_medium_random(self->m); self->m->trace_label=_p; }
          contO->p[i].oz[j] +=
              (int)(ctmag * nfm_medium_cos(self->m, contO->xz) * nfm_medium_cos(self->m, contO->zy));
          contO->p[i].ox[j] +=
              (int)(ctmag * nfm_medium_sin(self->m, contO->xz) * nfm_medium_cos(self->m, contO->xy));
          if (b) {
            self->hitmag += (int)(nfm_abs(ctmag));
            n3 += (int)(nfm_abs(ctmag));
          }
        }
      }
      if (ctmag != 0.0f) {
        if (nfm_abs(ctmag) >= 1.0f) {
          contO->p[i].chip = 1;
          contO->p[i].ctmag = ctmag;
        }
        if (!contO->p[i].nocol && contO->p[i].glass != 1)
          contO->p[i].bfase += (int)(nfm_abs(ctmag));
        if (contO->p[i].glass == 1)
          contO->p[i].gr += (int)(nfm_abs(ctmag * 1.5f));
      }
    }
  }
  nfm_log_dmg(self->m, "regz", self->im, self->cn, n, n2, _hm0, self->hitmag);
  if (nfm_env.dmglog && _hm0 != self->hitmag)
    fprintf(stderr, "REGZVERTS im=%d wheel=%d nhit=%d npl=%d enter=%llu exit=%llu\n",
      self->im, n, _nhit, contO->npl, _rng_enter, (unsigned long long)self->m->rng_calls);
  return n3;
}


void nfm_mad_rot(NfmMad* self, float* array, float* array2, int n, int n2, int n3, int n4) {
        if (n3 != 0) {

            for (int i = 0; i < n4; ++i) {

                float n5 = array[i];

                float n6 = array2[i];

                array[i] = n + ((n5 - n) * nfm_medium_cos(self->m, n3) - (n6 - n2) * nfm_medium_sin(self->m, n3));

                array2[i] = n2 + ((n5 - n) * nfm_medium_sin(self->m, n3) + (n6 - n2) * nfm_medium_cos(self->m, n3));

            }

        }

}

int nfm_mad_rpy(NfmMad* self, float n, float n2, float n3, float n4, float n5, float n6) {
        (void)self;
        return (int)((n - n2) * (n - n2) + (n3 - n4) * (n3 - n4) + (n5 - n6) * (n5 - n6));

}

