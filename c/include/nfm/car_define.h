#ifndef NFM_CAR_DEFINE_H
#define NFM_CAR_DEFINE_H

#ifdef __cplusplus
extern "C" {
#endif

enum { NFM_MAX_CARS = 56, NFM_BUILTIN_CARS = 16 };

typedef struct NfmCarDefine {
  int swits[NFM_MAX_CARS][3];
  float acelf[NFM_MAX_CARS][3];
  int handb[NFM_MAX_CARS];
  float airs[NFM_MAX_CARS];
  int airc[NFM_MAX_CARS];
  int turn[NFM_MAX_CARS];
  float grip[NFM_MAX_CARS];
  float bounce[NFM_MAX_CARS];
  float simag[NFM_MAX_CARS];
  float moment[NFM_MAX_CARS];
  float comprad[NFM_MAX_CARS];
  int push[NFM_MAX_CARS];
  int revpush[NFM_MAX_CARS];
  int lift[NFM_MAX_CARS];
  int revlift[NFM_MAX_CARS];
  int powerloss[NFM_MAX_CARS];
  int flipy[NFM_MAX_CARS];
  int msquash[NFM_MAX_CARS];
  int clrad[NFM_MAX_CARS];
  float dammult[NFM_MAX_CARS];
  int maxmag[NFM_MAX_CARS];
  float dishandle[NFM_MAX_CARS];
  float outdam[NFM_MAX_CARS];
  int cclass[NFM_MAX_CARS];
  char names[NFM_MAX_CARS][64];
  int enginsignature[NFM_MAX_CARS];
} NfmCarDefine;

void nfm_car_define_init(NfmCarDefine* cd);

#ifdef __cplusplus
}
#endif

#endif /* NFM_CAR_DEFINE_H */
