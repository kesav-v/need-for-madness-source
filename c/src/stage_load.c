#include "nfm/stage_load.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int getint(const char* key, const char* line, int idx) {
  size_t klen = strlen(key);
  size_t start;
  int n2 = 0;
  char cur[64];
  size_t curlen = 0;
  size_t i, linelen;

  /* Match C++ stage_load getint: both branches use key.size()+1. */
  if (strncmp(line, key, klen) == 0 && line[klen] == '(')
    start = klen + 1;
  else if (strncmp(line, key, klen) == 0)
    start = klen + 1;
  else
    return 0;

  linelen = strlen(line);
  for (i = start; i < linelen; ++i) {
    char ch = line[i];
    if (ch == ',' || ch == ')') {
      cur[curlen] = '\0';
      if (n2 == idx) return atoi(cur);
      ++n2;
      curlen = 0;
      continue;
    }
    if (n2 == idx && curlen + 1 < sizeof(cur)) cur[curlen++] = ch;
  }
  cur[curlen] = '\0';
  if (n2 == idx && curlen > 0) return atoi(cur);
  return 0;
}

static void trim_crlf(char* s) {
  size_t n;
  if (!s) return;
  n = strlen(s);
  while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
}

static void set_err(char* err, size_t errlen, const char* msg) {
  if (err && errlen) snprintf(err, errlen, "%s", msg);
}

int nfm_load_stage(const char* stage_path, int stage_id, NfmContO* bco,
                   NfmContO* world, int nplayers, const int* sc, NfmMedium* m,
                   NfmTrackers* t, NfmCheckPoints* cp, NfmStageLoadResult* out,
                   char* err, size_t errlen) {
  static const int kXstart[8] = {0, -350, 350, 0, -350, 350, 0, 0};
  static const int kZstart[8] = {-760, -380, -380, 0, 380, 380, 760, 0};
  FILE* in;
  char line[2048];
  int nob;
  int maxr_x = 0, maxl_x = 100, maxt_z = 0, maxb_z = 100;
  int i;

  if (!stage_path || !bco || !world || !sc || !m || !t || !cp) return 0;

  t->nt = 0;
  nob = nplayers;
  cp->n = 0;
  cp->nsp = 0;
  cp->fn = 0;
  cp->stage = stage_id;
  cp->haltall = false;
  cp->wasted = 0;
  cp->catchfin = 0;
  cp->nfix = 0;
  cp->nlaps = 0;
  cp->notb = false;
  m->ground = 250;
  m->resdown = 0;

  in = fopen(stage_path, "r");
  if (!in) {
    if (err && errlen)
      snprintf(err, errlen, "cannot open %s", stage_path);
    return 0;
  }

  while (fgets(line, sizeof(line), in)) {
    trim_crlf(line);
    if (line[0] == '\0') continue;

    if (strncmp(line, "set", 3) == 0) {
      int id = getint("set", line, 0);
      if (id >= 65 && id <= 75 && cp->notb) continue;
      id += 46;
      if (id < 0 || id >= 124) continue;
      if (nfm_conto_place(&world[nob], &bco[id], getint("set", line, 1),
                          m->ground - bco[id].grat, getint("set", line, 2),
                          getint("set", line, 3), m, t, true) != 0) {
        fclose(in);
        set_err(err, errlen, "place failed");
        return 0;
      }
      if (strstr(line, ")p")) {
        cp->x[cp->n] = getint("set", line, 1);
        cp->z[cp->n] = getint("set", line, 2);
        cp->y[cp->n] = 0;
        cp->typ[cp->n] = 0;
        if (strstr(line, ")pt")) cp->typ[cp->n] = -1;
        if (strstr(line, ")pr")) cp->typ[cp->n] = -2;
        if (strstr(line, ")po")) cp->typ[cp->n] = -3;
        if (strstr(line, ")ph")) cp->typ[cp->n] = -4;
        ++cp->n;
      }
      ++nob;
      continue;
    }
    if (strncmp(line, "chk", 3) == 0) {
      int id = getint("chk", line, 0) + 46;
      int gy = m->ground - bco[id].grat;
      if (id == 110) gy = getint("chk", line, 4);
      if (nfm_conto_place(&world[nob], &bco[id], getint("chk", line, 1), gy,
                          getint("chk", line, 2), getint("chk", line, 3), m, t,
                          true) != 0) {
        fclose(in);
        set_err(err, errlen, "chk place failed");
        return 0;
      }
      cp->x[cp->n] = getint("chk", line, 1);
      cp->z[cp->n] = getint("chk", line, 2);
      cp->y[cp->n] = gy;
      cp->typ[cp->n] = (getint("chk", line, 3) == 0) ? 1 : 2;
      cp->pcs = cp->n;
      ++cp->n;
      world[nob].checkpoint = cp->nsp + 1;
      ++cp->nsp;
      ++nob;
      continue;
    }
    if (cp->nfix != 5 && strncmp(line, "fix", 3) == 0) {
      int id = getint("fix", line, 0) + 46;
      if (nfm_conto_place(&world[nob], &bco[id], getint("fix", line, 1),
                          getint("fix", line, 3), getint("fix", line, 2),
                          getint("fix", line, 4), m, t, true) != 0) {
        fclose(in);
        set_err(err, errlen, "fix place failed");
        return 0;
      }
      cp->fx[cp->fn] = getint("fix", line, 1);
      cp->fz[cp->fn] = getint("fix", line, 2);
      cp->fy[cp->fn] = getint("fix", line, 3);
      world[nob].elec = true;
      cp->roted[cp->fn] = getint("fix", line, 4) != 0;
      world[nob].roted = cp->roted[cp->fn];
      cp->special[cp->fn] = strstr(line, ")s") != NULL;
      ++cp->fn;
      ++nob;
      continue;
    }
    if (strncmp(line, "pile", 4) == 0) {
      if (nfm_conto_pile(&world[nob], getint("pile", line, 0),
                         getint("pile", line, 1), getint("pile", line, 2), m, t,
                         getint("pile", line, 3), getint("pile", line, 4),
                         m->ground) != 0) {
        fclose(in);
        set_err(err, errlen, "pile failed");
        return 0;
      }
      ++nob;
      continue;
    }
    if (strncmp(line, "nlaps", 5) == 0) {
      cp->nlaps = getint("nlaps", line, 0);
      if (cp->nlaps < 1) cp->nlaps = 1;
      if (cp->nlaps > 15) cp->nlaps = 15;
      continue;
    }
    if (strncmp(line, "maxr", 4) == 0) {
      int count = getint("maxr", line, 0);
      int z0, k;
      maxr_x = getint("maxr", line, 1);
      z0 = getint("maxr", line, 2);
      for (k = 0; k < count; ++k) {
        if (nfm_conto_place(&world[nob], &bco[85], maxr_x,
                            m->ground - bco[85].grat, k * 4800 + z0, 0, m, t,
                            true) != 0) {
          fclose(in);
          set_err(err, errlen, "maxr place failed");
          return 0;
        }
        ++nob;
      }
      t->y[t->nt] = -5000;
      t->rady[t->nt] = 7100;
      t->x[t->nt] = maxr_x + 500;
      t->radx[t->nt] = 600;
      t->z[t->nt] = count * 4800 / 2 + z0 - 2400;
      t->radz[t->nt] = count * 4800 / 2;
      t->xy[t->nt] = 90;
      t->zy[t->nt] = 0;
      t->dam[t->nt] = 167;
      t->decor[t->nt] = false;
      t->skd[t->nt] = 0;
      ++t->nt;
      continue;
    }
    if (strncmp(line, "maxl", 4) == 0) {
      int count = getint("maxl", line, 0);
      int z0, k;
      maxl_x = getint("maxl", line, 1);
      z0 = getint("maxl", line, 2);
      for (k = 0; k < count; ++k) {
        if (nfm_conto_place(&world[nob], &bco[85], maxl_x,
                            m->ground - bco[85].grat, k * 4800 + z0, 180, m, t,
                            true) != 0) {
          fclose(in);
          set_err(err, errlen, "maxl place failed");
          return 0;
        }
        ++nob;
      }
      t->y[t->nt] = -5000;
      t->rady[t->nt] = 7100;
      t->x[t->nt] = maxl_x - 500;
      t->radx[t->nt] = 600;
      t->z[t->nt] = count * 4800 / 2 + z0 - 2400;
      t->radz[t->nt] = count * 4800 / 2;
      t->xy[t->nt] = -90;
      t->zy[t->nt] = 0;
      t->dam[t->nt] = 167;
      t->decor[t->nt] = false;
      t->skd[t->nt] = 0;
      ++t->nt;
      continue;
    }
    if (strncmp(line, "maxt", 4) == 0) {
      int count = getint("maxt", line, 0);
      int x0, k;
      maxt_z = getint("maxt", line, 1);
      x0 = getint("maxt", line, 2);
      for (k = 0; k < count; ++k) {
        if (nfm_conto_place(&world[nob], &bco[85], k * 4800 + x0,
                            m->ground - bco[85].grat, maxt_z, 90, m, t,
                            true) != 0) {
          fclose(in);
          set_err(err, errlen, "maxt place failed");
          return 0;
        }
        ++nob;
      }
      t->y[t->nt] = -5000;
      t->rady[t->nt] = 7100;
      t->z[t->nt] = maxt_z + 500;
      t->radz[t->nt] = 600;
      t->x[t->nt] = count * 4800 / 2 + x0 - 2400;
      t->radx[t->nt] = count * 4800 / 2;
      t->zy[t->nt] = 90;
      t->xy[t->nt] = 0;
      t->dam[t->nt] = 167;
      t->decor[t->nt] = false;
      t->skd[t->nt] = 0;
      ++t->nt;
      continue;
    }
    if (strncmp(line, "maxb", 4) == 0) {
      int count = getint("maxb", line, 0);
      int x0, k;
      maxb_z = getint("maxb", line, 1);
      x0 = getint("maxb", line, 2);
      for (k = 0; k < count; ++k) {
        if (nfm_conto_place(&world[nob], &bco[85], k * 4800 + x0,
                            m->ground - bco[85].grat, maxb_z, -90, m, t,
                            true) != 0) {
          fclose(in);
          set_err(err, errlen, "maxb place failed");
          return 0;
        }
        ++nob;
      }
      t->y[t->nt] = -5000;
      t->rady[t->nt] = 7100;
      t->z[t->nt] = maxb_z - 500;
      t->radz[t->nt] = 600;
      t->x[t->nt] = count * 4800 / 2 + x0 - 2400;
      t->radx[t->nt] = count * 4800 / 2;
      t->zy[t->nt] = -90;
      t->xy[t->nt] = 0;
      t->dam[t->nt] = 167;
      t->decor[t->nt] = false;
      t->skd[t->nt] = 0;
      ++t->nt;
      continue;
    }
  }
  fclose(in);

  nfm_trackers_devidetrackers(t, maxl_x, maxr_x - maxl_x, maxb_z,
                              maxt_z - maxb_z);

  if (cp->nsp < 2) {
    set_err(err, errlen, "nsp < 2");
    return 0;
  }

  for (i = 0; i < nplayers; ++i) {
    int cn = sc[i];
    if (nfm_conto_place(&world[i], &bco[cn], kXstart[i], 250 - bco[cn].grat,
                        kZstart[i], 0, m, t, true) != 0) {
      set_err(err, errlen, "car place failed");
      return 0;
    }
  }

  nfm_checkpoints_calprox(cp);

  if (out) {
    out->nob = nob;
    out->nlaps = cp->nlaps;
    out->nsp = cp->nsp;
    out->nfix = cp->nfix;
    out->sx = maxl_x;
    out->ex = maxr_x;
    out->sz = maxb_z;
    out->ez = maxt_z;
  }
  return 1;
}

void nfm_stage_cache_init(NfmStageCache* cache) {
  if (!cache) return;
  memset(cache, 0, sizeof(*cache));
  nfm_trackers_init(&cache->trackers);
  nfm_checkpoints_init(&cache->cp);
}

void nfm_stage_cache_free(NfmStageCache* cache) {
  int i;
  if (!cache) return;
  if (cache->world) {
    for (i = 0; i < cache->world_cap; ++i) nfm_conto_free(&cache->world[i]);
    free(cache->world);
    cache->world = NULL;
  }
  nfm_trackers_free_sect(&cache->trackers);
  cache->world_cap = 0;
  cache->nob = 0;
  cache->nplayers = 0;
}

int nfm_stage_cache_empty(const NfmStageCache* cache) {
  return !cache || cache->nob == 0;
}

static int copy_trackers(NfmTrackers* dst, const NfmTrackers* src) {
  int i, j;
  if (!dst || !src) return -1;
  nfm_trackers_free_sect(dst);
  memcpy(dst, src, sizeof(*dst));
  dst->sect = NULL;
  dst->sect_w = 0;
  dst->sect_h = 0;
  if (src->sect && src->sect_w > 0 && src->sect_h > 0) {
    dst->sect = (NfmSectList*)calloc((size_t)(src->sect_w * src->sect_h),
                                     sizeof(NfmSectList));
    if (!dst->sect) return -1;
    dst->sect_w = src->sect_w;
    dst->sect_h = src->sect_h;
    for (i = 0; i < src->sect_w * src->sect_h; ++i) {
      int n = src->sect[i].n;
      dst->sect[i].n = n;
      if (n > 0) {
        dst->sect[i].idx = (int*)malloc((size_t)n * sizeof(int));
        if (!dst->sect[i].idx) return -1;
        memcpy(dst->sect[i].idx, src->sect[i].idx, (size_t)n * sizeof(int));
      }
    }
  }
  (void)j;
  return 0;
}

void nfm_stage_cache_capture(NfmStageCache* cache, const NfmContO* world,
                             int nplayers, const int* sc, const NfmTrackers* t,
                             const NfmCheckPoints* cp,
                             const NfmStageLoadResult* slr) {
  int i;
  int cap;
  if (!cache || !world || !slr) return;

  nfm_stage_cache_free(cache);
  nfm_stage_cache_init(cache);

  cache->nplayers = nplayers;
  cache->nob = slr->nob;
  cache->slr = *slr;
  if (cp) cache->cp = *cp;
  if (t) copy_trackers(&cache->trackers, t);
  for (i = 0; i < 8; ++i) cache->sc[i] = sc ? sc[i] : 0;

  cap = nplayers;
  cache->world_cap = cap;
  cache->world = (NfmContO*)calloc((size_t)cap, sizeof(NfmContO));
  if (!cache->world) {
    cache->world_cap = 0;
    cache->nob = 0;
    return;
  }
  for (i = 0; i < cap; ++i) {
    nfm_conto_init_empty(&cache->world[i]);
    if (nfm_conto_copy(&cache->world[i], &world[i]) != 0) {
      nfm_stage_cache_free(cache);
      return;
    }
  }
}

int nfm_stage_cache_apply(const NfmStageCache* cache, NfmContO* world,
                          const int* sc, NfmMedium* m, NfmTrackers* t,
                          NfmCheckPoints* cp) {
  int i;
  if (nfm_stage_cache_empty(cache) || !world || !sc || !m || !t || !cp)
    return 0;
  for (i = 0; i < cache->nplayers; ++i) {
    if (sc[i] != cache->sc[i]) return 0;
  }

  *cp = cache->cp;
  m->ground = 250;
  m->resdown = 0;

  /* Cars only — stage meshes are unused after trackers are built. */
  for (i = 0; i < cache->nplayers; ++i) {
    if (nfm_conto_copy(&world[i], &cache->world[i]) != 0) return 0;
    world[i].m = m;
    world[i].t = t;
  }
  return 1;
}
