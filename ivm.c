// IVM - Indirect Virtual Machine
// (C) 2026 Jonas S Karlsson

// Indirect Virtual Machine
// ========================
//
// A byte-coded register virtual machine, where
// op-code are mostly alphabetcially readable.
//
// The most generic form is one can can be used
// to run many independent VMs at the same time.
// This naturally gives an overhead during execution
// because of indirect pointers.
//
// The C-code interpreter is about 7-8x slower than
// the asm implementation (all using this file).
//
// There is an embryo of a "jit" where all opcodes
// are expanded to optimal binary code, let's say
// zero page (configurable). This may give a boost
// of ~2x, giving a performance comparable to cc65.
//
//
// Features:
// - ACCumulator based arch. OPs morestly modify ACC.
// - alphabetical memomic byte codes ('L" - load...)
// - Unified global/const/stack/parameter/locals!
// - max 254 registers: [0, N[ per process
// - datatypes: char + word and pointers to them
// - no constants! folded into higher registers...
// - no stack - using FORTRAN-style STATIC STACK
// - parameters + local variables scoped
// - register machine (most: OP iADDR or just OP)
// - 64 single byte ndirect call vectors
// - optimized call by last parameter in ACCumulator
// - single byte load of 32 most common vars
// - common loops optimized for size and speed
// - Pure C implementation (7x, 2.1x using 6502 asm VM)
//
//
// STATIC STACK
// ------------
// It's observed that even large programs have mostly
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
// #$  \  :_  `         - FREE to use
// 12345678             - local vars 1-7, 8 ist last var (AX)
// AD H R T VY          - FREE op vars
//   possibly usage: cAr Cons cDr, Heap/Here, Recurse, Tail-call, Vector/index? Yndex?
//
// ? abcdefgh           - lisp?
// ? mnop               - ?index stuff, built in loops iGG GG if zero/non-zero?
// ? t uvw              - ?SSTACK saves, param: 1..7 (ACC is 0th)
// xyz                  - syscall function LN with 1,2,3 byte args indices+acc
//


// HIBIT
//   64: 00-3f            - conditional relative jump forward
//   64: (xx)             - vectored calls (max 64 funs)




// BYTE CODE DETAILS
// =================
//
// NOTE: any operator that MODIFIES a variable can only
//   modify the FIRST parameter, it's check in ivm-gen.c
//   constant relocation for "illegalit".
// 
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
//? A - car acc= acc[0] == @ TODO: redundant, find other USE!
//  B - back jump "B03" = back 3 bytes
//  C - copy ... cons(acc, $) - a variant of alloc cell and
//? D - cdr acc= acc[1] == 2 + @, LOL TODO: good for linked list
//  E - emit char = putchar(acc)
//  F - free var, set it 0 "Fvv"
//  G - conditional acc > 0: goto 1-255
//? H - TODO: heap/here something? Hollerith? LOL
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
//? T - type #ACCc chars from arg (or reverse), or Tailcall: Tvv
//  U - Unified op: ACC= var2 += var2
//? V - TODO: same using ACC as value?  Vss == +ssSss (2 not 4 B)
//  W - write var as number (unsigned) "Wvv"
//  X - return ACC; see ';' and Q
//? Y - TODO: readline?
//  Z - zero variable Zvv
//
//? a-h - save local param (h needed, meaning ACC?)
//  ?    = lisp functions = ???
//     a   - car/append
//     b   - memBer
//     c   - cons or :
//     d   - cdr
//     e   - eval
//     f   - filter
//     g   - assoc/G
//     h   - happend
//
//     m   - map(car)
//     o   - ordinal/length
//     p   - print any (readable?)
//     t   - terpri
//     u   - null
//     v   - 
//     w   - write/while
//
//     #   - numberp or MKINT (like *2 ?)
//     $   = string or consp ?
//     \   - lambda
//           nlist
//           read
//           setq   == !
//           setcar == !
//           setcdr
//           not    == ?
//           eq
//           equal
//           nth
//           consp
//
//  i   - loop <= with 'n' ('i' var upper skip)
//  j   - TODO: loop >=0 with 'n' (j var skip)
//  k   - switch-case ('k'ase low count (const back)...)
//
//  l   - load byte from slot
//? m   - TODO: mini-malloc? fixed-size? lol Method call
//  n   - next loop (together with ij ind/dec-for)
//? op  - TODO: object, 'm' method call 'p' ... ?
//  q   - quit program
//  r   - read byte (c@)
//  s   - save byte in slot (reset hibit)
//? t   -
//? u   - similar U ? or not need
//? v   - 
//  w   - write byte (c!)
//  xyz - call library functions w 2..4 args

//  [ - shift left variable steps "[vv"
//? '\' possible use instead of 'O'!!! \ is like literal... lol
//  ] - shift rigth varible steps "]vv"
//  ^ - xor "^vv"
//? _ - load ACC by name _cc (cc - const char* ptr)

//" ` - TODO: eval/superquote literal datastructure?
//  { - shift ACC left fixed steps "{nn}
//  | - or w var "|vv"
//  } - shift ACC right "}nn"
//
//  ~ - inverse ACC (oric: gray)
//? - PON; (ivm.c: nop backwards for bench)
//  - TODO: NEXTPAGE: DEL not visible (oric: full block)
//
// $80-$bf (64) conditional relative jump (forward) get rid of G
// $bf-$ff (64) subroutine calls (vectorized)


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>





#ifndef IVMVARTYPE
  #define IVMVARTYPE uint16_t
  typedef IVMVARTYPE ivmvar;
#endif // IVMVARTYPE


#ifndef UCHAR
  #define UCHAR
  typedef unsigned char   uchar;
  typedef uint16_t        word;
#endif


//#define JIT

#ifdef JIT
  #include "ivm-jit.c"
  #include "disasm.c"
  static int jit_only = 0;
  static int jit_force_zp = 0;
#endif



//#define COUNTOPS

// TODO: just ifndef 6502 or if "onhost"
#ifdef __CC65__
  // nothing
#else
  #ifdef OSCAR64
    #define register 
  #else
    #define HOST
    #define SOFT_HEAP
  #endif
#endif

#ifdef COUNTOPS
// host only; fine when profiling ./ivm-count
unsigned long countops = 0;

unsigned long opcount[256] = {0};
unsigned long paircount[256][256] = {0};

unsigned char last_op = 0;
unsigned char last_valid = 0;

#define COUNT(o) count(o)

void count(uchar o) {
  countops++;
  opcount[o]++;
  if (last_valid) paircount[last_op][o]++;
  last_op = o;
  last_valid = (o & 0x80) || !strchr("GBinjmYdQ;X", o);
}
    
// Call after any control-flow change so the next op does not form a pair
#define COUNT_RESET() do { last_valid = 0; } while (0)

void REPORT_COUNTS() {
  int i, j;
  unsigned long total = countops ? countops : 1;

  printf("\n=== COUNTOPS report (%lu ops) ===\n", countops);
  printf("%10s %7s  %-12s  NOTABLE PAIRS (>10%% of this op)\n",
    "COUNT", "PERC%", "----OP----");

  for (i = 0; i < 256; i++) {
    if (!opcount[i]) continue;

    printf("%10lu %6.2f%%  ", opcount[i], 100.0 * opcount[i] / total);

    printf("0x%02X ", i);
    if (i >= 32 && i < 127 && i != ' ') printf("   %c   ", i);
    else                                printf("       ");

    // pairs that start with this op
    for (j = 0; j < 256; j++) {
      if (paircount[i][j] * 10 > opcount[i]) {   // >10%
        printf("  %c%c:%.0f%%",
          (i>=32&&i<127)?i:'?',
          (j>=32&&j<127)?j:'?',
          100.0 * paircount[i][j] / opcount[i]);
      }
    }
    printf("\n");

  }
}

#else

  #define COUNT(o)      ((void)0)
  #define COUNT_RESET() ((void)0)

  #define REPORT_COUNTS ((void)0)

#endif // COUNTOPS 



// Host soft-heap: ivmvar is often 16-bit and cannot hold a real malloc pointer.
// We store an offset into soft_heap as the "pointer" value on non-cc65.
// On cc65/6502, M/F/@/!/(/) use real addresses.
#ifdef SOFT_HEAP

  #ifndef SOFT_HEAP_MAX
    #define SOFT_HEAP_MAX 32768   // addresses are 16-bit ivmvar: 64K is the hard limit
  #endif
  // offsets 0..SOFT_BASE-1 are never handed out: a pointer value 0 means NULL
  #define SOFT_BASE 2
  uchar soft_heap[SOFT_HEAP_MAX];
  word soft_top;
  #define IVM_PTR(addr) ((ivmvar *)(soft_heap + (unsigned)(addr)))

  // Host range check: n bytes at soft-heap offset a. Only [SOFT_BASE, soft_top)
  // is valid: the blob's strings plus what M handed out. On failure remember
  // what was tried and jump to the badptr: label in run_ivm().
  unsigned long ivm_bad_addr;
  unsigned ivm_bad_n;

  #define CHKP(a, n) do { \
      unsigned long a_ = (unsigned long)(a); \
      if (a_ < SOFT_BASE || a_ + (n) > soft_top) { \
        ivm_bad_addr = a_; ivm_bad_n = (n); goto badptr; } \
    } while (0)

ivmvar ivm_zalloc(word n) {
  // keep 16-bit access aligned
  n = (n + 1) & ~1u; 

  if (!n) {

    return 0; // NULL

  } else if (soft_top + n > SOFT_HEAP_MAX) {

    printf("\n[IVM] M: out of soft heap (want %u, used %u of %u)\n",
      n, (unsigned)soft_top, (unsigned)SOFT_HEAP_MAX);
    return 0; // NULL

  } else {

    ivmvar r = (ivmvar)soft_top;
    memset(soft_heap + soft_top, 0, n);
    soft_top += n;
    return  r;

  }
}

void ivm_free(ivmvar p) {
  // soft-heap is bump-only; just drop the reference
  // TODO: fix, use handles?
  return;
  (void)p;
}



#else

  #define IVM_PTR(addr) ((ivmvar *)(unsigned)(addr))
  #define CHKP(a, n) ((void)0)

ivmvar ivm_zalloc(word n) {
  return (ivmvar)(unsigned)calloc(n, 1);
}

void ivm_free(ivmvar p) {
  free((void *)(unsigned)p);
}
 
#endif



// TODO: just for sieve (out of index problem)
//#undef CHKP
//#define CHKP(a, n) ((void)0)





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
uchar *g_loaded; // blob base while running


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



// ===========================================================================
// HOST DEBUG SUPPORT: consistency check + break into the debugger
// ===========================================================================
#ifdef HOST

#include <signal.h>

extern ProcessHead zp;   // defined further down

// Stop here so gdb/lldb show the state (gdb: `signal 0` to carry on).
// Without a debugger the process dies with SIGTRAP.
// Alternatives: clang's __builtin_debugtrap() (gdb on arm64 may not resume
// past it), or __builtin_trap().
#ifndef IVM_BREAK
  #define IVM_BREAK() raise(SIGTRAP)
#endif

static void ivm_panic(Process *proc, const char *why, const uchar *ipc) {
  BlobHeader *h = &proc->h.hdr;

  printf("\n[IVM CHECK] %s\n", why);
  printf("  g_loaded  %p  len %u  code %u  main %u  data %u\n",
    (void *)g_loaded, h->len, h->code, h->main, h->data);
  printf("  ipc       %p  (blob offset %ld)\n",
    (const void *)ipc, (long)(ipc - g_loaded));
  printf("  zp.ipc    %p  zp.fp %d  zp.acc %u\n",
    (void *)zp.ipc, (int)zp.fp, (unsigned)zp.acc);
  printf("  proc      %p  proc->h.mem %p  zp.mem %p  memsz %u  constidx %u\n",
    (void *)proc, (void *)proc->h.mem, (void *)zp.mem, h->memsz, h->constidx);
  printf("  soft_heap %p  soft_top %u  max %u  base %u\n",
    (void *)soft_heap, (unsigned)soft_top, (unsigned)SOFT_HEAP_MAX,
    (unsigned)SOFT_BASE);
  printf("  last bad access: addr %lu  len %u\n", ivm_bad_addr, ivm_bad_n);
  fflush(stdout);
  IVM_BREAK();
}

// Returns 1 (after reporting + breaking) if the VM state is inconsistent.
static int ivm_check(Process *proc, const uchar *ipc) {
  BlobHeader *h = &proc->h.hdr;
  const char *why = NULL;
  unsigned end = h->data ? h->data : h->len;

  if (!g_loaded)
    why = "g_loaded is NULL";
  else if (ipc < g_loaded || ipc >= g_loaded + h->len)
    why = "ipc is outside the loaded blob";
  else if ((unsigned)(ipc - g_loaded) < h->code || (unsigned)(ipc - g_loaded) >= end)
    why = "ipc is outside the code section";
  else if ((uchar)zp.fp != (uchar)FP_MAIN && (unsigned)(uchar)zp.fp + FR_ACC >= h->memsz)
    why = "fp is beyond mem[] (FP_MAIN excepted; plain char is unsigned on arm64)";
  else if (zp.mem != proc->mem)
    why = "zp.mem != proc->mem";
  else if (memcmp(&zp.hdr, h, sizeof(*h)) != 0)
    why = "zp.hdr was overwritten (differs from proc->h.hdr)";
  else if (soft_top < SOFT_BASE || soft_top > SOFT_HEAP_MAX)
    why = "soft_top out of range";

  if (!why) return 0;
  ivm_panic(proc, why, ipc);
  return 1;
}

#else

  #define ivm_check(proc, ipc) 0

#endif // !__CC65__


#ifdef TRACE
  #undef TRACE
  #define TRACE(proc, p) trace(proc, p)

#define MAIN
#include "ivm-print.c"
#undef MAIN

// blob base while running (already in ivm.c)
extern uchar *g_loaded;

void trace(Process *proc, uchar *ipc) {
  word off_blob;
  word off_code;
  BlobHeader *h = &proc->h.hdr;

  // consistency first: never dereference a bad ipc (see ivm_check)
  if (ivm_check(proc, ipc)) return;

  if (!g_loaded || !ipc) {
    printf("\n????: (no base)\n");
    return;
  }

  off_blob = (word)(ipc - g_loaded);
  off_code = (word)(ipc - (g_loaded + h->code));

  // if there is any process output the line will not be aligned!
  
  printf("\t%04X (code+%04X): %02x %02x %02x %02x  %c   ",
         off_blob, off_code,
         ipc[0], ipc[1], ipc[2], ipc[3],
         (ipc[0] >= 32 && ipc[0] < 127) ? ipc[0] : ' ');

  putchar('\t');
  unsigned i = 0;
  ivm_print_one(stdout, ipc, 69, &i);

  // output of actual variable values!
  putchar('\t');
  for(int j = 1; j < i; ++j) 
    printf(" %5d ", proc->mem[ipc[j]]);

  putchar('\n');
}


#else
  #define TRACE(proc, p) (void)0
#endif


// Relocate blob-relative offsets to absolute pointers (cc65/6502 only).
// Header layout: main,code,call,fp,con,data are offsets;
// memsz, constidx are NOT offsets (must not be walked as pointers).
// Reloc region (one contiguous run of blob-relative words):
//   header main..data, call vectors, string const words, 0000.
// memsz/constidx sit BEFORE main and are never walked.
// 0000 ends the walk and is the real zero constant.
void relocate(uchar *base) {
  BlobHeader *h;
  word *v;

  h = (BlobHeader *)base;
  // One contiguous reloc region starting at main (memsz/constidx sit before it
  // and are NOT walked): header offsets, vectors, string const words, then 0000.
  // Every word is blob-relative. 0000 ends the walk (also the real zero const).
  v = &h->main;
#ifdef __CC65__
  while (*v) {
    *v = (word)(*v + (word)base);
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

  // TODO: move out to INIT_SOFT_HEAP(); ?
#ifdef SOFT_HEAP
  // host soft heap: [0..SOFT_BASE) unused (0 == NULL), then the blob's data
  // section (strings), then whatever M allocates. Always (re)initialised.
  soft_top = SOFT_BASE;
  if (bh->data && bh->len > bh->data) {
    word n = (word)(bh->len - bh->data);

    if (n > SOFT_HEAP_MAX - SOFT_BASE) n = SOFT_HEAP_MAX - SOFT_BASE;
    memcpy(soft_heap + SOFT_BASE, loaded + bh->data, n);
    soft_top = (word)((SOFT_BASE + n + 1) & ~1);   // keep 16-bit alignment
  }
#endif






  // Const image at h.con: [string blob-offs...][0000][ints...].
  // CC65: relocate() already applied base — copy absolute words into mem[].
  // Host SOFT_HEAP: file still blob-relative; data was copied to soft_heap at
  // SOFT_BASE, so string pointer = SOFT_BASE + (blob_off - data).
  // 0000 is real zero; ints after are pure values.
  if (bh->con && bh->constidx != 255) {
    word *pool;
    word nc, i;
    uchar seen0 = 0;
    word data_off = bh->data;

#ifdef __CC65__
    pool = (word *)(unsigned)bh->con;
#else
    pool = (word *)(loaded + bh->con);
#endif

    nc = (word)(253 - bh->constidx + 1);
    for (i = 0; i < nc; i++) {
      word w = pool[i];
      if (!seen0) {
        if (w == 0) {
          seen0 = 1;
          p->mem[bh->constidx + i] = 0;
        } else {
#ifdef SOFT_HEAP
          p->mem[bh->constidx + i] =
            (word)(SOFT_BASE + (w - data_off));
#else
          p->mem[bh->constidx + i] = w;
#endif
        }
      } else {
        p->mem[bh->constidx + i] = w;
      }
    }
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

#if defined(IVMASM) || defined(JIT)

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

// C helpers — ASM jsr _opmul etc; ACC/PAR already in ZP
void opmul(void)    {              ivm_acc *= ivm_par; }
void opdiv(void)    { if (ivm_par) ivm_acc /= ivm_par; }
void opmod(void)    { if (ivm_par) ivm_acc %= ivm_par; }
void opputnum(void) { printf("%u", (unsigned)ivm_acc); }
#endif

#ifdef IVMASM


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
  
  register uchar* tpc; // temporary pc (used by ij mn)
  
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
  TRACE(p, zp.ipc);

  op = *zp.ipc++;

  COUNT(op);
  
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
      // TODO: just one big static array assign? (or doesn't work cc65?)
      // build once in a local table, then memcpy → static (no per-op ifdefs)
      void *t[128];
      uchar k;

      for (k = 0; k < 128; k++) t[k] = &&op_unk;

      t['q'] = &&op_q;   t['Q'] = &&op_Q;   t['X'] = &&op_X;   t[';'] = &&op_semi;

      t['8'] = &&op_8;
      t['1'] = &&op_17;   t['2'] = &&op_17;  t['3'] = &&op_17;
      t['4'] = &&op_17;   t['5'] = &&op_17;  t['6'] = &&op_17; t['7'] = &&op_17;

      t['+'] = &&op_add;  t['-'] = &&op_sub; t['*'] = &&op_mul;
      t['/'] = &&op_div;  t['%'] = &&op_mod;
      t['&'] = &&op_and;  t['|'] = &&op_or;  t['^'] = &&op_xor;
      t['['] = &&op_shl;  t[']'] = &&op_shr;
      t['{'] = &&op_shlm; t['}'] = &&op_shrm;

      t['L'] = &&op_L;    t['S'] = &&op_S;   t['Z'] = &&op_Z;
      t['C'] = &&op_C;
      t['l'] = &&op_l;    t['s'] = &&op_s;

      t['I'] = &&op_I;    t['J'] = &&op_J;   t['U'] = &&op_U;

      t['M'] = &&op_M;    t['F'] = &&op_F;
      t['@'] = &&op_at;   t['!'] = &&op_bang;
      t['r'] = &&op_r;    t['w'] = &&op_w;
      t['\'']= &&op_ri;   t[','] = &&op_wi;
      t['('] = &&op_idxl; t[')'] = &&op_idxs;

      t['i'] = &&op_i;    t['n'] = &&op_n;   

      t['<'] = &&op_lt;   t['='] = &&op_eq;  t['>'] = &&op_gt;
      t['?'] = &&op_not;  t['~'] = &&op_bnot;

      t[' '] = &&op_sp;

      t['0'] = &&op_0;    t['9'] = &&op_9;

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

  switch (op) {

  #define Z(c)         case (c):
  #define ZL(label, c) case (c):
  #define L(label)

#endif


ZL(op_sp,  ' ')              goto xnext;

// Quit: TODO: replace by syscall!
ZL(op_q,   'q')              goto done;

// ReturnQ= return 0  X= return acc;  ';'=return RETURN;
ZL(op_Q,   'Q')  zp.acc = 0; goto op_X;
ZL(op_semi,';')
    zp.acc = zp.mem[zp.fp + FR_VAL];
    // fall through
op_X:    Z('X')
    // main returns via X into _start's trailing q (no FP_MAIN test)
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

// C dest src: mem[dest] = mem[src]; ACC = value  (simple assign without L+S)
ZL(op_C,   'C')  {
                   uchar d = zp.ipc[0], s = zp.ipc[1];
                   zp.acc = zp.mem[d] = zp.mem[s];
                   zp.ipc += 2;
                 }
                 goto xnext;

// Memory ops - word raw using variable
// TODO: why not use ACC???
ZL(op_at,  '@')  CHKP(PAR, 2); zp.acc = *IVM_PTR(PAR); goto pnext;
ZL(op_bang,'!')  CHKP(PAR, 2); *IVM_PTR(PAR) = zp.acc; goto pnext;


// We use a full 2 byte slot for single byte var, lol
ZL(op_l,   'l')  zp.acc = *(char*)(zp.mem+*zp.ipc); goto pnext;
ZL(op_s,   's')  zp.mem[*zp.ipc] = zp.acc & 0xff;   goto pnext;

#ifndef SOFT_HEAP
ZL(op_r,   'r')  zp.acc = *IVM_PTR(PAR) & 0xff; goto pnext;
ZL(op_w,   'w')  *IVM_PTR(PAR) = zp.acc & 0xff; goto pnext;
#else
// host: true byte access (the 16-bit store above clobbered the next byte)
ZL(op_r,   'r')  CHKP(PAR, 1); zp.acc = soft_heap[PAR];        goto pnext;
ZL(op_w,   'w')  CHKP(PAR, 1); soft_heap[PAR] = (uchar)zp.acc; goto pnext;
#endif

// string (char) pointer walkers
#ifndef SOFT_HEAP
ZL(op_ri,  '\'') zp.acc = *(uchar*)(PAR++); goto pnext;
ZL(op_wi,  ',' ) *(uchar*)(PAR++) = zp.acc; goto pnext;
#else
ZL(op_ri,  '\'') CHKP(zp.mem[*zp.ipc], 1); zp.acc = soft_heap[zp.mem[*zp.ipc]++];        goto pnext;
ZL(op_wi,  ',' ) CHKP(zp.mem[*zp.ipc], 1); soft_heap[zp.mem[*zp.ipc]++] = (uchar)zp.acc; goto pnext;
#endif

// indexed: (aaii, aa is index of base address, ii is index var
ZL(op_idxl,'(')
    CHKP(PAR + 2UL * zp.mem[zp.ipc[1]], 2);
    zp.acc = IVM_PTR(PAR)[zp.mem[zp.ipc[1]]];
    zp.ipc += 2; goto xnext; // 2 args
ZL(op_idxs,')')
    CHKP(PAR + 2UL * zp.mem[zp.ipc[1]], 2);
    IVM_PTR(PAR)[zp.mem[zp.ipc[1]]] = zp.acc;
    zp.ipc += 2; goto xnext; // 2 args

 
// Memmory heap
// operand = slot of pointer var; ACC = size in bytes
ZL(op_M,   'M') zp.acc = zp.mem[*zp.ipc] = ivm_zalloc(zp.acc); goto pnext;
ZL(op_F,   'F') ivm_free(PAR); zp.mem[*zp.ipc] = 0;            goto pnext;

ZL(op_I,   'I')  zp.acc = ++zp.mem[*zp.ipc]; goto pnext;
ZL(op_J,   'J')  zp.acc = --zp.mem[*zp.ipc]; goto pnext;
ZL(op_U,   'U')  zp.acc = (zp.mem[zp.ipc[0]]
                        += zp.mem[zp.ipc[1]]);
                                zp.ipc += 2; goto xnext; // two args

ZL(op_lt,  '<')  zp.acc = zp.acc < PAR;  goto pnext;
ZL(op_eq,  '=')  zp.acc = zp.acc == PAR; goto pnext;
ZL(op_gt,  '>')  zp.acc = zp.acc > PAR;  goto pnext;

ZL(op_0,   '0')  zp.acc = 0;            goto xnext;
ZL(op_9,   '9')  zp.acc = 0xffff;       goto xnext;
ZL(op_not, '?')  zp.acc = !zp.acc;      goto xnext;
ZL(op_bnot,'~')  zp.acc = ~zp.acc;      goto xnext;

ZL(op_K,   'K')  zp.acc = getchar();    goto xnext;
ZL(op_E,   'E')  putchar(zp.acc);       goto xnext;
ZL(op_N,   'N')  putchar('\n');         goto xnext;
ZL(op_P,   'P')
  // TODO: one macro
  #ifdef SOFT_HEAP
    CHKP(zp.acc, 1);
    printf("%s", (char *)(soft_heap + (unsigned)zp.acc));
  #else
    printf("%s", (char *)(unsigned)zp.acc);
  #endif
  goto xnext;
ZL(op_O,   'O')  putchar(*zp.ipc);      goto pnext; // inline char
ZL(op_W,   'W')  printf("%u", PAR);     goto pnext;
ZL(op_dot, '.')  printf("%u", zp.acc);  goto xnext;
ZL(op_str, '"')  while (*zp.ipc) putchar(*zp.ipc++);
                                        goto pnext;
// TODO: T printing string ? Type w length (from index)


ZL(op_G,   'G')  if ((int16_t)zp.acc > 0)
                   zp.ipc += *zp.ipc;   goto pnext;
ZL(op_B,   'B')  zp.ipc -= *zp.ipc;     goto pnext;


// -- Loop ops (save 6 bytes per *counting* loop)

// - ivm test.c - 37% faster with FOR-ops
//       (/ 9844950 7211499.0) 37% faster (1.365173870231418)
// - ivm-vm.asm - 18% faster with FOR-ops
//       (/ 2893598 2449567.0) 18% faster (1.1812691794100754)

// TODO: 'u'  ==  "U n' stepper that combines:
//     "Uiiss B<back> -> u<ss><back>    === 5 b -> 3 b !
//     "Uiiss n<back> -> u<ss><back>"   === 5 b -> 3 b


// loop: FOR i u end
ZL(op_i,   'i')  zp.acc = zp.mem[zp.ipc[0]];
                 if (zp.acc <= zp.mem[zp.ipc[1]]) {
                   zp.ipc += 3;               goto xnext; } // 3 args
                 zp.ipc += 2 + zp.ipc[2];     goto pnext; // skip
// NEXT loop ++
ZL(op_n,   'n')  tpc = zp.ipc - *zp.ipc + 1;
                 TRACE(p, tpc);
                 zp.acc = ++zp.mem[tpc[1]]; 
                 if (zp.acc > zp.mem[tpc[2]]) goto pnext; // fall out
                 zp.ipc = tpc + 4;            goto xnext; // body

// loop:      for(i= ...; i; i--) ... (down)
ZL(op_j,   'j')  zp.acc = zp.mem[zp.ipc[0]];
                 if (zp.acc) {
                   zp.ipc += 2;               goto xnext; } // 2 args
                 zp.ipc += 1 + zp.ipc[2];     goto pnext; // skip
// NEXT loop --
ZL(op_m,   'm')  tpc = zp.ipc - *zp.ipc + 1;
                 zp.acc = --zp.mem[tpc[1]];
                 if (!zp.acc)                 goto pnext; // done
                 zp.ipc = tpc + 3;            goto xnext; // back

/*

Your design is remarkably cohesive—every piece solves a specific 6502
and single-pass BNF constraint without adding runtime bloat.

1. Flushing K Blocks Before back Overflows 255

Emitting a K block mid-switch whenever accumulated_bytes + (3 + 2 *
count) > 255 is a great single-pass compiler trick:

• Why it works seamlessly: Because all breaks inside the switch jump
  via J <break_const> (which gets overwritten at } with the final
  end-of-switch relocated address), flushing an intermediate K block
  mid-switch doesn't break control flow at all.

• How fall-through and misses work:
	• If a K block matches, it subtracts its 1-byte back offset
          and jumps backward into the preceding case body.
	• If it misses, it falls through straight into the next batch
          of case bodies—which starts with a G jump over those bodies
          to the next K block. Every single back delta stays strictly
          1 byte (<= 255), even in a 2 KB switch.

2. Using 255 (0xFF) as the Mode Sentinel: Dense Range vs. Sparse Pairs

Using a single byte (low / mode) where 0xFF means "sparse (const,
back) pairs" and any other value (0x00..0xFE) means "dense range
starting at low" unifies both dispatch strategies into one opcode with
zero extra opcode table slots:

• Dense Mode (low != 0xFF, size = 4 + count bytes):

   [OP_K] [low] [count] [break_const] ( [back_0] [back_1] ) ...
   
	• Cost: Only 1 byte per case (half the size of sparse mode!).
	• 6502 execution: Acc - low -> CMP count -> BCS miss -> TAY ->
          LDA (IP),Y gives back in O(1) (~18 cycles)!
	• For our 6502 disassembler (0..31): All 32 cases pack into
          just 36 bytes (4 + 32), fitting effortlessly inside a single
          while loop and dispatching in ~18 cycles instead of 30 if
          checks!

• Sparse Mode (low == 0xFF, size = 4 + 2 * count bytes):

   [OP_K] [0xFF] [count] [break_const] [c0] [back_0] [c1] [back_1] ...

	• Cost: 2 bytes per case, handling arbitrary non-contiguous
          constants (like 0x00, 0x20, 0x40, 0x4C, 0x60, 0x6C in the
          second disassembler switch).

	• 6502 execution: CMP (IP),Y / BEQ match / INY / INY / DEX /
          BNE at 16 cycles per entry.

3. One Tiny Edge Case to Watch (low == 0xFF as a Dense Start)

In practice, a dense range almost never starts at 255 (0xFF) because 255 + count would wrap an 8-bit unsigned byte anyway—so 0xFF is naturally an impossible start value for a multi-element dense range.
Are you going to make OP_K read the switch expression directly from the Accumulator (so switch (expr) just evaluates expr into Acc and jumps to OP_K), or pass a variable slot index?  

*/

// "kase low count back ..." (const implied = low+i!)
// "kase xFF count (const back) ..."
ZL(op_k,   'k')  {
  uchar* base = zp.ipc;

  word low   = *zp.ipc;
  word n     = *++zp.ipc;
    
  if (low != 0xff) {
    // Dense range [low, low + n - 1] => index lookup
    word a  = zp.acc - low ;
    word back;

    if (a >= n) { zp.ipc += n; goto xnext; }

    // in range [low, low+n] index lookup
    back = zp.ipc[a];

    // hole -> fall through!
    if (!back)  { zp.ipc += n; goto xnext; }

    // we got it, go back
    zp.ipc = base - back; goto xnext;

  } else {
    // Sparse: (const back)... => serial scan
    while (n--)
      if (zp.acc == *++zp.ipc) { zp.ipc = base - *++zp.ipc; goto xnext; }
      else zp.ipc++;

    // not found - already at next op!
    goto xnext;
  }
 }

// OS API caller "x <lib:4><fun:4> one" (y, z adds +1 and +1 running)

ZL(op_x,   'x')  zp.acc= sys(1, zp.acc, zp.ipc); zp.ipc+= 1; goto xnext;
ZL(op_y,   'y')  zp.acc= sys(2, zp.acc, zp.ipc); zp.ipc+= 2; goto xnext;
ZL(op_z,   'z')  zp.acc= sys(3, zp.acc, zp.ipc); zp.ipc+= 3; goto xnext;

ZL(op_DEL,  127) goto xnext;

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

  goto done;   // unknown opcode falls out of the switch

#ifdef SOFT_HEAP
 badptr:
  {
    char why[80];

    // zp.ipc is one past the opcode here (operands not consumed yet)
    snprintf(why, sizeof(why), "op '%c' at blob offset %u: bad soft-heap access",
             isprint(op) ? op : '?', (unsigned)(zp.ipc - 1 - g_loaded));
    ivm_panic(p, why, zp.ipc - 1);
  }
  // after the debugger: end the run (fall into done)
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
  memset(&hdr, 0, sizeof(hdr));
  if ((int)fread(&hdr, 1, sizeof(hdr), f) != (int)sizeof(hdr)) {
    printf("short header %s\n", path);
    goto err;
  }

  // magic check
  if (hdr.magic[0] != 'I' || hdr.magic[1] != 'V'
    || hdr.magic[2] != 'M' || hdr.magic[3] != 0) {
    printf("load_blob: not an IVM file: %s (%02x%02x%02x%02x)\n",
      path, hdr.magic[0], hdr.magic[1], hdr.magic[2], hdr.magic[3]);
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

#ifdef JIT

word jit_compile(Process* proc) {
  BlobHeader *bh = &proc->h.hdr;

  word code_len;
  uchar ncall = 0;
  word vec_bc[64];
  word *map;
  uchar *out;
  word out_max = 8192;
  word out_org;
  word mem_base = (word)(unsigned)proc->h.mem; // slots already allocated
  word jit_size;
  int i;
  
  uchar *base = NULL;
  uchar *code;
  uchar *frame_base;
  word *callv;

  // I guess we can Why would this be differnt for CC65?
  #ifdef __CC65__
  base = (uchar *)proc->h.ipc - bh->main;
  #endif

  code = base + bh->code;
  frame_base = base + bh->fp;
  callv = (word *)(base + bh->call);

  code_len = (bh->data > bh->code) ? (bh->data - bh->code) : (bh->len - bh->code);

  // call table size from section gap
  ncall = (uchar)((bh->con - bh->call) / sizeof(word));
  if (ncall > 64) ncall = 64;

  // diagnostic
  printf("--- hdr: call=%u con=%u fp=%u code=%u ncall=%u ---\n",
    (unsigned)bh->call, (unsigned)bh->con, (unsigned)bh->fp,
    (unsigned)bh->code, ncall);

  for (i = 0; i < ncall; i++)
    vec_bc[i] = callv[i] - bh->code;

  map = (word *)calloc(code_len ? code_len : 1, sizeof(word));
  out = (uchar *)malloc(out_max);
  out_org = (word)(unsigned)out;
  if (!map || !out) {

    printf("--- JIT: alloc fail ---\n");
    free(map); free(out);
    return 0;

  } else {

    printf("--- JIT: code_len=%u ncall=%u memsz=%u ---\n",
      (unsigned)code_len, ncall, (unsigned)bh->memsz);

    lc_main_bc = (word)(bh->main - bh->code);
    lc_main_at = 0;
    if (bh->memsz <128) printf("--- JIT: using zero-page slots ---\n");
    lc_constidx = (uchar)bh->constidx;
    lc_constmem = proc->h.mem;

 mem_base = 0;

    jit_size = ivm_jit(code, code_len, out, out_max, map, code_len,
      mem_base, out_org, vec_bc, frame_base, ncall);

    if (jit_size) {

      // main address was noted during codegen
      if (lc_main_at && lc_jmp_at + 1 < jit_size) {
        word main_addr = (word)(out_org + lc_main_at);
        out[lc_jmp_at]     = (uchar)(main_addr & 0xff);
        out[lc_jmp_at + 1] = (uchar)(main_addr >> 8);
        printf("--- entry -> main at 0x%04X ---\n", main_addr);
      }

      // hexdump
      printf("--- HEXDUMP (%u bytes) ---\n", (unsigned)jit_size);
      {
        word k;
        for (k = 0; k < jit_size; k++) {
          if ((k & 15) == 0) printf("%04X  ", (unsigned)(out_org + k));
          printf("%02X ", out[k]);
          if ((k & 15) == 15 || k + 1 == jit_size) printf("\n");
        }
      }

      // disasm
      printf("--- DISASM ---\n");
      disasm((char *)out, (char *)(out + jit_size), 2);

      printf("\n--- JIT: emitted %u bytes (BC: %u) of 6502 at org 0x%04X ---\n\n",
        jit_size, code_len, out_org);

      // keep generated code buffer; return its address as entry
      free(map);
      return (word)(unsigned)out;

    } else {

      printf("--- JIT: compilation failed ---\n");
      free(map);
      free(out);
      return 0;

    }
  }
}
#endif // JIT

static int run_loaded_blob(uchar *img, int len) {
  static word ret; // static so can access in 6502 asm
  static word addr;

  Process *proc= Nevm(img);
    
  if (!proc) return -1;

  #ifdef JIT
    printf("--- JIT compiling IVM (blob %d) ---\n", len);
    addr = jit_compile(proc);
    printf("--- JIT: entry at %u ---\n", (unsigned)addr);
  #endif

  #ifdef JIT
    if (jit_only) {
      printf("--- JIT only, not running ---\n");
      return 0;
    }
  #endif

  printf("--- RUN IVM (blob %d) ---\n", len);

  #ifdef JIT
      // call the generated 6502 code at addr (JMP + vector table)
      // result returns in A/X (cc65 ACC style)
    __asm__ ("lda %v", addr);
    __asm__ ("ldx %v+1", addr);
    __asm__ ("jsr callax");
    __asm__ ("sta %v", ret);
    __asm__ ("stx %v+1", ret);
  #else
    ret = run_ivm(proc);
  #endif

  printf("\n--- END ---\n");
  printf("acc = %u\n", ret);

  free(proc);

  return ret;
  (void)addr;
}

int run_file(char* filename) {
  int len;
  uchar *img= load_blob(filename, &len);
  int r;
  
  if (!img) return -1;
    
  run_loaded_blob(img, len);
  free(img);

  #ifdef COUNTOPS
    printf("countops: %ld\n", countops);
    REPORT_COUNTS();
    return 1;
  #endif

  return r;
}


#ifndef MAIN

#ifdef OSCAR64
int main(void) {
  int argc = 2;
  const char* argv[]= {"ivm", #FILENAME, NULL};
#else
int main(int argc, char** argv) {
#endif
  
  #ifdef JIT
    {
      int a = 1;
      while (a < argc && argv[a][0] == '-') {
        if (argv[a][1] == 'j') jit_only = 1;
        if (argv[a][1] == 'z') jit_force_zp = 1;
        a++;
      }
      if (a < argc) return run_file((char*)argv[a]);
    }
  #endif

  if (argc==2)
    return run_file((char*)argv[1]);

  return 1;
}
#endif // MAIN
