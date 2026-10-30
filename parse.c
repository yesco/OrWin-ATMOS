// parse.c — iterative BNF bytecode interpreter
//
//   ./parse [-v|-vv] [-I|-C] grammar.bnf source.p
//
// BNF:
//   X=body     rule X (A-Z)
//   a-z/punct  match char (-I case-insensitive, default)
//   space      skip whitespace (c<=32) and { comments }
//   A-Z        call rule
//   \<         tail-recurse current rule (reset ip/alt, keep frame)
//   \C         commit: drop line buffer up to current point
//   \i         ident in scope (must be declared by \+)
//   \+         ident declare (match + add to current scope)
//   \f         declare function: name = ret-value cell of a new frame + vector
//   \p         declare parameter (frame cell base+4+n); \r ends the list
//   \r         end of parameter list (no input): last param -> base+3
//   \d         number
//   \s         string
//   \S         emit string bytes + \0 (opening " already matched)
//   \n         >=1 newline; eats surrounding horizontal space
//   \{ \}      enter / leave scope (\} drops names added in scope)
//   \| \c      literal | or other escaped char
//   |          alternative (epsilon allowed)
//   line '|'   continue previous rule (long alts)
//   line '.'   end of rules section
//   [ ... ]    production: after match, emit bytecode (tpi-style)
//   \1..\9     in production: insert result byte from nth value match
//              value matches: \i \+ \d and successful subrule calls
//
//   production only:
//         \aN force addr
//         \lN load capture N (not if ACC)
//         \t temp
//         \v call opcode
//         \k next call arg param slot
//         \x \y \w jump placeholder / patch / swap
//         \m \z loop mark / back
//         \[ \] literal   [ ]    [] alone: no code, result is ACC
//
// Explicit stack — no recursive match.
//
// capture == text held until alternative commits (then printed / flushed).


// EDING INSTRUCTION (don't remove!)
// =================================
//-2. do NOT remove my comments that describe sections!
//
//-1. DO NOT change lines/comments unless it is needed for functionality
// 0. Keep formatting as it is even if non-standard, adjust as neeed.
// 1. use // comments ONLY
// 2. prefer dynamic allocations ONLY except for static data
// 3. design to have an init(filename) method
// 4. assume there is "unbounded input" read using fgetc()
//    (buffer a programming line/statement, can reset beginnin of it)
//  4a. since we're doing backtracking, we may insert explicit \C
//      commit characters at the end of key rules. This resets the
//      buffer to beginning. For example once a statement has been
//      parsed there is no ambiguity; we're committed to accept it.
// 5. I think we could just read the bnf input, until a line
//    starts with a '.', there just stop reading rules.
// 6. use typedef unsigned char uchar; and remove ALL typecasts by using uchar everywhere
// 7. typedef uint16_t word; use "word" instead of int unless you need signed values,
//    prefer uchar if variable fits in it.
// 8. newline between defs and code
// 9. sizeof(xx) - parens a must on oscar64



// ARCHITECUTURE OVERVIEW
// ======================

// This is a step by step parser that using a simplified BNF
// description file parses a file of the specified langauge.
//
// rule[26] is one rule for every upper case letter.
// It stores a pointer to an allocated NULL terminated block
// of alt pointers into a string as follows:
//
// A single rule is stored as a continugous malloced space,
// including all alt variants.



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

#ifndef UCHAR
  #define UCHAR
  typedef unsigned char uchar;
  typedef uint16_t word;
#endif

#include "ivm-print.c"


// CONFIGURATION

#define MAX_BIN_LEN 4096
#define MAX_SLOTS 255

#define ERR_LINES_CONTEXT 4




// shared emit + real const addresses
static void note_load(uchar addr) { (void)addr; }
static void note_store(uchar addr) { (void)addr; }

#define IVM_GEN_FAIL(msg) do { fprintf(stderr, "%s\n", (msg)); abort(); } while (0)


#include "ivm-gen.c"

#define code_buf ((uchar *)bin)
#define code_len bin_ptr

uchar *g_loaded;

#define MAIN

// host libc already has strndup/memmem — keep ivm library, avoid name clash

#include <assert.h>
#include <string.h>
//#include <strings.h>

#include "ivm.c"

// A-Z
#define NRULES      26

#define MAX_STACK   64

// rule['X'-'A'] = NULL-terminated array of uchar* into one malloc blob
static uchar **rule[NRULES];

static FILE *src_fp;
static const char *src_name;

// input line buffer (unbounded source via fgetc; buffer one statement)
static uchar *line_buf;
static word line_no_base;
static word line_len;
static word line_cap;
static word line_pos;       // cursor in line_buf
static word line_abs;       // absolute file offset of line_buf[0]
static long file_pos;       // ftell-style count of bytes read from file

static word max_pos_abs;    // furthest absolute position
static uchar far_stack[MAX_STACK];
static uchar far_alt[MAX_STACK];
static uchar far_sp;

static uchar verbose;       // 0 off, 1 compact, 2+ detail
static uchar ignore_case;   // default 1

enum { ST_TRY, ST_RUN, ST_OK, ST_FAIL };

typedef struct {
  uchar rule;
  uchar alt;
  word ip;
  word save;          // line_pos at start of this alt
  word save_abs;      // absolute for furthest
  word cap_save;      // capture length at alt entry (for backtrack)
  word nnames_save;   // scope name count at alt entry
  uchar scope_save;   // scope_depth at alt entry
  uchar state;
  // production value captures \1..\9; result = rule value (word)
  // 0..254 slot, 0x80|idx const, 0xFFFF = ACC
  word prod_cap[10];
  uchar prod_n;
  word result;
  word code_save; // code_len at alt entry (backtrack)
  uchar slot_save;    // next_slot at alt entry (backtrack frees slots)
  uchar func_vec;     // vector of the function matched by \i in THIS frame (for \v)
  uchar arg_i;        // call args stored so far (rule Z, for \k)
  uchar npatch;       // pending forward jumps (\x pushes, \y pops)
  word patch[4];
  word mark;          // \m loop start (for \z)
} Frame;

static Frame stack[MAX_STACK];
static uchar sp;

// capture: hold matched text until alt commits (grows)
static uchar *capture[MAX_STACK];
static word  cap_len[MAX_STACK];
static word  cap_max[MAX_STACK];

// scopes: \+ adds name at scope_depth; \} drops names with name_scope >= depth
#define MAX_NAMES  256

// TODO: wtf, told you to move scoping/env/var stuff out to ivm-gen.c
//   now you're back to static?

#define NAME_LEN   32

static char  names[MAX_NAMES][NAME_LEN];
static uchar name_scope[MAX_NAMES];
static word  nnames;
static uchar scope_depth;
static char  last_undef[NAME_LEN]; // last \i that failed scope check
static uchar had_undef;
static uchar had_undef_func;
static word  undef_pos_abs;

// name -> slot index (same order as names[])
static uchar name_slot[MAX_NAMES];
static uchar name_vec[MAX_NAMES]; // 0xff = not a function; else vector index
static uchar name_pidx[MAX_NAMES]; // parameter number + 1, 0 = not a parameter
static uchar last_func_vec; // last \i that was a function (set by \f)
static uchar next_slot; // next free slot index for \+ decls / temps

// function frames (same layout as tpi.c):
//   base+0 ret addr, +1 ret value (= function name), +2 prev fp,
//   +3 last param (JSR stores ACC there), +4.. other params, then locals
#define MAX_PARAMS 4
static uchar fbase[MAX_VECTORS];              // frame base per vector -> blob fp table
static uchar fnparams[MAX_VECTORS];           // parameter count per vector
static uchar fpslot[MAX_VECTORS][MAX_PARAMS]; // slot of each parameter

// const + bin from ivm-gen.c (code_buf/code_len macros)
#define VAL_ACC  0xffff

// ---- logging ----

#include <stdio.h>

#define GREEN  "\033[32m"
#define NORMAL "\033[0m"

static void vlog(const char *fmt, ...) {
  va_list ap;

  if (verbose < 2) return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

// TODO: use or remove? prefer save bytes for cc65, use it if can
static void v1(const char *fmt, ...) {
  va_list ap;

  if (verbose < 1) return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

// TODO: use or remove? prefer save bytes for cc65, use it if can
static void v1_indent(void) {
  uchar i;

  if (verbose < 1) return;

  for (i = 0; i < sp; i++)
    fputc(' ', stderr);
}

// ---- line buffer ----

static int is_ws(uchar c) {
  // c <= 32 was the old rule; isspace is clearer and smaller on cc65
  return isspace(c);
}


// append one char from file into line_buf
static int line_grow(uchar c) {
  if (line_len + 1 >= line_cap) {
    word ncap = line_cap ? (word)(line_cap * 2) : 256;
    uchar *n = (uchar *)realloc(line_buf, ncap);

    if (!n) return 0;
    line_buf = n;
    line_cap = ncap;
  }
  line_buf[line_len++] = c;
  return 1;
}

// ensure at least one more char available at line_pos (read from file)
static int line_need(void) {
  int ch;

  if (line_pos < line_len) return 1;
  if (!src_fp) return 0;
  ch = fgetc(src_fp);
  if (ch == EOF) return 0;
  file_pos++;
  if (!line_grow((uchar)ch)) return 0;
  return 1;
}

static word abs_pos(void) {
  return (word)(line_abs + line_pos);
}

// \C commit: discard buffer before line_pos
static void line_commit(void) {
  word i, rest;

  if (line_pos == 0) return;

  // keep line numbers accurate without re-scanning the file
  for (i = 0; i < line_pos; i++)
    if (line_buf[i] == '\n')
      line_no_base++;

  rest = line_len - line_pos;
  for (i = 0; i < rest; i++)
    line_buf[i] = line_buf[line_pos + i];

  line_abs = (word)(line_abs + line_pos);
  line_len = rest;
  line_pos = 0;
}

static void capture_add(uchar c) {
  word i, nm;
  uchar *nbuf;

  if (sp == 0) return;

  i = (word)(sp - 1);
  if (cap_len[i] + 1 >= cap_max[i]) {
    nm = cap_max[i] ? (word)(cap_max[i] * 2) : 256;
    nbuf = (uchar *)realloc(capture[i], nm);
    if (!nbuf) return;
    capture[i] = nbuf;
    cap_max[i] = nm;
  }
  capture[i][cap_len[i]++] = c;
}

static void capture_commit(uchar frame) {
  word p, i, nm;
  uchar *nbuf;

  if (frame == 0) {
    for (i = 0; i < cap_len[0]; i++)
      putchar(capture[0][i]);
    fflush(stdout);
    cap_len[0] = 0;
    return;
  }

  p = (word)(frame - 1);
  for (i = 0; i < cap_len[frame]; i++) {
    if (cap_len[p] + 1 >= cap_max[p]) {
      nm = cap_max[p] ? (word)(cap_max[p] * 2) : 256;
      nbuf = (uchar *)realloc(capture[p], nm);
      if (!nbuf) break;
      capture[p] = nbuf;
      cap_max[p] = nm;
    }
    capture[p][cap_len[p]++] = capture[frame][i];
  }
  cap_len[frame] = 0;
}

static void note_progress(void) {
  uchar i;
  word a = abs_pos();

  if (a <= max_pos_abs) return;
  max_pos_abs = a;
  far_sp = sp;
  if (far_sp > MAX_STACK) far_sp = MAX_STACK;
  for (i = 0; i < far_sp; i++) {
    far_stack[i] = (uchar)('A' + stack[i].rule);
    far_alt[i] = stack[i].alt;
  }
}


// ---- scopes ----

static int name_eq(const char *a, const char *b) {
  // identifiers are always case-sensitive
  return strcmp(a, b) == 0;
}

static int name_in_scope(const char *id) {
  word i;

  for (i = 0; i < nnames; i++) {
    if (name_eq(names[i], id))
      return 1;
  }
  return 0;
}

// innermost declaration wins (names are appended in declaration order)
static word find_name(const char *id) {
  word i;

  for (i = nnames; i > 0; i--) {
    if (name_eq(names[i - 1], id))
      return (word)(i - 1);
  }
  return 0xffff;
}

// ---- bytecode emission (tpi-style) ----

// record a value-producing match into \\1..\\9 of current frame
static void record_capture(word val) {
  Frame *fr;

  if (sp == 0) return;
  fr = &stack[sp - 1];
  if (fr->prod_n < 9) {
    fr->prod_n++;
    fr->prod_cap[fr->prod_n] = val;
  }
  fr->result = val;
}

// slots: globals, frames, params, locals and temps share one counter
// (a failed alt gives its slots back, see restore_scope)
static uchar alloc_slot(void) {
  uchar s;

  if (next_slot >= CONST_TOP - 32) IVM_GEN_FAIL("out of slots");
  s = next_slot;
  next_slot++;
  return s;
}

// spill ACC to temp slot; return mem address for capture n
static word force_addr(uchar n) {
  Frame *fr;
  word v;
  uchar tmp;

  if (sp == 0) return 0;
  fr = &stack[sp - 1];
  if (n < 1 || n > fr->prod_n) return 0;
  v = fr->prod_cap[n];
  if (v != VAL_ACC) return v;
  tmp = alloc_slot();
  emitop2('S', tmp);
  fr->prod_cap[n] = tmp;
  if (verbose >= 1) {
    fprintf(stderr, "  spill->%02x\n", tmp);
    fflush(stderr);
  }
  return tmp;
}

static uchar lookup_slot(const char *id) {
  word i;

  i = find_name(id);
  if (i == 0xffff) return 0xff;
  return name_slot[i];
}


// run production at p (first char after '['); returns after ']'
// quiet: GEN is printed only when the owning alt commits (ST_OK)
static uchar *run_production(uchar *p) {
  Frame *fr;
  uchar n, b, *q;
  word w;

  fr = &stack[sp - 1];

  // ACC captures this production refers to are spilled FIRST, so the
  // spill code lands before the bytes that use it (\l takes ACC as is)
  for (q = p; *q && *q != ']'; q++) {
    if (*q != '\\') continue;
    q++;
    if (!*q) break;
    if (*q == 'l') continue;
    if (*q == 'a' && q[1]) q++;
    if (*q >= '1' && *q <= '9') {
      n = (uchar)(*q - '0');
      if (n <= fr->prod_n && fr->prod_cap[n] == VAL_ACC)
        force_addr(n);
      // TDOO: should 1..9 be relative to frame pointer?
    }
  }

  while (*p && *p != ']') {
    if (*p == '\\') {
      if (p[1] == 'v') {
        uchar op = (uchar)(0xC0 | (fr->func_vec & 0x3f));
        emitop(op);
        p += 2;
        continue;
      }
      if (p[1] == 'a' && p[2] >= '1' && p[2] <= '9') {
        n = (uchar)(p[2] - '0');
        w = force_addr(n);
        b = (uchar)(w & 0xff);
        emit(b);
        p += 3;
        continue;
      }
      // \lN: load capture N into ACC unless it already is there
      if (p[1] == 'l' && p[2] >= '1' && p[2] <= '9') {
        n = (uchar)(p[2] - '0');
        w = (n <= fr->prod_n) ? fr->prod_cap[n] : 0;
        if (w != VAL_ACC) {
          b = (uchar)as_addr((uchar)(w & 0xff));
          emitop2('L', b);
        }
        p += 3;
        continue;
      }
      if (p[1] >= '1' && p[1] <= '9') {
        n = (uchar)(p[1] - '0');
        w = (n <= fr->prod_n) ? fr->prod_cap[n] : 0;
        if (w == VAL_ACC) w = force_addr(n);
        b = (uchar)as_addr((uchar)(w & 0xff));
        emit(b); 
        p += 2;
        continue;
      }
      // \[ \] emit a literal bracket
      if (p[1] == '[' || p[1] == ']') {
        emitop(p[1]);
        p += 2;
        continue;
      }
      // \t: new temp slot, emit it, remember it as a capture
      if (p[1] == 't') {
        b = alloc_slot();
        emit(b);
        record_capture(b);
        p += 2;
        continue;
      }
      // \x: forward jump placeholder
      if (p[1] == 'x') {
        emit(0);
        if (fr->npatch < 4)
          fr->patch[fr->npatch++] = (word)(code_len - 1);
        p += 2;
        continue;
      }
      // \y: patch newest placeholder to jump here
      if (p[1] == 'y') {
        if (fr->npatch) {
          w = fr->patch[--fr->npatch];
          if (code_len - (w + 1) > 255) IVM_GEN_FAIL("jump too far");
          code_buf[w] = (uchar)(code_len - (w + 1));
        }
        p += 2;
        continue;
      }
      // \w: swap the two newest placeholders
      if (p[1] == 'w') {
        if (fr->npatch >= 2) {
          w = fr->patch[fr->npatch - 1];
          fr->patch[fr->npatch - 1] = fr->patch[fr->npatch - 2];
          fr->patch[fr->npatch - 2] = w;
        }
        p += 2;
        continue;
      }
      // \m: remember loop start
      if (p[1] == 'm') {
        fr->mark = (word)code_len;
        p += 2;
        continue;
      }
      // \z: backward jump delta to the mark (after the B opcode)
      if (p[1] == 'z') {
        w = (word)(code_len + 1 - fr->mark);
        if (w > 255) IVM_GEN_FAIL("loop body too large");
        emit(w);
        p += 2;
        continue;
      }
      // \k: param slot for the next call argument; frame above is the caller
      // (the rule that matched \i and called Z)
      if (p[1] == 'k') {
        uchar cv;

        cv = (sp >= 2) ? stack[sp - 2].func_vec : 0xff;
        if (cv == 0xff || (word)(fr->arg_i + 1) >= fnparams[cv])
          IVM_GEN_FAIL("too many arguments");
// TODO: shouldn't this generate a "Sxx" ???
        emit(fpslot[cv][fr->arg_i++]);
        p += 2;
        continue;
      }
    }
    b = *p++;
    emit(b);
  }

  if (*p == ']') p++;
  fr->result = VAL_ACC;
  return p;
}


static int name_add(const char *id) {
  word i;

  if (nnames >= MAX_NAMES) return 0;

  // only the current scope: an inner name may shadow an outer one
  for (i = 0; i < nnames; i++)
    if (name_scope[i] == scope_depth && name_eq(names[i], id)) return 1;

  // not found
  strncpy(names[nnames], id, NAME_LEN - 1);
  names[nnames][NAME_LEN - 1] = 0;
  name_scope[nnames] = scope_depth;
  name_slot[nnames] = alloc_slot();
  name_vec[nnames] = 0xff;
  name_pidx[nnames] = 0;
  nnames++;
  return 1;
}

// same, but at a fixed slot (function parameters)
static int name_add_at(const char *id, uchar slot, uchar pidx) {
  word i;

  if (nnames >= MAX_NAMES) return 0;

  for (i = 0; i < nnames; i++)
    if (name_scope[i] == scope_depth && name_eq(names[i], id)) return 0;

  strncpy(names[nnames], id, NAME_LEN - 1);
  names[nnames][NAME_LEN - 1] = 0;
  name_scope[nnames] = scope_depth;
  name_slot[nnames] = slot;
  name_vec[nnames] = 0xff;
  name_pidx[nnames] = pidx;
  nnames++;
  return 1;
}

static void scope_enter(void) {
  if (scope_depth < 255) scope_depth++;
}

static void scope_leave(void) {
  word i, w;

  if (scope_depth == 0) return;

  // remove last scope names
  w = 0;
  for (i = 0; i < nnames; i++) {
    if (name_scope[i] < scope_depth) {
      if (w != i) {
        memcpy(names[w], names[i], NAME_LEN);
        name_scope[w] = name_scope[i];
        name_slot[w] = name_slot[i];
        name_vec[w] = name_vec[i];
        name_pidx[w] = name_pidx[i];
      }
      w++;
    }
  }

  nnames = w;
  scope_depth--;
}

static void skip_hspace(void) {
  uchar c;

  while (line_need()) {
    c = line_buf[line_pos];
    if (c == ' ' || c == '\t' || c == '\r') {
      capture_add(c);
      line_pos++;
    } else
      break;
  }
}

// ---- skip whitespace and { comments } ----

static void skip_ws(void) {
  for (;;) {

    while (line_need() && is_ws(line_buf[line_pos])) {
      capture_add(line_buf[line_pos]);
      line_pos++;
      note_progress();
    }
    if (!line_need()) return;

    // skip { comments }
    if (line_buf[line_pos] != '{') return;
    capture_add(line_buf[line_pos]);

    line_pos++;
    while (line_need() && line_buf[line_pos] != '}') {
      capture_add(line_buf[line_pos]);
      line_pos++;
    }

    if (line_need() && line_buf[line_pos] == '}') {
      capture_add(line_buf[line_pos]);
      line_pos++;
      note_progress();
    }
  }
}

// \n in BNF: hspace, >=1 newline, hspace; then comments via skip_ws
static int match_newline(void) {
  word nnl;

  skip_hspace();
  nnl = 0;
  while (line_need() && line_buf[line_pos] == '\n') {
    capture_add('\n');
    line_pos++;
    nnl++;
    note_progress();
    skip_hspace();
  }
  if (nnl == 0) return 0;
  skip_ws();
  return 1;
}


static int match_lit(uchar want) {
  uchar g;

  skip_ws();
  if (!line_need()) return 0;
  g = line_buf[line_pos];
  // -I: fold case for keyword/literal letters only (idents use name_eq)
  if (ignore_case && isalpha(want)) {
    if (tolower(g) != tolower(want)) return 0;
  } else if (g != want) {
    return 0;
  }
  capture_add(g);
  line_pos++;
  note_progress();
  return 1;
}

static int match_ident_raw(char *idbuf, word idmax) {
  uchar c;
  word n;

  skip_ws();
  if (!line_need()) return 0;

  c = line_buf[line_pos];
  if (!isalpha(c))
    return 0;

  n = 0;
  while (line_need()) {
    c = line_buf[line_pos];
    // letter, digit, or underscore
    if (isalnum(c) || c == '_') {
      if (n + 1 < idmax)
        idbuf[n++] = c;
      capture_add(c);
      line_pos++;
    } else
      break;
  }
  idbuf[n] = 0;
  note_progress();
  return 1;
}

// \i — ident must already be in scope; result = its slot
static int match_ident(void) {
  char id[NAME_LEN];
  uchar slot, c;
  word start_abs, save_pos, i;

  start_abs = abs_pos();
  if (!match_ident_raw(id, NAME_LEN)) return 0;
  if (!name_in_scope(id)) {
    save_pos = line_pos;
    skip_ws();
    c = line_need() ? line_buf[line_pos] : 0;
    line_pos = save_pos;
    if (c == '(') {
      if (!had_undef_func || start_abs >= undef_pos_abs) {
        strncpy(last_undef, id, NAME_LEN - 1);
        last_undef[NAME_LEN - 1] = 0;
        had_undef = 1;
        had_undef_func = 1;
        undef_pos_abs = start_abs;
      }
    } else if (!had_undef_func) {
      strncpy(last_undef, id, NAME_LEN - 1);
      last_undef[NAME_LEN - 1] = 0;
      had_undef = 1;
      undef_pos_abs = start_abs;
    }
    vlog("  ident '%s' not in scope\n", id);
    return 0;
  }
  i = find_name(id);
  slot = name_slot[i];
  last_func_vec = name_vec[i];
  // kept in the frame: later \i matches (call arguments) must not change \v
  stack[sp - 1].func_vec = last_func_vec;
  record_capture(slot);
  return 1;
}

// \+ — declare ident into current scope; result = new slot
static int match_ident_decl(void) {
  char id[NAME_LEN];
  uchar slot;

  if (!match_ident_raw(id, NAME_LEN)) return 0;
  if (!name_add(id)) return 0;
  slot = lookup_slot(id);
  record_capture(slot);
  vlog("  decl '%s' scope %u slot %u\n", id, (unsigned)scope_depth, (unsigned)slot);
  return 1;
}

// \f — declare function: new frame + vector index; mark entry at current bin_ptr later
static int match_ident_func_decl(void) {
  char id[NAME_LEN];
  uchar slot, base;
  word i;

  if (!match_ident_raw(id, NAME_LEN)) return 0;

  // frame: base+0 ret addr, +1 ret value (the function name), +2 prev fp,
  // +3 last param (JSR stores ACC there), +4.. other params, then locals
  base = next_slot;
  if (base >= CONST_TOP - 40) return 0;
  next_slot = (uchar)(base + 1);        // name_add takes the ret-value cell
  if (!name_add(id)) {
    next_slot = base;
    return 0;
  }
  next_slot = (uchar)(base + 4);
  slot = lookup_slot(id);
  i = find_name(id);
  name_vec[i] = nvector;
  fbase[nvector] = base;
  fnparams[nvector] = 0;
  if (nvector < (uchar)(MAX_VECTORS - 1)) nvector++;
  last_func_vec = name_vec[i];
  pending_func_entry = last_func_vec;
  record_capture(slot);
  vlog("  func '%s' slot %u vec %u frame %u\n", id, (unsigned)slot, (unsigned)last_func_vec, (unsigned)base);
  return 1;
}

// \p — declare parameter n of the function just declared by \f
static int match_param_decl(void) {
  char id[NAME_LEN];
  uchar k, slot, v;
  word i;

  if (!match_ident_raw(id, NAME_LEN)) return 0;
  v = last_func_vec;
  if (v == 0xff) return 0;
  // parameter number = params already in this scope (rewinds on backtrack)
  k = 0;
  for (i = 0; i < nnames; i++)
    if (name_scope[i] == scope_depth && name_pidx[i]) k++;
  if (k >= MAX_PARAMS) return 0;
  slot = (uchar)(fbase[v] + 4 + k);
  if (!name_add_at(id, slot, (uchar)(k + 1))) return 0;
  if (next_slot <= slot) next_slot = (uchar)(slot + 1);
  record_capture(slot);
  vlog("  param '%s' #%u slot %u\n", id, (unsigned)k, (unsigned)slot);
  return 1;
}

// \r — end of parameter list (no input): the last param lives at fp+3
static int params_done(void) {
  uchar v, n;
  word i, last;

  v = last_func_vec;
  if (v == 0xff) return 0;
  n = 0;
  last = 0;
  for (i = 0; i < nnames; i++) {
    if (name_scope[i] == scope_depth && name_pidx[i]) {
      fpslot[v][n] = name_slot[i];
      last = i;
      n++;
    }
  }
  if (n) {
    name_slot[last] = (uchar)(fbase[v] + 3);
    fpslot[v][n - 1] = name_slot[last];
    next_slot--;        // its old cell was the top one: reuse it for locals
  }
  fnparams[v] = n;
  vlog("  params done vec %u n=%u\n", (unsigned)v, (unsigned)n);
  return 1;
}

static int match_num(void) {
  word v;
  uchar slot;

  skip_ws();
  if (!line_need()) return 0;
  if (!isdigit(line_buf[line_pos])) return 0;

  v = 0;
  while (line_need() && isdigit(line_buf[line_pos])) {
    v = (word)(v * 10 + (line_buf[line_pos] - '0'));
    capture_add(line_buf[line_pos]);
    line_pos++;
  }
  note_progress();
  slot = (uchar)as_addr((uchar)lit_addr((unsigned int)v));
  record_capture(slot);
  return 1;
}

static int match_str_emit(void) {
  if (!line_need()) return 0;
  while (line_need() && line_buf[line_pos] != '"') {
    uchar c = line_buf[line_pos];
    if (c == '\\') {
      line_pos++;
      if (!line_need()) return 0;
      c = line_buf[line_pos];
    }
    emit(c);
    capture_add(c);
    line_pos++;
  }
  if (!line_need() || line_buf[line_pos] != '"') return 0;
  line_pos++;
  emit(0);
  note_progress();
  return 1;
}

static int match_str(void) {
  skip_ws();
  if (!line_need() || line_buf[line_pos] != '"') return 0;

  capture_add(line_buf[line_pos]);
  line_pos++;

  while (line_need() && line_buf[line_pos] != '"') {

    // \c quoted char
    if (line_buf[line_pos] == '\\') {
      capture_add(line_buf[line_pos]);
      line_pos++;
      if (!line_need()) return 0;
      capture_add(line_buf[line_pos]);
      line_pos++;
    } else {
      capture_add(line_buf[line_pos]);
      line_pos++;
    }
  }

  if (!line_need() || line_buf[line_pos] != '"') return 0;
  capture_add(line_buf[line_pos]);
  line_pos++;

  note_progress();

  return 1;
}

// ---- BNF load: one malloc blob per rule, alts[] NULL-terminated ----

static void free_rules(void) {
  uchar r;

  for (r = 0; r < NRULES; r++) {
    if (rule[r]) {
      // alts point into blob; blob is rule[r][0]'s base — store blob at [-1]?
      // we keep blob pointer as rule[r][0] only if first alt non-null.
      // simpler: first entry is special — actually free blob via separate table
      free(rule[r]);
      rule[r] = NULL;
    }
  }
}

// blobs[r] holds the contiguous text for rule r
static uchar *blobs[NRULES];

// escapes that stay "\c" in the blob (all others become the plain char c)
static int esc_kept(uchar e) {
  if (e >= '1' && e <= '9') return 1;
  return strchr("insSCfv<d+{}[]#=aptrlxywmzk", e) != NULL;
}

static void build_one_rule(uchar r, char *rhs) {
  char *p;
  uchar *blob, **altp, e, depth;
  word blob_len, a, bi, na, alt_starts[64];

  // expand \| and \c (not specials) into blob; keep \i \n \s \C \< \d \+ \{ \}
  // count alts and build contiguous string with 0 separators between alts
  blob_len = 0;
  for (p = rhs; *p; p++) {
    if (*p == '\\' && p[1]) {
      e = (uchar)p[1];
      if (esc_kept(e))
        blob_len = (word)(blob_len + 2);
      else
        blob_len++;
      p++;
      continue;
    }
    if (*p == '|') {
      blob_len++;
      continue;
    }
    blob_len++;
  }

  blob = (uchar *)malloc(blob_len + 64);
  if (!blob) return;
  free(blobs[r]);
  free(rule[r]);
  blobs[r] = blob;

  bi = 0;
  na = 0;
  depth = 0;
  alt_starts[na++] = 0;
  for (p = rhs; *p; p++) {
    if (*p == '\\' && p[1]) {
      e = (uchar)p[1];
      if (esc_kept(e)) {
        blob[bi++] = '\\';
        blob[bi++] = e;
      } else {
        blob[bi++] = e;
      }
      p++;
      continue;
    }
    // '|' separates alts only outside a bare [ ... ] production
    if (*p == '|' && depth == 0) {
      // skip empty alts from || or line-join artifacts
      if (na > 0 && alt_starts[na - 1] == bi)
        continue;
      blob[bi++] = 0;
      if (na < 64)
        alt_starts[na++] = bi;
      continue;
    }
    // bare [ ] = production; | inside is a literal (\[ \] are handled above)
    if (*p == '[') depth++;
    else if (*p == ']' && depth) depth--;
    blob[bi++] = (uchar)*p;
  }
  blob[bi++] = 0;

  altp = (uchar **)malloc(sizeof(uchar *) * (na + 1));
  if (!altp) {
    free(blob);
    blobs[r] = NULL;
    return;
  }
  for (a = 0; a < na; a++)
    altp[a] = blob + alt_starts[a];
  altp[na] = NULL;
  rule[r] = altp;
}

#define MAX_LINE 1024
#define MAX_ACCUM 4096

static int load_bnf(const char *path) {
  FILE *f = NULL;
  char *line = malloc(MAX_LINE);
  char *acc = malloc(MAX_ACCUM);
  uchar r, cur;
  char *nl, *eq, *p;
  word acc_len, have_cur;
  char ret = 0;
  
  if (!line || !acc) goto done;
  
  free_rules();

  for (r = 0; r < NRULES; r++) {
    blobs[r] = NULL;
    rule[r] = NULL;
  }

  f = fopen(path, "r");
  if (!f) {
    perror(path);
    goto done;
  }

  have_cur = 0;
  cur = 0;
  acc_len = 0;
  acc[0] = 0;

  while (fgets(line, MAX_LINE, f)) {

    nl = strchr(line, '\n');
    if (nl) *nl = 0;

    if (line[0] == '.') break;
    if (line[0] == 0 || line[0] == '#') continue;

    p = line;
    while (*p == ' ' || *p == '\t') p++;

    // continuation: line starts with |
    if (*p == '|' && have_cur) {
      while (*p && acc_len + 1 < MAX_ACCUM)
        acc[acc_len++] = *p++;
      acc[acc_len] = 0;
      continue;
    }

    // new rule: flush previous
    if (have_cur)
      build_one_rule(cur, acc);

    if (!isupper(line[0])) {
      have_cur = 0;
      continue;
    }

    eq = strchr(line, '=');
    if (!eq) {
      have_cur = 0;
      continue;
    }

    cur = (uchar)(line[0] - 'A');
    have_cur = 1;
    p = eq + 1;
    acc_len = 0;
    while (*p && acc_len + 1 < MAX_ACCUM)
      acc[acc_len++] = *p++;
    acc[acc_len] = 0;
  }

  if (have_cur)
    build_one_rule(cur, acc);

  ret = 1;

 done:
  if (f) fclose(f);
  free(line);
  free(acc);
  return ret;
}

static int rule_defined(uchar r) {
  return rule[r] != NULL;
}

static word rule_nalt(uchar r) {
  word n = 0;

  if (!rule[r]) return 0;
  while (rule[r][n]) n++;
  return n;
}

// ---- stack ----

static void push(uchar r, uchar state) {
  if (sp >= MAX_STACK) {
    fprintf(stderr, "stack overflow\n");
    exit(1);
  }
  stack[sp].rule = r;
  stack[sp].alt = 0;
  stack[sp].ip = 0;
  stack[sp].save = line_pos;
  stack[sp].save_abs = abs_pos();
  stack[sp].cap_save = 0;
  stack[sp].nnames_save = nnames;
  stack[sp].scope_save = scope_depth;
  stack[sp].slot_save = next_slot;
  stack[sp].func_vec = 0xff;
  stack[sp].arg_i = 0;
  stack[sp].npatch = 0;
  stack[sp].mark = 0;
  stack[sp].state = state;
  stack[sp].prod_n = 0;
  stack[sp].result = 0;
  stack[sp].code_save = code_len;
  {
    uchar zi;
    for (zi = 0; zi < 10; zi++)
      stack[sp].prod_cap[zi] = 0;
  }
  // program main begin → blob.main
  if (r == (uchar)('B' - 'A') && sp > 0 &&
      stack[sp - 1].rule == (uchar)('P' - 'A'))
    main_offset = bin_ptr;
  cap_len[sp] = 0;
  sp++;
}

static void restore_scope(Frame *fr) {
  nnames = fr->nnames_save;
  scope_depth = fr->scope_save;
  next_slot = fr->slot_save;
}

// hex digit value for \#xx
static uchar hexval(uchar c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = tolower(c);
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

// ---- one step ----
// estimated ~50-200us per step on 6502 @ 1MHz (dispatch + match)

static int step(void) {
  Frame *fr;
  uchar *alt;
  uchar c;
  word nalt;

  if (sp == 0) return 0;
  fr = &stack[sp - 1];
  nalt = rule_nalt(fr->rule);

  if (fr->state == ST_TRY) {
    if (fr->alt >= nalt) {
      vlog("fail %c (no alt) @%u\n", 'A' + fr->rule, abs_pos());
      sp--;
      if (sp == 0) return -1;
      fr = &stack[sp - 1];
      // parent's current alt failed: ST_FAIL restores pos, capture,
      // code, scope, then moves to the next alt
      fr->state = ST_FAIL;
      return 1;
    }
    line_pos = fr->save;
    cap_len[sp - 1] = fr->cap_save;
    code_len = fr->code_save;
    fr->prod_n = 0;
    fr->npatch = 0;
    had_undef = 0;
    fr->ip = 0;
    fr->state = ST_RUN;
    vlog("try %c alt %u @%u \"%s\"\n",
         'A' + fr->rule, fr->alt, abs_pos(),
         rule[fr->rule][fr->alt] ? (char *)rule[fr->rule][fr->alt] : "");
    return 1;
  }

  if (fr->state == ST_OK) {
    vlog("ok %c alt %u -> @%u\n", 'A' + fr->rule, fr->alt, abs_pos());
    // -v: one GEN per statement (K), after alt commits
    if (verbose >= 1 && code_len > fr->code_save &&
        (uchar)('A' + fr->rule) == 'K') {
      fprintf(stderr, NORMAL "GEN:          [ ");
      ivm_print_code_range(stderr, code_buf, fr->code_save, code_len, 0);
      fprintf(stderr, "]\n");
      fflush(stderr);
    }
    if (verbose >= 1 && cap_len[sp - 1] > 0) {
      word i;
      uchar rch = (uchar)('A' + fr->rule);
      // -v: SRC for K/B/P only; -vv: all rules + tags
      if (verbose >= 2 || rch == 'K' || rch == 'B' || rch == 'P') {
        if (verbose >= 2) {
          fprintf(stderr, "    %c", rch);
          if (nalt > 1) fprintf(stderr, "%u", fr->alt);
          fprintf(stderr, ":\n");
        }
        fprintf(stderr, "SRC: " GREEN);
        for (i = 0; i < cap_len[sp - 1]; i++) {
          uchar ch = capture[sp - 1][i];
          if (ch == '\n') fprintf(stderr, NORMAL "\n" "SRC:" GREEN);
          else fputc(ch, stderr);
        }
        fprintf(stderr, NORMAL "\n");
        if (verbose >= 2)
          fprintf(stderr, "    <%c\n", rch);
        fflush(stderr);
      }
    }

    capture_commit((uchar)(sp - 1));
    {
      word child_res = stack[sp - 1].result;
      sp--;
      if (sp == 0) return 0;
      // parent records subrule result as next \n capture
      record_capture(child_res);
      fr = &stack[sp - 1];
      fr->ip++;
      fr->state = ST_RUN;
    }
    return 1;
  }

  // current alt failed — next alt of same rule
  if (fr->state == ST_FAIL) {
    line_pos = fr->save;
    cap_len[sp - 1] = fr->cap_save;
    code_len = fr->code_save;
    fr->prod_n = 0;
    fr->npatch = 0;
    restore_scope(fr);
    fr->alt++;
    fr->ip = 0;
    fr->state = ST_TRY;
    return 1;
  }

  // ST_RUN
  alt = rule[fr->rule][fr->alt];
  if (!alt || alt[fr->ip] == 0) {
    fr->state = ST_OK;
    return 1;
  }

  c = alt[fr->ip];

  // space in BNF = required ws gap: skip comments/whitespace, no input char consumed as literal
  if (c == ' ') {
    skip_ws();
    fr->ip++;
    return 1;
  }

  // bare [ ... ] = production (use \[ \] in BNF to match bracket chars)
  if (c == '[') {
    uchar *p = run_production(alt + fr->ip + 1);
    fr->ip = (word)(p - alt);
    return 1;
  }

  if (isupper(c)) {
    uchar r = (uchar)(c - 'A');

    if (!rule_defined(r)) {
      fprintf(stderr, "\n[no rule %c]\n", c);
      fr->state = ST_FAIL;
      return 1;
    }
    vlog("  call %c from %c @%u\n", c, 'A' + fr->rule, abs_pos());
    push(r, ST_TRY);
    return 1;
  }

  if (c == '\\') {
    uchar e = alt[fr->ip + 1];
    int ok = 0;

    if (e == '<') {
      // tail-recurse: restart alts at current input, keep capture
      fr->alt = 0;
      fr->ip = 0;
      fr->save = line_pos;
      fr->save_abs = abs_pos();
      fr->cap_save = cap_len[sp - 1];
      fr->nnames_save = nnames;
      fr->scope_save = scope_depth;
      fr->slot_save = next_slot;
      fr->code_save = code_len;
      fr->prod_n = 0;
      fr->state = ST_TRY;
      vlog("  tail %c @%u\n", 'A' + fr->rule, abs_pos());
      return 1;
    }

    if (e == 'C') {
      // commit: drop buffer prefix; keep matching
      line_commit();
      // fix save positions relative to new buffer
      {
        uchar i;

        for (i = 0; i < sp; i++) {
          if (stack[i].save > 0)
            stack[i].save = 0;
        }
      }
      fr->ip = (word)(fr->ip + 2);
      vlog("  commit @%u\n", abs_pos());
      return 1;
    }

    if (e == '{') {
      scope_enter();
      fr->ip = (word)(fr->ip + 2);
      vlog("  scope enter %u\n", (unsigned)scope_depth);
      return 1;
    }

    if (e == '}') {
      scope_leave();
      fr->ip = (word)(fr->ip + 2);
      vlog("  scope leave %u\n", (unsigned)scope_depth);
      return 1;
    }

    // \#xx: result := hex xx, same as \d having matched that number (no input)
    if (e == '#') {
      word v;
      uchar slot;

      if (!alt[fr->ip + 2] || !alt[fr->ip + 3]) {
        fr->state = ST_FAIL;
        return 1;
      }
      v = (word)(hexval(alt[fr->ip + 2]) * 16 + hexval(alt[fr->ip + 3]));
      slot = (uchar)as_addr((uchar)lit_addr((unsigned int)v));
      record_capture(slot);
      fr->ip = (word)(fr->ip + 4);
      vlog("  hex const %u\n", (unsigned)v);
      return 1;
    }

    // \=n: result := capture n (no new capture recorded)
    if (e == '=') {
      uchar n = (uchar)(alt[fr->ip + 2] - '0');

      if (n >= 1 && n <= fr->prod_n)
        fr->result = fr->prod_cap[n];
      fr->ip = (word)(fr->ip + 3);
      return 1;
    }

    // \[ \] = literal brackets (not production)
    if (e == '[' || e == ']') {
      skip_ws();
      ok = match_lit(e);
    }
    else if (e == 'i') ok = match_ident();
    else if (e == '+') ok = match_ident_decl();
    else if (e == 'f') ok = match_ident_func_decl();
    else if (e == 'p') ok = match_param_decl();
    else if (e == 'r') ok = params_done();
    else if (e == 'd') ok = match_num();
    else if (e == 'n') ok = match_newline();
    else if (e == 's') ok = match_str();
    else if (e == 'S') ok = match_str_emit();
    else {
      skip_ws();
      ok = match_lit(e);
    }
    if (!ok) {
      vlog("  fail \\%c @%u\n", e, abs_pos());
      fr->state = ST_FAIL;
      return 1;
    }
    vlog("  ok \\%c @%u\n", e, abs_pos());
    fr->ip = (word)(fr->ip + 2);
    return 1;
  }

  skip_ws();
  if (!match_lit(c)) {
    vlog("  fail lit '%c' @%u got '%c'\n", c, abs_pos(),
         line_need() ? line_buf[line_pos] : '?');
    fr->state = ST_FAIL;
    return 1;
  }
  vlog("  ok lit '%c' @%u\n", c, abs_pos());
  fr->ip++;
  return 1;
}

// run up to n steps
// ~50-200 us/step on 6502 @ 1MHz → n=1000 ≈ 50-200 ms
static int run(word n) {
  word i;
  int rc = 1;

  for (i = 0; i < n && rc == 1; i++)
    rc = step();
  return rc;
}

static void report_reject(void) {
  word i;

  fprintf(stderr, "\n--- REJECT at %u (furthest %u) ---\n",
          abs_pos(), max_pos_abs);

  fprintf(stderr, "STACK:");
  if (far_sp == 0) fprintf(stderr, " (empty)");

  for (i = 0; i < far_sp; i++)
    fprintf(stderr, " %c%u", far_stack[i], (unsigned)far_alt[i]);

  fprintf(stderr, "\n");

  if (far_sp > 0) {
    uchar r = (uchar)(far_stack[far_sp - 1] - 'A');
    uchar a = far_alt[far_sp - 1];

    fprintf(stderr, "EXPECTED: rule %c", far_stack[far_sp - 1]);
    if (rule_defined(r) && a < rule_nalt(r))
      fprintf(stderr, " alt %u \"%s\"", a,
              rule[r][a] ? rule[r][a] : "");

    fprintf(stderr, "\n");
  }

  if (had_undef && had_undef_func)
    fprintf(stderr, "ERROR: undefined function: %s\n", last_undef);
  else if (had_undef)
    fprintf(stderr, "ERROR: identifier \"%s\" not defined here\n", last_undef);

  // Show error from line_buf only — no rewind/fseek (cc65-friendly).
  // line_no_base tracks the line number of line_buf[0] across \C commits.
  fprintf(stderr, "REJECT:\n");
  {
    word want = (word)((had_undef && undef_pos_abs) ? undef_pos_abs : max_pos_abs);
    word i, start, off, col, line, endpos;
    word nlines;

    if (!line_buf || line_len == 0) {
      fprintf(stderr, "  (no buffered source)\n");
    } else if (want < line_abs) {

      // furthest was discarded by \C; still report position
      fprintf(stderr, "%s: (offset %u, before current buffer @%u)\n",
              src_name ? src_name : "?", (unsigned)want, (unsigned)line_abs);

    } else {

      off = (word)(want - line_abs);
      if (off > line_len) off = line_len;

      // line start / column within buffer
      start = 0;
      line = line_no_base;
      col = 0;
      for (i = 0; i < off; i++) {
        if (line_buf[i] == '\n') {
          line++;
          start = i + 1;
          col = 0;
        } else
          col++;
      }

      // error line text
// TODO: seems to only print the current
      fprintf(stderr, "%s:%u: ", src_name ? src_name : "?", line);
      for (i = start; i < line_len && line_buf[i] != '\n'; i++)
        fputc(line_buf[i], stderr);
      fputc('\n', stderr);

      // caret under column
      fprintf(stderr, "%s:%u: ", src_name ? src_name : "?", line);
      for (i = 0; i < col; i++)
        fputc(' ', stderr);
      fprintf(stderr, "^\n");

      // following lines still in buffer (no second open of the file)
      endpos = start;
      while (endpos < line_len && line_buf[endpos] != '\n')
        endpos++;
      if (endpos < line_len && line_buf[endpos] == '\n')
        endpos++;

      nlines = 0;
      while (nlines < ERR_LINES_CONTEXT && endpos < line_len) {
        fprintf(stderr, "%s:%u: ", src_name ? src_name : "?",
          line + 1 + nlines);
        while (endpos < line_len && line_buf[endpos] != '\n') {
          fputc(line_buf[endpos], stderr);
          endpos++;
        }
        fputc('\n', stderr);
        if (endpos < line_len && line_buf[endpos] == '\n')
          endpos++;
        nlines++;
      }
    }
  }

}


// init: load grammar + open source
static int init(const char *bnf_path, const char *src_path) {
  uchar i;

  src_name = src_path;
  if (!load_bnf(bnf_path)) return 0;

  if (src_fp) fclose(src_fp);
  src_fp = fopen(src_path, "r");
  if (!src_fp) {
    perror(src_path);
    return 0;
  }

  line_buf = NULL;
  line_no_base = 0;
  line_cap = 0;
  line_len = 0;
  line_pos = 0;
  line_abs = 0;
  file_pos = 0;
  max_pos_abs = 0;
  far_sp = 0;
  sp = 0;
  nnames = 0;
  scope_depth = 0;
  had_undef = 0;
  had_undef_func = 0;
  undef_pos_abs = 0;
  last_undef[0] = 0;
  next_slot = 0;
  gen_reset();
  nvector = 0;
  last_func_vec = 0xff;
  memset(fbase, 0, sizeof(fbase));
  memset(fnparams, 0, sizeof(fnparams));
  memset(fpslot, 0, sizeof(fpslot));
  // no pre-seeded names — X must be declared in source if used
  for (i = 0; i < MAX_STACK; i++) {
    capture[i] = NULL;
    cap_len[i] = 0;
    cap_max[i] = 0;
  }

  if (!rule_defined((uchar)('P' - 'A'))) {
    fprintf(stderr, "missing start rule P\n");
    return 0;
  }
  push((uchar)('P' - 'A'), ST_TRY);

  return 1;
}

static void shutdown(void) {
  uchar i;

  if (src_fp) {
    fclose(src_fp);
    src_fp = NULL;
  }
  free(line_buf);
  line_buf = NULL;

  for (i = 0; i < MAX_STACK; i++) {
    free(capture[i]);
    capture[i] = NULL;
  }

  for (i = 0; i < NRULES; i++) {
    free(rule[i]);
    rule[i] = NULL;
    free(blobs[i]);
    blobs[i] = NULL;
  }
}

int main(int argc, char **argv) {
  const char *bnf = NULL;
  const char *srcpath = NULL;
  word i, a;
  int rc;
  int err= 0;

  verbose = 0;
  ignore_case = 1;

  // parse arguments
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0)
      verbose++;
    else if (strcmp(argv[i], "-I") == 0)
      ignore_case = 1;
    else if (strcmp(argv[i], "-C") == 0)
      ignore_case = 0;
    else if (!bnf)
      bnf = argv[i];
    else if (!srcpath)
      srcpath = argv[i];
  }

  if (!bnf || !srcpath) {
    fprintf(stderr, "usage: %s [-v|-vv] [-I|-C] grammar.bnf source.p\n", argv[0]);
    return 1;
  }

  // setup
  if (!init(bnf, srcpath)) return 1;

  // print rules
  if (verbose >= 2) {

    fprintf(stderr, "--- rules ---\n");
    for (i = 0; i < NRULES; i++)

      if (rule_defined((uchar)i)) {
        fprintf(stderr, "%c =", 'A' + (int)i);
        for (a = 0; rule[i][a]; a++) {
          if (a) fprintf(stderr, "|");
          fprintf(stderr, "%s", (char *)rule[i][a]);
        }
        fprintf(stderr, "\n");
      }
  }
  if (verbose)
    fprintf(stderr, "--- parse ---\n");

  // run in chunks of 10000 steps until done
  do {
    rc = run(10000);
  } while (rc == 1);

  // consistency check
  putchar('\n');
  if (rc == 0) {
    // OK
    
    skip_ws();
    if (line_need()) {
      fprintf(stderr, "--- OK but trailing input ---\n");
      err = 2; goto doclean;
    }

    fprintf(stderr, "--- ACCEPT ---\n");
      
    fprintf(stderr, "--- BYTECODE (%u bytes) ---\n", (unsigned)code_len);

    ivm_print_code(stderr, code_buf, code_len);
    fputc('\n', stderr);

    // dump hex
    for (i = 0; i < code_len; i++) {
      fprintf(stderr, "%02x", code_buf[i]);
      if ((i & 15) == 15) fputc('\n', stderr);
      else fputc(' ', stderr);
    }
    if (code_len & 15) fputc('\n', stderr);

    // quit so run_ivm can stop
    emitop('q');
    {
      int blen = 0;
      uchar *blob = build_blob(&blen);
      Process *proc;
      ivmvar ret;

      fprintf(stderr, "--- BLOB (%d bytes) main=%u nvector=%d ---\n",
        blen, main_offset, nvector);

      if (blob) {
        // frame bases: JSR keeps ret addr / ACC at fp+0 / fp+3, ';' returns fp+1
        for (i = 0; i < nvector; i++) {
          blob[((BlobHeader *)blob)->fp + i] = fbase[i];
          if (verbose >= 1)
            fprintf(stderr, "  frame V%02x fp=%u nparams=%u\n",
              i, fbase[i], fnparams[i]);
        }

        proc = Nevm(blob);
        if (proc) {
          word *pool = (word *)(blob + ((BlobHeader *)blob)->con);
          for (i = 0; i < nconst; i++)
            proc->mem[CONST_TOP - i] = pool[i];
          fprintf(stderr, "--- RUN ---\n");
          ret = run_ivm(proc);
          fprintf(stderr, "--- END acc=%u ---\n", ret);
          free(proc);
        }
        free(blob);
      }
    }
    err = 0; goto doclean;
  }

  // error
  report_reject();
  err = 1;
  
doclean:
  shutdown();
  return err;
}
