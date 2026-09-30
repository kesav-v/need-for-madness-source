#include "nfm/car_define.h"

#include <string.h>

void nfm_car_define_init(NfmCarDefine* cd) {
  int i, j;
  static const int k_swits[][3] = {
      {50, 185, 282},   {100, 200, 310}, {60, 180, 275},  {76, 195, 298},
      {70, 170, 275},   {70, 202, 293},  {60, 170, 289},  {70, 206, 291},
      {90, 210, 295},   {90, 190, 276},  {70, 200, 295},  {50, 160, 270},
      {90, 200, 305},   {50, 130, 210},  {80, 200, 300},  {70, 210, 290}};
  static const float k_acelf[][3] = {
      {11.0f, 5.0f, 3.0f}, {14.0f, 7.0f, 5.0f}, {10.0f, 5.0f, 3.5f},
      {11.0f, 6.0f, 3.5f}, {10.0f, 5.0f, 3.5f}, {12.0f, 6.0f, 3.0f},
      {7.0f, 9.0f, 4.0f},  {11.0f, 5.0f, 3.0f}, {12.0f, 7.0f, 4.0f},
      {12.0f, 7.0f, 3.5f}, {11.5f, 6.5f, 3.5f}, {9.0f, 5.0f, 3.0f},
      {13.0f, 7.0f, 4.5f}, {7.5f, 3.5f, 3.0f},  {11.0f, 7.5f, 4.0f},
      {12.0f, 6.0f, 3.5f}};
  static const int k_handb[] = {7, 10, 7, 15, 12, 8, 9, 10,
                                5, 7,  8, 10, 8,  12, 7, 7};
  static const float k_airs[] = {1.0f, 1.2f, 0.95f, 1.0f, 2.2f, 1.0f, 0.9f, 0.8f,
                                 1.0f, 0.9f, 1.15f, 0.8f, 1.0f, 0.3f, 1.3f, 1.0f};
  static const int k_airc[] = {70, 30, 40, 40, 30, 50, 40, 90,
                               40, 50, 75, 10, 50, 0,  100, 60};
  static const int k_turn[] = {6, 9, 5, 7, 8, 7, 5, 5, 9, 7, 7, 4, 6, 5, 7, 6};
  static const float k_grip[] = {20.0f, 27.0f, 18.0f, 22.0f, 19.0f, 20.0f,
                                 25.0f, 20.0f, 19.0f, 24.0f, 22.5f, 25.0f,
                                 30.0f, 27.0f, 25.0f, 27.0f};
  static const float k_bounce[] = {1.2f, 1.05f, 1.3f, 1.15f, 1.3f, 1.2f,
                                   1.15f, 1.1f, 1.2f, 1.1f, 1.15f, 0.8f,
                                   1.05f, 0.8f, 1.1f, 1.15f};
  static const float k_simag[] = {0.9f, 0.85f, 1.05f, 0.9f, 0.85f, 0.9f,
                                  1.05f, 0.9f, 1.0f, 1.05f, 0.9f, 1.1f,
                                  0.9f, 1.3f, 0.9f, 1.15f};
  static const float k_moment[] = {1.3f, 0.75f, 1.4f, 1.2f, 1.1f, 1.38f,
                                   1.43f, 1.48f, 1.35f, 1.7f, 1.42f, 2.0f,
                                   1.26f, 3.0f, 1.5f, 2.0f};
  static const float k_comprad[] = {0.5f, 0.4f, 0.8f, 0.5f, 0.4f, 0.5f,
                                    0.5f, 0.5f, 0.5f, 0.8f, 0.5f, 1.5f,
                                    0.5f, 0.8f, 0.5f, 0.8f};
  static const int k_push[] = {2, 2, 3, 3, 2, 2, 2, 4, 2, 2, 2, 4, 2, 2, 2, 2};
  static const int k_revpush[] = {2, 3, 2, 2, 2, 2, 2, 1,
                                  2, 1, 2, 1, 2, 2, 2, 1};
  static const int k_lift[] = {0, 30, 0, 20, 0, 30, 0, 0,
                               20, 0, 0, 0, 10, 0, 30, 0};
  static const int k_revlift[] = {0, 0, 15, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 32};
  static const int k_powerloss[] = {
      2500000, 2500000, 3500000, 2500000, 4000000, 2500000, 3200000, 3200000,
      2750000, 5500000, 2750000, 4500000, 3500000, 16700000, 3000000, 5500000};
  static const int k_flipy[] = {-50, -60, -92, -44, -60, -57, -54, -60,
                                -77, -57, -82, -85, -28, -100, -63, -127};
  static const int k_msquash[] = {7, 4, 7, 2, 8, 4, 6, 4,
                                  3, 8, 4, 10, 3, 20, 3, 8};
  static const int k_clrad[] = {3300, 1700, 4700, 3000, 2000, 4500, 3500, 5000,
                                10000, 15000, 4000, 7000, 10000, 15000, 5500, 5000};
  static const float k_dammult[] = {0.75f, 0.8f, 0.45f, 0.8f, 0.42f, 0.7f,
                                    0.72f, 0.6f, 0.58f, 0.41f, 0.67f, 0.45f,
                                    0.61f, 0.25f, 0.38f, 0.52f};
  static const int k_maxmag[] = {7600, 4200, 7200, 6000, 6000, 15000, 17200, 17000,
                                 18000, 11000, 19000, 10700, 13000, 45000, 5800, 18000};
  static const float k_dishandle[] = {0.65f, 0.6f, 0.55f, 0.77f, 0.62f, 0.9f,
                                      0.6f, 0.72f, 0.45f, 0.8f, 0.95f, 0.4f,
                                      0.87f, 0.42f, 1.0f, 0.95f};
  static const float k_outdam[] = {0.68f, 0.35f, 0.8f, 0.5f, 0.42f, 0.76f,
                                   0.82f, 0.76f, 0.72f, 0.62f, 0.79f, 0.95f,
                                   0.77f, 1.0f, 0.85f, 1.0f};
  static const int k_cclass[] = {0, 0, 0, 0, 0, 1, 2, 2,
                                 2, 2, 3, 4, 4, 4, 4, 4};
  static const int k_eng[] = {0, 1, 2, 1, 0, 3, 2, 2,
                              1, 0, 3, 4, 1, 4, 0, 3};
  static const char* knames[] = {
      "Tornado Shark", "Formula 7",        "Wow Caninaro",  "La Vita Crab",
      "Nimi",          "MAX Revenge",      "Lead Oxide",    "Kool Kat",
      "Drifter X",     "Sword of Justice", "High Rider",    "EL KING",
      "Mighty Eight",  "M A S H E E N",    "Radical One",   "DR Monstaa"};

  if (!cd) return;
  memset(cd, 0, sizeof(*cd));

  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 3; ++j) {
      cd->swits[i][j] = k_swits[i][j];
      cd->acelf[i][j] = k_acelf[i][j];
    }
    cd->handb[i] = k_handb[i];
    cd->airs[i] = k_airs[i];
    cd->airc[i] = k_airc[i];
    cd->turn[i] = k_turn[i];
    cd->grip[i] = k_grip[i];
    cd->bounce[i] = k_bounce[i];
    cd->simag[i] = k_simag[i];
    cd->moment[i] = k_moment[i];
    cd->comprad[i] = k_comprad[i];
    cd->push[i] = k_push[i];
    cd->revpush[i] = k_revpush[i];
    cd->lift[i] = k_lift[i];
    cd->revlift[i] = k_revlift[i];
    cd->powerloss[i] = k_powerloss[i];
    cd->flipy[i] = k_flipy[i];
    cd->msquash[i] = k_msquash[i];
    cd->clrad[i] = k_clrad[i];
    cd->dammult[i] = k_dammult[i];
    cd->maxmag[i] = k_maxmag[i];
    cd->dishandle[i] = k_dishandle[i];
    cd->outdam[i] = k_outdam[i];
    cd->cclass[i] = k_cclass[i];
    cd->enginsignature[i] = k_eng[i];
    strncpy(cd->names[i], knames[i], sizeof(cd->names[i]) - 1);
  }
}
