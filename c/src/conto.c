#include "nfm/conto.h"
#include "nfm/env_flags.h"

#include "nfm/java_random.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- helpers ---------------------------------------------------------- */

static void trim_inplace(char* s) {
  size_t n;
  if (!s) return;
  n = strlen(s);
  while (n > 0 &&
         (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) {
    s[--n] = '\0';
  }
}

static int getvalue(const char* key, const char* line, int idx) {
  size_t klen = strlen(key);
  size_t start = 0;
  int n2 = 0;
  char cur[64];
  size_t curlen = 0;
  size_t i;
  size_t linelen;

  if (strncmp(line, key, klen) == 0 && line[klen] == '(')
    start = klen + 1;
  else if (strncmp(line, key, klen) == 0)
    start = klen;
  else
    return 0;

  linelen = strlen(line);
  cur[0] = '\0';
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

static void rot2(NfmMedium* med, int* x, int* z, int cx, int cz, int ang) {
  NfmMedium tmp;
  NfmMedium* m = med;
  float c, s;
  int dx, dz;
  if (!m) {
    nfm_medium_init(&tmp, false, 0);
    m = &tmp;
  }
  c = nfm_medium_cos(m, ang);
  s = nfm_medium_sin(m, ang);
  dx = *x - cx;
  dz = *z - cz;
  *x = cx + (int)(dx * c - dz * s);
  *z = cz + (int)(dx * s + dz * c);
}

static int* realloc_ints(int* p, int old_n, int new_n) {
  int* q = (int*)realloc(p, (size_t)new_n * sizeof(int));
  if (!q) return NULL;
  if (new_n > old_n)
    memset(q + old_n, 0, (size_t)(new_n - old_n) * sizeof(int));
  return q;
}

static bool* realloc_bools(bool* p, int old_n, int new_n) {
  bool* q = (bool*)realloc(p, (size_t)new_n * sizeof(bool));
  if (!q) return NULL;
  if (new_n > old_n)
    memset(q + old_n, 0, (size_t)(new_n - old_n) * sizeof(bool));
  return q;
}

static void free_plane(NfmPlane* pl) {
  if (!pl) return;
  free(pl->ox);
  free(pl->oy);
  free(pl->oz);
  pl->ox = pl->oy = pl->oz = NULL;
  pl->n = 0;
}

static void free_planes(NfmContO* c) {
  int i;
  if (!c || !c->p) return;
  for (i = 0; i < c->npl; ++i) free_plane(&c->p[i]);
  free(c->p);
  c->p = NULL;
  c->npl = 0;
}

void nfm_conto_init_empty(NfmContO* c) {
  if (!c) return;
  memset(c, 0, sizeof(*c));
  c->grounded = 1.0f;
  c->fix = -1;
}

void nfm_conto_free(NfmContO* c) {
  if (!c) return;
  free_planes(c);
  free(c->ox);
  free(c->oy);
  free(c->oz);
  free(c->txy);
  free(c->tzy);
  free(c->tradx);
  free(c->tradz);
  free(c->trady);
  free(c->tx);
  free(c->ty);
  free(c->tz);
  free(c->skd);
  free(c->dam);
  free(c->notwall);
  free(c->tc0);
  free(c->tc1);
  free(c->tc2);
  memset(c, 0, sizeof(*c));
  c->grounded = 1.0f;
  c->fix = -1;
}

void nfm_conto_clear(NfmContO* c) { nfm_conto_free(c); }

void nfm_conto_clear_tracks_template(NfmContO* c) {
  if (!c) return;
  c->tnt = 0;
  free(c->txy);
  c->txy = NULL;
  /* Match C++: only clears txy vector; leave other track arrays alone. */
}

static int dup_int_array(int** dst, const int* src, int n) {
  if (n <= 0) {
    *dst = NULL;
    return 0;
  }
  *dst = (int*)malloc((size_t)n * sizeof(int));
  if (!*dst) return -1;
  memcpy(*dst, src, (size_t)n * sizeof(int));
  return 0;
}

static int dup_bool_array(bool** dst, const bool* src, int n) {
  if (n <= 0) {
    *dst = NULL;
    return 0;
  }
  *dst = (bool*)malloc((size_t)n * sizeof(bool));
  if (!*dst) return -1;
  memcpy(*dst, src, (size_t)n * sizeof(bool));
  return 0;
}

static int copy_plane(NfmPlane* dst, const NfmPlane* src) {
  memset(dst, 0, sizeof(*dst));
  *dst = *src;
  dst->ox = dst->oy = dst->oz = NULL;
  if (dup_int_array(&dst->ox, src->ox, src->n) ||
      dup_int_array(&dst->oy, src->oy, src->n) ||
      dup_int_array(&dst->oz, src->oz, src->n)) {
    free_plane(dst);
    return -1;
  }
  return 0;
}

static int conto_shape_matches(const NfmContO* dst, const NfmContO* src) {
  int i;
  if (!dst || !src) return 0;
  if (dst->npl != src->npl || dst->npts != src->npts || dst->tnt != src->tnt)
    return 0;
  if (src->npl > 0 && src->p) {
    if (!dst->p) return 0;
    for (i = 0; i < src->npl; ++i) {
      if (!dst->p[i].ox || !dst->p[i].oy || !dst->p[i].oz) return 0;
      if (dst->p[i].n != src->p[i].n) return 0;
    }
  } else if (dst->p) {
    return 0;
  }
  if (src->npts > 0 && (!dst->ox || !dst->oy || !dst->oz)) return 0;
  if (src->tnt > 0 &&
      (!dst->txy || !dst->tzy || !dst->tradx || !dst->tradz || !dst->trady ||
       !dst->tx || !dst->ty || !dst->tz || !dst->skd || !dst->dam ||
       !dst->notwall || !dst->tc0 || !dst->tc1 || !dst->tc2))
    return 0;
  return 1;
}

/** Bit-identical ContO restore; reuses dst heap buffers when shapes match. */
int nfm_conto_copy(NfmContO* dst, const NfmContO* src) {
  int i;
  NfmContO tmp;
  NfmMedium* keep_m;
  NfmTrackers* keep_t;
  if (!dst || !src) return -1;
  if (dst == src) return 0;

  /* Fast path: overwrite in place — no free/malloc (arena-friendly). */
  if (conto_shape_matches(dst, src)) {
    keep_m = src->m;
    keep_t = src->t;
    dst->m = keep_m;
    dst->t = keep_t;
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
    dst->xz = src->xz;
    dst->xy = src->xy;
    dst->zy = src->zy;
    dst->wxz = src->wxz;
    dst->wzy = src->wzy;
    dst->grat = src->grat;
    memcpy(dst->keyx, src->keyx, sizeof(dst->keyx));
    memcpy(dst->keyz, src->keyz, sizeof(dst->keyz));
    dst->maxR = src->maxR;
    dst->grounded = src->grounded;
    dst->decor = src->decor;
    dst->shadow = src->shadow;
    dst->dist = src->dist;
    dst->fix = src->fix;
    dst->fcnt = src->fcnt;
    dst->checkpoint = src->checkpoint;
    dst->elec = src->elec;
    dst->roted = src->roted;
    dst->ust = src->ust;
    for (i = 0; i < src->npl; ++i) {
      NfmPlane* dp = &dst->p[i];
      const NfmPlane* sp = &src->p[i];
      int* ox = dp->ox;
      int* oy = dp->oy;
      int* oz = dp->oz;
      int n = dp->n;
      *dp = *sp;
      dp->ox = ox;
      dp->oy = oy;
      dp->oz = oz;
      dp->n = n;
      if (n > 0) {
        memcpy(dp->ox, sp->ox, (size_t)n * sizeof(int));
        memcpy(dp->oy, sp->oy, (size_t)n * sizeof(int));
        memcpy(dp->oz, sp->oz, (size_t)n * sizeof(int));
      }
    }
    if (src->npts > 0) {
      memcpy(dst->ox, src->ox, (size_t)src->npts * sizeof(int));
      memcpy(dst->oy, src->oy, (size_t)src->npts * sizeof(int));
      memcpy(dst->oz, src->oz, (size_t)src->npts * sizeof(int));
    }
    if (src->tnt > 0) {
      int tcap = src->tnt;
      memcpy(dst->txy, src->txy, (size_t)tcap * sizeof(int));
      memcpy(dst->tzy, src->tzy, (size_t)tcap * sizeof(int));
      memcpy(dst->tradx, src->tradx, (size_t)tcap * sizeof(int));
      memcpy(dst->tradz, src->tradz, (size_t)tcap * sizeof(int));
      memcpy(dst->trady, src->trady, (size_t)tcap * sizeof(int));
      memcpy(dst->tx, src->tx, (size_t)tcap * sizeof(int));
      memcpy(dst->ty, src->ty, (size_t)tcap * sizeof(int));
      memcpy(dst->tz, src->tz, (size_t)tcap * sizeof(int));
      memcpy(dst->skd, src->skd, (size_t)tcap * sizeof(int));
      memcpy(dst->dam, src->dam, (size_t)tcap * sizeof(int));
      memcpy(dst->notwall, src->notwall, (size_t)tcap * sizeof(bool));
      memcpy(dst->tc0, src->tc0, (size_t)tcap * sizeof(int));
      memcpy(dst->tc1, src->tc1, (size_t)tcap * sizeof(int));
      memcpy(dst->tc2, src->tc2, (size_t)tcap * sizeof(int));
    }
    return 0;
  }

  memset(&tmp, 0, sizeof(tmp));
  tmp.m = src->m;
  tmp.t = src->t;
  tmp.x = src->x;
  tmp.y = src->y;
  tmp.z = src->z;
  tmp.xz = src->xz;
  tmp.xy = src->xy;
  tmp.zy = src->zy;
  tmp.wxz = src->wxz;
  tmp.wzy = src->wzy;
  tmp.grat = src->grat;
  memcpy(tmp.keyx, src->keyx, sizeof(tmp.keyx));
  memcpy(tmp.keyz, src->keyz, sizeof(tmp.keyz));
  tmp.maxR = src->maxR;
  tmp.npl = src->npl;
  tmp.grounded = src->grounded;
  tmp.decor = src->decor;
  tmp.shadow = src->shadow;
  tmp.dist = src->dist;
  tmp.fix = src->fix;
  tmp.fcnt = src->fcnt;
  tmp.checkpoint = src->checkpoint;
  tmp.elec = src->elec;
  tmp.roted = src->roted;
  tmp.ust = src->ust;
  tmp.npts = src->npts;
  tmp.tnt = src->tnt;

  if (src->npl > 0 && src->p) {
    tmp.p = (NfmPlane*)calloc((size_t)src->npl, sizeof(NfmPlane));
    if (!tmp.p) goto fail;
    for (i = 0; i < src->npl; ++i) {
      if (copy_plane(&tmp.p[i], &src->p[i])) goto fail;
    }
  } else {
    /* Pile ContOs set npl=5 with no planes (match C++ empty p vector). */
    tmp.p = NULL;
  }
  if (dup_int_array(&tmp.ox, src->ox, src->npts) ||
      dup_int_array(&tmp.oy, src->oy, src->npts) ||
      dup_int_array(&tmp.oz, src->oz, src->npts))
    goto fail;

  if (src->tnt > 0 || src->txy) {
    /* Track template capacity may exceed tnt (pre-sized from tracks(N)). */
    int tcap = src->tnt;
    if (src->txy && tcap <= 0) {
      /* cleared template left dangling arrays in C++ partial clear — copy tnt only */
      tcap = 0;
    }
    if (tcap > 0) {
      if (dup_int_array(&tmp.txy, src->txy, tcap) ||
          dup_int_array(&tmp.tzy, src->tzy, tcap) ||
          dup_int_array(&tmp.tradx, src->tradx, tcap) ||
          dup_int_array(&tmp.tradz, src->tradz, tcap) ||
          dup_int_array(&tmp.trady, src->trady, tcap) ||
          dup_int_array(&tmp.tx, src->tx, tcap) ||
          dup_int_array(&tmp.ty, src->ty, tcap) ||
          dup_int_array(&tmp.tz, src->tz, tcap) ||
          dup_int_array(&tmp.skd, src->skd, tcap) ||
          dup_int_array(&tmp.dam, src->dam, tcap) ||
          dup_bool_array(&tmp.notwall, src->notwall, tcap) ||
          dup_int_array(&tmp.tc0, src->tc0, tcap) ||
          dup_int_array(&tmp.tc1, src->tc1, tcap) ||
          dup_int_array(&tmp.tc2, src->tc2, tcap))
        goto fail;
    }
  }

  nfm_conto_free(dst);
  *dst = tmp;
  return 0;

fail:
  nfm_conto_free(&tmp);
  return -1;
}

NfmContO* nfm_conto_clone(const NfmContO* src) {
  NfmContO* out;
  if (!src) return NULL;
  out = (NfmContO*)malloc(sizeof(NfmContO));
  if (!out) return NULL;
  nfm_conto_init_empty(out);
  if (nfm_conto_copy(out, src) != 0) {
    free(out);
    return NULL;
  }
  return out;
}

/* ---- from_rad --------------------------------------------------------- */

int nfm_conto_from_rad(NfmContO* out, const uint8_t* data, size_t len,
                       NfmMedium* med, NfmTrackers* trk) {
  char* text = NULL;
  char* saveptr = NULL;
  char* line;
  bool in_track_block = false;
  bool in_track = false;
  float div = 1.0f;
  float iwid = 1.0f;
  float scale[3] = {1.f, 1.f, 1.f};
  int wheel_i = 0;
  int* px = NULL;
  int* py = NULL;
  int* pz = NULL;
  int pn = 0;
  int pcap = 0;
  bool in_p = false;
  int last_ground = 0;
  int track_cap = 0;
  int ok = 1;

  if (!out) return -1;
  nfm_conto_free(out);
  nfm_conto_init_empty(out);
  out->m = med;
  out->t = trk;

  text = (char*)malloc(len + 1);
  if (!text) return -1;
  memcpy(text, data, len);
  text[len] = '\0';

  for (line = strtok_r(text, "\n", &saveptr); line;
       line = strtok_r(NULL, "\n", &saveptr)) {
    trim_inplace(line);
    if (line[0] == '\0') continue;

    if (strncmp(line, "div(", 4) == 0) {
      div = getvalue("div", line, 0) / 10.0f;
      continue;
    }
    if (strncmp(line, "idiv(", 5) == 0) {
      div = getvalue("idiv", line, 0) / 100.0f;
      continue;
    }
    if (strncmp(line, "iwid(", 5) == 0) {
      iwid = getvalue("iwid", line, 0) / 100.0f;
      continue;
    }
    if (strncmp(line, "ScaleX(", 7) == 0) {
      scale[0] = getvalue("ScaleX", line, 0) / 100.0f;
      continue;
    }
    if (strncmp(line, "ScaleY(", 7) == 0) {
      scale[1] = getvalue("ScaleY", line, 0) / 100.0f;
      continue;
    }
    if (strncmp(line, "ScaleZ(", 7) == 0) {
      scale[2] = getvalue("ScaleZ", line, 0) / 100.0f;
      continue;
    }
    if (strncmp(line, "grounded(", 9) == 0) {
      out->grounded = getvalue("grounded", line, 0) / 100.0f;
      continue;
    }
    if (strcmp(line, "decorative") == 0) {
      out->decor = true;
      continue;
    }
    if (strcmp(line, "shadow") == 0) {
      out->shadow = true;
      continue;
    }

    if (strncmp(line, "tracks", 6) == 0) {
      int n = getvalue("tracks", line, 0);
      track_cap = n;
      out->txy = realloc_ints(NULL, 0, n);
      out->tzy = realloc_ints(NULL, 0, n);
      out->tradx = realloc_ints(NULL, 0, n);
      out->tradz = realloc_ints(NULL, 0, n);
      out->trady = realloc_ints(NULL, 0, n);
      out->tx = realloc_ints(NULL, 0, n);
      out->ty = realloc_ints(NULL, 0, n);
      out->tz = realloc_ints(NULL, 0, n);
      out->skd = realloc_ints(NULL, 0, n);
      out->dam = realloc_ints(NULL, 0, n);
      out->notwall = realloc_bools(NULL, 0, n);
      out->tc0 = realloc_ints(NULL, 0, n);
      out->tc1 = realloc_ints(NULL, 0, n);
      out->tc2 = realloc_ints(NULL, 0, n);
      if (n > 0 && (!out->txy || !out->tzy || !out->tradx || !out->tradz ||
                    !out->trady || !out->tx || !out->ty || !out->tz ||
                    !out->skd || !out->dam || !out->notwall || !out->tc0 ||
                    !out->tc1 || !out->tc2)) {
        ok = 0;
        break;
      }
      {
        int i;
        for (i = 0; i < n; ++i) out->dam[i] = 1;
      }
      out->tnt = 0;
      in_track_block = true;
      continue;
    }

    if (in_track_block) {
      if (strcmp(line, "<track>") == 0) {
        in_track = true;
        if (out->tnt < track_cap) {
          out->notwall[out->tnt] = false;
          out->dam[out->tnt] = 1;
          out->skd[out->tnt] = 0;
          out->ty[out->tnt] = out->tx[out->tnt] = out->tz[out->tnt] = 0;
          out->txy[out->tnt] = out->tzy[out->tnt] = 0;
          out->trady[out->tnt] = out->tradx[out->tnt] = out->tradz[out->tnt] =
              0;
        }
        continue;
      }
      if (in_track && out->tnt < track_cap) {
        if (strncmp(line, "c(", 2) == 0) {
          out->tc0[out->tnt] = getvalue("c", line, 0);
          out->tc1[out->tnt] = getvalue("c", line, 1);
          out->tc2[out->tnt] = getvalue("c", line, 2);
        }
        if (strncmp(line, "xy", 2) == 0) out->txy[out->tnt] = getvalue("xy", line, 0);
        if (strncmp(line, "zy", 2) == 0) out->tzy[out->tnt] = getvalue("zy", line, 0);
        if (strncmp(line, "radx", 4) == 0)
          out->tradx[out->tnt] = (int)(getvalue("radx", line, 0) * div);
        if (strncmp(line, "rady", 4) == 0)
          out->trady[out->tnt] = (int)(getvalue("rady", line, 0) * div);
        if (strncmp(line, "radz", 4) == 0)
          out->tradz[out->tnt] = (int)(getvalue("radz", line, 0) * div);
        if (strncmp(line, "ty", 2) == 0)
          out->ty[out->tnt] = (int)(getvalue("ty", line, 0) * div);
        if (strncmp(line, "tx", 2) == 0)
          out->tx[out->tnt] = (int)(getvalue("tx", line, 0) * div);
        if (strncmp(line, "tz", 2) == 0)
          out->tz[out->tnt] = (int)(getvalue("tz", line, 0) * div);
        if (strncmp(line, "skid", 4) == 0)
          out->skd[out->tnt] = getvalue("skid", line, 0);
        if (strncmp(line, "dam", 3) == 0) out->dam[out->tnt] = 3;
        if (strncmp(line, "notwall", 7) == 0) out->notwall[out->tnt] = true;
      }
      if (strcmp(line, "</track>") == 0) {
        in_track = false;
        ++out->tnt;
        continue;
      }
    }

    if (strcmp(line, "<p>") == 0) {
      in_p = true;
      pn = 0;
      continue;
    }
    if (in_p && strncmp(line, "p(", 2) == 0) {
      int vx = (int)(getvalue("p", line, 0) * div * iwid * scale[0]);
      int vy = (int)(getvalue("p", line, 1) * div * scale[1]);
      int vz = (int)(getvalue("p", line, 2) * div * scale[2]);
      int r;
      if (pn >= pcap) {
        int ncap = pcap ? pcap * 2 : 16;
        int* nx = realloc_ints(px, pcap, ncap);
        int* ny = realloc_ints(py, pcap, ncap);
        int* nz = realloc_ints(pz, pcap, ncap);
        if (!nx || !ny || !nz) {
          free(nx);
          free(ny);
          free(nz);
          ok = 0;
          break;
        }
        px = nx;
        py = ny;
        pz = nz;
        pcap = ncap;
      }
      px[pn] = vx;
      py[pn] = vy;
      pz[pn] = vz;
      ++pn;
      r = (int)sqrtf((float)(vx * vx + vy * vy + vz * vz));
      if (r > out->maxR) out->maxR = r;
      continue;
    }
    if (strcmp(line, "</p>") == 0) {
      NfmPlane* npl_arr;
      NfmPlane pl;
      int* nox;
      int* noy;
      int* noz;
      in_p = false;
      memset(&pl, 0, sizeof(pl));
      pl.n = pn;
      if (pn > 0) {
        pl.ox = (int*)malloc((size_t)pn * sizeof(int));
        pl.oy = (int*)malloc((size_t)pn * sizeof(int));
        pl.oz = (int*)malloc((size_t)pn * sizeof(int));
        if (!pl.ox || !pl.oy || !pl.oz) {
          free_plane(&pl);
          ok = 0;
          break;
        }
        memcpy(pl.ox, px, (size_t)pn * sizeof(int));
        memcpy(pl.oy, py, (size_t)pn * sizeof(int));
        memcpy(pl.oz, pz, (size_t)pn * sizeof(int));
      }
      npl_arr =
          (NfmPlane*)realloc(out->p, (size_t)(out->npl + 1) * sizeof(NfmPlane));
      if (!npl_arr) {
        free_plane(&pl);
        ok = 0;
        break;
      }
      out->p = npl_arr;
      out->p[out->npl++] = pl;

      nox = realloc_ints(out->ox, out->npts, out->npts + pn);
      noy = realloc_ints(out->oy, out->npts, out->npts + pn);
      noz = realloc_ints(out->oz, out->npts, out->npts + pn);
      if ((pn > 0) && (!nox || !noy || !noz)) {
        free(nox);
        free(noy);
        free(noz);
        ok = 0;
        break;
      }
      out->ox = nox;
      out->oy = noy;
      out->oz = noz;
      if (pn > 0) {
        memcpy(out->ox + out->npts, px, (size_t)pn * sizeof(int));
        memcpy(out->oy + out->npts, py, (size_t)pn * sizeof(int));
        memcpy(out->oz + out->npts, pz, (size_t)pn * sizeof(int));
      }
      out->npts += pn;
      continue;
    }

    if (strncmp(line, "w(", 2) == 0 && wheel_i < 4) {
      int wy, wh;
      float n11;
      out->keyx[wheel_i] = (int)(getvalue("w", line, 0) * div * scale[0]);
      out->keyz[wheel_i] = (int)(getvalue("w", line, 2) * div * scale[2]);
      wy = (int)(getvalue("w", line, 1) * div * scale[1]);
      wh = (int)(getvalue("w", line, 5) * div);
      n11 = wh / 10.0f;
      last_ground = (int)(wy + 13.0f * n11);
      ++wheel_i;
      continue;
    }
  }

  free(px);
  free(py);
  free(pz);
  free(text);

  if (!ok) {
    nfm_conto_free(out);
    return -1;
  }
  out->grat = last_ground;
  return 0;
}

/* ---- place ------------------------------------------------------------ */

int nfm_conto_place(NfmContO* out, const NfmContO* base, int px, int py, int pz,
                    int yaw, NfmMedium* med, NfmTrackers* trk,
                    bool register_tracks) {
  int i, k;
  NfmMedium tmp_med;
  NfmMedium* rot_m;

  if (!out || !base) return -1;
  if (nfm_conto_copy(out, base) != 0) return -1;

  out->m = med;
  out->t = trk;

  nfm_medium_init(&tmp_med, false, 0);
  rot_m = &tmp_med;

  for (i = 0; i < out->npl; ++i) {
    NfmPlane* pl = &out->p[i];
    int j;
    for (j = 0; j < pl->n; ++j) {
      int vx = pl->ox[j], vz = pl->oz[j];
      rot2(rot_m, &vx, &vz, 0, 0, yaw);
      pl->ox[j] = vx;
      pl->oz[j] = vz;
    }
  }
  for (i = 0; i < out->npts; ++i) {
    int vx = out->ox[i], vz = out->oz[i];
    rot2(rot_m, &vx, &vz, 0, 0, yaw);
    out->ox[i] = vx;
    out->oz[i] = vz;
  }

  out->x = px;
  out->y = py;
  out->z = pz;
  out->xz = out->xy = out->zy = 0;
  out->wxz = 0;
  out->wzy = 0;
  out->dist = 0;
  out->fix = 0;
  out->fcnt = 0;
  out->ust = 0;
  out->elec = false;
  out->roted = false;
  out->checkpoint = 0;

  if (register_tracks && base->tnt != 0 && trk && med) {
    const int n = yaw;
    for (k = 0; k < base->tnt; ++k) {
      int idx;
      int absn;
      if (trk->nt >= NFM_TRACKERS_MAX) break;
      idx = trk->nt;
      trk->xy[idx] = (int)(base->txy[k] * nfm_medium_cos(med, n) -
                           base->tzy[k] * nfm_medium_sin(med, n));
      trk->zy[idx] = (int)(base->tzy[k] * nfm_medium_cos(med, n) +
                           base->txy[k] * nfm_medium_sin(med, n));
      trk->c[idx][0] = base->tc0[k];
      trk->c[idx][1] = base->tc1[k];
      trk->c[idx][2] = base->tc2[k];
      trk->x[idx] = (int)(out->x + base->tx[k] * nfm_medium_cos(med, n) -
                          base->tz[k] * nfm_medium_sin(med, n));
      trk->z[idx] = (int)(out->z + base->tz[k] * nfm_medium_cos(med, n) +
                          base->tx[k] * nfm_medium_sin(med, n));
      trk->y[idx] = out->y + base->ty[k];
      trk->skd[idx] = base->skd[k];
      trk->dam[idx] = base->dam[k];
      trk->notwall[idx] = base->notwall[k];
      trk->decor[idx] = out->decor;
      absn = abs(n);
      if (absn == 180) absn = 0;
      trk->radx[idx] = (int)fabsf(base->tradx[k] * nfm_medium_cos(med, absn) +
                                  base->tradz[k] * nfm_medium_sin(med, absn));
      trk->radz[idx] = (int)fabsf(base->tradx[k] * nfm_medium_sin(med, absn) +
                                  base->tradz[k] * nfm_medium_cos(med, absn));
      trk->rady[idx] = base->trady[k];
      ++trk->nt;
    }
  }
  return 0;
}

/* ---- pile ------------------------------------------------------------- */

int nfm_conto_pile(NfmContO* out, int seed, int width, int height,
                   NfmMedium* med, NfmTrackers* trk, int px, int pz, int py) {
  NfmJavaRandom random;
  int array[8], array2[8], array3[8], array4[8], array5[8];
  float n4, n5, n6, n7, n8, n9, n10, n11, n12, n13;
  float n16, n17;
  int i, j, k, n23;
  int array10[2] = {0, 0};
  int array11[2] = {0, 0};

  if (!out) return -1;
  nfm_conto_free(out);
  nfm_conto_init_empty(out);
  out->m = med;
  out->t = trk;
  out->x = px;
  out->z = pz;
  out->y = py;
  out->xz = out->xy = out->zy = 0;
  out->grat = 0;
  out->grounded = 115.0f;
  out->decor = true;
  out->shadow = false;
  out->npl = 5;

  nfm_java_random_init(&random, seed);
  n4 = (float)width;
  n5 = (float)height;
  if (n5 < 2.0f) n5 = 2.0f;
  if (n5 > 6.0f) n5 = 6.0f;
  if (n4 < 2.0f) n4 = 2.0f;
  if (n4 > 6.0f) n4 = 6.0f;
  n6 = n4 / 1.5f;
  n7 = n5 / 1.5f * (1.0f + (n6 - 2.0f) * 0.1786f);
  n8 = (float)(50.0 + 100.0 * nfm_java_random_next_double(&random));
  array[0] = -(int)(n8 * n6 * 0.7071f);
  array2[0] = (int)(n8 * n6 * 0.7071f);
  n9 = (float)(50.0 + 100.0 * nfm_java_random_next_double(&random));
  array[1] = 0;
  array2[1] = (int)(n9 * n6);
  n10 = (float)(50.0 + 100.0 * nfm_java_random_next_double(&random));
  array[2] = (int)(n10 * n6 * 0.7071);
  array2[2] = (int)(n10 * n6 * 0.7071);
  array[3] =
      (int)((float)(50.0 + 100.0 * nfm_java_random_next_double(&random)) * n6);
  array2[3] = 0;
  n11 = (float)(50.0 + 100.0 * nfm_java_random_next_double(&random));
  array[4] = (int)(n11 * n6 * 0.7071);
  array2[4] = -(int)(n11 * n6 * 0.7071);
  n12 = (float)(50.0 + 100.0 * nfm_java_random_next_double(&random));
  array[5] = 0;
  array2[5] = -(int)(n12 * n6);
  n13 = (float)(50.0 + 100.0 * nfm_java_random_next_double(&random));
  array[6] = -(int)(n13 * n6 * 0.7071);
  array2[6] = -(int)(n13 * n6 * 0.7071);
  array[7] =
      -(int)((float)(50.0 + 100.0 * nfm_java_random_next_double(&random)) * n6);
  array2[7] = 0;

  for (i = 0; i < 8; ++i) {
    array3[i] =
        (int)(array[i] * (0.2 + 0.4 * nfm_java_random_next_double(&random)));
    array4[i] =
        (int)(array2[i] * (0.2 + 0.4 * nfm_java_random_next_double(&random)));
    array5[i] =
        -(int)((10.0 + 15.0 * nfm_java_random_next_double(&random)) * n7);
  }

  out->maxR = 0;
  for (j = 0; j < 8; ++j) {
    int n14 = j - 1;
    int n15 = j + 1;
    int maxR1, maxR2;
    if (n14 == -1) n14 = 7;
    if (n15 == 8) n15 = 0;
    array[j] = ((array[n14] + array[n15]) / 2 + array[j]) / 2;
    array2[j] = ((array2[n14] + array2[n15]) / 2 + array2[j]) / 2;
    array3[j] = ((array3[n14] + array3[n15]) / 2 + array3[j]) / 2;
    array4[j] = ((array4[n14] + array4[n15]) / 2 + array4[j]) / 2;
    array5[j] = ((array5[n14] + array5[n15]) / 2 + array5[j]) / 2;
    maxR1 = (int)sqrt((double)(array[j] * array[j] + array2[j] * array2[j]));
    if (maxR1 > out->maxR) out->maxR = maxR1;
    maxR2 = (int)sqrt((double)(array3[j] * array3[j] + array5[j] * array5[j] +
                               array4[j] * array4[j]));
    if (maxR2 > out->maxR) out->maxR = maxR2;
  }

  n16 = -1.0f;
  n17 = (n6 / n7 - 0.33f) / 33.4f;
  if (n17 < 0.005f) n17 = 0.0f;
  if (n17 > 0.057f) n17 = 0.057f;
  for (k = 0; k < 4; ++k) {
    float n20;
    for (n20 = (float)((0.17 - n17) * nfm_java_random_next_double(&random));
         fabsf(n16 - n20) < 0.03f - n17 * 0.176f;
         n20 = (float)((0.17 - n17) * nfm_java_random_next_double(&random))) {
    }
    n16 = n20;
    (void)n20;
  }
  (void)nfm_java_random_next_double(&random);

  if (!trk) return 0;

  for (n23 = 0; n23 < 4; ++n23) {
    int n24, n25, n26;
    if (trk->nt >= NFM_TRACKERS_MAX) break;
    n24 = n23 * 2 + 1;
    trk->y[trk->nt] = array5[n24] / 2;
    trk->rady[trk->nt] = abs(array5[n24] / 2);
    if (n23 == 0 || n23 == 2) {
      trk->z[trk->nt] = (array2[n24] + array4[n24]) / 2;
      trk->radz[trk->nt] = abs(trk->z[trk->nt] - array2[n24]);
      n25 = n23 * 2 + 2;
      if (n25 == 8) n25 = 0;
      trk->x[trk->nt] = (array[n23 * 2] + array[n25]) / 2;
      trk->radx[trk->nt] = abs(trk->x[trk->nt] - array[n23 * 2]);
    } else {
      trk->x[trk->nt] = (array[n24] + array3[n24]) / 2;
      trk->radx[trk->nt] = abs(trk->x[trk->nt] - array[n24]);
      n26 = n23 * 2 + 2;
      if (n26 == 8) n26 = 0;
      trk->z[trk->nt] = (array2[n23 * 2] + array2[n26]) / 2;
      trk->radz[trk->nt] = abs(trk->z[trk->nt] - array2[n23 * 2]);
    }
    if (n23 == 0) {
      array11[0] = trk->z[trk->nt] - trk->radz[trk->nt];
      trk->zy[trk->nt] = (int)(atan((double)(trk->rady[trk->nt] / trk->radz[trk->nt])) /
                               0.017453292519943295);
      if (trk->zy[trk->nt] > 40) trk->zy[trk->nt] = 40;
      trk->xy[trk->nt] = 0;
    }
    if (n23 == 1) {
      array10[0] = trk->x[trk->nt] - trk->radx[trk->nt];
      trk->xy[trk->nt] = (int)(atan((double)(trk->rady[trk->nt] / trk->radx[trk->nt])) /
                               0.017453292519943295);
      if (trk->xy[trk->nt] > 40) trk->xy[trk->nt] = 40;
      trk->zy[trk->nt] = 0;
    }
    if (n23 == 2) {
      array11[1] = trk->z[trk->nt] + trk->radz[trk->nt];
      trk->zy[trk->nt] =
          -(int)(atan((double)(trk->rady[trk->nt] / trk->radz[trk->nt])) /
                 0.017453292519943295);
      if (trk->zy[trk->nt] < -40) trk->zy[trk->nt] = -40;
      trk->xy[trk->nt] = 0;
    }
    if (n23 == 3) {
      array10[1] = trk->x[trk->nt] + trk->radx[trk->nt];
      trk->xy[trk->nt] =
          -(int)(atan((double)(trk->rady[trk->nt] / trk->radx[trk->nt])) /
                 0.017453292519943295);
      if (trk->xy[trk->nt] < -40) trk->xy[trk->nt] = -40;
      trk->zy[trk->nt] = 0;
    }
    trk->x[trk->nt] += out->x;
    trk->z[trk->nt] += out->z;
    trk->y[trk->nt] += out->y;
    trk->c[trk->nt][0] = trk->c[trk->nt][1] = trk->c[trk->nt][2] = 0;
    trk->skd[trk->nt] = 2;
    trk->dam[trk->nt] = 1;
    trk->notwall[trk->nt] = false;
    trk->decor[trk->nt] = true;
    trk->rady[trk->nt] += 10;
    ++trk->nt;
  }

  if (trk->nt >= NFM_TRACKERS_MAX) return 0;
  trk->y[trk->nt] = 0;
  for (i = 0; i < 8; ++i) trk->y[trk->nt] += array5[i];
  trk->y[trk->nt] /= 8;
  trk->y[trk->nt] += out->y;
  trk->rady[trk->nt] = 200;
  trk->radx[trk->nt] = array10[0] - array10[1];
  trk->radz[trk->nt] = array11[0] - array11[1];
  trk->x[trk->nt] = (array10[0] + array10[1]) / 2 + out->x;
  trk->z[trk->nt] = (array11[0] + array11[1]) / 2 + out->z;
  trk->zy[trk->nt] = 0;
  trk->xy[trk->nt] = 0;
  trk->c[trk->nt][0] = trk->c[trk->nt][1] = trk->c[trk->nt][2] = 0;
  trk->skd[trk->nt] = 4;
  trk->dam[trk->nt] = 1;
  trk->notwall[trk->nt] = false;
  trk->decor[trk->nt] = true;
  ++trk->nt;
  return 0;
}

/* ---- dust ------------------------------------------------------------- */

void nfm_conto_dust(NfmContO* c, int n, float n2, float n3, float n4, int n5,
                    int n6, float n7, int n8, bool b) {
  bool b2 = false;
  float n9;
  if (!c || !c->m) return;
  if (n8 > 5 && (n == 0 || n == 2)) b2 = true;
  if (n8 < -5 && (n == 1 || n == 3)) b2 = true;
  n9 = (float)((sqrt((double)n5 * n5 + (double)n6 * n6) - 40.0) / 160.0);
  if (n9 > 1.0f) n9 = 1.0f;
  if (nfm_env.dustlog && c->m->rng_calls >= 12540 &&
      c->m->rng_calls <= 12610) {
    fprintf(stderr, "DUSTCHK n=%d n9=%.5f b2=%d b=%d sc=%.1f,%.1f rng=%llu\n", n,
            n9, (int)b2, (int)b, (float)n5, (float)n6,
            (unsigned long long)c->m->rng_calls);
  }
  if ((double)n9 > 0.2 && !b2) {
    ++c->ust;
    if (c->ust == 20) c->ust = 0;
    if (!b) {
      const char* prev = c->m->trace_label;
      c->m->trace_label = "dust";
      if (nfm_env.drvrng && c->m->rng_calls >= 221290 &&
          c->m->rng_calls <= 221320)
        fprintf(stderr,
                "DUSTRNG n=%d n9=%.8f b2=%d b=%d sc=%d,%d tilt=%d rng=%llu\n", n,
                n9, (int)b2, (int)b, n5, n6, n8,
                (unsigned long long)c->m->rng_calls);
      (void)nfm_medium_random(c->m);
      c->m->trace_label = prev;
    } else if (nfm_env.drvrng && c->m->rng_calls >= 221290 &&
               c->m->rng_calls <= 221320) {
      fprintf(stderr, "DUSTSKIP_B n=%d n9=%.8f rng=%llu\n", n, n9,
              (unsigned long long)c->m->rng_calls);
    }
  } else if (nfm_env.drvrng && c->m->rng_calls >= 221290 &&
             c->m->rng_calls <= 221320) {
    fprintf(stderr,
            "DUSTSKIP_N9 n=%d n9=%.8f b2=%d sc=%d,%d tilt=%d rng=%llu\n", n, n9,
            (int)b2, n5, n6, n8, (unsigned long long)c->m->rng_calls);
  }
  (void)n2;
  (void)n3;
  (void)n4;
  (void)n7;
}

/* ---- load models ------------------------------------------------------ */

static void set_err(char* err, size_t errlen, const char* msg) {
  if (!err || errlen == 0) return;
  snprintf(err, errlen, "%s", msg);
}

static int load_one_rad(NfmContO* out, const char* models_dir, const char* name,
                        NfmMedium* m, NfmTrackers* t, char* err, size_t errlen) {
  char path[1024];
  FILE* f;
  long sz;
  uint8_t* buf;
  size_t nread;
  int rc;

  snprintf(path, sizeof(path), "%s/%s.rad", models_dir, name);
  f = fopen(path, "rb");
  if (!f) {
    if (err && errlen)
      snprintf(err, errlen, "missing %s", path);
    return 0;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    set_err(err, errlen, "seek failed");
    return 0;
  }
  sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    set_err(err, errlen, "ftell failed");
    return 0;
  }
  rewind(f);
  buf = (uint8_t*)malloc((size_t)sz);
  if (!buf) {
    fclose(f);
    set_err(err, errlen, "oom");
    return 0;
  }
  nread = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (nread != (size_t)sz) {
    free(buf);
    set_err(err, errlen, "short read");
    return 0;
  }
  rc = nfm_conto_from_rad(out, buf, (size_t)sz, m, t);
  free(buf);
  return rc == 0 ? 1 : 0;
}

bool nfm_load_models_zip(const char* models_dir, NfmContO* out124, NfmMedium* m,
                         NfmTrackers* t, char* err, size_t errlen) {
  static const char* cars[16] = {
      "2000tornados", "formula7", "canyenaro", "lescrab", "nimi",
      "maxrevenge",   "leadoxide", "koolkat",  "drifter", "policecops",
      "mustang",      "king",      "audir8",   "masheen", "radicalone",
      "drmonster"};
  static const char* pieces[68] = {
      "road",         "froad",        "twister2",     "twister1",
      "turn",         "offroad",      "bumproad",     "offturn",
      "nroad",        "nturn",        "roblend",      "noblend",
      "rnblend",      "roadend",      "offroadend",   "hpground",
      "ramp30",       "cramp35",      "dramp15",      "dhilo15",
      "slide10",      "takeoff",      "sramp22",      "offbump",
      "offramp",      "sofframp",     "halfpipe",     "spikes",
      "rail",         "thewall",      "checkpoint",   "fixpoint",
      "offcheckpoint","sideoff",      "bsideoff",     "uprise",
      "riseroad",     "sroad",        "soffroad",     "tside",
      "launchpad",    "thenet",       "speedramp",    "offhill",
      "slider",       "uphill",       "roll1",        "roll2",
      "roll3",        "roll4",        "roll5",        "roll6",
      "opile1",       "opile2",       "aircheckpoint","tree1",
      "tree2",        "tree3",        "tree4",        "tree5",
      "tree6",        "tree7",        "tree8",        "cac1",
      "cac2",         "cac3",         "8sroad",       "8soffroad"};
  int i, j;

  if (!models_dir || !out124) return false;

  for (i = 0; i < 16; ++i) {
    if (!load_one_rad(&out124[i], models_dir, cars[i], m, t, err, errlen))
      return false;
  }
  for (j = 0; j < 68; ++j) {
    if (!load_one_rad(&out124[j + 56], models_dir, pieces[j], m, t, err,
                      errlen))
      return false;
  }
  return true;
}
