#include "nfm/state_recorder.h"

#include <stdio.h>
#include <string.h>

static void put_be32(FILE* o, int32_t v) {
  unsigned char b[4] = {
      (unsigned char)((v >> 24) & 0xff),
      (unsigned char)((v >> 16) & 0xff),
      (unsigned char)((v >> 8) & 0xff),
      (unsigned char)(v & 0xff),
  };
  fwrite(b, 1, 4, o);
}

static void put_bef32(FILE* o, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  put_be32(o, (int32_t)v);
}

static int32_t get_be32(FILE* in) {
  unsigned char b[4];
  if (fread(b, 1, 4, in) != 4) return 0;
  return ((int32_t)b[0] << 24) | ((int32_t)b[1] << 16) | ((int32_t)b[2] << 8) |
         (int32_t)b[3];
}

static float get_bef32(FILE* in) {
  uint32_t v = (uint32_t)get_be32(in);
  float f;
  memcpy(&f, &v, 4);
  return f;
}

int nfm_state_recorder_open(NfmStateRecorder* r, const char* path, int stage,
                            int focus_car, int nplayers, int nlaps, int nsp,
                            const int* sc) {
  int i;
  memset(r, 0, sizeof(*r));
  r->nplayers = nplayers;
  r->out = fopen(path, "wb");
  if (!r->out) return -1;
  fwrite("NFMS", 1, 4, r->out);
  put_be32(r->out, NFM_NFMS_VERSION);
  put_be32(r->out, stage);
  put_be32(r->out, focus_car);
  put_be32(r->out, nplayers);
  put_be32(r->out, nlaps);
  put_be32(r->out, nsp);
  for (i = 0; i < nplayers; ++i) put_be32(r->out, sc[i]);
  printf("[sim] writing world state -> %s\n", path);
  return 0;
}

void nfm_state_recorder_capture(NfmStateRecorder* r, const NfmContO* cars,
                                const NfmMad* mad) {
  int i;
  if (!r || r->finished || !r->out) return;
  for (i = 0; i < r->nplayers; ++i) {
    const NfmContO* c = &cars[i];
    const NfmMad* m = &mad[i];
    put_be32(r->out, c->x);
    put_be32(r->out, c->y);
    put_be32(r->out, c->z);
    put_be32(r->out, c->xz);
    put_be32(r->out, c->xy);
    put_be32(r->out, c->zy);
    put_be32(r->out, c->wxz);
    put_be32(r->out, c->wzy);
    put_bef32(r->out, m->speed);
    put_bef32(r->out, m->power);
    put_be32(r->out, m->hitmag);
    put_be32(r->out, m->squash);
    put_be32(r->out, m->cn);
    put_be32(r->out, m->mxz);
    put_be32(r->out, m->cxz);
    put_be32(r->out, m->clear);
    {
      unsigned char d = m->dest ? 1 : 0;
      fwrite(&d, 1, 1, r->out);
    }
  }
  ++r->frames;
}

void nfm_state_recorder_finish(NfmStateRecorder* r) {
  if (!r || r->finished) return;
  r->finished = true;
  if (r->out) {
    fflush(r->out);
    fclose(r->out);
    r->out = NULL;
  }
  printf("[sim] state frames=%d\n", r->frames);
}

NfmstDiff nfm_diff_nfmst(const char* path_a, const char* path_b) {
  NfmstDiff d;
  FILE *a, *b;
  char ma[4], mb[4];
  int nplayers;
  int frame = 0;
  const char* fields[] = {"x",     "y",     "z",     "xz",    "xy",
                          "zy",    "wxz",   "wzy",   "speed", "power",
                          "hitmag","squash","cn",    "mxz",   "cxz",
                          "clear", "dest"};
  memset(&d, 0, sizeof(d));
  d.equal = true;
  d.frame = -1;
  d.player = -1;
  a = fopen(path_a, "rb");
  b = fopen(path_b, "rb");
  if (!a || !b) {
    d.equal = false;
    snprintf(d.detail, sizeof(d.detail), "open failed");
    if (a) fclose(a);
    if (b) fclose(b);
    return d;
  }
  if (fread(ma, 1, 4, a) != 4 || fread(mb, 1, 4, b) != 4 ||
      memcmp(ma, mb, 4) != 0) {
    d.equal = false;
    snprintf(d.detail, sizeof(d.detail), "magic mismatch");
    fclose(a);
    fclose(b);
    return d;
  }
#define HI(name)                                                               \
  do {                                                                         \
    int32_t va = get_be32(a), vb = get_be32(b);                                \
    if (va != vb) {                                                            \
      d.equal = false;                                                         \
      snprintf(d.field, sizeof(d.field), "%s", name);                          \
      snprintf(d.detail, sizeof(d.detail), "%d vs %d", (int)va, (int)vb);      \
      fclose(a);                                                               \
      fclose(b);                                                               \
      return d;                                                                \
    }                                                                          \
  } while (0)
  HI("version");
  HI("stage");
  HI("focusCar");
  {
    int32_t va = get_be32(a), vb = get_be32(b);
    if (va != vb) {
      d.equal = false;
      snprintf(d.field, sizeof(d.field), "nplayers");
      snprintf(d.detail, sizeof(d.detail), "%d vs %d", (int)va, (int)vb);
      fclose(a);
      fclose(b);
      return d;
    }
    nplayers = (int)va;
  }
  HI("nlaps");
  HI("nsp");
#undef HI
  {
    int i;
    for (i = 0; i < nplayers; ++i) {
      if (get_be32(a) != get_be32(b)) {
        d.equal = false;
        snprintf(d.field, sizeof(d.field), "sc[%d]", i);
        fclose(a);
        fclose(b);
        return d;
      }
    }
  }
  while (!feof(a) && !feof(b)) {
    int p, f;
    int c0 = fgetc(a);
    int c1 = fgetc(b);
    if (c0 == EOF && c1 == EOF) break;
    if (c0 == EOF || c1 == EOF) {
      d.equal = false;
      snprintf(d.detail, sizeof(d.detail), "length mismatch after frame %d",
               frame);
      break;
    }
    ungetc(c1, b);
    ungetc(c0, a);
    for (p = 0; p < nplayers; ++p) {
      for (f = 0; f < 8; ++f) {
        int32_t va = get_be32(a), vb = get_be32(b);
        if (va != vb) {
          d.equal = false;
          d.frame = frame;
          d.player = p;
          snprintf(d.field, sizeof(d.field), "%s", fields[f]);
          snprintf(d.detail, sizeof(d.detail), "%d vs %d", (int)va, (int)vb);
          fclose(a);
          fclose(b);
          return d;
        }
      }
      for (f = 0; f < 2; ++f) {
        float va = get_bef32(a), vb = get_bef32(b);
        uint32_t ba, bb;
        memcpy(&ba, &va, 4);
        memcpy(&bb, &vb, 4);
        if (ba != bb) {
          d.equal = false;
          d.frame = frame;
          d.player = p;
          snprintf(d.field, sizeof(d.field), "%s", fields[8 + f]);
          snprintf(d.detail, sizeof(d.detail), "float bits differ");
          fclose(a);
          fclose(b);
          return d;
        }
      }
      for (f = 0; f < 6; ++f) {
        int32_t va = get_be32(a), vb = get_be32(b);
        if (va != vb) {
          d.equal = false;
          d.frame = frame;
          d.player = p;
          snprintf(d.field, sizeof(d.field), "%s", fields[10 + f]);
          snprintf(d.detail, sizeof(d.detail), "%d vs %d", (int)va, (int)vb);
          fclose(a);
          fclose(b);
          return d;
        }
      }
      {
        unsigned char da, db;
        if (fread(&da, 1, 1, a) != 1 || fread(&db, 1, 1, b) != 1 || da != db) {
          d.equal = false;
          d.frame = frame;
          d.player = p;
          snprintf(d.field, sizeof(d.field), "dest");
          fclose(a);
          fclose(b);
          return d;
        }
      }
    }
    ++frame;
  }
  fclose(a);
  fclose(b);
  return d;
}
