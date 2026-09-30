#ifndef NFM_JAVA_RANDOM_H
#define NFM_JAVA_RANDOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Exact java.util.Random (48-bit LCG) for --seed bit-identity. */
typedef struct NfmJavaRandom {
  int64_t seed;
} NfmJavaRandom;

static inline void nfm_java_random_set_seed(NfmJavaRandom* r, int64_t seed) {
  r->seed = (seed ^ 0x5DEECE66DLL) & ((1LL << 48) - 1);
}

static inline void nfm_java_random_init(NfmJavaRandom* r, int64_t seed) {
  nfm_java_random_set_seed(r, seed);
}

static inline int32_t nfm_java_random_next(NfmJavaRandom* r, int32_t bits) {
  r->seed = (r->seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
  return (int32_t)(r->seed >> (48 - bits));
}

static inline int32_t nfm_java_random_next_int(NfmJavaRandom* r) {
  return nfm_java_random_next(r, 32);
}

static inline int32_t nfm_java_random_next_int_bound(NfmJavaRandom* r,
                                                    int32_t bound) {
  if (bound <= 0) return 0;
  if ((bound & -bound) == bound) {
    return (int32_t)((bound * (int64_t)nfm_java_random_next(r, 31)) >> 31);
  }
  int32_t bits, val;
  do {
    bits = nfm_java_random_next(r, 31);
    val = bits % bound;
  } while (bits - val + (bound - 1) < 0);
  return val;
}

static inline double nfm_java_random_next_double(NfmJavaRandom* r) {
  return (((int64_t)nfm_java_random_next(r, 26) << 27) +
          nfm_java_random_next(r, 27)) /
         (double)(1LL << 53);
}

static inline float nfm_java_random_next_float(NfmJavaRandom* r) {
  return nfm_java_random_next(r, 24) / (float)(1 << 24);
}

#ifdef __cplusplus
}
#endif

#endif /* NFM_JAVA_RANDOM_H */
