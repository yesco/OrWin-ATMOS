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
  #define MAX_CONSTS    256
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

// ---------------------------------------------------------------------------
// Vectors / main
// ---------------------------------------------------------------------------

#ifndef MAX_VECTORS
  #define MAX_VECTORS 64
#endif

uint16_t vectors[MAX_VECTORS];
uchar frame_base[MAX_VECTORS];
uchar nvector;
word main_offset;
uchar main_vec;           // vector index of function "main", 0xff = none

// The current func_entry being compiled
uchar pending_func_entry; // 0xff = none

// Deferred global initializers: applied at the start of main()
#ifndef MAX_GINIT
  #define MAX_GINIT 64
#endif
uchar ginit_slot[MAX_GINIT]; // destination var slot
uchar ginit_src[MAX_GINIT];  // source const slot
uchar nginit;


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
    uchar v = pending_func_entry;
    pending_func_entry = 0xff; /* clear first — gen may emit */
    gen_func_entry(v);
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
uint16_t const_pool[MAX_CONSTS];
uchar    const_is_reloc[MAX_CONSTS]; // 1 = value is offset into data section
uchar nconst = 0;
uchar constidx = 255; // lowest const slot; 255 = none
uchar nreloc_const = 0; // how many reloc entries (for pack)

// String / data pool (appended to blob at h.data)
#ifndef MAX_DATA
  #define MAX_DATA 2048
#endif
uchar data_pool[MAX_DATA];
word  data_len = 0;

// Type tags for captures / symbols (high byte of word results)
// Low 8 bits of a capture = slot or IMM; high 8 = type info.
// Bit 15 set (0x8000) => pointer. Low type char in bits 8-14: 'w'/'c'.
// Type lives in the HIGH byte; low byte is the slot/const index.
#define TY_WORD   ((word)(('w') << 8))   /* 0x7700 */
#define TY_CHAR   ((word)(('c') << 8))   /* 0x6300 */
#define TY_PTR    ((word)0x8000)         /* pointer flag in bit 15 */
#define CAP_SLOT(w)   ((uchar)((w) & 0xff))
#define CAP_TYPE(w)   ((word)((w) & 0xff00))
#define CAP_IS_PTR(w) (((w) & TY_PTR) != 0)
#define MAKE_CAP(slot, ty) ((word)(((word)(uchar)(slot)) | (word)(ty)))

uchar add_const_raw(unsigned int v, uchar reloc) {
  uchar i, addr;

  // dedup only non-reloc (integer) values; string offsets are unique by content
  if (!reloc) {
    for (i = 0; i < (uchar)nconst; i++)
      if (!const_is_reloc[i] && const_pool[i] == v)
        return (uchar)(CONST_TOP - i);
  }

  if (nconst >= MAX_CONSTS)
    IVM_GEN_FAIL("constant pool full");
  if ((word)(CONST_TOP - nconst) < (word)FRAME_BASE)
    IVM_GEN_FAIL("constants collided with frames");

  const_pool[nconst] = (uint16_t)v;
  const_is_reloc[nconst] = reloc;
  if (reloc) nreloc_const++;
  addr = (uchar)(CONST_TOP - nconst);
  nconst++;
  constidx = addr;
  return addr;
}

uchar add_const(unsigned int v) {
  return add_const_raw(v, 0);
}

// Append bytes to data_pool; return offset from start of data section.
// Then register a relocatable const whose value is that offset (filled with
// absolute data-section base at pack time → final value = data_base + off).
word data_append(const uchar *bytes, word n) {
  word off;
  if ((word)(data_len + n) > MAX_DATA)
    IVM_GEN_FAIL("data pool full");
  off = data_len;
  memcpy(data_pool + data_len, bytes, n);
  data_len = (word)(data_len + n);
  return off;
}

// Add a C string (already escaped, including trailing NUL) as a data constant.
// Returns the mem slot for the pointer const (like lit_addr for numbers).
uchar add_string_const(const uchar *bytes, word n) {
  word off = data_append(bytes, n);
  // value stored = offset within data section; pack will add h.data
  return add_const_raw((unsigned int)off, 1);
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

// ---------------------------------------------------------------------------
// Symbols and scopes — shared by tpi.c and parse.c
//
// Allocated on demand (grows SYM_GROW at a time), names are malloc'd at their
// exact length, so nothing is reserved for symbols that don't exist.
//
//   VAR   address = slot
//   FUNC  address = vector index, ret_addr_slot = frame base,
//         ret_slot = return value cell, nparams/param_slot = parameters
//
// scope: depth the name lives in; scope_leave() drops everything declared at
// the depth being left (names are appended, so those are the newest ones).
// sym_truncate() drops back to an earlier count (parse.c backtracking).
//
// Pointers from sym_add() stay valid only until the next sym_add(): keep the
// index (nsym-1) and use &syms[idx] again afterwards.
// ---------------------------------------------------------------------------
#define SYM_ANY   0xff
#define SYM_NONE  0xffff
#define SYM_GROW  16
//#define SYM_GROW  250

typedef enum { SYM_VAR, SYM_FUNC } SymbolType;

typedef struct {
  char *name;          // malloc'd
  SymbolType type;
  uchar address;       // VAR: slot   FUNC: vector index
  uchar nparams;
  uchar param_slot[MAX_PARAMS];
  uchar ret_slot;
  uchar ret_addr_slot; // FUNC: frame base
  uchar is_ptr;        // 1 if pointer type (still one cell)
  uchar vtype;         // TY_WORD ('w') or TY_CHAR ('c') — element type
  uchar scope;         // scope depth it was declared in
  uchar pidx;          // parameter number + 1 (0 = not a parameter)
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

    printf("nsym= %u symcap = %u\n", nsym, sym_cap);
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
  s->nparams = 0;
  s->ret_slot = 0xff;
  s->ret_addr_slot = 0xff;
  s->is_ptr = 0;
  s->vtype = 'w';
  s->scope = scope_depth;
  s->pidx = 0;
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

  for (i = nsym; i > 0; i--)
    if (syms[i - 1].type == SYM_FUNC && syms[i - 1].address == v)
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

// emitload/emitstore live in tpi.c (frame-relative); parse uses emit/emitop2

void gen_reset(void) {
  bin_ptr = 0;
  nconst = 0;
  constidx = 255;
  nreloc_const = 0;
  data_len = 0;
  pending_func_entry = 0xff;
  main_vec = 0xff;
  main_offset = 0;
  nginit = 0;

  acc_invalidate();
}

void gen_func_entry(uchar v) {
  // main: emit deferred global inits first so they run on entry
  if (v == main_vec) {
    uchar i;
    main_offset = (word)bin_ptr;
    for (i = 0; i < nginit; i++) {
      emitop2('L', ginit_src[i]);
      emitop2('S', ginit_slot[i]);
    }
  }
  if (v < (uchar)MAX_VECTORS)
    vectors[v] = (uint16_t)(v == main_vec ? main_offset : bin_ptr);
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
// Const pool keeps original index order (bytecode slots = CONST_TOP - i).
// Relocatable string consts store a *data-relative* offset; the loader adds
// (blob_base + h.data) to form a usable pointer. Integer consts are raw values.
// Data section holds string bytes (and any other static data).
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
  int i;

  nv = nvector;

  need = (unsigned)sizeof(GenBlobHeader)
       + (unsigned)nv * sizeof(word) + sizeof(word)
       + (unsigned)nconst * sizeof(word)
       + (unsigned)nv
       + (unsigned)bin_ptr
       + (unsigned)data_len + 16;

  body = (uchar *)malloc(need);
  if (!body) IVM_GEN_FAIL("malloc blob");

  p = body + sizeof(GenBlobHeader);

  h.call = gen_memadd(body, &p, 0, (word)(nv * sizeof(word)));
  gen_memadd(body, &p, &zero, sizeof(zero));
  h.con  = gen_memadd(body, &p, const_pool, (word)(nconst * sizeof(word)));

  h.fp   = gen_memadd(body, &p, frame_base, (word)nv);
  h.code = gen_memadd(body, &p, bin, (word)bin_ptr);
  h.main = (word)(h.code + (word)main_offset);

  // Data section (strings). Reloc consts in the pool are offsets *into* this.
  h.data = gen_memadd(body, &p, data_pool, data_len);

  // Rewrite reloc const values in the blob to blob-relative addresses
  // (h.data + data_offset) so one +base reloc makes them absolute.
  {
    word *pool = (word *)(body + h.con);
    for (i = 0; i < (int)nconst; i++)
      if (const_is_reloc[i])
        pool[i] = (word)(h.data + const_pool[i]);
  }

  h.memsz = (word)(CONST_TOP + 1);
  h.constidx = (word)constidx;
  h.magic[0] = 'I';
  h.magic[1] = 'V';
  h.magic[2] = 'M';
  h.magic[3] = 0;
  h.len = (word)(p - body);

  for (i = 0; i < nv; i++)
    ((word *)(body + h.call))[i] = (word)(vectors[i] + h.code);

  memcpy(body, &h, sizeof(h));
  if (out_len)
    *out_len = (int)h.len;
  return body;
}

#endif // IVM_GEN_C
