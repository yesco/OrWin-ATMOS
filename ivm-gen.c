// Shared IVM codegen: emit buffer, const pool (real slots), ACC mirror, emitload.
// Include after FAIL/IVM_GEN_FAIL is available. No .h — #include "ivm-gen.c"

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
typedef uint16_t word;
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

// optional ref-counting hooks (tpi defines real ones; parse can stub)
void note_load(uchar addr);
void note_store(uchar addr);

// ---------------------------------------------------------------------------
// Bytecode buffer
// ---------------------------------------------------------------------------
char bin[MAX_BIN_LEN];
int bin_ptr = 0;

// ACC mirror: which mem slot ACC currently holds (-1 = none/unknown)
char acc_slot;
char acc_valid;

void acc_invalidate(void) {
  acc_valid = 0;
  acc_slot = (char)-1;
}

void acc_set_slot(char slot) {
  acc_slot = slot;
  acc_valid = 1;
}

void emit(char c) {
  if (bin_ptr >= MAX_BIN_LEN - 1) IVM_GEN_FAIL("bytecode buffer overflow");
  bin[bin_ptr++] = c;
}

void emitop(char op) {
  emit(op);
  if (op == '0' || op == '9' || op == '~' || op == '?' || op == 'G' || op == 'B')
    acc_invalidate();
}

void emitop2(char op, char arg) {
  emitop(op);
  emit(arg);
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
char nconst = 0;
uchar constidx = 255; // lowest const slot; 255 = none

uchar add_const(unsigned int v) {
  uchar i, addr;
  for (i = 0; i < (uchar)nconst; i++)
    if (const_pool[i] == v)
      return (uchar)(CONST_TOP - i);
  if (nconst >= MAX_CONSTS) IVM_GEN_FAIL("constant pool full");
  if (CONST_TOP - nconst < FRAME_BASE) IVM_GEN_FAIL("constants collided with frames");
  const_pool[nconst] = (uint16_t)v;
  addr = (uchar)(CONST_TOP - nconst);
  nconst++;
  constidx = addr;
  return addr;
}

#define isconst(a) ((uchar)(a) >= constidx && (uchar)(a) <= CONST_TOP)
#define const_val(a) (const_pool[CONST_TOP - (uchar)(a)])

#define IMM0 ((uchar)0xfe)
#define IMM1 ((uchar)0xff)

int is_lit(uchar a) {
  return a == IMM0 || a == IMM1 || isconst(a);
}

unsigned int lit_val(char a) {
  if ((uchar)a == IMM0) return 0;
  if ((uchar)a == IMM1) return (unsigned int)-1;
  return const_val(a);
}

char as_addr(uchar a) {
  if (a == IMM0) return (char)add_const(0);
  if (a == IMM1) return (char)add_const((unsigned int)-1);
  return (char)a;
}

char lit_addr(unsigned int v) {
  if (v == 0) return (char)IMM0;
  if (v == (unsigned int)-1) return (char)IMM1;
  return (char)add_const(v);
}

// emitload/emitstore live in tpi.c (frame-relative); parse uses emit/emitop2

void gen_reset(void) {
  bin_ptr = 0;
  nconst = 0;
  constidx = 255;
  acc_invalidate();
}

#endif // IVM_GEN_C
