// IVM - Indirect Virtual Machine
// (C) 2026 Jonas S Karlsson

// Default type to 2 bytes for 6502,
// however, on unix runtime int can be int/long!

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifndef IVMVARTYPE
  #define IVMVARTYPE uint16_t
  typedef IVMVARTYPE ivmvar;
#endif // IVMVARTYPE

// Host soft-heap: ivmvar is often 16-bit and cannot hold a real malloc pointer.
// We store an offset into soft_heap as the "pointer" value on non-cc65.
// On cc65/6502, M/F/@/!/(/) use real addresses.
#ifndef __CC65__
  #define SOFT_HEAP_MAX 4096
  static uchar soft_heap[SOFT_HEAP_MAX];
  static word soft_top;
  #define IVM_PTR(addr) ((ivmvar *)(soft_heap + (unsigned)(addr)))
#else
  #define IVM_PTR(addr) ((ivmvar *)(unsigned)(addr))
#endif



#ifndef UCHAR
  #define UCHAR
  typedef unsigned char   uchar;
  typedef uint16_t        word;
#endif

// --- limits
#define MAX_SLOTS      255 // Last one not used, is error

// Frame layout at fp:
//   fp+0  RETURN
//   fp+1  VAL
//   fp+2  PREV
//   fp+3  ACC home / last param  — JSR always stores ACC here (no prologue S)
//   fp+4… earlier params, then locals
#define FR_RET  0
#define FR_VAL  1
#define FR_PREV 2
#define FR_ACC  3
#define FP_MAIN ((char)0xFF)


// No fixed global image buffer — load_blob/finalise malloc.
extern uchar* g_loaded;


// ===========================================================================
// BLOB: sequential pack, header struct, memcpy sections, 0000 ends reloc.
// ===========================================================================
// PROGRAM BLOB / LOAD / PROCESS / RUN
// ===========================================================================
//
// finalise() builds an *offset-only* blob (constants live in the blob, not
// copied into a 0x80 band of mem as the permanent store).
//
// Layout (all multi-byte fields are offsets from blob base until loader
// rewrites them; reloc walk stops at 0000):
//
//   <prefix / BlobHeader>  // main,code,call,fp,con,data = offsets; memsz = COUNT
//   <call vectors>         // one word per FUNCTION (code entry offset)
//   <0000>                 // end of relocatable vector region
//   NOTE: loader +base must NOT treat memsz as an offset — it is a cell count.
//   <const words>          // pooled constants (NOT relocated)
//   <fp byte array>        // frame_base[v] — subr# → frame pointer slot
//   <code array>           // bytecode
//   <data array>           // future / strings if externalised
//
// loader(base):
//   Relocate first vectors until 0000: while (*v) { *v += base; v++; }
//   That covers BlobHeader prefix + call table. Const section follows 0000
//   and is left as raw values.
//
// Nevm(loaded) → Process:
//   loaded is already relocated (template). Allocates process state:
//     hdr (copy of prefix), ipc, fp, mem, acc, then mem[memsz].
//   Resolves ipc from main; mem → process mem[]; consts via hdr.con.
//   One loaded blob can back many Process instances.
//
// run_ivm(Process *p):
//   memcpy Process head → ZP globals (same layout), interpret, memcpy back.
//   PAR: if operand & 0x80 → consts[n], else mem[slot]
//   (0x80+n is still the bytecode encoding for const indices; can change later)
// ===========================================================================

typedef struct BlobHeader {
  // file identity (not relocated)
  char magic[4]; // "IVM\0"
  word len;      // total image size in bytes

  // counts (not relocated) — sit *before* reloc words so a single
  // while(*v) from &main can walk header reloc fields + call table to 0000
  word memsz;
  word constidx;

  // contiguous relocatable offsets → call table follows this struct in the image
  word main;
  word code;
  word call;
  word fp;
  word con;
  word data;
} BlobHeader;

// Process head — layout of ZP image (memcpy in/out of run_ivm)
typedef struct ProcessHead {
  BlobHeader hdr;
  uchar     *ipc;
  char       fp;
  ivmvar    *mem;
  ivmvar     acc;
} ProcessHead;

typedef struct Process {
  ProcessHead h;
  ivmvar mem[1];
} Process;


// Relocate blob-relative offsets to absolute pointers (cc65/6502 only).
// Header layout: main,code,call,fp,con,data are offsets;
// memsz, constidx, xslot are NOT offsets (must not be walked as pointers).
// Call table at offset `call` ends with 0000.
// BUG FIX: old while(*v) from blob start stopped at xslot==0 and never
// relocated the call vectors → JSR jumped to a tiny offset → IPC in junk.
void relocate(uchar *base) {
  BlobHeader *h;
  word *v;

  h = (BlobHeader *)base;
  // Reloc words are contiguous from main through call table until 0000.
  // memsz/constidx/magic/len sit before main and are not touched.
  v = &h->main;
#ifdef __CC65__
  while (*v) {
    *v = *v + (word)base;
    v++;
  }
#else
  (void)v;
#endif
}

Process* Nevm(uchar *loaded) {
  Process *p;
  unsigned int cells, bytes;
  BlobHeader *bh;

  bh = (BlobHeader *)loaded;
  cells = bh->memsz;
  if (!cells) cells = MAX_SLOTS;

  bytes = (unsigned int)(sizeof(ProcessHead) + cells * sizeof(ivmvar));
  p = (Process *)calloc(1, bytes);
  if (!p) {
    printf("calloc fail %u bytes (%u slots)\n", bytes, cells);
    return NULL;
  }

  memcpy(&p->h.hdr, loaded, sizeof(BlobHeader));

  #ifdef __CC65__
    p->h.ipc = (uchar *)(unsigned)p->h.hdr.main;
  #else
    p->h.ipc = loaded + p->h.hdr.main;
  #endif

  p->h.fp = FP_MAIN;
  p->h.mem = p->mem;
  p->h.acc = 0;

  // Install constants into process mem[] (PAR = mem[idx] only)
  if (bh->constidx != 255 && bh->memsz > bh->constidx) {
    word *pool;
    word nc = bh->memsz - bh->constidx;
    word i;
    #ifdef __CC65__
      pool = (word *)(unsigned)bh->con;
    #else
      pool = (word *)(loaded + bh->con);
    #endif
    // pool[i] lives at mem[constidx + i]
    for (i = 0; i < nc; i++)
      p->mem[bh->constidx + i] = pool[i];
  }
  return p;
}


// Byte codes used:
// ================ 
// 0-31:                - fast var load (load typically 4x more common th store)
// #$ ' , () : \ _ `    - FREE to use byte read/write/indexing?
// 12345678             - local vars 1-7, 8 ist last var (AX)
// ACD H R T VY         - FREE op vars
//   possibly usage: cAr Cons cDr, Heap/Here, Recurse, Tail-call, Vector/index? Yndex?
//
// 23: a-w              - FREE OrwinOS API or system library strXX functions?
// ? abcdefgh           - store local (need analyze code savings)
// ? ijklmnop           - index stuff, built in loops iGG GG if zero/non-zero?
// ? qrstuvw
// ? xyz                - syscall function LN with 1,2,3 byte args indices+acc
//
// HIBIT
//   64: 00-3f            - conditional relative jump forward
//   64: (xx)             - vectored calls (max 64 funs)
// 
// Notes:
//   ;  - load acc w return register (pascal) and do return
//   X  - return with value in acc
//   Q  - return with acc = 0
// 
// Missing: (q)
//   memove  v
//   memset  w
//   strcpy  p
//   strcat  t
//   strchr  r
//   strstr  s
//   strdup  u
//   atoi    q
//
//   strcmp  $
//   strlen  `
//   strN... ...


// TODO: make them zeropage cc65/oscar64

// ZP global (same layout as ProcessHead)
ProcessHead zp;

//uchar op;


// Just enable to see compilation size differnce
#ifdef NOIVM

// (- 9396 5047) == 4349 bytes for JUST the VM!

ivmvar run_ivm(Process *p) {
  return 0;
  (void)p;
}

#else

#ifdef IVMASM

// -DIVMASM + link ivm-vm.s
// Single ZP block for importzp; process switch = save/restore this block.

#ifdef __CC65__
#pragma bss-name (push, "ZEROPAGE")
#endif

uchar *ivm_ipc;
char   ivm_fp;
ivmvar ivm_acc;
uchar  ivm_op;
uchar *ivm_memlo;
uchar *ivm_memhi;

#ifdef __CC65__
#pragma bss-name (pop)
#endif

static uchar slice_lo[256];
static uchar slice_hi[256];
static ivmvar *live_mem;
static word live_n;

static void pivot(ivmvar *mem, word n) {
  word i;
  live_mem = mem;
  live_n = n > 256 ? 256 : n;
  for (i = 0; i < live_n; i++) {
    slice_lo[i] = (uchar)(mem[i] & 0xff);
    slice_hi[i] = (uchar)(mem[i] >> 8);
  }
  for (; i < 256; i++)
    slice_lo[i] = slice_hi[i] = 0;
  ivm_memlo = slice_lo;
  ivm_memhi = slice_hi;
}

static void unpivot(void) {
  word i;
  for (i = 0; i < live_n; i++)
    live_mem[i] = (ivmvar)slice_lo[i] | ((ivmvar)slice_hi[i] << 8);
}

extern ivmvar run_ivm_asm(void);

// ASM fallback for one complex opcode
void run_ivm_once(void) {
  uchar op;
  unpivot();
  op = *ivm_ipc++;
  switch (op) {
  case '.':
    printf("%u", (unsigned)ivm_acc);
    break;
  case 'N':
    putchar('\n');
    break;
  case '"':
    while (*ivm_ipc)
      putchar(*ivm_ipc++);
    ivm_ipc++;
    break;
  case '*':
    ivm_acc *= live_mem[*ivm_ipc++];
    break;
  case '/':
    if (live_mem[*ivm_ipc])
      ivm_acc /= live_mem[*ivm_ipc];
    ivm_ipc++;
    break;
  case '%':
    if (live_mem[*ivm_ipc])
      ivm_acc %= live_mem[*ivm_ipc];
    ivm_ipc++;
    break;
  default:
    if (op >= 0xC0)
      printf("[IVMASM] JSR vec %u unhandled\n", op & 0x3F);
    else
      ivm_ipc++;
    break;
  }
  pivot(live_mem, live_n);
}

ivmvar run_ivm(Process *p) {
  word cells;
  cells = p->h.hdr.memsz;
  if (!cells) cells = MAX_SLOTS;

  pivot(p->mem, cells);
  ivm_ipc = p->h.ipc;
  ivm_fp  = p->h.fp;
  ivm_acc = p->h.acc;

  ivm_acc = run_ivm_asm();

  p->h.ipc = ivm_ipc;
  p->h.fp  = ivm_fp;
  p->h.acc = ivm_acc;
  unpivot();
  return ivm_acc;
}

#else // !IVMASM




ivmvar run_ivm(Process *p) {
  // - parameter ops use PAR then goto pnext
  // - zero-arg ops goto xnext
  //
  // ZP: memcpy Process head ↔ globals (ivm_hdr, ipc, fp, mem, acc)

  register uchar op; // register => zp on cc65: 2% faster!
  
  // load process head → ZP global
  memcpy(&zp, &p->h, sizeof(zp));
  zp.mem = p->mem;

  #ifdef __CC65__
    // pool only used if needed elsewhere
  #else
    g_loaded = (uchar *)p->h.ipc - p->h.hdr.main;
  #endif

  // Unified address space: consts were copied into mem[] by Nevm
  #define PAR (zp.mem[*zp.ipc])

  goto xnext;

  // Eat up one byte parameter from last op
 pnext:
  ++zp.ipc;

  // Next when no parameters
  // TODO: 26 x cmp beq ...
  //         net ++pc lda  jmp cmp/beq
  //   avg (+ 3  8    5    1   (* 13 4)) = 69c
  //   69c dispatch-overhead AT LEAST!

  // TODO: oscar64 gives strange error if this is changed to next!
  //   maybe thinking it's c++? LOL: std::next() ???


 xnext:
  op = *zp.ipc++;

  // fused load 0..31 — before table (opcode itself is the slot)
  // TODO: maybe faster, but slows everything else down!
  // TODO: make it a label jump after test
  if (op < 32) {
    zp.acc = zp.mem[op];
    goto xnext;
  }

  // Dispatch: USE_LABELS → goto *vt[oi] (oi is uchar, never an expression).
  // Array is 128 entries (indices 0..127); hibit opcodes use op_hibit.
  // Z(c) → "case c:" when switch path, empty when label path.

#ifdef USE_LABELS
  {
    static void *vt[128];
    static uchar inited;
    if (!inited) {
      // build once in a local table, then memcpy → static (no per-op ifdefs)
      void *t[128];
      uchar k;
      for (k = 0; k < 128; k++) t[k] = &&op_unk;
      t['q'] = &&op_q;   t['Q'] = &&op_Q;   t['X'] = &&op_X;
      t[';'] = &&op_semi;
      t['8'] = &&op_8;
      t['1'] = &&op_17;   t['2'] = &&op_17;  t['3'] = &&op_17;
      t['4'] = &&op_17;   t['5'] = &&op_17;  t['6'] = &&op_17; t['7'] = &&op_17;
      t['+'] = &&op_add;  t['-'] = &&op_sub; t['*'] = &&op_mul;
      t['/'] = &&op_div;  t['%'] = &&op_mod;
      t['&'] = &&op_and;  t['|'] = &&op_or;  t['^'] = &&op_xor;
      t['['] = &&op_shl;  t[']'] = &&op_shr;
      t['{'] = &&op_shlm; t['}'] = &&op_shrm;
      t['L'] = &&op_L;    t['S'] = &&op_S;   t['Z'] = &&op_Z;
      t['M'] = &&op_M;    t['F'] = &&op_F;
      t['@'] = &&op_at;   t['!'] = &&op_bang;
      t['('] = &&op_idxl; t[')'] = &&op_idxs;
      t['I'] = &&op_I;    t['J'] = &&op_J;   t['U'] = &&op_U;
      t['<'] = &&op_lt;   t['='] = &&op_eq;  t['>'] = &&op_gt;
      t[' '] = &&op_sp;
      t['0'] = &&op_0;    t['9'] = &&op_9;
      t['?'] = &&op_not;  t['~'] = &&op_bnot;
      t['K'] = &&op_K;    t['E'] = &&op_E;   t['N'] = &&op_N;
      t['P'] = &&op_P;    t['O'] = &&op_O;   t['W'] = &&op_W;
      t['.'] = &&op_dot;  t['"'] = &&op_str;
      t['G'] = &&op_G;    t['B'] = &&op_B;
      memcpy(vt, t, sizeof(vt));
      inited = 1;
    }
    if (op & 0x80) goto op_hibit;
    goto *vt[op];
  }

  #define Z(c)
  #define ZL(label, c) label:
  #define L(label) label:
  

#else


  switch (op) {

  #define Z(c)         case (c):
  #define ZL(label, c) case (c):
  #define L(label)

#endif


// Quit: TODO: replace by syscall!
ZL(op_q,   'q')              goto done;

// ReturnQ= return 0  X= return acc;  ';'=return RETURN;
ZL(op_Q,   'Q')  zp.acc = 0; goto op_X;
ZL(op_semi,';')
    zp.acc = zp.mem[zp.fp + FR_VAL];
    // fall through
op_X:    Z('X')
    // TODO: remove
    if (zp.fp == FP_MAIN)    goto done;

    #ifdef __CC65__
      zp.ipc = (uchar *)zp.mem[zp.fp + FR_RET];
    #else
      zp.ipc = g_loaded + zp.mem[zp.fp + FR_RET];
    #endif

    zp.fp = (char)zp.mem[zp.fp + FR_PREV];
    goto xnext;

// Local/parameters access 1..7, 8=last!
L(op_8)  Z('8')  zp.acc = zp.mem[zp.fp + FR_ACC]; goto xnext;
L(op_17) Z('1') Z('2') Z('3') Z('4') Z('5') Z('6') Z('7')
    zp.acc = zp.mem[zp.fp + FR_ACC + (op - '0')];
    goto xnext;

// Math ops
ZL(op_add, '+')  zp.acc += PAR; goto pnext;
ZL(op_sub, '-')  zp.acc -= PAR; goto pnext;
ZL(op_mul, '*')  zp.acc *= PAR; goto pnext;
ZL(op_div, '/')  if (PAR) zp.acc /= PAR; goto pnext;
ZL(op_mod, '%')  if (PAR) zp.acc %= PAR; goto pnext;
ZL(op_and, '&')  zp.acc &= PAR; goto pnext;
ZL(op_or,  '|')  zp.acc |= PAR; goto pnext;
ZL(op_xor, '^')  zp.acc ^= PAR; goto pnext;
ZL(op_shl, '[')  zp.acc <<= PAR; goto pnext;
ZL(op_shr, ']')  zp.acc >>= PAR; goto pnext;
ZL(op_shlm,'{')  zp.mem[*zp.ipc] <<= zp.acc; goto pnext;
ZL(op_shrm,'}')  zp.mem[*zp.ipc] >>= zp.acc; goto pnext;

// Memory ops
ZL(op_L,   'L')  zp.acc = PAR;             goto pnext;
ZL(op_S,   'S')  zp.mem[*zp.ipc] = zp.acc; goto pnext;
ZL(op_Z,   'Z')  zp.mem[*zp.ipc] = 0;      goto pnext;

ZL(op_M,   'M')
    // operand = slot of pointer var; ACC = size in bytes
    {
      unsigned n = (unsigned)zp.acc;
      #ifdef __CC65__
        zp.mem[*zp.ipc] = (ivmvar)(unsigned)calloc(n, 1);
      #else
        if (soft_top + n > SOFT_HEAP_MAX) n = 0;
        zp.mem[*zp.ipc] = (ivmvar)soft_top;
        if (n) { memset(soft_heap + soft_top, 0, n); soft_top += n; }
      #endif
      zp.acc = zp.mem[*zp.ipc];
    }
    goto pnext;


ZL(op_F,   'F')
    // operand = slot of pointer var; free and clear
    #ifdef __CC65__
      free((void *)(unsigned)zp.mem[*zp.ipc]);
    #else
      // soft-heap is bump-only; just drop the reference
    #endif
    zp.mem[*zp.ipc] = 0;
    goto pnext;


// pointer in slot is a raw address (heap/static), not a mem[] index
ZL(op_at,  '@')  zp.acc = *IVM_PTR(PAR); goto pnext;
ZL(op_bang,'!')  *IVM_PTR(PAR) = zp.acc; goto pnext;

// indexed: pointer in base slot, element index in idx slot
ZL(op_idxl,'(')
    zp.acc = IVM_PTR(zp.mem[zp.ipc[0]])[zp.mem[zp.ipc[1]]];
    zp.ipc += 2; goto xnext;
ZL(op_idxs,')')
    IVM_PTR(zp.mem[zp.ipc[0]])[zp.mem[zp.ipc[1]]] = zp.acc;
    zp.ipc += 2; goto xnext;

ZL(op_I,   'I')  zp.acc = ++zp.mem[*zp.ipc]; goto pnext;
ZL(op_J,   'J')  zp.acc = --zp.mem[*zp.ipc]; goto pnext;
ZL(op_U,   'U')  zp.acc = (zp.mem[zp.ipc[0]]
                        += zp.mem[zp.ipc[1]]);
                                zp.ipc += 2; goto xnext;

ZL(op_lt,  '<')  zp.acc = zp.acc < PAR;  goto pnext;
ZL(op_eq,  '=')  zp.acc = zp.acc == PAR; goto pnext;
ZL(op_gt,  '>')  zp.acc = zp.acc > PAR;  goto pnext;

ZL(op_sp,  ' ')                         goto xnext;
ZL(op_0,   '0')  zp.acc = 0;            goto xnext;
ZL(op_9,   '9')  zp.acc = 0xffff;       goto xnext;
ZL(op_not, '?')  zp.acc = !zp.acc;      goto xnext;
ZL(op_bnot,'~')  zp.acc = ~zp.acc;      goto xnext;

ZL(op_K,   'K')  zp.acc = getchar();    goto xnext;
ZL(op_E,   'E')  putchar(zp.acc);       goto xnext;
ZL(op_N,   'N')  putchar('\n');         goto xnext;
ZL(op_P,   'P')  printf("%u", (unsigned)zp.acc); goto xnext;
ZL(op_O,   'O')  putchar(PAR);          goto pnext;
ZL(op_W,   'W')  printf("%u", PAR);     goto pnext;
ZL(op_dot, '.')  printf("%u", zp.acc);  goto xnext;
ZL(op_str, '"')  while (*zp.ipc) putchar(*zp.ipc++);
                                        goto pnext;

ZL(op_G,   'G')  if ((int16_t)zp.acc > 0) zp.ipc += *zp.ipc; goto pnext;
ZL(op_B,   'B')  zp.ipc -= *zp.ipc; goto pnext;

  #ifndef USE_LABELS
  default:
  #endif

L(op_unk)
L(op_hibit)
    if (op >= 0x80) {
      if (op < 0xc0) {
        if (!zp.acc)
          zp.ipc += op & 0x3f;
        goto xnext;
      }

      // vectorised JSR
      {
        char v = op & 0x3f;
        char nfp;
        word *callv;
        #ifdef __CC65__
          nfp = ((uchar *)(unsigned)zp.hdr.fp)[v];
          callv = (word *)(unsigned)zp.hdr.call;
          zp.mem[nfp + FR_PREV] = zp.fp;
          zp.mem[nfp + FR_RET] = (unsigned int)zp.ipc;
          zp.mem[nfp + FR_ACC] = zp.acc;
          zp.fp = nfp;
          zp.ipc = (uchar *)(unsigned)callv[v];
        #else
          nfp = g_loaded[zp.hdr.fp + v];
          callv = (word *)(g_loaded + zp.hdr.call);
          zp.mem[nfp + FR_PREV] = zp.fp;
          zp.mem[nfp + FR_RET] = (unsigned int)(zp.ipc - g_loaded);
          zp.mem[nfp + FR_ACC] = zp.acc;
          zp.fp = nfp;
          zp.ipc = g_loaded + callv[v];
      #endif
      }
      goto xnext;
    }
    printf("\n[IVM] unknown opcode 0x%02X '%c'\n", op, isprint(op) ? op : 0);

#undef Z

#ifndef USE_LABELS
  } // switch
#endif

 done:

  // TODO: crashes on linux ./tpi-test-unix when -DUSE_LABELS
  //   ???

  // save ZP global → process head
  memcpy(&p->h, &zp, sizeof(zp));

  return zp.acc;

#undef PAR
}
#endif // !IVMASM

#endif // !IVMSIZE



// Load whole IVM image (malloc). Caller may free().
// Image = BlobHeader (magic "IVM\0", len, …) + body; one buffer, no global required.
uchar *load_blob(const char *path, int *out_len) {
  FILE *f = NULL;
  uchar *img = NULL;
  int n, cap;
  BlobHeader hdr;

  if (out_len) *out_len = 0;

  f = fopen(path, "rb");
  if (!f) {
    printf("cannot open %s\n", path);
    return NULL;
  }

  // read size from header without trusting a fixed MAX
  if ((int)fread(&hdr, 1, sizeof(hdr), f) != (int)sizeof(hdr)) {
    printf("short header %s\n", path);
    goto err;
  }

  // magic check
  if (hdr.magic[0] != 'I' || hdr.magic[1] != 'V'
    || hdr.magic[2] != 'M' || hdr.magic[3] != 0) {
    printf("not an IVM file: %s\n", path);
    goto err;
  }

  // constisten? TODO: what is cap?
  cap = (int)hdr.len;
  if (cap < (int)sizeof(BlobHeader)) {
    printf("bad IVM len %s\n", path);
    goto err;
  }

  img = (uchar *)malloc((unsigned)cap);
  if (!img) {
    printf("malloc %d fail\n", cap);
    goto err;
  }
  memcpy(img, &hdr, sizeof(hdr));

  n = (int)fread(img + sizeof(hdr), 1, (unsigned)(cap - (int)sizeof(hdr)), f);
  if (n != cap - (int)sizeof(hdr)) {
    printf("short read %s\n", path);
    goto err;
  }

  if (out_len) *out_len = cap;
  printf("loaded %s (%d bytes)\n", path, cap);

  relocate(img);

err:
  if (f) fclose(f);
    
  return img;
}

char save_blob(const char *path, uchar *img, int len) {
  FILE *f;

  f = fopen(path, "wb");
  if (!f) {
    printf("cannot write %s\n", path);
    return 0;
  }
  if ((int)fwrite(img, 1, (unsigned)len, f) != len) {
    printf("write failed %s\n", path);
    fclose(f);
    return 0;
  }
  fclose(f);
  printf("wrote %s (%d bytes)\n", path, len);
  return 1;
}

static int run_loaded_blob(uchar *img, int len) {
  unsigned int ret;
  Process *proc= Nevm(img);

  if (!proc) return -1;

  printf("--- RUN IVM (blob %d) ---\n", len);
  ret = run_ivm(proc);
  printf("\n--- END ---\n");
  printf("acc = %u\n", ret);
  free(proc);
  return ret;
}

int run_file(char* filename) {
  int len;
  uchar *img= load_blob(filename, &len);

  if (!img) return -1;
  {
    int r= run_loaded_blob(img, len);
    free(img);
    return r;
  }
}


#ifndef MAIN
int main(int argc, char** argv) {

  if (argc==2)
    return run_file(argv[1]);

  return 1;
}
#endif // MAIN
