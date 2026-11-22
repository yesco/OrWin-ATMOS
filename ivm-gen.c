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

// opcode table: operand bytes / slots / jumps (all bytecode walkers)
#include "ivm-walk.c"

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

#ifndef MAX_PARAMS
  #define MAX_PARAMS    4 // max arguments per function
#endif

#ifndef MAX_SYMBOLS
  #define MAX_SYMBOLS   250
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

// Called for every op that WRITES its variable operand (see WRITES_VAR
// below). Defined by parse.c / tpi.c: must report an error (IVM_GEN_FAIL /
// FAIL) if slot ix belongs to a `const int foo = 42;` symbol.
void check_store(uchar ix);

// A `const int foo = 42;` is an alias of the const-pool slot (no storage of
// its own, nothing to initialise), so every write op naming it is an error.
// Define IVM_STORE_CHECK 0 before include to switch the check off.

// ---------------------------------------------------------------------------
// Vectors / main
// ---------------------------------------------------------------------------

#ifndef MAX_VECTORS
  #define MAX_VECTORS 64
#endif

uint16_t vectors[MAX_VECTORS];
uchar frame_base[MAX_VECTORS];
uchar frame_nparams[MAX_VECTORS]; // parameters of function v (set by params_end)
uchar nvector;

// -O frame packing: frames of functions never active at the same time
// share cells. Callees are compiled before callers, so when a call is
// emitted the callee's fneed is final (recorded by emitop()):
//   fneed[v] = cells below v's frame = max over callees c of
//              fneed[c] + size(c),  size(c) = frame_base[c+1] - frame_base[c]
// Backtracking can only make it bigger (safe, just less packing).
uchar fneed[MAX_VECTORS];
uchar cur_func;           // function being emitted (gen_func_entry), 0xff none
uchar next_slot; // frames/locals bump (and all slots when !optimize)
// optimize==0: intermixed layout from 0 (as before); short L if slot<32
// optimize!=0: REGISTER 0..31, other globals 32..95, frames at FRAME_BASE (96)+
uchar optimize;
uchar next_reg;   // next REGISTER global (0..31)
uchar next_glob;  // next normal global when optimize (32..95)
word main_offset;
uchar main_vec;           // vector index of function "main", 0xff = none

// The current func_entry being compiled
uchar pending_func_entry; // 0xff = none

// Global initializers ("int g = 5;" at scope 0): nothing runs there, so
// their code is generated right away into ginit_code[], and copied to the
// start of main by gen_func_entry().
#ifndef MAX_GINIT
  #define MAX_GINIT 64
#endif
uchar ginit_code[MAX_GINIT * 2];   // same RAM as the old slot+src arrays
uchar ginit_len;
uchar ginit_acc;   // const slot the init code leaves in ACC, 0xff none


void gen_func_entry(uchar v);
#ifndef IVM_NO_VN
word vn_label;   // last label (jump target) position: value numbering
#endif
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
    uchar v = pending_func_entry;
    pending_func_entry = 0xff; // clear first — gen may emit
    gen_func_entry(v);
  }

  if (bin_ptr >= MAX_BIN_LEN - 1)
    IVM_GEN_FAIL("bytecode buffer overflow");

  bin[bin_ptr++] = c;
}

void emitop(uchar op) {
  emit(op);

  // -O: the callee's frame goes below ours (see fneed)
  if (op >= 0xC0 && cur_func < nvector) {
    uchar c = op & 0x3f, n;
    if (c < cur_func) {
      n = fneed[c] + frame_base[c + 1] - frame_base[c];
      if (n > fneed[cur_func]) fneed[cur_func] = n;
    }
  }

  if (op == '0' || op == '9' || op == '~' || op == '?' || op == 'G' || op == 'B')
    acc_invalidate();
}

// Ops whose FIRST operand is a variable slot that the VM modifies
// (ivm.c run_ivm):
//   S  mem[v] = acc          Z  mem[v] = 0
//   C  mem[dst] = mem[src]   (only dst, the 1st operand, is written)
//   I  ++mem[v]              J  --mem[v]
//   U  mem[v1] += mem[v2]    (only v1, the 1st operand, is written)
//   M  mem[v] = alloc(acc)   F  free(mem[v]); mem[v] = 0
//   s  mem[v] = acc & 0xff
//   {  mem[v] <<= acc        }  mem[v] >>= acc
//   '  *mem[v]++ read        ,  *mem[v]++ = acc   (pointer var is advanced)
// Loop ops i/n/j/m also modify their loop variable (n: ++, m: --), but they
// are emitted with emit()/emitop() by parse.c, so parse.c must call
// check_store() on the loop variable itself.
// NOT writes to the variable: + - * / % & | ^ [ ] < = > L l @ r W  (read it),
// and ! w ) (write THROUGH the pointer in the var, var itself unchanged).
#define WRITES_VAR(op) \
  ((op) == 'S' || (op) == 'Z' || (op) == 'C' || (op) == 'I' || (op) == 'J' || \
   (op) == 'U' || (op) == 'M' || (op) == 'F' || (op) == 's' || \
   (op) == '{' || (op) == '}' || (op) == '\''|| (op) == ',' || \
   (op) == 'i' || (op) == 'j' || (op) == 'H' || \
   (op) == 't' || (op) == 'u' || (op) == 'k' \
  )

void emitop2(uchar op, uchar arg) {
  // const variable guard: refuse to emit a write to a const slot
#if IVM_STORE_CHECK
  if (WRITES_VAR(op))
    check_store(arg);
#endif

  // !optimize: short L for slots 0..31; optimize: always long (phase-1 "stupid")
  if (op == 'L' && arg < 32 && !optimize)
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
//
// Two kinds of pool entries:
//   - integer literals (not relocated)
//   - data offsets for string literals (relocated at load: base + data + off)
//
// Packing order in the blob const section:
//   [reloc_off_1, reloc_off_2, ..., 0, int_1, int_2, ...]
// The 0 terminator separates relocatable pointer consts from plain ints.
// Slot addresses (CONST_TOP - i) stay stable for bytecode; only the *packed*
// order for the blob puts reloc entries first.
// ---------------------------------------------------------------------------
uint16_t const_pool    [MAX_CONSTS];
uchar    const_is_reloc[MAX_CONSTS]; // 1 = value is offset into data section
                                     // 2 = offset into bss (static arrays)
uchar    nconst = 0;
uchar    constidx = 255; // lowest const slot; 255 = none
uchar    nreloc_const = 0; // how many reloc entries (for pack)

// String / data pool (appended to blob at h.data)
#ifndef MAX_DATA
  #define MAX_DATA 2048
#endif

uchar data_pool[MAX_DATA];
word  data_len = 0;

// Static arrays (bss): zeroed memory the loader sets aside after the
// blob's end; never in the file. Their consts are reloc words (kind 2):
// blob offset len + off, so a loader finds them as words >= len. One more
// kind 2 const marks the end: the loader's bss size
word  bss_len = 0;
uchar bss_end_slot = 0;          // the end marker's const slot, 0 none

// Type tags for captures / symbols (high byte of word results)
// Low 8 bits of a capture = slot or IMM; high 8 = type char.
//   'c' char   'w' word   'C' char*   'W' word*
// Pointer is uppercase (bit 5 of the type char clear).
#define TY_WORD   ((word)(('w') << 8))   // 0x7700
#define TY_CHAR   ((word)(('c') << 8))   // 0x6300
#define TY_WORD_PTR ((word)(('W') << 8)) // 0x5700
#define TY_CHAR_PTR ((word)(('C') << 8)) // 0x4300

#define CAP_SLOT(w)   ((uchar)((w) & 0xff))
#define CAP_TYPE(w)   ((word)((w) & 0xff00))
#define CAP_IS_PTR(w) ((((w) >> 8) & 0x20) == 0)
#define MAKE_CAP(slot, ty) ((word)(((word)(uchar)(slot)) | (word)(ty)))

uchar add_const_raw(unsigned int v, uchar reloc) {
  uchar i, addr;

  // dedup only non-reloc (integer) values
  if (!reloc) {
    for (i = 0; i < (uchar)nconst; i++)
      if (!const_is_reloc[i] && const_pool[i] == v)
        return (uchar)(CONST_TOP - i);
  }

  if (nconst >= MAX_CONSTS)
    IVM_GEN_FAIL("constant pool full");
  // collide with real low-slot high-water (next_slot), not fixed FRAME_BASE
  if ((uchar)(CONST_TOP - nconst) < next_slot)
    IVM_GEN_FAIL("constants collided with frames");

  // save value
  const_pool[nconst] = (uint16_t)v;
  const_is_reloc[nconst] = reloc;
  if (reloc) nreloc_const++;

  // give mem index
  addr = (uchar)(CONST_TOP - nconst);
  nconst++;
  constidx = addr;

  return addr;
}

uchar add_const(unsigned int v) {
  return add_const_raw(v, 0);
}

// Append one byte to data_pool. Returns 0 if full.
void data_putc(char c) {
  if (data_len >= MAX_DATA)
    IVM_GEN_FAIL("data pool full");
  data_pool[data_len++] = c;
}

#if 0
// Append bytes; return offset of first byte (or FAIL if full).
word data_append(const uchar *bytes, word n) {
  word off = data_len;
  word i;

  if (data_len + n > MAX_DATA)
    IVM_GEN_FAIL("data pool full");

  // TODO: this used to be memcopy, someone changed :-(
  for (i = 0; i < n; i++)
    data_pool[data_len++] = bytes[i];
  return off;
}
#endif // NOT USED (yet, maybe by array)

// Start a string in the data pool.
// Returns offset where the string will begin.
word data_string_start(void) {
  // Pad so offset 0 is never a string (0 ends the reloc walk). 
  if (data_len == 0) {
    data_pool[0] = 0;
    data_len = 1;
  }
  return data_len;
}

// Register a relocatable const pointing at data offset off (string start).
uchar add_string_reloc(word off) {
  return add_const_raw((unsigned int)off, 1);
}

// A static array of n bytes: its const slot (the address after loading).
// 256 bytes or more: page aligned (the loader aligns the bss), else word
uchar add_bss_array(word n) {
  word off;
  uchar k;

  data_string_start();                       // no 0 offset (0 ends reloc)
  off = n >= 256 ? (word)((bss_len + 255) & ~255u) : (word)((bss_len + 1) & ~1u);
  bss_len = (word)(off + n);
  k = add_const_raw(off, 2);

  // the end marker
  if (!bss_end_slot) bss_end_slot = add_const_raw(bss_len, 2);
  else const_pool[CONST_TOP - bss_end_slot] = bss_len;
  return k;
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
  // Real const-pool entry for every value, including 0.
  // (IMM0/IMM1 remain available as optional one-byte encodings later.)
  if (v == (unsigned int)-1) return IMM1;
  return add_const(v);
}

// ---------------------------------------------------------------------------
// Symbols and scopes — shared by tpi.c and parse.c
//
// Allocated on demand (grows SYM_GROW at a time), names are malloc'd at their
// exact length, so nothing is reserved for symbols that don't exist.
//
//   VAR   address = slot
//   FUNC  address = vector index, ret_addr_slot = frame base,
//         ret_slot = return value cell (parameters: see func_begin)
//
// scope: depth the name lives in; scope_leave() drops everything declared at
// the depth being left (names are appended, so those are the newest ones).
// sym_truncate() drops back to an earlier count (parse.c backtracking).
//
// Pointers from sym_add() stay valid only until the next sym_add(): keep the
// index (nsym-1) and use &syms[idx] again afterwards.
// ---------------------------------------------------------------------------
#define SYM_ANY   0xff

// TODO: why can't this just be 0?

#define SYM_NONE  0xffff
#define SYM_GROW  16

//#define SYM_GROW  250

typedef enum { SYM_VAR, SYM_FUNC } SymbolType;

// ~15 bytes + name; shrink later if needed (pack flags into vtype etc.)
typedef struct {
  char *name;          // malloc'd
  uchar address;       // VAR: slot   FUNC: vector index

  // FUNC layout helpers (also frame_base[vec] in the blob)
  uchar ret_slot;
  uchar ret_addr_slot; // FUNC: frame base

  // is_ptr: prefer TY_PTR in typed captures; kept here for sym_find/decl
  uchar is_ptr;        // 1 if pointer type (still one cell)
  uchar is_const;      // 1 = const / #define, no stores allowed
  uchar vtype;         // TY_WORD ('w') or TY_CHAR ('c') — element type
  word  nelem;         // static array: element count (its size: nelem *
                       // (vtype 'c' ? 1 : 2)); address: a const slot. 0 none

  uchar scope;         // scope depth it was declared in

  // type + pidx: could pack (pidx>0 => param; type==FUNC => function)
  SymbolType type;
  uchar pidx;          // parameter number + 1 (0 = not a parameter)

  // FUNC: parameter count is frame_nparams[vector], their slots follow
  // from frame_base[] (func_begin / param_add / params_end / arg_slot)

} Symbol;

Symbol *syms;          // grows on demand
word nsym;
uchar scope_depth;
static word sym_cap;

Symbol *sym_add(const char *name, SymbolType type, uchar address) {
  Symbol *s;
  char *nm;
  word n;

  if (nsym >= MAX_SYMBOLS) IVM_GEN_FAIL("symbol table full");

  if (nsym >= sym_cap) {
    Symbol *ns = (Symbol *)realloc(syms, (sym_cap + SYM_GROW) * sizeof(Symbol));

    if (!ns) IVM_GEN_FAIL("out of memory (symbols)");
    syms = ns;
    sym_cap = sym_cap + SYM_GROW;
  }

  n = (word)(strlen(name) + 1);
  nm = (char *)malloc(n);
  if (!nm) IVM_GEN_FAIL("out of memory (symbol name)");

  memcpy(nm, name, n);

  s = &syms[nsym++];
  s->name = nm;
  s->type = type;
  s->address = address;
  s->ret_slot = 0xff;
  s->ret_addr_slot = 0xff;
  s->is_ptr = 0;
  s->is_const = 0;     // was never set: realloc() garbage made random "const"
  s->vtype = 'w';
  s->scope = scope_depth;
  s->pidx = 0;
  s->nelem = 0;

  return s;
}

// innermost declaration wins. type: SYM_VAR, SYM_FUNC or SYM_ANY.
// local=1: only names declared in the current scope. SYM_NONE if not found.
word sym_find(const char *name, uchar type, uchar local) {
  word i;
  Symbol *s;

  for (i = nsym; i > 0; i--) {
    s = &syms[i - 1];
    if (local && s->scope != scope_depth) continue;
    if ((type == SYM_ANY || s->type == type) && strcmp(s->name, name) == 0)
      return (word)(i - 1);
  }

  return SYM_NONE;
}

// the FUNC symbol of vector v
word sym_func(uchar v) {
  word i;
  Symbol *s = syms + nsym;

  for (i = nsym; i > 0; i--)
    if ((--s)->type == SYM_FUNC && s->address == v)
      return (word)(i - 1);

  return SYM_NONE;
}

// drop symbols declared after the first n
void sym_truncate(word n) {
  while (nsym > n) {
    nsym--;
    free(syms[nsym].name);
    syms[nsym].name = NULL;
  }
}

void sym_reset(void) {
  sym_truncate(0);
  scope_depth = 0;
}

void sym_free(void) {
  sym_reset();
  free(syms);
  syms = NULL;
  sym_cap = 0;
}

void scope_enter(void) {
  if (scope_depth < 255) scope_depth++;
}

void scope_leave(void) {
  if (scope_depth == 0) return;

  while (nsym > 0 && syms[nsym - 1].scope >= scope_depth) {
    nsym--;
    free(syms[nsym].name);
    syms[nsym].name = NULL;
  }
  scope_depth--;
}

// ---------------------------------------------------------------------------
// Functions: frame + vector + symbols, all in one place (\f \p \r \k)
//
// Frame of function v at base = frame_base[v]:
//   base+0 ret addr, +1 ret value (the function name), +2 prev fp,
//   +3 last param (the call leaves it in ACC, JSR stores it there),
//   +4.. the other params in order, then locals.
// Params take the next slots, so param k is base+4+k and the count is just
// next_slot - base - 4: no symbol table walks, and backtracking (which
// restores next_slot) undoes it for free.
// ---------------------------------------------------------------------------

// declare function name: new frame (4 header cells) + vector.
// Returns the vector, 0xff if name is already a function here.
uchar func_begin(const char *name) {
  uchar base, vec;
  Symbol *s;

  if (sym_find(name, SYM_FUNC, 1) != SYM_NONE) return 0xff;
  if (nvector >= (uchar)(MAX_VECTORS - 1)) IVM_GEN_FAIL("too many functions");

  // -O: frames start at FRAME_BASE (96), after the global bands 0..95
  if (optimize && next_slot < FRAME_BASE)
    next_slot = FRAME_BASE;

  base = next_slot;
  if (base >= CONST_TOP - 40) IVM_GEN_FAIL("out of slots");
  next_slot = base + 4;

  vec = nvector++;
  s = sym_add(name, SYM_FUNC, vec);
  s->ret_addr_slot = base;
  s->ret_slot = base + 1;

  // return value variable: only visible inside the body (dropped by \})
  s = sym_add(name, SYM_VAR, base + 1);
  s->scope = scope_depth + 1;

  frame_base[vec] = base;
  frame_nparams[vec] = 0;
  pending_func_entry = vec;
  if (strcmp(name, "main") == 0)
    main_vec = vec;

  return vec;
}

// next parameter of function v (base+4+k). NULL if declared twice.
Symbol *param_add(uchar v, const char *name) {
  uchar slot = next_slot;
  Symbol *s;

  if (sym_find(name, SYM_VAR, 1) != SYM_NONE) return NULL;
  s = sym_add(name, SYM_VAR, slot);
  s->pidx = (uchar)(slot - frame_base[v] - 3);   // k + 1
  next_slot = slot + 1;

  return s;
}

// end of the parameter list of v: the last param is the newest symbol, it
// moves to base+3, and its cell is free again (for locals)
void params_end(uchar v) {
  uchar n = (uchar)(next_slot - frame_base[v] - 4);

  if (n) {
    syms[nsym - 1].address = frame_base[v] + 3;
    next_slot--;
  }
  frame_nparams[v] = n;
}

// slot for argument b of a call to v (the last argument stays in ACC)
uchar arg_slot(uchar v, uchar b) {
  if ((word)(b + 1) >= frame_nparams[v]) IVM_GEN_FAIL("too many arguments");
  return frame_base[v] + 4 + b;
}

// global init "dest = value of const slot src", as code, now:
//   value 0            Z dest
//   same src as last   S dest          (C leaves the value in ACC)
//   else               C dest src
void gen_ginit(uchar dest, uchar src) {
  uchar *p = ginit_code + ginit_len;

  if (ginit_len > sizeof(ginit_code) - 3) IVM_GEN_FAIL("too many global inits");

  if (isconst(src) && !const_val(src)) {
    *p++ = 'Z';
    *p++ = dest;
  } else if (src == ginit_acc) {
    *p++ = 'S';
    *p++ = dest;
  } else {
    *p++ = 'C';
    *p++ = dest;
    *p++ = src;
    ginit_acc = src;
  }
  ginit_len = (uchar)(p - ginit_code);
}

// emitload/emitstore live in tpi.c (frame-relative); parse uses emit/emitop2

void gen_reset(void) {
  bin_ptr = 0;
  nconst = 0;
  constidx = 255;
  next_reg = 0;
  next_glob = 32;
  nreloc_const = 0;
  data_len = 0;
  bss_len = 0;
  bss_end_slot = 0;
  pending_func_entry = 0xff;
  main_vec = 0xff;
  main_offset = 0;
  ginit_len = 0;
  ginit_acc = 0xff;
  cur_func = 0xff;
  memset(fneed, 0, sizeof(fneed));

  acc_invalidate();
}

void gen_func_entry(uchar v) {
#ifndef IVM_NO_VN
  vn_label = (word)bin_ptr;
#endif
  cur_func = v;
  
  // main: the global init code goes first, so it runs on entry
  if (v == main_vec) {
    main_offset = (word)bin_ptr;
    if (bin_ptr + ginit_len >= MAX_BIN_LEN - 1)
      IVM_GEN_FAIL("bytecode buffer overflow");
    memcpy(bin + bin_ptr, ginit_code, ginit_len);
    bin_ptr += ginit_len;
  }

  if (v < (uchar)MAX_VECTORS)
    vectors[v] = (uint16_t)(v == main_vec ? main_offset : bin_ptr);
}

// Append n bytes at *pp (copy src, or zero-fill if src==NULL).
// Returns offset from base (section start in the blob). Advances *pp.
static word gen_memadd(uchar *base, uchar **pp, const void *src, word n) {
  word off = (word)(*pp - base);

  if (src)
    memcpy(*pp, src, (size_t)n);
  else
    memset(*pp, 0, (size_t)n);

  *pp += n;
  return off;
}

// ---------------------------------------------------------------------------
// -O peephole: rewrite in place, filling with DEL (0x7f, a NOP), then
// gen_squeeze() removes the DELs and fixes every jump (ivm_jump()).
// A user ' ' (e.g. nop;[ ] for timing) is never touched.
// A pattern is only applied if no jump lands INSIDE it (on its first
// byte is fine): tgt[] bitmap of all jump targets.
//
//   L xx        -> xx            one-byte load (xx < 32, final slots)
//   ? G rr      -> zjp+rr        jump if ACC == 0 (rr < 64)
//   ? ? ?       -> ?             !!!x == !x
//   S xx L xx   -> S xx          ACC already holds mem[xx]
//   S xx xx     -> S xx          (xx < 32: one-byte load)
//   0 X         -> Q             return 0
//   X/Q/;/B ... -> X/Q/;/B       dead code up to the next jump target
//   K zjp       -> zjp           K == 0 (const): always jumps -> dead code
//   K zjp       ->               K != 0 (const): never jumps
//
// NOT done: ?? before G (G is signed > 0: !!-1 jumps, -1 does not),
//           0; (';' reloads the return value, Q returns 0)
#define OP_DEL 0x7f
#define OP_ZJP 0x80

// one bitmap allocation, two halves: jump targets, DELs (op positions only)
static uchar *gen_tgt, *gen_del;

#define BIT(m, x)    ((m)[(x) >> 3] & (1 << ((x) & 7)))
#define SETBIT(m, x) ((m)[(x) >> 3] |= (1 << ((x) & 7)))
#define TGT(x)       BIT(gen_tgt, x)

// replace the op byte at x by a DEL (squeezed out later)
static void DEL(word x) {
  bin[x] = OP_DEL;
  SETBIT(gen_del, x);
}

static void gen_peephole(void) {
  word pc, n, t, nb = (word)((bin_ptr >> 3) + 1);
  word kv = 0, kpc = 0;     // ACC == kv, loaded by the op at kpc (kn bytes)
  uchar *a, op, kn = 0, kc = 0, dead = 0;
  uchar a0, a1, a2, a3, t1, t2;

  gen_tgt = (uchar *)calloc(nb, 2);
  if (!gen_tgt) IVM_GEN_FAIL("out of memory (peephole)");
  gen_del = gen_tgt + nb;

  // jump targets
  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;
    t = ivm_jump(a, pc);
    if (t < (word)bin_ptr) SETBIT(gen_tgt, t);
  }
  // function entries are called, not jumped to: targets too (an empty
  // function is a lone X right after the previous function's X)
  for (n = 0; n < nvector; n++)
    SETBIT(gen_tgt, vectors[n]);
  SETBIT(gen_tgt, main_offset);

  // patterns
  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    // a jump lands here: ACC unknown, code alive again. Until then, dead
    // code: every byte becomes a DEL (targets are always op starts)
    n = 1;
    if (TGT(pc)) dead = kc = 0;
    if (dead) {
      DEL(pc);
      continue;
    }

    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;

    a0 =  a[0];
    a1 =  a[1];
    a2 =  a[2];
    a3 =  a[3];
    
    t1 = !TGT(pc + 1);
    t2 = !TGT(pc + 2);
    
    // L xx -> xx: one-byte load. No target check: nothing jumps onto an
    // operand, and a jump to the L lands on xx after the squeeze.
    if (a0 == 'L' && a1 < 32) {
      DEL(pc);
      n = 1;          // visit the one-byte load next (constant tracking)
    }
    else if (a0 == '?' && a1 == 'G' && a2 < 64 && t1) {
      DEL(pc); DEL(pc + 1);
      a[2] = OP_ZJP + a[2];
    }
    else if (a0 == '?' && a1 == '?' && a2 == '?' && t1 && t2) {
      DEL(pc); DEL(pc + 1);
    }
    else if (a0 == 'S' && a2 == 'L' && a3 == a1 && t2) {
      DEL(pc + 2); DEL(pc + 3);
    }
    else if (a0 == 'S' && a2 == a1 && a1 < 32 && t2)
      DEL(pc + 2);
    else if (a0 == '0' && a1 == 'X' && t1) {
      DEL(pc);
      a[1] = 'Q';
    }

    // what is ACC now? (look at the op as the patterns left it)
    op = *a;
    if (op == OP_DEL || op == ' ') continue;  // NOP: ACC as before

    // zjp after a known constant: 0 always jumps (rest dead until the
    // target), anything else never jumps (drop zjp and the load)
    if (op == OP_ZJP) {                  // jumps 0: never matters
      DEL(pc);
      if (kc) for (t = 0; t < kn; t++) DEL(kpc + t);
      kc = 0;
      continue;
    }
    if ((uchar)(op - OP_ZJP) < 64 && kc) {
      if (!kv) dead = 1;
      else {
        DEL(pc);
        for (t = 0; t < kn; t++) DEL(kpc + t);
      }
      kc = 0;
      continue;
    }

    // ACC := known constant?
    kc = 1; kpc = pc; kn = (uchar)n;
    if (op == '0') kv = 0;
    else if (op == '9') kv = 0xffff;
    else if (op < 32 && op >= constidx) kv = const_pool[op - constidx];
    else if (op == 'L' && a[1] >= constidx) kv = const_pool[a[1] - constidx];
    else kc = 0;

    // nothing after these runs until the next target (X X, code after
    // return, the other branch of if (0) / if (1) ...)
    if (op == 'X' || op == 'Q' || op == ';' || op == 'B') dead = 1;
  }
}

// DELs in [lo, hi[
static word gen_dels(word lo, word hi) {
  word d = 0;

  for (; lo < hi; lo++)
    if (BIT(gen_del, lo)) d++;
  return d;
}

// remove the DELs, fix all jumps, vectors[] and main_offset
static void gen_squeeze(void) {
  word pc, n, t, r;
  uchar *a, i, k;

  // jumps: rel -= DELs between the jump and its target
  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;
    t = ivm_jump(a, pc);
    if (t == 0xffff) continue;

    if ((uchar)(*a - OP_ZJP) < 64) { *a -= gen_dels(pc + 1, t); continue; }
    i = ivm_opinfo(*a);
    k = OPI_NARGS(i);
    r = pc + k + 1;                       // after the rel byte
    a[k] -= (i & OPI_BACK) ? gen_dels(t, r) : gen_dels(r, t);
  }

  // (a count from 0 per vector: nvector * code length bit tests)
  for (k = 0; k < nvector; k++)
    vectors[k] -= gen_dels(0, vectors[k]);
  main_offset -= gen_dels(0, main_offset);

  // compact: DEL bits are only set on ops, so a byte loop is enough
  for (pc = r = 0; pc < (word)bin_ptr; pc++)
    if (!BIT(gen_del, pc)) bin[r++] = bin[pc];
  bin_ptr = r;

  free(gen_tgt);
}

// ---- value numbering while generating (parse.c \t) ------------------------
//
// When the grammar puts ACC into a new temp ("S \t"), the code since ACC
// got a fresh value (a load, then pure ops: the "chain") is hashed. Seen
// before, and the slot that got it then still holds it: drop the chain,
// emit "L t1" (free in the JIT, the next op mostly overwrites ACC) and
// the production gets t1 instead of a new temp. Nothing to rename later.
// Valid only while, between there and here: no label (vn_label: \m \y,
// function starts), no call / loop op, no write of t1 or of a slot the
// chain reads, no store through a pointer if the chain reads memory.
// Backtracking: an entry is used only if its bytes are still there.
#ifndef IVM_NO_VN
#define VN_N 16
static struct { word h, at; uchar len, t; } vn_t[VN_N];

static word vn_hash(word at, uchar len) {
  word h = 0;
  while (len--) h = (word)(h * 31 + bin[at++] + 1);
  return h;
}
// the ops at [from, to[: pure chain? (a fresh load, then + - ... only)
// -> its start (or 0xffff)
static word vn_chain(word from, word to) {
  word pc, n, st = 0xffff;
  uchar op;
  for (pc = from; pc < to; pc += n) {
    op = bin[pc];
    n = ivm_oplen(bin + pc);
    if (!n) return 0xffff;
    if (op < 32 || strchr("L([@rl09", op)) st = pc;
    else if (!(strchr("+-*/%&|^{}<=>?~", op))) st = 0xffff;
  }
  return pc == to ? st : 0xffff;
}
// [at, at + len[ is a chain that reads slot v? memory?
static int vn_reads(word at, uchar len, uchar v, uchar mem) {
  word pc, n;
  const uchar *q;
  int r = 0;
  for (pc = at; pc < at + len; pc += n) {
    n = ivm_oplen(bin + pc);
    if (bin[pc] < 32) { if (bin[pc] == v) r = 1; continue; }
    if (mem && strchr("([@rl", bin[pc])) r = 1;
    IVM_EACH_SLOT(bin + pc, q, { if (*q == v) r = 1; });
  }
  return r;
}
// "S" just emitted at bin_ptr - 1, a new temp to follow. Same value in a
// slot already? -> that slot (the chain dropped, L slot emitted), else 0
// after recording the chain for temp t. from: the alternative's start.
uchar vn_temp(word from, uchar t) {
  word s = (word)(bin_ptr - 1), st = vn_chain(from, s), h, e, pc, n, w;
  uchar len, k, ok;
  if (st == 0xffff || s - st < 2 || s - st > 32) return 0;
  len = (uchar)(s - st);
  h = vn_hash(st, len);
  k = (uchar)(h & (VN_N - 1));
  e = vn_t[k].at;
  ok = vn_t[k].h == h && vn_t[k].len == len &&
       e + len + 2 <= st && e >= vn_label && vn_label <= st &&
       bin[e + len] == 'S' && bin[e + len + 1] == vn_t[k].t &&
       !memcmp(bin + e, bin + st, len);
  // nothing between changes it
  for (pc = e + len + 2; ok && pc < st; pc += n) {
    uchar op = bin[pc];
    n = ivm_oplen(bin + pc);
    if (!n || op >= 0xC0 || strchr("tunBkxyzXQ;q", op)) { ok = 0; break; }
    if (op < 32 || op >= 0x80) continue;
    w = WRITES_VAR(op) ? bin[pc + 1] : 0xffff;
    if (w == vn_t[k].t || (w != 0xffff && vn_reads(e, len, (uchar)w, 0)))
      ok = 0;
    if (strchr(")]!w,Am", op) && vn_reads(e, len, 0xff, 1)) ok = 0;
  }
  if (ok && pc == st) {
    bin_ptr = st;
    emitop2('L', vn_t[k].t);
    return vn_t[k].t;
  }
  vn_t[k].h = h; vn_t[k].at = st; vn_t[k].len = len; vn_t[k].t = t;
  return 0;
}
#endif

// IVM_CSE: dead loads + gen_swap: on for host builds, off for cc65 unless
// -DIVM_CSE. The value numbering (vn_temp) is always on (-DIVM_NO_VN: off)
#if !defined(__CC65__) && !defined(IVM_NO_CSE)
  #define IVM_CSE
#endif

#ifdef IVM_CSE
// "load x; OP y; S t" at pc? -> its length, x op y t
static uchar cse_seq(word pc, uchar *x, uchar *op, uchar *y, uchar *t) {
  const uchar *a = bin + pc;
  uchar l;
  if (a[0] < 32) { *x = a[0]; l = 1; }
  else if (a[0] == 'L') { *x = a[1]; l = 2; }
  else return 0;
  if (pc + l + 4 > (word)bin_ptr) return 0;
  if (!strchr("+-*/%&|^", a[l]) || a[l + 2] != 'S') return 0;
  if (TGT(pc + l) || TGT(pc + l + 2)) return 0;
  *op = a[l]; *y = a[l + 1]; *t = a[l + 3];
  if (*t == *x || *t == *y) return 0;
  return (uchar)(l + 4);
}

// "L x" (or x) right before an op that overwrites ACC without reading it:
// the load is dead (vn_temp's "L t1" before "( a t1")
static int gen_deadload(void) {
  word pc, n, t, nb = (word)((bin_ptr >> 3) + 1), m;
  uchar *a, hit = 0, k;

  gen_tgt = (uchar *)calloc(nb, 2);
  if (!gen_tgt) IVM_GEN_FAIL("out of memory (deadload)");
  gen_del = gen_tgt + nb;
  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;
    t = ivm_jump(a, pc);
    if (t < (word)bin_ptr) SETBIT(gen_tgt, t);
  }
  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;
    if (!(a[0] < 32 || a[0] == 'L') || pc + n >= (word)bin_ptr) continue;
    m = pc + n;
    if (TGT(m)) continue;
    // next op sets ACC from scratch (and does not read it)
    if (bin[m] < 32 || strchr("L09([@rlK", bin[m])) {
      for (k = 0; k < n; k++) DEL(pc + k);
      hit = 1;
    }
  }
  return hit;
}

static int gen_swap(void) {
  word pc, n, t, nb = (word)((bin_ptr >> 3) + 1), q;
  uchar *a, x, op, y, k, m, ti, l, hit = 0, base, i;

  gen_tgt = (uchar *)calloc(nb, 2);
  if (!gen_tgt) IVM_GEN_FAIL("out of memory (swap)");
  gen_del = gen_tgt + nb;
  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;
    t = ivm_jump(a, pc);
    if (t < (word)bin_ptr) SETBIT(gen_tgt, t);
  }
  for (n = 0; n < nvector; n++) SETBIT(gen_tgt, vectors[n]);
  SETBIT(gen_tgt, main_offset);

  for (pc = 0; pc < (word)bin_ptr; pc += n) {
    a = bin + pc;
    n = ivm_oplen(a);
    if (!n) n = 1;
    if (a[0] != '(' || pc + 5 > (word)bin_ptr || a[3] != 'S') continue;
    base = a[1]; i = a[2]; ti = a[4];
    q = pc + 5;
    m = cse_seq(q, &x, &op, &y, &k);        // k computed here?
    if (!m && bin[q] == 'C' && !TGT(q)) {   // or copied (value numbering)
      m = 3; k = bin[q + 1];
    }
    if (m && (k == base || k == i || k == ti)) continue;
    q += m;
    if (q + 13 > (word)bin_ptr || bin[q] != '(' || bin[q + 1] != base) continue;
    if (!m) k = bin[q + 2];
    else if (bin[q + 2] != k) continue;
    if (k == base || k == i || ti == base || ti == i || ti == k) continue;
    if (bin[q + 3] != ')' || bin[q + 4] != base || bin[q + 5] != i) continue;
    if (bin[q + 6] < 32) { if (bin[q + 6] != ti) continue; l = 1; }
    else if (bin[q + 6] == 'L' && bin[q + 7] == ti) l = 2;
    else continue;
    if (bin[q + 6 + l] != ')' || bin[q + 7 + l] != base || bin[q + 8 + l] != k)
      continue;
    // no jump into it
    {
      word e = q + 9 + l, z;
      for (z = pc + 1; z < e && !TGT(z); z++) ;
      if (z < e) continue;
      // rewrite: middle part first, then A a i k, S t, the rest DEL
      memmove(bin + pc, bin + pc + 5, m);
      z = pc + m;
      bin[z] = 'A'; bin[z + 1] = base; bin[z + 2] = i; bin[z + 3] = k;
      bin[z + 4] = 'S'; bin[z + 5] = ti;
      for (z += 6; z < e; z++) DEL(z);
      hit = 1;
      n = (word)(e - pc);
    }
  }
  return hit;
}
#endif // IVM_CSE

// -O: [registers][globals][frames packed by call graph]
// Fills map[] for the variable slots, returns the first free slot (consts).
static uchar gen_pack(uchar *map) {
  uchar s, v, f0, b, e, top;
  word i;
  Symbol *y;

  // registers + globals: only slots a symbol still names, in slot order
  // (failed alts don't give back next_reg / next_glob: those are holes)
  f0 = 0;
  for (s = 0; s < FRAME_BASE; s++)
    for (i = 0, y = syms; i < nsym; i++, y++)
      if (y->type == SYM_VAR && y->address == s) {
        map[s] = f0++;
        break;
      }
  top = f0;

  // frame of v starts fneed[v] cells above f0
  for (v = 0; v < nvector; v++) {
    b = frame_base[v];
    e = (v + 1 < nvector) ? frame_base[v + 1] : next_slot;
    for (s = b; s < e; s++)
      map[s] = f0 + fneed[v] + s - b;
    if (f0 + fneed[v] + e - b > top)
      top = f0 + fneed[v] + e - b;
  }

  // temps allocated before the first frame: own cells on top
  e = nvector ? frame_base[0] : next_slot;
  for (s = FRAME_BASE; s < e; s++)
    map[s] = top++;

  return top;
}

// after remap: frame bases and symbols follow map[] (also const aliases)
static void gen_map_syms(uchar *map) {
  word i;
  Symbol *s;

  for (i = 0; i < nvector; i++)
    frame_base[i] = map[frame_base[i]];

  for (i = 0; i < nsym; i++) {
    s = &syms[i];
    if (s->type == SYM_VAR) {
      s->address = map[s->address];
      continue;
    }
    // FUNC: address is the vector
    s->ret_slot = map[s->ret_slot];
    s->ret_addr_slot = map[s->ret_addr_slot];
  }
}

// Build IVM image (magic IVM\0 + sections). Caller free().
// Const pool keeps original index order (bytecode slots = CONST_TOP - i).
// String consts are packed as blob-relative offsets to data (after fixup).
// One reloc walk from header.main through vectors + strings stops at 0000.
// Integer consts after 0000 are not relocated. Data bytes live at h.data.
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
  uchar nv, entry_vec;
  int i;
  word v;
  uchar has0;
  uchar tramp[2];
  
  // Pascal program body has main_offset but no FUNC vector — add one for _start
  nv = nvector;
  if (main_vec == (uchar)0xff && nv < (uchar)MAX_VECTORS)
    nv = (uchar)(nv + 1);

  need = (unsigned)sizeof(GenBlobHeader)
       + (unsigned)nv * sizeof(word)
       + (unsigned)(nconst + 1) * sizeof(word) // reloc.. + 0 + ints (0 may already be in pool)
       + (unsigned)nv
       + (unsigned)bin_ptr
       + 2 // _start: Vmain q
       + (unsigned)data_len + 16;

  body = (uchar *)malloc(need);
  if (!body) IVM_GEN_FAIL("malloc blob");

  p = body + sizeof(GenBlobHeader);




  // Call vectors only — no 0000 terminator. Reloc walk continues into
  // string const words that follow, then stops at the 0000 zero const.
  h.call = gen_memadd(body, &p, 0, (word)(nv * sizeof(word)));

  // Ensure real 0 is in the pool (also terminates reloc walk in the image)
  has0 = 0;
  for (i = 0; i < (int)nconst; i++)
    if (!const_is_reloc[i] && const_pool[i] == 0) { has0 = 1; break; }
  if (!has0) add_const(0);

  // common subexpressions, array swaps (before the slot remap: also
  // without -O)
#ifdef IVM_CSE
  if (gen_deadload()) gen_squeeze();
  else free(gen_tgt);
  if (gen_swap()) gen_squeeze();
  else free(gen_tgt);
#endif

  // Pack const image at h.con: [string data-offs...][0000][other ints].
  // Map old slot -> new slot; rewrite only real slot *operands* in bin[]
  // (not a blind scan — that could hit opcodes / G-rel / string bytes).
  {
    uchar *map; // old_slot -> new_slot
    uchar nn = 0, n0 = nconst, os;
    int k;
    word pc, w;
    uchar op;

    map = (uchar *)malloc(256);
    if (!map)
      IVM_GEN_FAIL("out of memory (const pack)");
    for (i = 0; i < 256; i++) map[i] = (uchar)i;

    // count packed length first (strings + zero + other ints)
    for (i = 0; i < (int)n0; i++)
      if (const_is_reloc[i]) nn++;
    nn++; // zero
    for (i = 0; i < (int)n0; i++)
      if (!const_is_reloc[i] && const_pool[i] != 0) nn++;

    // vars: -O closes the gaps (gen_pack), consts right after (no gap)
    constidx = optimize ? gen_pack(map) : next_slot;
    if (constidx + nn > CONST_TOP + 1) IVM_GEN_FAIL("constants collided with frames");
    k = 0;
    // fill map and write words straight into the blob (no new_pool copy)
    h.con = (word)(p - body);

    // 1) reloc strings (read only const_pool[]; write LE words into blob)
    for (i = 0; i < (int)n0; i++) {
      if (!const_is_reloc[i]) continue;
      os = (uchar)(CONST_TOP - i);
      map[os] = (uchar)(constidx + k);
      v = const_pool[i];
      if (const_is_reloc[i] == 2) v = (word)(data_len + v);   // bss: past data
      p[0] = (uchar)v;
      p[1] = (uchar)(v >> 8);
      p += 2;
      k++;
    }
    // 2) zero — reloc walk end + real 0 constant
    for (i = 0; i < (int)n0; i++) {
      if (!const_is_reloc[i] && const_pool[i] == 0) {
        map[(uchar)(CONST_TOP - i)] = (uchar)(constidx + k);
        break;
      }
    }
    p[0] = 0;
    p[1] = 0;
    p += 2;
    k++;
    // 3) other integer consts
    for (i = 0; i < (int)n0; i++) {
      if (const_is_reloc[i] || const_pool[i] == 0) continue;
      os = (uchar)(CONST_TOP - i);
      map[os] = (uchar)(constidx + k);
      v = const_pool[i];
      p[0] = (uchar)v;
      p[1] = (uchar)(v >> 8);
      p += 2;
      k++;
    }
    nconst = (uchar)k;
    // refresh pool from what we packed (blob LE); reloc flags: [1..nstr) then 0
    {
      uchar nstr = 0;
      uchar *q = body + h.con;
      for (i = 0; i < (int)n0; i++)
        if (const_is_reloc[i]) nstr++;
      for (i = 0; i < (int)nconst; i++) {
        const_pool[i] = (uint16_t)q[0] | ((uint16_t)q[1] << 8);
        const_is_reloc[i] = (i < nstr) ? 1 : 0;
        q += 2;
      }
    }

    // RELOCATE CONSTANTS by scanning code
  
    // Remap slot operands only (walk opcodes; skip string bodies / rel bytes)
    for (pc = 0; pc < (word)bin_ptr; pc += w) {
      uchar *a = bin + pc, *q;

      op = *a;
      w = ivm_oplen(a);
      if (!w) {
        printf("ivm-gen:FAIL: unknown opcode: %02x '%c'",
          op, isprint(op)?op:' ');
        exit(1);
      }

      // short load of slot op — only remap if that slot moved
      if (op < 32) {
        if (map[op] != op) *a = map[op];
        continue;
      }

      // Enforce that constants cannot be modified
      if (WRITES_VAR(op)) check_store(a[1]);

      IVM_EACH_SLOT(a, q, *q = map[*q]);
    }

    gen_map_syms(map);

    free(map);
  }

  // -O: peephole on the FINAL slot numbers (after the remap above), then
  // squeeze the DEL fillers out (fixes jumps, vectors[], main_offset).
  // Must run before frame bases / code / vectors are copied into the blob.
  if (optimize) {
    // twice: a removed jump's target, or a jump over removed code (zjp+0),
    // only shows up in the next round
    for (i = 0; i < 2; i++) {
      gen_peephole();
      gen_squeeze();
    }
  }

  // Entry for _start trampoline (call entry_vec then q):
  // 1) main_vec set — normal C "main"
  // 2) else synthesize vector for Pascal program body (main_offset only)
  // 3) else entry_vec=0 if vector table full
  if (main_vec != (uchar)0xff)
    entry_vec = main_vec;
  else {
    // program \+; begin... has main_offset only — not a FUNC vector
    entry_vec = nvector;
    if (entry_vec < (uchar)MAX_VECTORS) {
      vectors[entry_vec] = main_offset;
      frame_base[entry_vec] = 0;
    } else
      entry_vec = 0;
  }
  
  h.fp   = gen_memadd(body, &p, frame_base, (word)nv);
  h.code = gen_memadd(body, &p, bin, (word)bin_ptr);

  // _start: call entry_vec, then q (X returns here — no FP_MAIN test)
  tramp[0] = (uchar)(0xC0 | (entry_vec & 0x3f));
  tramp[1] = 'q';
  h.main = gen_memadd(body, &p, tramp, 2);

  // Data section (strings); reloc consts index into this from 0
  h.data = gen_memadd(body, &p, data_pool, data_len);

  h.memsz = constidx + nconst; // vars + consts, no gap

  h.constidx = constidx;

  // magic header put first
  h.magic[0] = 'I';
  h.magic[1] = 'V';
  h.magic[2] = 'M';
  h.magic[3] = 0;
  h.len = p - body;

  // set call vectors offset telative to blob start?
  for (i = 0; i < nv; i++)
    ((word *)(body + h.call))[i] = (word)(vectors[i] + h.code);

  // String const words were data-relative; convert to blob-relative so the
  // single reloc walk (while *v) { *v += base }) works for vectors + strings.
  // Layout at h.con: [string blob-offs...][0000][ints...]. 0000 ends the walk.
  {
    word *cw = (word *)(body + h.con);
    while (*cw) {
      *cw = (word)(*cw + h.data);
      cw++;
    }
  }

  // wrap it all up with header of offsets into it
  memcpy(body, &h, sizeof(h));

  if (out_len) *out_len = h.len;

  return body;
}

#endif // IVM_GEN_C
