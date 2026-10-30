// ivm-gen.c: emitter, constant, variables, and scopes mamager

#ifndef IVM_GEN_C
#define IVM_GEN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifndef UCHAR
  #define UCHAR
  typedef unsigned char uchar;
  typedef uint16_t      word;
#endif

// --- limits (override before include if needed) ---
#ifndef MAX_BIN_LEN
  #define MAX_BIN_LEN   2048
#endif

#ifndef MAX_CONSTS
  #define MAX_CONSTS    128
#endif

#ifndef MAX_SLOTS
  #define MAX_SLOTS     256
#endif

#ifndef FRAME_BASE
  #define FRAME_BASE    96
#endif

#ifndef CONST_TOP
  #define CONST_TOP     253
#endif

// fail hook — tpi: #define IVM_GEN_FAIL(m) FAIL(m) before include
#ifndef IVM_GEN_FAIL
  #define IVM_GEN_FAIL(msg) do { \
    fprintf(stderr, "ivm-gen: %s\n", (msg)); \
    abort(); \
  } while (0)
#endif

// optional stats-counting hooks
// (tpi defines real ones; parse can stub)

void note_load(uchar addr);
void note_store(uchar addr);

// ---------------------------------------------------------------------------
// Vectors / main (shared; tpi may #define IVM_GEN_NO_VECTORS to use its own)
// ---------------------------------------------------------------------------

// TODO: compliated and ll the #ifdef in the code is ugly, why not
//   just do it one way, here once for all instead of different
//   ways elsewhere?

// TODO: add answer to WHY they are differnt?

#ifndef MAX_VECTORS
  #define MAX_VECTORS 64
#endif

#ifdef IVM_GEN_NO_VECTORS
  // tpi supplies vectors[], frame_base[], nvector, main_offset
  extern uint16_t vectors[];
  extern char frame_base[];
  extern char nvector;
  extern int main_offset;
#else
  uint16_t vectors[MAX_VECTORS];
  char frame_base[MAX_VECTORS];
  char nvector;
  int main_offset;
#endif

// The current func_entry being compiled
uchar pending_func_entry; // 0xff = none

void gen_func_entry(uchar v);
uchar *build_blob(int *out_len);

// ---------------------------------------------------------------------------
// Bytecode buffer
// ---------------------------------------------------------------------------
uchar bin[MAX_BIN_LEN];
int bin_ptr = 0;

// ACC mirror: which mem slot ACC currently holds (-1 = none/unknown)
uchar acc_slot;
uchar acc_valid;

void acc_invalidate(void) {
  acc_valid = 0;
  acc_slot = (uchar)-1;
}

void acc_set_slot(uchar slot) {
  acc_slot = slot;
  acc_valid = 1;
}

void emit(uchar c) {
  if (pending_func_entry != 0xff) {
    gen_func_entry(pending_func_entry);
    pending_func_entry = 0xff;
  }

  if (bin_ptr >= MAX_BIN_LEN - 1)
    IVM_GEN_FAIL("bytecode buffer overflow");

  bin[bin_ptr++] = c;
}

void emitop(uchar op) {
  emit(op);

  if (op == '0' || op == '9' || op == '~' || op == '?' || op == 'G' || op == 'B')
    acc_invalidate();
}

void emitop2(uchar op, uchar arg) {
  if (op == 'L' && arg < 32)
    emit(arg); // one byte short-code
  else {
    emitop(op);
    emit(arg);
  }

  // update acc and stats
  if (op == 'S' || op == 'L' || op == 'U') {

    acc_set_slot(arg);
    if (op == 'L') note_load((uchar)arg);
    if (op == 'S') note_store((uchar)arg);

  } else if (strchr("+-*/%&|^IJ[]{}MF@", op))

    acc_invalidate();

}

// ---------------------------------------------------------------------------
// Constants — grow downward from CONST_TOP (253,252,…)
// ---------------------------------------------------------------------------
uint16_t const_pool[MAX_CONSTS];
uchar nconst = 0;
uchar constidx = 255; // lowest const slot; 255 = none

uchar add_const(unsigned int v) {
  uchar i, addr;

  for (i = 0; i < (uchar)nconst; i++)
    if (const_pool[i] == v)
      return (uchar)(CONST_TOP - i);

  if (nconst >= MAX_CONSTS)
    IVM_GEN_FAIL("constant pool full");
  if (CONST_TOP - nconst < FRAME_BASE)
    IVM_GEN_FAIL("constants collided with frames");

  const_pool[nconst] = (uint16_t)v;
  addr = (uchar)(CONST_TOP - nconst);
  nconst++;
  constidx = addr;

  return addr;
}

#define isconst(a) ((uchar)(a) >= constidx && (uchar)(a) <= CONST_TOP)
#define const_val(a) (const_pool[CONST_TOP - (uchar)(a)])

// 0 and 1 are special as '0' '1' byte code can generate
#define IMM0 ((uchar)0xfe)
#define IMM1 ((uchar)0xff)

int is_lit(uchar a) {
  return a == IMM0 || a == IMM1 || isconst(a);
}

unsigned int lit_val(uchar a) {
  if (a == IMM0) return 0;
  if (a == IMM1) return (unsigned int)-1;
  return const_val(a);
}

uchar as_addr(uchar a) {
  if (a == IMM0) return (uchar)add_const(0);
  if (a == IMM1) return (uchar)add_const((unsigned int)-1);
  return a;
}

uchar lit_addr(unsigned int v) {
  if (v == 0) return IMM0;
  if (v == (unsigned int)-1) return IMM1;
  return add_const(v);
}

// emitload/emitstore live in tpi.c (frame-relative); parse uses emit/emitop2

void gen_reset(void) {
  bin_ptr = 0;
  nconst = 0;
  constidx = 255;
  pending_func_entry = 0xff;

#ifndef IVM_GEN_NO_VECTORS
  {
    int i;

    nvector = 0;
    main_offset = 0;
    for (i = 0; i < MAX_VECTORS; i++) {
      vectors[i] = 0;
      frame_base[i] = 0;
    }
  }
#endif

  acc_invalidate();
}

void gen_func_entry(uchar v) {

#ifndef IVM_GEN_NO_VECTORS
  if (v < (uchar)MAX_VECTORS)
    vectors[v] = (uint16_t)bin_ptr;
#else
  (void)v;
#endif

}

// TODO: write why it's doing this?
static word gen_memadd(uchar *base, uchar **pp, const void *src, word n) {
  word off = (word)(*pp - base);

  if (src)
    memcpy(*pp, src, (size_t)n);
  else
    memset(*pp, 0, (size_t)n);

  *pp += n;
  return off;
}

// Build IVM image (magic IVM\0 + sections). Caller free().
uchar *build_blob(int *out_len) {

  typedef struct {
    char magic[4];
    word len;
    word memsz;
    word constidx;
    word main;
    word code;
    word call;
    word fp;
    word con;
    word data;
  } GenBlobHeader;

  GenBlobHeader h;
  uchar *body, *p;
  word zero = 0;
  unsigned need;
  char nv;

#ifndef IVM_GEN_NO_VECTORS
  nv = nvector;
#else
  nv = 0;
#endif

  need = (unsigned)sizeof(GenBlobHeader)
       + (unsigned)nv * sizeof(word) + sizeof(word)
       + (unsigned)nconst * sizeof(word)
       + (unsigned)nv
       + (unsigned)bin_ptr + 8;

  body = (uchar *)malloc(need);
  if (!body) IVM_GEN_FAIL("malloc blob");

  p = body + sizeof(GenBlobHeader);

  h.call = gen_memadd(body, &p, 0, (word)(nv * sizeof(word)));
  gen_memadd(body, &p, &zero, sizeof(zero));
  h.con = gen_memadd(body, &p, const_pool, (word)(nconst * sizeof(word)));

#ifndef IVM_GEN_NO_VECTORS
  h.fp = gen_memadd(body, &p, frame_base, (word)nv);
#else
  h.fp = gen_memadd(body, &p, 0, 0);
#endif

  h.code = gen_memadd(body, &p, bin, (word)bin_ptr);

#ifndef IVM_GEN_NO_VECTORS
  h.main = (word)(h.code + (word)main_offset);
#else
  h.main = h.code;
#endif

  h.data = (word)(p - body);
  h.memsz = (word)(CONST_TOP + 1);
  h.constidx = (word)constidx;
  h.magic[0] = 'I';
  h.magic[1] = 'V';
  h.magic[2] = 'M';
  h.magic[3] = 0;
  h.len = (word)(p - body);

#ifndef IVM_GEN_NO_VECTORS
  {
    int i;
    
    for (i = 0; i < nv; i++)
      ((word *)(body + h.call))[i] = (word)(vectors[i] + h.code);
  }
#endif

  memcpy(body, &h, sizeof(h));
  if (out_len)
    *out_len = (int)h.len;
  return body;
}

#endif // IVM_GEN_C
