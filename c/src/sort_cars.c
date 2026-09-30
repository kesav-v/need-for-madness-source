#include "nfm/sort_cars.h"

#include <stdbool.h>

void nfm_sort_cars(NfmMedium* m, int* sc, int stage, int nplayers) {
  bool array[7] = {false};
  int n, n2, n4, n5, n6, j;
  bool b = false;

  if (!m || !sc) return;
  if (stage == 0) return;

  for (j = 1; j < 7; ++j) sc[j] = -1;

  n = stage;
  if (n < 0) n = 27;
  n2 = 7;
  if (nplayers < 7) n2 = nplayers;

  if (n <= 10) {
    int n3 = 6;
    if ((n == 1 || n == 2) && sc[0] != 5) {
      sc[n3] = 5;
      n2 = n3;
    }
    if ((n == 3 || n == 4) && sc[0] != 6) {
      sc[n3] = 6;
      n2 = n3;
    }
    if ((n == 5 || n == 6) && sc[0] != 11) {
      sc[n3] = 11;
      n2 = n3;
    }
    if ((n == 7 || n == 8) && sc[0] != 14) {
      sc[n3] = 14;
      n2 = n3;
    }
    if ((n == 9 || n == 10) && sc[0] != 15) {
      sc[n3] = 15;
      n2 = n3;
    }
  } else {
    n -= 10;
    b = true;
    if (sc[0] != 7 + (n + 1) / 2 && n != 17) {
      sc[6] = 7 + (n + 1) / 2;
      n2 = 6;
    }
  }

  n4 = 16;
  n5 = 1;
  n6 = 2;
  for (j = 1; j < n2; ++j) {
    array[j] = false;
    while (!array[j]) {
      float n7 = 10.0f;
      float n9;
      int k;
      if (b) n7 = 17.0f;
      sc[j] = (int)(nfm_medium_next_unit(m) * (24.0f + 8.0f * (n / n7)));
      if (sc[j] >= 16) sc[j] -= 16;
      array[j] = true;
      for (k = 0; k < 7; ++k) {
        if (j != k && sc[j] == sc[k]) array[j] = false;
      }
      if (b) n7 = 16.0f;
      n9 = (15 - sc[j]) / 15.0f * (n / n7);
      if (n9 > 0.8f) n9 = 0.8f;
      if (n == 17 && n9 > 0.5f) n9 = 0.5f;
      if (n9 > nfm_medium_next_unit(m)) array[j] = false;
    }
    if (sc[j] < n4) {
      n4 = sc[j];
      if (n5 != j) {
        n6 = n5;
        n5 = j;
      }
    }
  }

  /* force_if_missing: gmode==0 so needs nextUnit>nextUnit when !always */
#define FORCE_IF_MISSING(car_id, slot, always)                         \
  do {                                                                 \
    bool found = false;                                                \
    int ii;                                                            \
    for (ii = 0; ii < 7; ++ii)                                         \
      if (sc[ii] == (car_id)) found = true;                            \
    if (!found &&                                                      \
        ((always) ||                                                   \
         nfm_medium_next_unit(m) > nfm_medium_next_unit(m)))           \
      sc[(slot)] = (car_id);                                           \
  } while (0)

  if (!b && n == 10) {
    FORCE_IF_MISSING(11, n5, false);
    FORCE_IF_MISSING(14, n6, false);
  }
  if (n == 12) {
    bool found = false;
    int i;
    for (i = 0; i < 7; ++i)
      if (sc[i] == 11) found = true;
    if (!found) sc[n5] = 11;
  }
  if (n == 14) {
    FORCE_IF_MISSING(12, n5, false);
    FORCE_IF_MISSING(10, n6, false);
  }
  if (n == 15) {
    FORCE_IF_MISSING(11, n5, false);
    FORCE_IF_MISSING(13, n6, false);
  }
  if (n == 16) {
    FORCE_IF_MISSING(13, n5, false);
    FORCE_IF_MISSING(12, n6, false);
  }
#undef FORCE_IF_MISSING
  (void)nplayers;
}
