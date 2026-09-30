#ifndef NFM_STATE_RECORDER_H
#define NFM_STATE_RECORDER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nfm/conto.h"
#include "nfm/mad.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { NFM_NFMS_VERSION = 1 };

typedef struct NfmStateRecorder {
  FILE* out;
  int nplayers;
  int frames;
  bool finished;
} NfmStateRecorder;

/** Open NFMS v1 dump (big-endian). Returns 0 on success. */
int nfm_state_recorder_open(NfmStateRecorder* r, const char* path, int stage,
                            int focus_car, int nplayers, int nlaps, int nsp,
                            const int* sc);
void nfm_state_recorder_capture(NfmStateRecorder* r, const NfmContO* cars,
                                const NfmMad* mad);
void nfm_state_recorder_finish(NfmStateRecorder* r);

typedef struct NfmstDiff {
  bool equal;
  int frame;
  int player;
  char field[64];
  char detail[128];
} NfmstDiff;

NfmstDiff nfm_diff_nfmst(const char* path_a, const char* path_b);

#ifdef __cplusplus
}
#endif

#endif /* NFM_STATE_RECORDER_H */
