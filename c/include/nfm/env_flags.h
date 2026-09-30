#ifndef NFM_ENV_FLAGS_H
#define NFM_ENV_FLAGS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Cached getenv diagnostics — call nfm_env_flags_init() once at startup.
 *  Hot paths must not call getenv (it takes a process-global lock). */
typedef struct NfmEnvFlags {
  int inited;
  int rngtrace;
  int rngval;
  int dmglog;
  int ylog;
  int track;
  int drvrng;
  int skiddust;
  int acos;
  int bnc;
  int pzy;
  int dustlog;
} NfmEnvFlags;

extern NfmEnvFlags nfm_env;

void nfm_env_flags_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NFM_ENV_FLAGS_H */
