#ifndef NFM_SORT_CARS_H
#define NFM_SORT_CARS_H

#include "nfm/medium.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Port of xtGraphics.sortcars for autorace (gmode=0). Mutates sc[1..6]. */
void nfm_sort_cars(NfmMedium* m, int* sc, int stage, int nplayers);

#ifdef __cplusplus
}
#endif

#endif /* NFM_SORT_CARS_H */
