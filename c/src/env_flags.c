#include "nfm/env_flags.h"

#include <stdlib.h>

NfmEnvFlags nfm_env;

void nfm_env_flags_init(void) {
  if (nfm_env.inited) return;
  nfm_env.rngtrace = getenv("NFM_RNGTRACE") != NULL;
  nfm_env.rngval = getenv("NFM_RNGVAL") != NULL;
  nfm_env.dmglog = getenv("NFM_DMGLOG") != NULL;
  nfm_env.ylog = getenv("NFM_YLOG") != NULL;
  nfm_env.track = getenv("NFM_TRACK") != NULL;
  nfm_env.drvrng = getenv("NFM_DRVRNG") != NULL;
  nfm_env.skiddust = getenv("NFM_SKIDDUST") != NULL;
  nfm_env.acos = getenv("NFM_ACOS") != NULL;
  nfm_env.bnc = getenv("NFM_BNC") != NULL;
  nfm_env.pzy = getenv("NFM_PZY") != NULL;
  nfm_env.dustlog = getenv("NFM_DUSTLOG") != NULL;
  nfm_env.inited = 1;
}
