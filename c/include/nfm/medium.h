#ifndef NFM_MEDIUM_H
#define NFM_MEDIUM_H

#include <stdbool.h>
#include <stdint.h>

#include "nfm/java_random.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Sim-relevant Medium: seeded RNG + lookup sin/cos (Java Medium subset). */
typedef struct NfmMedium {
  uint64_t rng_calls;
  const char* trace_label;
  int mgen;
  int resdown;
  int checkpoint;
  bool lastcheck;
  int ground;
  int noelec;

  NfmJavaRandom* rng; /* points at rng_storage when seeded */
  NfmJavaRandom rng_storage;
  bool use_rng;

  int cntrn;
  bool diup[3];
  int rand_[3];
  int trn;
  float tcos[360];
  float tsin[360];
} NfmMedium;

void nfm_medium_init(NfmMedium* m, bool seeded, int64_t seed);
/** Reset RNG + race fields in place (keeps sin/cos; ContO m pointers stay valid). */
void nfm_medium_reseed(NfmMedium* m, int64_t seed);
double nfm_medium_next_unit(NfmMedium* m);
float nfm_medium_random(NfmMedium* m);
float nfm_medium_cos(const NfmMedium* m, int i);
float nfm_medium_sin(const NfmMedium* m, int i);

#ifdef __cplusplus
}
#endif

#endif /* NFM_MEDIUM_H */
