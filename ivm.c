// IVM - Indirect Virtual Machine
// (C) 2026 Jonas S Karlsson

// Default type to 2 bytes for 6502,
// however, on unix runtime int can be int/long!

// Indirect Virtual Machine
// ========================
//
// A byte-coded register virtual machine, where
// op-code are mostly alphabetcially readable.
//
//
// Features:
// - ACCumulator based arch. OPs morestly modify ACC.
// - Unified global/const/stack/parameter/locals!
// - alphabetical memomic byte codes ('L" - load...)
// - register machine (most: OP iADDR or just OP)
// - 64 indirect call vectors by single byte code call
// - optimized call by last parameter in ACCumulator
// - 32 most common vars, single byte load (more common than save)
// - local var access single byte (max 8)
// - max 256 registers: [0, N[ per process
// - no constants! folded into higher registers...
// - no stack - using FORTRAN-style STATIC STACK
// - Pure C implementation (2.75x 6502 asm VM, 10x compiled asm)
//
//
// STATIC STACK
// ------------
// It's observed that even large programs most have
// less than 256 variables active at any point, this includes
// all global variables, and parameters and local variables
// active on the stack. An array is counted as one variable.
// Recursion isn't directly available. It can be implemented
// using copying to heap allocated objects.
//
// Each function has a logical parameter block:
//   0: return address
//   1: return value
//   2: frame pointer (1 byte free)
//   3: last parameter (load by '8')
// 3+n: parameter n
// ...: local var/temp...
//
// Using a simple call-graphc analysis, functions that
// do not call each other overlap in storage can be packed
// into a single static stack stack.
//   For example, 80 slots comes down to 14 slots! This is
// the job of a compiler/optimizer. This job has been
// done when this engine ivm.c or ivm-vm.asm is run.
// 
//
// MEMORY
// ------
// During optimization, the optimizer should just relocate
// the variable map, and when "compiling" a second time,
// can eactly relocate to using those slots, and more
// efficient instructions, like one-byte loads/saves.
//
// Memory is packed as follows:
//    0-... : registers ++ static stack ++ variables ++ consts
//
// A simple load count analysis, with adoption to recognize
// if an op is inside a loop and thus boost it usage +100 (?),
// should promote the variable to a "register". There are
// single byte load instructions for 0-31 (currently not svae
// pending real usage), where one can estimate byte savings,
// as well as speed saving.
//
// During (second) compilation. A simple emitop('L', index)
// can generate the cheaper single-byte code instruction.
//
// A separate counter, can not if "inside" loop. At least
// directly for the code in that function.
//
// TODO: you could boost a function (set a bit) if it's
// called inside a function and thereby boost it's global
// access (load) count, too.


// BYTE CODES OVERVIEW
// ====================
// 0-31                 - fast var load (load typically 4x more common th store)
// #$ \ (:_) (`)        - FREE to use
// 12345678             - local vars 1-7, 8 ist last var (AX)
// ACD H R T VY         - FREE op vars
//   possibly usage: cAr Cons cDr, Heap/Here, Recurse, Tail-call, Vector/index? Yndex?
//
// 23: a-w              - FREE OrwinOS API or system library strXX functions?
// ? abcd   efgh        - ?store local (need analyze code savings)
// ? ijk(l) mnop        - ?index stuff, built in loops iGG GG if zero/non-zero?
// ? q(rs)t uv(w)       - ?SSTACK saves, param: 1..7 (ACC is 0th)
// ? xyz                - syscall function LN with 1,2,3 byte args indices+acc
//


// HIBIT
//   64: 00-3f            - conditional relative jump forward
//   64: (xx)             - vectored calls (max 64 funs)




// BYTE CODE DETAILS
// =================
//
//? === not implemented
//
//0-31: global memory read value
// ' '- nop
//  ! - store at global address ACC
//  " - inline print string end with \0
//? # - number? TODO: atoi?!?!
//? $ - string ptr at end? $ == \
//  % - mod
//  & - and
//  ' - read next char using pointer++ "'vv": ACC= *vv++;
//  ( - load from indexed array "(aaii" to ACC
//  ) - store to indexed array ACC to ")aaii"
// *+ - mul, add
//  , - write char: inc ptr ",vv": *vv++= ACC;
//  - - subtract
//  . - print number
//  / - div
//  0 - ACC = 0/null/nil
//  1234576 - parameter/local var read
//  8 - first parameter
//  9 - ACC= -1/TRUE/0xffff
//? : - store ACC by name :cc (cc - const char* ptr), see _
//  ; - return (ACC=returnvalue, for pascal), see X and Q, too.
//  <=> - cmp - lt eq gt => 0 or 1
//  ? - NOT acc (C: !)
//  @ - read from global address ACC
//? A - car acc= acc[0] == @
//  B - back jump "B03" = back 3 bytes
//? C - cons(acc, $) - a variant of alloc cell and , ???
//? D - cdr acc= acc[1] == 2 + @, LOL
//  E - emit char = putchar(acc)
//  F - free var, set it 0 "Fvv"
//  G - conditional acc > 0: goto 1-255
//? H - TODO: heap/here something?
//  IJ - inc/dec var, leave val in ACC
//  K - key = getchar (waits?)
//  L - load var
//  M - calloc to variable and acc "Mvv"
//  N - print newline 
//  O - output next byte "O:" putchar(':')
//  P - print string from ACC global address, acc points to \0
//  Q - "Quit" local function == return 0;
//? R - recurse (save current frame, malloc?)
//  S - store ACC in variable "Svv"
//? T - type #ACCc chars from arg (or reverse), or Tailcall: Tii
//  U - Unified op: ACC= var2 += var2
//? V - TODO: same using ACC as value?  Vss == +ssSss (2 not 4 B)
//  W - write var as number (unsigned) "Wvv"
//  X - return ACC; see ';' and Q
//? Y - 
//  Z - zero variable Zvv
//
//? a-h - save local param (h needed, meaning ACC?)
//? ijk - indexing stuff?
//  l   - load byte from slot
//? mn  -
//? op  - 
//? qr  -
//  s   - save byte in slot (reset hibit)
//? t   -
//? u   - similar U ? or not need
//? vw
//  xyz - call library functions w 2..4 args

//  [ - shift left variable steps "[vv]
//? '\' possible use instead of 'O'!!! \ is like literal... lol
//  ] - shift rigth varible steps "]vv"
//  ^ - xor "^vv"
//? _ - load ACC by name _cc (cc - const char* ptr)

//" ` - 
//  { - shift ACC left fixed steps "{nn}
//  | - or w var "|vv"
//  } - shift ACC right "}nn"
//
//  ~ - inverse ACC (oric: gray)
//? - TODO: NEXTPAGE: DEL not visible (oric: full block)
//
// $80-$bf (64) conditional relative jump (forward) get rid of G
// $bf-$ff (64) subroutine calls (vectorized)


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>


#define COUNTOPS

#ifdef COUNTOPS 
unsigned long countops = 0;
#endif


#ifndef IVMVARTYPE
  #define IVMVARTYPE uint16_t
  typedef IVMVARTYPE ivmvar;
#endif // IVMVARTYPE


#ifndef UCHAR
  #define UCHAR
  typedef unsigned char   uchar;
  typedef uint16_t        word;
#endif



// Host soft-heap: ivmvar is often 16-bit and cannot hold a real malloc pointer.
// We store an offset into soft_heap as the "pointer" value on non-cc65.
// On cc65/6502, M/F/@/!/(/) use real addresses.
#ifndef __CC65__
  #define SOFT_HEAP_MAX 4096
  uchar soft_heap[SOFT_HEAP_MAX];
  word soft_top;
  #define IVM_PTR(addr) ((ivmvar *)(soft_heap + (unsigned)(addr)))
#else
  #define IVM_PTR(addr) ((ivmvar *)(unsigned)(addr))
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



// TODO: make them zeropage cc65/oscar64

// ZP global (same layout as ProcessHead)
ProcessHead zp;



// Missing: (q)
//   memove  v
//   memset  w
//   strcpy  p
//   strcat  t
//   strchr  r
//   strstr  s - test... 
//   strdup  u
//   atoi    q
//
//   strcmp  $
//   strlen  `
//   strN... ...

// 5 args  - bsearch(key, base, num, size, compare)
// cc65 UI - draw_window(x, y, width, height, title)
//         - draw_sprite(x, y, sprite_id, palette, flip_flags)
//
// - strtoimax_l(nptr, endptr, base, loc) / Complex String Parsers: Some extended localized string parsing functions in POSIX or C23 use 5 to 7 arguments to pass locales and error states.
// - mmap(addr, length, prot, flags, fd, offset) (6 Arguments): A foundational POSIX system call used to map files or devices into memory.
// - sendto(sockfd, buf, len, flags, dest_addr, addrlen) (6 Arguments): The standard network socket function used to send UDP packets.
//
// Just pass a struct... lol

// TODO: rename to 2,3,4? lol
typedef word (*libfun_2)(word acc, word a);
typedef word (*libfun_3)(word acc, word a, word b);
typedef word (*libfun_4)(word acc, word a, word b, word c);


#define LIBRARY

#ifndef LIBRARYY

void*** library[16]= { 0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  };

#else // LIBRARY

void* osfuns_N[]= {
  0,0,0,0,
  0,0,0,0,
  0,0,0,0,
  0,0,0,0,
};
// Just 0-15, no duplications
void** osfuns[]= { osfuns_N, osfuns_N, osfuns_N };


void* fsfuns_N[]= {
  0,0,0,0,
  0,0,0,0,
  0,0,0,0,
  0,0,0,0,
};
// Just 0-15, no duplications
void** fsfuns[]= { fsfuns_N, fsfuns_N, fsfuns_N };

void* winfuns_N[]= {
  0,0,0,0,
  0,0,0,0,
  0,0,0,0,
  0,0,0,0,
};
// Just 0-15, no duplications
void** windowfuns[]= { winfuns_N, winfuns_N, winfuns_N };


// "X<str><14><what>" (3 byte sequence)
// n: isalnum, a: isalpha, c: iscntrl,  d: isidigit
// g: isgraph, l: islower, p: isprint,  .: ispunct
// s: isspace, u: isupper, x: isxdigit, A: isascii
// b: isblank, h: hibit,   ...
int isWHAT(int c, int w) {
  switch(w) {
  // (+ 4 4 3 2) = 13
  case 'n': return isalnum(c);
  case 'a': return isalpha(c);
  case 'c': return iscntrl(c);
  case 'd': return isdigit(c);

  case 'g': return isgraph(c);
  case 'l': return islower(c);
  case 'p': return isprint(c);
  case '.': return ispunct(c);

  case 's': return isspace(c);
  case 'u': return isupper(c);
  case 'x': return isxdigit(c);
  case 'A': return isascii(c);

  case 'b': return isblank(c);
  case 'h': return c & 0x80? 1: 0; // hibit

  default:  return 0;
  }
};


#ifdef __CC65__

  #define BZERO

#else

  // linux
  #define STRNDUP

#endif



#ifndef MEMDUP
  #define MEMDUP

  void* memdup(void* p, word n) {
    char* r= NULL;

    if (!p || !n) return NULL;

    r= calloc(n, 1);
    if (r) memmove(r, p, n);
    return r;
  }

#endif // MEMDUP

#ifndef STRNDUP
  #define STRNDUP

  uchar* strndup(uchar* s, word n) {
    word l = s? strlen((char*)s): 0;
    word z = (n<l? n: l) + 1;

    return memdup(s, z==1? 0: z);
  }

#endif // STRNDUP

// just use mine, lol
void* ivm_bzero(void* p, word n) {
  return memset(p, 0, n);
}

word strprefix(char* a, char* b) {
  word n= 0;

  if (!a || !b) return 0;
  
  while(*a==*b && *a++ && *b++) ++n;
  
  return n;
}
  
uchar* ivm_memmem(uchar* a, uchar* b) {
  assert(!"TODO");
  return NULL;
  (void)a; (void)b;
}

void* strfuns_2[]= {
  ivm_bzero,   0,        strcpy,     strlen,
  strchr,      strrchr,  strstr,     strcmp,
  strcasecmp,  strcpy,   strcat,     strndup,
  strprefix,   isWHAT,   atoi,       0,
};
  
void* strfuns_3[]= {
  memset,      memmove,  strncpy,    0,
  0,           0,        ivm_memmem, strncmp,
  strncasecmp, strncpy,  strncat,    memdup,
  atoi,        strspn,   strcspn,    strpbrk,
};
  
// we dispatch on the NUBMER of ARGUMENTS! (max 32 funs!)
void** strfuns[]= { strfuns_2, strfuns_3, NULL };


void*** library[16]= {
  osfuns,      fsfuns,   windowfuns, strfuns,
  0,           0,        0,          0,
  0,           0,        0,          0,
  0,           0,        0,          0,
};

#endif // LIBRARYY

#if 1

word sys(uchar n, word acc, uchar* ipc) {
  word id = ipc[0];
  char fidx = id & 0x0f;
  char lib_idx = id >> 4;
  void **liblib, **libbase;
  
  if (n > 3) return (word)-1;
  
  liblib = (void*)(library[lib_idx]);
  if (!liblib) return (word)-1;
  
  libbase = (void**)(liblib[n-1]);
  if (!libbase || !libbase[fidx]) return (word)-1;

  switch(n) {
  case 1:  return ((libfun_2)libbase[fidx])(acc, ipc[1]);
  case 2:  return ((libfun_3)libbase[fidx])(acc, ipc[1], ipc[2]);
  case 3:  return ((libfun_4)libbase[fidx])(acc, ipc[1], ipc[2], ipc[3]);
  default: return (word)-1;
  }
}

#else
  
// n is number of args in addition to ACC
word sys(char n, word acc, char* ipc) {
  word id= ipc[0], a= ipc[1], b= ipc[2], c= ipc[3];
  libfun_2* libbase= library[id >> 4][n-1];
  char fidx= id & 0x0f;
  
  // error
  if (!libbase) n= -1;
     
  switch(n) {
  case 1:  return (*(libfun_2)(libbase[fidx])) (acc, a);
  case 2:  return (*(libfun_3)(libbase[fidx])) (acc, a, b);
  case 3:  return (*(libfun_4)(libbase[fidx])) (acc, a, b, c);
  default: return (word)-1;
  }
}

#endif

// Just enable to see compilation size differnce
#ifdef NOIVM

// (- 9396 5047) == 4349 bytes for JUST the VM!

ivmvar run_ivm(Process *p) {
  return 0;
  (void)p;
}

#else

#ifdef IVMASM

// Single ZP block for importzp (_name in asm). Process switch = save/restore.

#ifdef __CC65__
#pragma bss-name (push, "ZEROPAGE")
#endif
uchar *ivm_ipc;
char   ivm_fp;
ivmvar ivm_acc;
uchar  ivm_op;
uchar *ivm_memlo;
uchar *ivm_memhi;
ivmvar ivm_par;   // ASM stores PAR here before opmul/opdiv/opmod
uchar *ivm_fptab; // frame_base[v]
word  *ivm_callv; // call vectors (abs on cc65 after relocate)
#ifdef __CC65__
#pragma bss-name (pop)
#endif

// TODO: pivot in-place during loading
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

// TODO: maybe not needed
static void unpivot(void) {
  word i;

  for (i = 0; i < live_n; i++)
    live_mem[i] = (ivmvar)slice_lo[i] | ((ivmvar)slice_hi[i] << 8);
}

extern ivmvar run_ivm_asm(void);

// C helpers — ASM jsr _opmul etc; ACC/PAR already in ZP
void opmul(void)    {              ivm_acc *= ivm_par; }
void opdiv(void)    { if (ivm_par) ivm_acc /= ivm_par; }
void opmod(void)    { if (ivm_par) ivm_acc %= ivm_par; }
void opputnum(void) { printf("%u", (unsigned)ivm_acc); }

// TODO: add a run_ivm-for-a-while

ivmvar run_ivm(Process *p) {
  word cells;

  cells = p->h.hdr.memsz;
  if (!cells) cells = MAX_SLOTS;

  pivot(p->mem, cells);
  ivm_ipc = p->h.ipc;
  ivm_fp  = p->h.fp;
  ivm_acc = p->h.acc;
  #ifdef __CC65__
    // relocated absolute pointers
    ivm_fptab = (uchar *)(unsigned)p->h.hdr.fp;
    ivm_callv = (word *)(unsigned)p->h.hdr.call;
  #else
  {
    uchar *base = (uchar *)p->h.ipc - p->h.hdr.main;
    ivm_fptab = base + p->h.hdr.fp;
    ivm_callv = (word *)(base + p->h.hdr.call);
    // host: callv[] still relative offsets (16-bit); asm is for 6502
  }
  #endif

  ivm_acc = run_ivm_asm();

  p->h.ipc = ivm_ipc;
  p->h.fp  = ivm_fp;
  p->h.acc = ivm_acc;
  // no needed
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
      t['l'] = &&op_l;    t['s'] = &&op_s;
      t['M'] = &&op_M;    t['F'] = &&op_F;
      t['@'] = &&op_at;   t['!'] = &&op_bang;
      t['r'] = &&op_r;    t['w'] = &&op_w;
      t['\'']= &&op_ri;   t[','] = &&op_wi;
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
      t['x'] = &&op_x;    t['y'] = &&op_y;   t['z'] = &&op_z;
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


#ifdef COUNTOPS
  countops++;
#endif
  
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


// TODO: Global name address lookup (sys registry)

// Memory ops - slots
ZL(op_L,   'L')  zp.acc = PAR;             goto pnext;
ZL(op_S,   'S')  zp.mem[*zp.ipc] = zp.acc; goto pnext;
ZL(op_Z,   'Z')  zp.mem[*zp.ipc] = 0;      goto pnext;

// Memory ops - word raw using variable
// TODO: why not use ACC???
ZL(op_at,  '@')  zp.acc = *IVM_PTR(PAR); goto pnext;
ZL(op_bang,'!')  *IVM_PTR(PAR) = zp.acc; goto pnext;


// We use a full 2 byte slot for single byte var, lol
ZL(op_l,   'l')  zp.acc = *(char*)(zp.mem+*zp.ipc); goto pnext;
ZL(op_s,   's')  zp.mem[*zp.ipc] = zp.acc & 0xff;   goto pnext;

ZL(op_r,   'r')  zp.acc = *IVM_PTR(PAR) & 0xff; goto pnext;
ZL(op_w,   'w')  *IVM_PTR(PAR) = zp.acc & 0xff; goto pnext;

// string (char) pointer walkers
ZL(op_ri,  '\'') zp.acc = *(uchar*)(PAR++); goto pnext;
ZL(op_wi,  ',' ) *(uchar*)(PAR++) = zp.acc; goto pnext;


// indexed: (aaii, aa is index of base address, ii is index var
ZL(op_idxl,'(')
    zp.acc = IVM_PTR(PAR)[zp.mem[zp.ipc[1]]];
    zp.ipc += 2; goto xnext; // 2 args
ZL(op_idxs,')')
    IVM_PTR(PAR)[zp.mem[zp.ipc[1]]] = zp.acc;
    zp.ipc += 2; goto xnext; // 2 args


 
// Memmory heap
ZL(op_M,   'M')
    // operand = slot of pointer var; ACC = size in bytes
    {
      unsigned n = (unsigned)zp.acc;
      #ifdef __CC65__
        zp.mem[*zp.ipc] = (ivmvar)(unsigned)calloc(n, 1);
      #else
        // TODO: "halloc" - make a function
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


ZL(op_I,   'I')  zp.acc = ++zp.mem[*zp.ipc]; goto pnext;
ZL(op_J,   'J')  zp.acc = --zp.mem[*zp.ipc]; goto pnext;
ZL(op_U,   'U')  zp.acc = (zp.mem[zp.ipc[0]]
                        += zp.mem[zp.ipc[1]]);
                                zp.ipc += 2; goto xnext; // two args

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
ZL(op_P,   'P')
  #ifndef __CC65__
    printf("%s", (char *)(soft_heap + (unsigned)zp.acc));
  #else
    printf("%s", (char *)(unsigned)zp.acc);
  #endif
  goto xnext;
ZL(op_O,   'O')  putchar(*zp.ipc);      goto pnext; // inline char
ZL(op_W,   'W')  printf("%u", PAR);     goto pnext;
// TODO: T printing string ? Type w length (from index)
ZL(op_dot, '.')  printf("%u", zp.acc);  goto xnext;
ZL(op_str, '"')  while (*zp.ipc) putchar(*zp.ipc++);
                                        goto pnext;

ZL(op_G,   'G')  if ((int16_t)zp.acc > 0)
                   zp.ipc += *zp.ipc;   goto pnext;
ZL(op_B,   'B')  zp.ipc -= *zp.ipc;     goto pnext;

// optimized counting up loops (pascal style): Ivv <uu Goo (6) => Yvvuuoo (4)
ZL(op_Y,   'Y')  if (   (zp.acc = ++zp.mem[*zp.ipc])
                     <= zp.mem[zp.ipc[1]] ) {
                               zp.ipc += 3; goto xnext; // 3 args
                 } else  zp.ipc += *zp.ipc; goto pnext;
// TODO: counting up

ZL(op_x,   'x')  zp.acc= sys(1, zp.acc, zp.ipc); zp.ipc+= 1; goto xnext;
ZL(op_y,   'y')  zp.acc= sys(2, zp.acc, zp.ipc); zp.ipc+= 2; goto xnext;
ZL(op_z,   'z')  zp.acc= sys(3, zp.acc, zp.ipc); zp.ipc+= 3; goto xnext;

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
    #ifdef COUNTOPS
      printf("countops: %ld\n", countops);
    #endif
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
