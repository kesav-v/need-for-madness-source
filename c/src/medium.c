#include "nfm/medium.h"
#include "nfm/env_flags.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void nfm_medium_init(NfmMedium* m, bool seeded, int64_t seed) {
  int i;
  for (i = 0; i < 360; ++i) {
    m->tcos[i] = (float)cos(i * 0.017453292519943295);
    m->tsin[i] = (float)sin(i * 0.017453292519943295);
  }
  m->rng_calls = 0;
  m->trace_label = NULL;
  m->mgen = 0;
  m->resdown = 0;
  m->checkpoint = -1;
  m->lastcheck = false;
  m->ground = 250;
  m->noelec = 0;
  m->rng = NULL;
  m->use_rng = false;
  m->cntrn = 0;
  m->trn = 0;
  for (i = 0; i < 3; ++i) {
    m->diup[i] = false;
    m->rand_[i] = 0;
  }
  if (seeded) {
    nfm_medium_reseed(m, seed);
  } else {
    m->use_rng = false;
    m->rng = NULL;
    m->mgen = 0;
    /* Match Medium.java: gofo consumes one nextUnit at construct. */
    (void)(0.33000001311302185 + nfm_medium_next_unit(m) * 1.34);
  }
}

void nfm_medium_reseed(NfmMedium* m, int64_t seed) {
  int i;
  nfm_java_random_set_seed(&m->rng_storage, seed);
  m->use_rng = true;
  m->rng = &m->rng_storage;
  m->rng_calls = 0;
  m->cntrn = 0;
  m->trn = 0;
  for (i = 0; i < 3; ++i) {
    m->diup[i] = false;
    m->rand_[i] = 0;
  }
  m->resdown = 0;
  m->checkpoint = -1;
  m->lastcheck = false;
  m->ground = 250;
  m->noelec = 0;
  m->mgen = (int)(nfm_java_random_next_double(m->rng) * 100000.0);
  (void)(0.33000001311302185 + nfm_medium_next_unit(m) * 1.34);
}

double nfm_medium_next_unit(NfmMedium* m) {
  if (m->use_rng && m->rng) return nfm_java_random_next_double(m->rng);
  /* Unseeded Medium is for sin/cos tables only. */
  {
    static int emergency_inited = 0;
    static NfmJavaRandom emergency;
    if (!emergency_inited) {
      int64_t s =
          (int64_t)0x5EED ^ (int64_t)time(NULL) ^ (int64_t)clock();
      nfm_java_random_set_seed(&emergency, s);
      emergency_inited = 1;
    }
    return nfm_java_random_next_double(&emergency);
  }
}

float nfm_medium_random(NfmMedium* m) {
  int i, j;
  float out;
  if (!nfm_env.inited) nfm_env_flags_init();
  ++m->rng_calls;
  if (nfm_env.rngtrace && m->rng_calls >= 12550 &&
      m->rng_calls <= 12610) {
    fprintf(stderr, "RNG %llu %s\n", (unsigned long long)m->rng_calls,
            m->trace_label ? m->trace_label : "-");
  }
  if (m->cntrn == 0) {
    for (i = 0; i < 3; ++i) {
      m->rand_[i] = (int)(10.0 * nfm_medium_next_unit(m));
      /* Java evaluates left nextUnit before right. */
      {
        const double a = nfm_medium_next_unit(m);
        const double b = nfm_medium_next_unit(m);
        if (a > b)
          m->diup[i] = false;
        else
          m->diup[i] = true;
      }
    }
    m->cntrn = 20;
  } else {
    --m->cntrn;
  }
  for (j = 0; j < 3; ++j) {
    if (m->diup[j]) {
      ++m->rand_[j];
      if (m->rand_[j] == 10) m->rand_[j] = 0;
    } else {
      --m->rand_[j];
      if (m->rand_[j] == -1) m->rand_[j] = 9;
    }
  }
  ++m->trn;
  if (m->trn == 3) m->trn = 0;
  out = m->rand_[m->trn] / 10.0f;
  if (nfm_env.rngval && m->rng_calls >= 12550 &&
      m->rng_calls <= 12610) {
    fprintf(stderr, "RNGVAL %llu -> %.8f label=%s\n",
            (unsigned long long)m->rng_calls, out,
            m->trace_label ? m->trace_label : "-");
  }
  return out;
}

float nfm_medium_cos(const NfmMedium* m, int i) {
  while (i >= 360) i -= 360;
  while (i < 0) i += 360;
  return m->tcos[i];
}

float nfm_medium_sin(const NfmMedium* m, int i) {
  while (i >= 360) i -= 360;
  while (i < 0) i += 360;
  return m->tsin[i];
}
