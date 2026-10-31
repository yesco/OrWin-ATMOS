// parse.c — iterative BNF bytecode interpreter
//
//   ./parse [-v|-vv] [-I|-C] grammar.bnf source.p
//
// BNF:
//   X=body     rule X (A-Z)
//   a-z/punct  match char (-I case-insensitive, default)
//   space      skip whitespace and comments (styles from #= lines)
//   #= { }     block comment style (start end); multiple #= allowed
//   #= /* */   e.g. C block comments
//   #= //      line comment (to newline); no end token
//              default if no #=: Pascal { }
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
#include <setjmp.h>

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

// FAIL: real (non-backtracking) errors. Message is printed ONCE where
// setjmp is caught in main(), together with the source position.
// 6502: pointer fits in int → longjmp(jb, msg).
// Host: int is 32-bit, pointers 64-bit → stash in fail_msg.
// (a failed *match* is NOT an error: that is normal backtracking, return 0)
static jmp_buf fail_jmp;
static const char *fail_msg;
static const char *fail_file;   // optional: file name for "cannot open"

#ifdef __CC65__
  #define FAIL(msg)  longjmp(fail_jmp, (int)(msg))
#else
  #define FAIL(msg)  do { fail_msg = (msg); longjmp(fail_jmp, 1); } while (0)
#endif

// ivm-gen.c still uses the old name
#define IVM_GEN_FAIL(msg) FAIL(msg)


#include "ivm-gen.c"

#define code_buf ((uchar *)bin)
#define code_len bin_ptr

uchar *g_loaded;

#define MAIN

// host libc already has strndup/memmem — keep ivm library, avoid name clash

#include <assert.h>
#include <string.h>
//#include <strings.h>

#ifdef RUN
  #include "ivm.c"
#endif

// A-Z
#define NRULES      26

#define MAX_STACK   64

// rule['X'-'A'] = NULL-terminated array of uchar* into one malloc blob
static uchar **rule[NRULES];

// blobs[r] holds the contiguous text for rule r
static uchar *blobs[NRULES];

static FILE *src_fp;
static char *src_name;

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
  word nsym_save;     // symbol count at alt entry (backtrack drops newer ones)
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

// grows on demand (8 frames at a time, up to MAX_STACK)
static Frame *stack;
static uchar stack_cap;
static uchar sp;

// newest frame == &stack[sp-1], cached: only push (grow/++) and pop (--)
// change it. Valid only while sp > 0. Parent frame is cur[-1].
static Frame *cur;

// capture: hold matched text until alt commits (grows)
static uchar *capture[MAX_STACK];
static word  cap_len[MAX_STACK];
static word  cap_max[MAX_STACK];

// Symbols and scopes (\+ \f \p \i \{ \}) live in ivm-gen.c, allocated on demand:
//   sym_add / sym_find / sym_func / sym_truncate, scope_enter / scope_leave
// Function frames use the tpi.c layout, frame_base[vec] goes into the blob:
//   base+0 ret addr, +1 ret value (= function name), +2 prev fp,
//   +3 last param (JSR stores ACC there), +4.. other params, then locals

// longest identifier (stack buffers only)
#define NAME_LEN   32

// the identifier just matched by match_ident_raw (shared, no arguments)
static char  id_buf[NAME_LEN];

static char  last_undef[NAME_LEN]; // furthest undefined ident (for reject msg)
static uchar had_undef;            // sticky: saw undefined ident
static uchar had_undef_func;       // sticky: it was a function call
static word  undef_pos_abs;        // sticky: position of that ident

static uchar last_func_vec; // function being declared by \f (for \p \r)
static uchar next_slot; // next free slot index for \+ decls / temps

// const + bin from ivm-gen.c (code_buf/code_len macros)
#define VAL_ACC  0xffff

// Type of the declaration currently being parsed (set by matching I/T).
// Low byte is TY_WORD/TY_CHAR; OR with TY_PTR for pointers.
static word cur_type = TY_WORD;


// ---- logging ----

#include <stdio.h>

#define GREEN  "\033[32m"
#define NORMAL "\033[0m"

// -DNOVLOG: all vlog() calls (and their string literals) vanish
#ifdef NOVLOG

  #define vlog(...) ((void)0)

#else

static void vlog(const char *fmt, ...) {
  va_list ap;

  if (verbose < 2) return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

#endif

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

// c <= 32 was the old rule; isspace is clearer and smaller on cc65
#define is_ws(c) isspace(c)


// append one char from file into line_buf
static void line_grow(uchar c) {
  if (line_len + 1 >= line_cap) {
    word ncap = line_cap ? (word)(line_cap * 2) : 256;
    uchar *n = realloc(line_buf, ncap);

    if (!n) FAIL("out of memory (line)");
    line_buf = n;
    line_cap = ncap;
  }
  line_buf[line_len++] = c;
}

// read ONE more char from the file onto the end of line_buf
static int line_fetch(void) {
  int ch;

  if (!src_fp) return 0;
  ch = fgetc(src_fp);
  if (ch == EOF) return 0;
  file_pos++;
  line_grow(ch);
  return 1;
}

// ensure at least one more char available at line_pos (read from file)
static int line_need(void) {
  if (line_pos < line_len) return 1;
  return line_fetch();
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

// make room for one more byte in capture[i]
static void cap_room(word i) {
  word nm;
  uchar *nbuf;

  if (cap_len[i] + 1 < cap_max[i]) return;

  nm = cap_max[i] ? (word)(cap_max[i] * 2) : 256;
  nbuf = realloc(capture[i], nm);
  if (!nbuf) FAIL("out of memory (capture)");
  capture[i] = nbuf;
  cap_max[i] = nm;
}

static void capture_add(uchar c) {
  word i;

  // (also called after the run finished, sp == 0)
  if (sp == 0) return;

  i = (word)(sp - 1);
  cap_room(i);
  capture[i][cap_len[i]++] = c;
}

// consume one source char into the capture
static void eat(void) {
  capture_add(line_buf[line_pos++]);
}

// consume n source chars into the capture
static void eat_n(uchar n) {
  while (n--) eat();
}

static void capture_commit(uchar frame) {
  word p, i;

  if (frame == 0) {
    for (i = 0; i < cap_len[0]; i++)
      putchar(capture[0][i]);
    fflush(stdout);
    cap_len[0] = 0;
    return;
  }

  p = (word)(frame - 1);
  for (i = 0; i < cap_len[frame]; i++) {
    cap_room(p);
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
    far_stack[i] = 'A' + stack[i].rule;
    far_alt[i] = stack[i].alt;
  }
}


// ---- bytecode emission (tpi-style) ----

// hex digit value for \#xx
static uchar hexval(uchar c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = tolower(c);
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

// two hex digits at p -> byte
static uchar hex2(uchar *p) {
  return hexval(p[0]) * 16 + hexval(p[1]);
}

// const-pool slot holding value v
static uchar const_slot(unsigned int v) {
  return as_addr(lit_addr(v));
}

// record a value-producing match into \\1..\\9 of current frame
static void record_capture(word val) {
  if (cur->prod_n < 9) {
    cur->prod_n++;
    cur->prod_cap[cur->prod_n] = val;
  }

  cur->result = val;
}

// capture n of the current frame (0 if there is none)
static word cap_val(uchar n) {
  return (n <= cur->prod_n) ? cur->prod_cap[n] : 0;
}

// slots: globals, frames, params, locals and temps share one counter
// (a failed alt gives its slots back, see restore_scope)
static uchar alloc_slot(void) {
  uchar s;

  if (next_slot >= CONST_TOP - 32) FAIL("out of slots");

  s = next_slot;
  next_slot++;

  return s;
}

// spill ACC to temp slot; return mem address for capture n
static word force_addr(uchar n) {
  word v;
  uchar tmp;

  if (n < 1 || n > cur->prod_n) return 0;

  v = cur->prod_cap[n];
  if (v != VAL_ACC) return CAP_SLOT(v);

  tmp = alloc_slot();
  emitop2('S', tmp);

  cur->prod_cap[n] = tmp;
  if (verbose >= 1) {
    fprintf(stderr, "  spill->%02x\n", tmp);
    fflush(stderr);
  }
  return tmp;
}


// run production at p (first char after '['); returns after ']'
// quiet: GEN is printed only when the owning alt commits (ST_OK)
static uchar *run_production(uchar *p) {
  uchar n, b, e, adv, cv, dest, src, *q;
  word w, f;
  unsigned int val;
  Symbol *fs;

  // ACC captures this production refers to are spilled FIRST, so the
  // spill code lands before the bytes that use it (\l takes ACC as is)
  for (q = p; *q && *q != ']'; q++) {
    if (*q != '\\') continue;
    q++;
    if (!*q) break;
    if (*q == 'l') continue;
    if (*q == 'a' && q[1]) q++;
    if (*q >= '1' && *q <= '9') {
      n = *q - '0';
      if (n <= cur->prod_n && cur->prod_cap[n] == VAL_ACC)
        force_addr(n);
      // TDOO: should 1..9 be relative to frame pointer?
    }
  }

  while (*p && *p != ']') {
    if (*p == '\\') {
      e = p[1];
      n = p[2] - '0';   // capture digit for \a \l \c \h
      adv = 2;          // chars used; 0 = not an escape, emit it literally

      if (e >= '1' && e <= '9') {
        // \N: insert result byte of capture N
        w = cap_val(e - '0');
        if (w == VAL_ACC) w = force_addr(e - '0');
        emit(as_addr(w & 0xff));
      } else if ((e == 'a' || e == 'l' || e == 'c' || e == 'h') && n >= 1 && n <= 9) {
        adv = 3;
        w = cap_val(n);
        if (e == 'a') {
          // \aN: force addr
          emit(force_addr(n) & 0xff);
        } else if (e == 'l') {
          // \lN: load capture N into ACC unless it already is there
          if (w != VAL_ACC)
            emitop2('L', as_addr(w & 0xff));
        } else {
          // \cN: emit low byte of capture N's *value* (consts from \d / 'x')
          // \hN: emit high byte of that value
          val = 0;
          if (w != VAL_ACC) {
            b = w & 0xff;
            if (is_lit(b) || isconst(b))
              val = lit_val(b);
          }
          emit((e == 'c' ? val : val >> 8) & 0xff);
        }
      } else switch (e) {

      case 'v':
        emitop(0xC0 | (cur->func_vec & 0x3f));
        break;

      // \[ \] emit a literal bracket
      case '[': case ']':
        emitop(e);
        break;

      // \t: new temp slot, emit it, remember it as a capture
      // \e: exclusive limit — S temp; J temp (store ACC-1)
      case 't': case 'e':
        b = alloc_slot();
        if (e == 'e') {
          emitop2('S', b);
          emitop2('J', b);
        } else
          emit(b);
        record_capture(b);
        break;

      // \x: forward jump placeholder
      case 'x':
        emit(0);
        if (cur->npatch < 4)
          cur->patch[cur->npatch++] = code_len - 1;
        break;

      // \y: patch newest placeholder to jump here
      case 'y':
        if (cur->npatch) {
          w = cur->patch[--cur->npatch];
          if (code_len - (w + 1) > 255) FAIL("jump too far");
          code_buf[w] = code_len - (w + 1);
        }
        break;

      // \w: swap the two newest placeholders
      case 'w':
        if (cur->npatch >= 2) {
          w = cur->patch[cur->npatch - 1];
          cur->patch[cur->npatch - 1] = cur->patch[cur->npatch - 2];
          cur->patch[cur->npatch - 2] = w;
        }
        break;

      // \m: remember loop start
      case 'm':
        cur->mark = code_len;
        break;

      // \z: backward jump delta to the mark (after the B opcode)
      case 'z':
        w = code_len + 1 - cur->mark;
        if (w > 255) FAIL("loop body too large");
        emit(w);
        break;

      // \k: param slot for the next call argument; frame above is the caller
      // (the rule that matched \i and called Z)
      case 'k':
        cv = (sp >= 2) ? cur[-1].func_vec : 0xff;
        f = (cv == 0xff) ? SYM_NONE : sym_func(cv);
        if (f == SYM_NONE) FAIL("call of unknown function");
        fs = &syms[f];
        if ((word)(cur->arg_i + 1) >= fs->nparams)
          FAIL("too many arguments");
        // (the "S" opcode is not missing: the BNF has [S\k], \k is only the slot)
        emit(fs->param_slot[cur->arg_i++]);
        break;

      // \#xx: emit raw hex byte (e.g. \#00 for NUL, \#20 for space)
      case '#':
        if (p[2] && p[3]) {
          emit(hex2(p + 2));
          adv = 4;
        } else
          adv = 0;
        break;

      // \g: global/local init from captures \1=dest slot, \2=src const
      //   scope 0: defer until main entry; else emit L+S now
      case 'g':
        dest = cur->prod_cap[1] & 0xff;
        src  = as_addr(cur->prod_cap[2] & 0xff);
        if (scope_depth == 0) {
          if (nginit >= MAX_GINIT)
            FAIL("too many global inits");
          ginit_slot[nginit] = dest;
          ginit_src[nginit]  = src;
          nginit++;
          vlog("  ginit defer slot %u <- const %u\n", dest, src);
        } else {
          emitop2('L', src);
          emitop2('S', dest);
        }
        break;

      default:
        adv = 0;
      }

      if (adv) {
        p += adv;
        continue;
      }
    }
    b = *p++;
    emit(b);
  }

  if (*p == ']') p++;
  cur->result = VAL_ACC;
  return p;
}


static void skip_hspace(void) {
  uchar c;

  while (line_need()) {
    c = line_buf[line_pos];
    if (c == ' ' || c == '\t' || c == '\r')
      eat();
    else
      break;
  }
}

// ---- skip whitespace and comments (configured from BNF via #= lines) ----

#include <stdbool.h>
#include <string.h>

// Comment styles loaded from grammar:
//   #= { }        block comment  start="{", end="}"
//   #= /* */      block comment  start="/*", end="*/"
//   #= //         line  comment  start="//", runs to newline
// Multiple #= lines are allowed (C needs both /* */ and //).
// Default (no #=): Pascal-style { } only.

#define MAX_COMMENTS 4
#define COMM_START_MAX 4
#define COMM_END_MAX   4

typedef struct {
  char start[COMM_START_MAX]; // NUL-terminated, length 1..3
  char end[COMM_END_MAX];     // empty => line comment (to \n)
  uchar start_len;
  uchar end_len;              // 0 = line comment
} CommentStyle;

static CommentStyle comments[MAX_COMMENTS];
static uchar ncomments;

// Parse one #= directive body (after the "#="). Returns 1 on success.
// Forms:  "{ }" | "/* */" | "//"   (whitespace separates start/end)
static int comments_add_from_line(const char *p) {
  CommentStyle *cs;
  word i;

  if (ncomments >= MAX_COMMENTS) return 0;
  while (*p == ' ' || *p == '\t') p++;
  if (!*p) return 0;

  cs = &comments[ncomments];
  cs->start_len = 0;
  cs->end_len = 0;
  cs->start[0] = 0;
  cs->end[0] = 0;

  // start delimiter: non-space run
  i = 0;
  while (*p && *p != ' ' && *p != '\t' && i + 1 < COMM_START_MAX) {
    cs->start[i++] = *p++;
  }
  cs->start[i] = 0;
  cs->start_len = i;
  if (!cs->start_len) return 0;

  while (*p == ' ' || *p == '\t') p++;

  // optional end delimiter => block comment; none => line comment
  i = 0;
  while (*p && *p != ' ' && *p != '\t' && i + 1 < COMM_END_MAX) {
    cs->end[i++] = *p++;
  }
  cs->end[i] = 0;
  cs->end_len = i;

  ncomments++;
  return 1;
}

// Default: { } block (Pascal). Cleared/replaced when BNF has #= lines.
static void comments_set_default(void) {
  ncomments = 0;
  comments_add_from_line("{ }");
}

// True if line_buf[line_pos..] starts with s[0..len)
// Must read past line_len even when line_pos < line_len (line_need alone is not enough).
static int match_at(const char *s, uchar len) {
  uchar i;

  if (!len) return 0;
  for (i = 0; i < len; i++) {
    while (line_pos + i >= line_len)
      if (!line_fetch()) return 0;
    if (line_buf[line_pos + i] != (uchar)s[i]) return 0;
  }
  return 1;
}

static void skip_ws(void) {
  uchar ci;
  CommentStyle *cs;

  for (;;) {

    // whitespace
    while (line_need() && is_ws(line_buf[line_pos]))
      eat();
    note_progress();
    if (!line_need()) return;

    // try each configured comment style
    for (ci = 0; ci < ncomments; ci++) {
      cs = &comments[ci];
      if (!match_at(cs->start, cs->start_len)) continue;

      // consume start
      eat_n(cs->start_len);

      if (cs->end_len == 0) {
        // line comment: to newline (keep the \n) or EOF
        while (line_need() && line_buf[line_pos] != '\n')
          eat();
      } else {
        // block comment: to end marker
        while (line_need()) {
          if (match_at(cs->end, cs->end_len)) {
            eat_n(cs->end_len);
            break;
          }
          eat();
        }
      }
      note_progress();
      break;
    }

    // nothing matched
    if (ci >= ncomments) return;
  }
}

// \n in BNF: hspace, >=1 newline, hspace; then comments via skip_ws
static int match_newline(void) {
  word nnl;

  skip_hspace();
  nnl = 0;
  while (line_need() && line_buf[line_pos] == '\n') {
    eat();
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

  /* bare space in a rule is a ws-gap (handled in step); \  matches one space */
  if (want != ' ' && want != '\t')
    skip_ws();
  if (!line_need()) return 0;
  g = line_buf[line_pos];
  // -I: fold case for keyword/literal letters only (idents use strcmp)
  if (ignore_case && isalpha(want)) {
    if (tolower(g) != tolower(want)) return 0;
  } else if (g != want) {
    return 0;
  }
  eat();
  note_progress();
  return 1;
}

// match an identifier into id_buf
static int match_ident_raw(void) {
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
      if (n + 1 < NAME_LEN)
        id_buf[n++] = c;
      eat();
    } else
      break;
  }
  id_buf[n] = 0;
  note_progress();
  return 1;
}

// next non-blank source char after line_pos; nothing consumed or captured
static uchar peek_char(void) {
  word p, sv;
  uchar c;

  sv = line_pos;
  p = sv;
  for (;;) {
    line_pos = p;
    if (!line_need()) {
      c = 0;
      break;
    }
    c = line_buf[p];
    if (!is_ws(c)) break;
    p++;
  }
  line_pos = sv;
  return c;
}

// \i — ident must already be in scope; result = its slot
//   name followed by ( is looked up as a function (like tpi.c), else a variable


// Record undefined ident (id_buf) for reject message (keep furthest; funcs win over vars)
static void note_undef(uchar is_func, word pos) {
  if (!is_func && had_undef_func) return;
  if (had_undef && pos < undef_pos_abs) return;
  strncpy(last_undef, id_buf, NAME_LEN - 1);
  last_undef[NAME_LEN - 1] = 0;
  had_undef = 1;
  had_undef_func = is_func;
  undef_pos_abs = pos;
}

// type word of a symbol (for MAKE_CAP)
static word sym_type(Symbol *s) {
  return ((word)s->vtype << 8) | (s->is_ptr ? TY_PTR : 0);
}

// give a new symbol the type being declared (cur_type)
static void set_decl_type(Symbol *s) {
  s->vtype = CAP_TYPE(cur_type) >> 8;
  s->is_ptr = CAP_IS_PTR(cur_type) ? 1 : 0;
}

// \1..\9 — like \i, but slot must equal capture n (same variable).
// Fail quietly (no had_undef): rule should fall through to next alt.
static int match_ident_same(uchar n) {
  word i, expect;
  uchar slot;

  if (n < 1 || n > cur->prod_n) return 0;
  expect = cur->prod_cap[n];
  if (expect == VAL_ACC) return 0;

  if (!match_ident_raw()) return 0;
  i = sym_find(id_buf, SYM_VAR, 0);
  if (i == SYM_NONE) return 0; /* not defined: just fail this alt */
  slot = syms[i].address;
  if (slot != (expect & 0xff)) return 0;
  record_capture(MAKE_CAP(slot, sym_type(&syms[i])));
  return 1;
}

static int match_ident(void) {
  uchar c, slot;
  word start_abs, i;
  Symbol *s;

  start_abs = abs_pos();
  if (!match_ident_raw()) return 0;

  c = peek_char();

  // TODO: how about function pointers, lol
  i = sym_find(id_buf, (c == '(') ? SYM_FUNC : SYM_VAR, 0);

  if (i == SYM_NONE) {
    note_undef(c == '(', start_abs);
    vlog("  ident '%s' not in scope\n", id_buf);
    return 0;
  }

  s = &syms[i];
  if (s->type == SYM_FUNC) {

    // It's a function name?
    last_func_vec = s->address;
    slot = s->ret_slot;

  } else {

    // It's a variable name?
    last_func_vec = 0xff;
    slot = s->address;
  }
  record_capture(MAKE_CAP(slot, sym_type(s)));

  // kept in the frame: later \i matches (call arguments) must not change \v
  cur->func_vec = last_func_vec;

  return 1;
}

// \+ — declare ident into current scope; result = new slot
static int match_ident_decl(void) {
  uchar slot;
  word i;

  if (!match_ident_raw()) return 0;
  i = sym_find(id_buf, SYM_VAR, 1);
  if (i != SYM_NONE)
    slot = syms[i].address;
  else {
    slot = alloc_slot();
    set_decl_type(sym_add(id_buf, SYM_VAR, slot));
  }
  record_capture(MAKE_CAP(slot, cur_type));
  vlog("  decl '%s' scope %u slot %u type %c%s\n",
       id_buf, (unsigned)scope_depth, (unsigned)slot,
       (char)(CAP_TYPE(cur_type) >> 8), CAP_IS_PTR(cur_type) ? "*" : "");
  return 1;
}

// \f — declare function: new frame + vector index; mark entry at current bin_ptr later
static int match_ident_func_decl(void) {
  uchar base, vec;
  Symbol *s;

  if (!match_ident_raw()) return 0;
  if (sym_find(id_buf, SYM_FUNC, 1) != SYM_NONE) return 0; // declared twice
  if (nvector >= (uchar)(MAX_VECTORS - 1)) FAIL("too many functions");

  // frame: base+0 ret addr, +1 ret value (the function name), +2 prev fp,
  // +3 last param (JSR stores ACC there), +4.. other params, then locals
  base = next_slot;
  if (base >= CONST_TOP - 40) FAIL("out of slots");
  next_slot = base + 4;

  vec = nvector;
  s = sym_add(id_buf, SYM_FUNC, vec);
  s->ret_addr_slot = base;
  s->ret_slot = base + 1;

  // return value variable: only visible inside the body (dropped by \})
  s = sym_add(id_buf, SYM_VAR, base + 1);
  s->scope = scope_depth + 1;

  frame_base[vec] = base;
  nvector++;
  last_func_vec = vec;
  pending_func_entry = vec;
  if (strcmp(id_buf, "main") == 0)
    main_vec = vec;
  record_capture(base + 1);
  vlog("  func '%s' slot %u vec %u frame %u\n", id_buf, (unsigned)(base + 1), (unsigned)vec, (unsigned)base);
  return 1;
}

// \p — declare parameter n of the function just declared by \f
static int match_param_decl(void) {
  uchar k, slot, v;
  word i;
  Symbol *s;

  if (!match_ident_raw()) return 0;
  v = last_func_vec;
  if (v == 0xff) return 0;
  // parameter number = params already in this scope (rewinds on backtrack)
  k = 0;
  for (i = 0; i < nsym; i++)
    if (syms[i].scope == scope_depth && syms[i].pidx) k++;
  if (k >= MAX_PARAMS) FAIL("too many parameters");
  if (sym_find(id_buf, SYM_VAR, 1) != SYM_NONE) return 0;
  slot = frame_base[v] + 4 + k;
  s = sym_add(id_buf, SYM_VAR, slot);
  s->pidx = k + 1;
  set_decl_type(s);
  if (next_slot <= slot) next_slot = slot + 1;
  record_capture(MAKE_CAP(slot, cur_type));
  vlog("  param '%s' #%u slot %u type %c%s\n",
       id_buf, (unsigned)k, (unsigned)slot,
       (char)(CAP_TYPE(cur_type) >> 8), CAP_IS_PTR(cur_type) ? "*" : "");
  return 1;
}

// \r — end of parameter list (no input): the last param lives at fp+3
static int params_done(void) {
  uchar v, n;
  word i, last, f;
  Symbol *fs;

  v = last_func_vec;
  if (v == 0xff) return 0;
  f = sym_func(v);
  if (f == SYM_NONE) return 0;
  fs = &syms[f];        // nothing is added below: pointer stays valid

  n = 0;
  last = 0;
  for (i = 0; i < nsym; i++) {
    if (syms[i].scope == scope_depth && syms[i].pidx) {
      fs->param_slot[n] = syms[i].address;
      last = i;
      n++;
    }
  }
  if (n) {
    syms[last].address = frame_base[v] + 3;
    fs->param_slot[n - 1] = syms[last].address;
    next_slot--;        // its old cell was the top one: reuse it for locals
  }
  fs->nparams = n;
  vlog("  params done vec %u n=%u\n", (unsigned)v, (unsigned)n);
  return 1;
}



// ---------------------------------------------------------------------------
// String / char escapes — one decoder, shared by \S \s \c
// ---------------------------------------------------------------------------

// simple escapes: esc_in[i] decodes to esc_out[i]
static const char esc_in[]  = "ntrabfv";
static const char esc_out[] = "\n\t\r\a\b\f\v";

// After a leading '\\' has been consumed: decode one escape into *out.
// Advances line_pos. Returns 0 on failure (EOF / bad \x).
static int decode_escape(uchar *out) {
  uchar c, v, i;
  char *s;

  if (!line_need()) return 0;
  c = line_buf[line_pos++];
  if (c == '0') {
    *out = 0;
  } else if (c == 'x' || c == 'X') {
    v = 0;
    for (i = 0; i < 2; i++) {
      if (!line_need()) break;
      c = line_buf[line_pos];
      if (!isxdigit(c)) break;
      v = (v << 4) + hexval(c);
      line_pos++;
    }
    *out = v;
  } else {
    // \\ \" \' and unknown escapes → the char itself (like C)
    s = c ? strchr(esc_in, c) : NULL;
    *out = s ? esc_out[s - esc_in] : c;
  }
  return 1;
}

// Next body character of a "..." or '...' string (handles \\ escapes).
// Returns 0 on failure; 1 and *out on success. Does not consume the closer.
static int next_body_char(uchar *out) {
  uchar c;

  if (!line_need()) return 0;
  c = line_buf[line_pos++];
  if (c == '\\')
    return decode_escape(out);
  *out = c;
  return 1;
}

// Read body until closing delim (' or "). Opening delim already consumed.
// Calls put(c, ctx) for each decoded byte (not the closing delim).
// Consumes the closing delim. Returns 0 on failure.
static int read_quoted_body(uchar delim, void (*put)(uchar c, void *ctx), void *ctx) {
  uchar c;

  while (line_need() && line_buf[line_pos] != delim) {
    if (!next_body_char(&c)) return 0;
    put(c, ctx);
  }
  if (!line_need() || line_buf[line_pos] != delim) return 0;
  line_pos++;
  note_progress();
  return 1;
}

/* put helpers for the three call sites */

static void put_emit(uchar c, void *ctx) {
  (void)ctx;
  emit(c);
  capture_add(c);
}

typedef struct { uchar *buf; word n, max; } StrBuf;

static void put_buf(uchar c, void *ctx) {
  StrBuf *sb = ctx;
  if (sb->n + 1 < sb->max) {
    sb->buf[sb->n++] = c;
    capture_add(c);
  }
}

static int match_str_emit(void) {
  if (!line_need()) return 0;
  if (!read_quoted_body('"', put_emit, 0)) return 0;
  emit(0);
  return 1;
}

// \s — constant string → data pool + relocatable pointer const
static int match_str_const(void) {
  uchar buf[512];
  StrBuf sb;
  uchar slot;

  skip_ws();
  if (!line_need() || line_buf[line_pos] != '"') return 0;
  line_pos++;
  sb.buf = buf; sb.n = 0; sb.max = sizeof(buf);
  if (!read_quoted_body('"', put_buf, &sb)) return 0;
  if (sb.n + 1 >= sb.max) return 0;
  buf[sb.n++] = 0;
  slot = add_string_const(buf, sb.n);
  record_capture(MAKE_CAP(slot, TY_CHAR | TY_PTR));
  return 1;
}


static int match_num(void) {
  word v, ty;
  uchar ch;

  skip_ws();
  if (!line_need()) return 0;

  if (line_buf[line_pos] == '\'') {
    /* character literal 'x' / '\n' / ... → small integer const */
    line_pos++;
    if (!next_body_char(&ch)) return 0;
    if (!line_need() || line_buf[line_pos] != '\'') return 0;
    line_pos++;
    v = ch;
    ty = TY_CHAR;
  } else {
    if (!isdigit(line_buf[line_pos])) return 0;
    v = 0;
    while (line_need() && isdigit(line_buf[line_pos])) {
      v = v * 10 + (line_buf[line_pos] - '0');
      eat();
    }
    ty = TY_WORD;
  }
  note_progress();
  record_capture(MAKE_CAP(const_slot(v), ty));
  return 1;
}


// frees rule blobs + alt tables (rule[r] and blobs[r] are separate allocations)
static void free_rules(void) {
  uchar r;

  for (r = 0; r < NRULES; r++) {
    free(rule[r]);
    rule[r] = NULL;
    free(blobs[r]);
    blobs[r] = NULL;
  }
}

// escapes that stay "\c" in the blob (all others become the plain char c)
static int esc_kept(uchar e) {
  if (isdigit(e) || e == ' ' || e == '\\') return 1;
  return strchr("insSCcfhvge<d+{}[]#=aptrlxywmzkz", e) != NULL;
}

static void build_one_rule(uchar r, char *rhs) {
  char *p;
  uchar *blob, **altp, e, depth;
  word a, bi, na, alt_starts[64];

  // expand \| and \c (not specials) into blob; keep \i \n \s \C \< \d \+ \{ \}
  // build contiguous string with 0 separators between alts.
  // output is never longer than rhs (+ final 0), so no counting pass is needed
  blob = malloc(strlen(rhs) + 2);
  if (!blob) FAIL("out of memory (rule)");
  free(blobs[r]);
  free(rule[r]);
  blobs[r] = blob;

  bi = 0;
  na = 0;
  depth = 0;
  alt_starts[na++] = 0;
  for (p = rhs; *p; p++) {
    if (*p == '\\' && p[1]) {
      e = p[1];
      if (esc_kept(e))
        blob[bi++] = '\\';
      blob[bi++] = e;
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
    blob[bi++] = *p;
  }
  blob[bi++] = 0;

  altp = malloc(sizeof(uchar *) * (na + 1));
  if (!altp) FAIL("out of memory (rule)");
  for (a = 0; a < na; a++)
    altp[a] = blob + alt_starts[a];
  altp[na] = NULL;
  rule[r] = altp;
}

#define MAX_LINE 1024
#define MAX_ACCUM 4096

static void load_bnf(const char *path) {
  FILE *f;
  char *line = malloc(MAX_LINE);
  char *acc = malloc(MAX_ACCUM);
  uchar cr;
  char *nl, *eq, *p;
  word acc_len, have_cur;
  
  if (!line || !acc) FAIL("out of memory (grammar)");
  
  free_rules();

  fail_file = path;
  f = fopen(path, "r");
  if (!f) FAIL("cannot open grammar");
  fail_file = NULL;

  have_cur = 0;
  cr = 0;
  acc_len = 0;
  acc[0] = 0;

  // comment styles from #= lines; default Pascal { } if none given
  ncomments = 0;

  while (fgets(line, MAX_LINE, f)) {

    nl = strchr(line, '\n');
    if (nl) *nl = 0;

    if (line[0] == '.') break;
    if (line[0] == 0) continue;

    // #= comment-style directive (must be before bare # skip)
    if (line[0] == '#' && line[1] == '=') {
      comments_add_from_line(line + 2);
      continue;
    }
    if (line[0] == '#') continue;

    p = line;
    while (*p == ' ' || *p == '\t') p++;

    // continuation: line starts with |  (p stays, text is appended below)
    if (!(*p == '|' && have_cur)) {

      // new rule: flush previous
      if (have_cur)
        build_one_rule(cr, acc);
      have_cur = 0;

      if (!isupper(line[0])) continue;

      eq = strchr(line, '=');
      if (!eq) continue;

      cr = line[0] - 'A';
      have_cur = 1;
      p = eq + 1;
      acc_len = 0;
    }

    while (*p && acc_len + 1 < MAX_ACCUM)
      acc[acc_len++] = *p++;
    acc[acc_len] = 0;
  }

  if (have_cur)
    build_one_rule(cr, acc);

  if (ncomments == 0)
    comments_set_default();

  fclose(f);
  free(line);
  free(acc);
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

// snapshot the input/capture/code/scope state of the newest frame as the
// start of an alt (used by push and by tail-recursion \<)
static void frame_mark(void) {
  cur->alt = 0;
  cur->ip = 0;
  cur->save = line_pos;
  cur->save_abs = abs_pos();
  cur->cap_save = cap_len[sp - 1];
  cur->nsym_save = nsym;
  cur->scope_save = scope_depth;
  cur->slot_save = next_slot;
  cur->code_save = code_len;
  cur->prod_n = 0;
}

static void push(uchar r, uchar state) {
  if (sp >= MAX_STACK) FAIL("stack overflow");

  if (sp >= stack_cap) {
    Frame *ns;
    uchar ncap = stack_cap + 8;

    if (ncap > MAX_STACK) ncap = MAX_STACK;
    ns = realloc(stack, ncap * sizeof(Frame));
    if (!ns) FAIL("out of memory (stack)");
    stack = ns;
    stack_cap = ncap;
    // the block moved: rebase the cached pointer (rare)
    cur = sp ? stack + (sp - 1) : NULL;
  }

  // program main begin → blob.main
  if (r == 'B' - 'A' && sp > 0 && cur->rule == 'P' - 'A')
    main_offset = bin_ptr;

  cap_len[sp] = 0;
  cur = sp ? cur + 1 : stack;
  sp++;

  cur->rule = r;
  cur->func_vec = 0xff;
  cur->arg_i = 0;
  cur->npatch = 0;
  cur->mark = 0;
  cur->result = 0;
  memset(cur->prod_cap, 0, sizeof(cur->prod_cap));
  frame_mark();
  cur->state = state;
}

static void restore_scope(void) {
  sym_truncate(cur->nsym_save);
  scope_depth = cur->scope_save;
  next_slot = cur->slot_save;
}

// back to the start of the current alt: input, capture, code, productions
static void frame_rewind(void) {
  line_pos = cur->save;
  cap_len[sp - 1] = cur->cap_save;
  code_len = cur->code_save;
  cur->prod_n = 0;
  cur->npatch = 0;
}

// ---- one step ----
// estimated ~50-200us per step on 6502 @ 1MHz (dispatch + match)

static int step(void) {
  uchar *alt;
  uchar c, e, rch;
  int ok;
  word nalt, adv;

  if (sp == 0) return 0;

  if (cur->state == ST_TRY) {
    nalt = rule_nalt(cur->rule);
    if (cur->alt >= nalt) {
      vlog("fail %c (no alt) @%u\n", 'A' + cur->rule, abs_pos());
      sp--;
      cur--;
      if (sp == 0) return -1;
      // parent's current alt failed: ST_FAIL restores pos, capture,
      // code, scope, then moves to the next alt
      cur->state = ST_FAIL;
      return 1;
    }
    frame_rewind();
    cur->ip = 0;
    cur->state = ST_RUN;
    vlog("try %c alt %u @%u \"%s\"\n",
         'A' + cur->rule, cur->alt, abs_pos(),
         (char *)rule[cur->rule][cur->alt]);
    return 1;
  }

  if (cur->state == ST_OK) {
    rch = 'A' + cur->rule;

    // Type rules I and T set cur_type for subsequent \+/\p decls
    if (rch == 'I') {
      // alts: int=0, word=1, void=2 (word sized), char=3 (order in c.bnf)
      cur_type = (cur->alt == 3) ? TY_CHAR : TY_WORD;
    }
    if (rch == 'T') {
      // T=I *|I^*|I  → alt 0/1 pointer, alt 2 plain
      if (cur->alt < 2)
        cur_type |= TY_PTR;
    }
    vlog("ok %c alt %u -> @%u\n", rch, cur->alt, abs_pos());
    // -v: one GEN per statement (K), after alt commits
    if (verbose >= 1 && code_len > cur->code_save && rch == 'K') {
      fprintf(stderr, NORMAL "GEN:          [ ");
      ivm_print_code_range(stderr, code_buf, cur->code_save, code_len, 0);
      fprintf(stderr, "]\n");
      fflush(stderr);
    }
    if (verbose >= 1 && cap_len[sp - 1] > 0) {
      word i;
      // -v: SRC for K/B/P only; -vv: all rules + tags
      if (verbose >= 2 || rch == 'K' || rch == 'B' || rch == 'P') {
        if (verbose >= 2) {
          fprintf(stderr, "    %c", rch);
          if (rule_nalt(cur->rule) > 1) fprintf(stderr, "%u", cur->alt);
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

    capture_commit(sp - 1);
    {
      word child_res = cur->result;
      sp--;
      cur--;
      if (sp == 0) return 0;
      // parent records subrule result as next \n capture
      record_capture(child_res);
      cur->ip++;
      cur->state = ST_RUN;
    }
    return 1;
  }

  // current alt failed — next alt of same rule
  if (cur->state == ST_FAIL) {
    frame_rewind();
    restore_scope();
    cur->alt++;
    cur->ip = 0;
    cur->state = ST_TRY;
    return 1;
  }

  // ST_RUN
  alt = rule[cur->rule][cur->alt];
  if (!alt || alt[cur->ip] == 0) {
    cur->state = ST_OK;
    return 1;
  }

  c = alt[cur->ip];

  // space in BNF = required ws gap: skip comments/whitespace, no input char consumed as literal
  if (c == ' ') {
    skip_ws();
    cur->ip++;
    return 1;
  }

  // bare [ ... ] = production (use \[ \] in BNF to match bracket chars)
  if (c == '[') {
    cur->ip = run_production(alt + cur->ip + 1) - alt;
    return 1;
  }

  if (isupper(c)) {
    uchar r = c - 'A';

    if (!rule_defined(r)) FAIL("rule not defined");
    vlog("  call %c from %c @%u\n", c, 'A' + cur->rule, abs_pos());
    push(r, ST_TRY);
    return 1;
  }

  if (c == '\\') {
    e = alt[cur->ip + 1];
    ok = 1;
    adv = 2;      // bytes of the BNF alt used by this escape

    if (e == '<') {
      // tail-recurse: restart alts at current input, keep capture
      frame_mark();
      cur->state = ST_TRY;
      vlog("  tail %c @%u\n", 'A' + cur->rule, abs_pos());
      return 1;
    }

    if (e == 'C') {
      // commit: drop buffer prefix; keep matching
      line_commit();
      // fix save positions relative to new buffer
      for (c = 0; c < sp; c++)
        stack[c].save = 0;
    }
    else if (e == '{') scope_enter();
    else if (e == '}') scope_leave();

    // \#xx: result := hex xx, same as \d having matched that number (no input)
    else if (e == '#') {
      if (!alt[cur->ip + 2] || !alt[cur->ip + 3]) FAIL("bad \\# in grammar");
      record_capture(const_slot(hex2(alt + cur->ip + 2)));
      adv = 4;
    }

    // \=n: result := capture n (no new capture recorded)
    else if (e == '=') {
      c = alt[cur->ip + 2] - '0';
      if (c >= 1 && c <= cur->prod_n)
        cur->result = cur->prod_cap[c];
      adv = 3;
    }

    else if (isdigit(e)) ok = match_ident_same(e - '0');
    else if (e == 'i') ok = match_ident();
    else if (e == '+') ok = match_ident_decl();
    else if (e == 'f') ok = match_ident_func_decl();
    else if (e == 'p') ok = match_param_decl();
    else if (e == 'r') ok = params_done();
    else if (e == 'd') ok = match_num();
    else if (e == 'n') ok = match_newline();
    else if (e == 's') ok = match_str_const();
    else if (e == 'S') ok = match_str_emit();
    else if (e == '\\') ok = match_lit('\\');
    else if (e == 'z') {
      // sizeof value from cur_type (after matching T)
      record_capture(MAKE_CAP(const_slot(
        (!CAP_IS_PTR(cur_type) && CAP_TYPE(cur_type) == TY_CHAR) ? 1 : 2), TY_WORD));
    }
    else
      ok = match_lit(e); /* match_lit skips ws unless want is space/tab */

    if (!ok) {
      vlog("  fail \\%c @%u\n", e, abs_pos());
      cur->state = ST_FAIL;
      return 1;
    }
    vlog("  ok \\%c @%u\n", e, abs_pos());
    cur->ip += adv;
    return 1;
  }

  skip_ws();
  if (!match_lit(c)) {
    vlog("  fail lit '%c' @%u got '%c'\n", c, abs_pos(),
         line_need() ? line_buf[line_pos] : '?');
    cur->state = ST_FAIL;
    return 1;
  }
  vlog("  ok lit '%c' @%u\n", c, abs_pos());
  cur->ip++;
  return 1;
}

// run up to n steps
// ~50-200 us/step on 6502 @ 1MHz → n=1000 ≈ 50-200 ms
static int run(word n) {
  word i;
  int rc = 1;

  for (i = 0; i < n && rc == 1; i++)
    rc = step();

  // quit so run_ivm can stop
  if (!rc) emit('q');
  
  return rc;
}

// "file:line: " prefix of an error line
static void err_prefix(word line) {
  fprintf(stderr, "%s:%u: ", src_name ? src_name : "?", line);
}

// print buffered source line starting at i; returns index after its '\n'
static word show_line(word line, word i) {
  err_prefix(line);
  while (i < line_len && line_buf[i] != '\n')
    fputc(line_buf[i++], stderr);
  fputc('\n', stderr);
  if (i < line_len) i++;
  return i;
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
    uchar r = far_stack[far_sp - 1] - 'A';
    uchar a = far_alt[far_sp - 1];

    fprintf(stderr, "EXPECTED: rule %c", far_stack[far_sp - 1]);
    if (rule_defined(r) && a < rule_nalt(r))
      fprintf(stderr, " alt %u \"%s\"", a, (char *)rule[r][a]);

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
    word want = (had_undef && undef_pos_abs) ? undef_pos_abs : max_pos_abs;
    word start, off, col, line, endpos;
    word nlines;

    if (!line_buf || line_len == 0) {
      fprintf(stderr, "  (no buffered source)\n");
    } else if (want < line_abs) {

      // furthest was discarded by \C; still report position
      fprintf(stderr, "%s: (offset %u, before current buffer @%u)\n",
        src_name ? src_name : "?",
        want, line_abs);

    } else {

      off = want - line_abs;
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
      endpos = show_line(line, start);

      // caret under column
      err_prefix(line);
      for (i = 0; i < col; i++)
        fputc(' ', stderr);
      fprintf(stderr, "^\n");

      // following lines still in buffer (no second open of the file)
      for (nlines = 0; nlines < ERR_LINES_CONTEXT && endpos < line_len; nlines++)
        endpos = show_line(line + 1 + nlines, endpos);
    }
  }

}


// init: load grammar + open source (errors: FAIL)
static void init(char *bnf_path, char *src_path) {

  src_name = src_path;
  load_bnf(bnf_path);

  if (src_fp) fclose(src_fp);
  fail_file = src_path;
  src_fp = fopen(src_path, "r");
  if (!src_fp) FAIL("cannot open source");
  fail_file = NULL;

  line_buf = NULL;
  line_no_base = 0;
  line_cap = 0;
  line_len = 0;
  line_pos = 0;
  line_abs = 0;
  file_pos = 0;
  max_pos_abs = 0;
  far_sp = 0;
  stack = NULL;
  stack_cap = 0;
  sp = 0;
  cur = NULL;
  sym_reset();
  had_undef = 0;
  had_undef_func = 0;
  undef_pos_abs = 0;
  last_undef[0] = 0;
  next_slot = 0;
  gen_reset();
  nvector = 0;
  last_func_vec = 0xff;
  // no pre-seeded names — X must be declared in source if used
  // (capture[] / cap_len[] / cap_max[] are zero at start, shutdown() re-zeroes them)

  if (!rule_defined('P' - 'A')) FAIL("missing start rule P");
  push('P' - 'A', ST_TRY);
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
    cap_len[i] = 0;
    cap_max[i] = 0;
  }

  free_rules();

  free(stack);
  stack = NULL;
  stack_cap = 0;
  sym_free();
}

int main(int argc, char **argv) {
  char *bnf = NULL;
  char *srcpath = NULL;
  word i, a;
  int rc;
  int err;

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

  // FAIL
  // - longjmp value is error message pointer (or 1 on host: see fail_msg)
  err = setjmp(fail_jmp);
  if (err) {

    #ifdef __CC65__
      fail_msg = (char *)err;
    #endif

    fprintf(stderr, "\n%%ERROR: %s\n", fail_msg);
    if (fail_file) fprintf(stderr, "  file: %s\n", fail_file);
    report_reject();
    shutdown();
    return 1;
  }

  // setup
  init(bnf, srcpath);

  // print rules
  if (verbose >= 2) {

    fprintf(stderr, "--- rules ---\n");
    for (i = 0; i < NRULES; i++)

      if (rule_defined(i)) {
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
  if (rc) FAIL("syntax error");

  // OK
  skip_ws();
  if (line_need()) FAIL("trailing input");

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

  {
    int blen = 0;
    // frame_base[] (set by \f) goes into the blob's fp table
    uchar *blob = build_blob(&blen);

    fprintf(stderr, "--- BLOB (%d bytes) main=%u nvector=%d ---\n",
      blen, main_offset, nvector);

    // RUN
    if (blob) {

      // SAVE

      // TODO: save it!

#ifdef RUN
      {
        Process *proc;
        ivmvar ret;

        if (verbose >= 1)
          for (i = 0; i < nvector; i++)
            fprintf(stderr, "  frame V%02x fp=%u\n", i, frame_base[i]);

        proc = Nevm(blob);
        if (proc) {
          word *pool = (word *)(blob + ((BlobHeader *)blob)->con);

#ifndef __CC65__
          {
            extern word soft_top;
            soft_top = 0;
          }
#endif
          for (i = 0; i < nconst; i++) {
            word cv = pool[i];
#ifndef __CC65__
            if (const_is_reloc[i]) {
              /* host: copy string into soft_heap, store offset */
              extern uchar soft_heap[];
              extern word soft_top;
              const uchar *src = blob + cv;
              word n = 0;
              while (src[n]) n++;
              n++;
              if (soft_top + n <= 4096) {
                memcpy(soft_heap + soft_top, src, n);
                cv = soft_top;
                soft_top = (word)(soft_top + n);
              } else
                cv = 0;
            }
#endif
            proc->mem[CONST_TOP - i] = cv;
          }

          fprintf(stderr, "--- RUN ---\n");
          ret = run_ivm(proc);
          fprintf(stderr, "--- END acc=%u ---\n", ret);
          free(proc);
        }
      }
#endif // RUN

      free(blob);
    }
  }

  shutdown();
  return 0;
}
