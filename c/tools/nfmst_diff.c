#include "nfm/state_recorder.h"

#include <stdio.h>

int main(int argc, char** argv) {
  NfmstDiff d;
  if (argc != 3) {
    fprintf(stderr, "usage: nfmst_diff a.nfmst b.nfmst\n");
    return 2;
  }
  d = nfm_diff_nfmst(argv[1], argv[2]);
  if (d.equal) {
    printf("equal\n");
    return 0;
  }
  printf("DIFF frame=%d player=%d field=%s %s\n", d.frame, d.player, d.field,
         d.detail);
  return 1;
}
