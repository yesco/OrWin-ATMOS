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
uchar nvector;
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
    pending_func_entry = 0xff; // clear first — gen may emit
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
   (op) == 'i' || (op) == 'j' || (op) == 'k' \
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
uchar    nconst = 0;
uchar    constidx = 255; // lowest const slot; 255 = none
uchar    nreloc_const = 0; // how many reloc entries (for pack)

// String / data pool (appended to blob at h.data)
#ifndef MAX_DATA
  #define MAX_DATA 2048
#endif

uchar data_pool[MAX_DATA];
word  data_len = 0;

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

  uchar scope;         // scope depth it was declared in

  // type + pidx: could pack (pidx>0 => param; type==FUNC => function)
  SymbolType type;
  uchar pidx;          // parameter number + 1 (0 = not a parameter)
  uchar nparams;

  uchar param_slot[MAX_PARAMS];

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
  next_reg = 0;
  next_glob = 32;
  nreloc_const = 0;
  data_len = 0;
  pending_func_entry = 0xff;
  main_vec = 0xff;
  main_offset = 0;
  nginit = 0;

  acc_invalidate();
}

void gen_func_entry(uchar v) {
  uchar i, s;
  
  // main: emit deferred global inits first so they run on entry
  if (v == main_vec) {
    main_offset = (word)bin_ptr;
    for (i = 0; i < nginit; i++) {
      s= ginit_src[i];
      if (s == IMM0) {
        emitop2('Z', ginit_slot[i]);
      } else {
        emitop2('C', ginit_slot[i]); emit(ginit_src[i]);
      }
    }
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

  // Pack const image at h.con: [string data-offs...][0000][other ints].
  // Map old slot -> new slot; rewrite only real slot *operands* in bin[]
  // (not a blind scan — that could hit opcodes / G-rel / string bytes).
  {
    uchar *map; // old_slot -> new_slot
    uchar nn = 0, n0 = nconst, os;
    int k;
    word pc;
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

    constidx = (uchar)(CONST_TOP - nn + 1);
    k = 0;
    // fill map and write words straight into the blob (no new_pool copy)
    h.con = (word)(p - body);

    // 1) reloc strings (read only const_pool[]; write LE words into blob)
    for (i = 0; i < (int)n0; i++) {
      if (!const_is_reloc[i]) continue;
      os = (uchar)(CONST_TOP - i);
      map[os] = (uchar)(constidx + k);
      v = const_pool[i];
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

    // Remap slot operands only (walk opcodes; skip string bodies / rel bytes)
    for (pc = 0; pc < (word)bin_ptr; ) {
      op = bin[pc++];
      if (op < 32) {
        // short load of slot op — only remap if that slot moved
        if (map[op] != op) bin[pc - 1] = map[op];
        continue;
      }
      if (op & 0x80) continue; // call / hi-bit forms: not const slots
      // only first arg can be modified
      if (WRITES_VAR(op)) check_store(bin[pc]);
      switch (op) {
      case 'L': case 'S': case 'Z': case 'I': case 'J':
      case 'C': // C dest src — two slot operands
        if (op == 'C') {
          if (pc < (word)bin_ptr) { bin[pc] = map[bin[pc]]; pc++; }
          if (pc < (word)bin_ptr) { bin[pc] = map[bin[pc]]; pc++; }
          break;
        }
        if (pc < (word)bin_ptr) { bin[pc] = map[bin[pc]]; pc++; }
        break;
      case 'U':
        if (pc < (word)bin_ptr) { bin[pc] = map[bin[pc]]; pc++; }
        if (pc < (word)bin_ptr) { bin[pc] = map[bin[pc]]; pc++; }
        break;
      case 'i':
        if (pc + 2 < (word)bin_ptr) {
          bin[pc] = map[bin[pc]];
          bin[pc + 1] = map[bin[pc + 1]];
          pc += 3;
        }
        break;
      case 'n':
        if (pc < (word)bin_ptr) pc++; // relative back only
        break;
      case 'G': case 'B': // relative jump distance, not a slot
      case 'O': case 'W': // immediate char / print const? W uses PAR slot in some builds
        if (pc < (word)bin_ptr) pc++;
        break;
      // One slot operand (PAR = mem[slot]) — must remap after const pack
      case '+': case '-': case '*': case '/': case '%':
      case '&': case '|': case '^': case '[': case ']':
      case '{': case '}': case '<': case '=': case '>':
      case 'M': case 'F': case '@': case '!':
      case 'r': case 'w': case 'l': case 's': case '\'': case ',':
        if (pc < (word)bin_ptr) { bin[pc] = map[bin[pc]]; pc++; }
        break;
      case '(': case ')':
        if (pc + 1 < (word)bin_ptr) {
          bin[pc] = map[bin[pc]]; // array base slot
          pc += 2;
        }
        break;
      case '"':
        while (pc < (word)bin_ptr && bin[pc]) pc++;
        if (pc < (word)bin_ptr) pc++;
        break;
      default:
        break;
      }
    }

    free(map);
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

  h.memsz = CONST_TOP + 1; // compaction TODO

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
