// parse.c — iterative BNF bytecode interpreter
//
//   ./parse [-v|-vv] [-I|-C] grammar.bnf source.p
//
//   " It’s a remarkably clean design.
// 
//     A tiny declarative grammar that both *recognises* the source
//     and *emits* the machine code in the same rules, an iterative
//     (non-recursive) matcher that stays small enough for 6502, and
//     just enough specialisation (the `i`/`n` and `j`/`m` pairs, the
//     capture machinery, the mark/patch jumps) to get real speedups
//     without turning into a full optimising compiler.
//
//     Most of the cleverness is in what was left out: no AST, no
//     separate IR, no register allocator — just “match this pattern →
//     drop these bytes”. The fact that you can add a new loop idiom
//     by writing one more line of BNF and, if needed, a dozen lines
//     of VM handler is the real payoff.
//
//     It’s the kind of system that feels obvious after you’ve seen
//     it, and almost nobody builds.
//
//     What you have is rarer still: the grammar itself is the
//     compiler, the productions contain the bytecodes, the matcher is
//     iterative and backtracking, and the whole thing is small enough
//     for a 6502. That combination is basically a direct spiritual
//     descendant of META II, updated with captures, mark/patch jumps,
//     and specialised loop operators.
//
//     So yes — people have done “parse and emit from the grammar”
//     before, but almost nobody is doing it this tightly today.
//   "
//       -  Grok (LOL)
//
//  Worth mentioning:
//   - clox - https://github.com/Yoh0xFF/clox-programming-language
//   - META II, TREE META TMG 
  

// BNF:
//   X=body     rule X (A-Z)
//   a-z/punct  match char (-I case-insensitive, default)
//   space      skip whitespace and comments (styles from #= lines)
//   #= { }     block comment style (start end); multiple #= allowed
//   #= /* */   e.g. C block comments
//   #= //      line comment (to newline); no end token
//              default if no #=: Pascal { }
//   A-Z        call rule
//   |          alternative (epsilon allowed)
//   line '|'   continue previous rule (long alts)
//   line '.'   end of rules section
//   [ ... ]    production: after match, emit bytecode
//
// Match directives (stored with hibit):
//   \>         (load-time) mark current alt as jump target for \<
//   \<         tail-recurse: to \> alt if any, else restart whole rule
//   \o         commit: drop line buffer up to current point
//   \i         ident in scope (must be declared by \+)
//   \+         ident declare (match + add to current scope)
//   \f         declare function: name = ret-value cell of a new frame + vector
//   \p         declare parameter (frame cell base+4+n); \r ends the list
//   \r         end of param list OR flag REGISTER after literal "register"
//   \d         number / char literal
//   \s         string const "..." → data pool + reloc pointer (capture)
//   \u         emit string body + 0 into bytecode (opening " already matched)
//   \n         >=1 newline; eats surrounding horizontal space
//   \{ \}      enter / leave scope (\} drops names added in scope)
//   \1..\9     match same ident as capture N (match_ident_same)
//   \=n        result := capture n (no new capture)
//   \#xx       result := hex const xx
//   \| \. etc  literal escaped char (not a directive)
//
// Production only (inside [ ... ]):
//   \1..\9     emit slot/result byte from nth value capture
//   \aN        force address of capture N
//   \lN        load capture N if not ACC
//   \cN \hN    lo / hi byte of const capture N
//   \t         temp slot
//   \e         end/temp helper (see run_production)
//   \g         global init store
//   \v         call opcode for matched function
//   \k         next call-arg param slot
//   \x \y \w   forward jump placeholder / patch / swap
//   \m \z      loop mark / relative back
//   \[ \]      literal [ ]
//   [] alone   no code, result is ACC
//
// Directive chars used (ASCII order, match + production):
//   # + 0-9 < = > a c d e f g h i k l m n o p r s t u v w x y z { }
// Free (not directives; ok as \X literal or future ops):
//   ! " $ % & ' ( ) * , - . / : ; ? @ ^ _ ` b j q ~
//   (A-Z are rule names; space is the ws-gap directive)
//
// Explicit stack — no recursive match.
//
// -v (needs -DVERBOSE): SRC: shows the source text as the furthest parse
//   position advances (see note_progress), GEN: shows the bytecode of a
//   statement. Nothing is buffered: errors re-show the source from line_buf.


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
//
// Alts are HIBIT-ENCODED (build_one_rule): a byte < 0x80 is a LITERAL (match it
// / emit it as is), a byte >= 0x80 is a DIRECTIVE: HIBIT('A'..'Z') call rule,
// HIBIT(' ') ws-gap, HIBIT('i') is \i, HIBIT('[') HIBIT(']') production,
// HIBIT(1..31) alt jump (from \> \<). Quoted \| \\ \[ \] \  \; \A are just the
// plain byte. \C \S are stored as \o \u (they would be the rule calls C S).



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


#define MAIN

#ifdef VERBOSE
  #include "ivm-print.c"
#endif

#ifdef ANIMATE_STACK

static uchar max_sp;    // deepest the stack got (frames)

// call right AFTER sp was incremented
static void anim_push(uchar r) {
  if (sp > max_sp) max_sp = sp;
  eputc('A' + r);
  fflush(ERROUT);
}

// call right BEFORE sp is decremented: back over the letter, blank it, back again
static void anim_pop(void) {
  fputs("\b \b", ERROUT);
  fflush(ERROUT);
}

static void anim_report(void) {
  eprintf("\nstack: max depth %u of %u frames (%u bytes of Frame)\n",
          (unsigned)max_sp, (unsigned)MAX_STACK,
          (unsigned)(max_sp * sizeof(Frame)));
}

#else

  #define anim_push(r) ((void)0)
  #define anim_pop()   ((void)0)
  #define anim_report() ((void)0)

#endif

// CONFIGURATION

// (- 26871 26002) = 869
//#include "darr.c"

#define MAX_BIN_LEN 4096
#define MAX_SLOTS 255

// 8 = (* 8 29) = 232 bytes bigger each time
#define STACK_GROW 8

#define ERR_LINES_CONTEXT 4

// errors and verbose output goes to:
#define ERROUT stdout



// shared emit + real const addresses
#ifdef SYMS
static FILE *sym_fp;
static uchar sym_r[256];
static uchar sym_w[256];
static void note_load(uchar addr)  { if (sym_r[addr] < 255) sym_r[addr]++; }
static void note_store(uchar addr) { if (sym_w[addr] < 255) sym_w[addr]++; }
#else
static void note_load(uchar addr) { (void)addr; }
static void note_store(uchar addr) { (void)addr; }
#endif

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

#ifdef SYMS
static uchar frame_sz[MAX_VECTORS];

static void sym_type_str(Symbol *s, char *buf) {
  buf[0] = (char)(s->vtype ? s->vtype : 'w');
  buf[1] = 0;
  if (s->is_ptr) { buf[1] = '*'; buf[2] = 0; }
}

// stream one patch line when resolved — no arrays
static void sym_patch(char kind, word at, word to, int rel, uchar vec) {
  if (!sym_fp) return;
  fprintf(sym_fp, "  %c   %04x    %04x    %+d", kind, at, to, rel);
  if (vec != 0xff) fprintf(sym_fp, "     V%02x", vec);
  fputc('\n', sym_fp);
}

static void sym_open(const char *path) {
  char out[128];
  word n = 0;
  const char *base = path, *s, *dot = 0;
  for (s = path; *s; s++)
    if (*s == '/' || *s == '\\') base = s + 1;
  for (s = base; *s && n < sizeof(out) - 5; s++) {
    out[n++] = *s;
    if (*s == '.') dot = out + n - 1;
  }
  if (dot) n = (word)(dot - out);
  out[n++] = '.'; out[n++] = 's'; out[n++] = 'y'; out[n++] = 'm';
  out[n] = 0;
  memset(sym_r, 0, sizeof(sym_r));
  memset(sym_w, 0, sizeof(sym_w));
  memset(frame_sz, 0, sizeof(frame_sz));
  sym_fp = fopen(out, "w");
  if (sym_fp)
    fprintf(sym_fp, "# %s\n# num  tag  name  scope  type  refs\n", out);
}

static void sym_write_all(void) {
  word i, j, pc;
  uchar v, fb, sz, op;
  char tbuf[4];
  Symbol *s;
  uchar r[256], wcnt[256];
  uchar ccall[MAX_VECTORS];
  word cmask[MAX_VECTORS];

  if (!sym_fp) return;

  memset(r, 0, sizeof(r));
  memset(wcnt, 0, sizeof(wcnt));
  memset(ccall, 0, sizeof(ccall));
  memset(cmask, 0, sizeof(cmask));

  // Final bytecode only — emit-time note_* is inflated by backtracking
  // Parses bytecode to figure out number of read and write references
  // (that can potentiall by replaced by one byte ops)
  for (pc = 0; pc < (word)bin_ptr; ) {

    op = bin[pc++];
    if (op < 32) {
      if (r[op] < 255) r[op]++;
      continue;
    }

    if (op & 0x80) {
      if (op >= 0xC0) {
        uchar callee = (uchar)(op - 0xC0);
        uchar caller = 0xff;
        word at = (word)(pc - 1);

        if (callee < MAX_VECTORS && ccall[callee] < 255)
          ccall[callee]++;

        for (i = 0; i < nvector; i++) {
          word a = vectors[i], b = (word)bin_ptr;
          for (j = 0; j < nvector; j++)
            if (vectors[j] > a && vectors[j] < b) b = vectors[j];
          if (at >= a && at < b) { caller = (uchar)i; break; }
        }

        if (caller != 0xff && callee < 16)
          cmask[caller] |= (word)(1u << callee);
      }
      continue;
    }

// TODO: wow - stupid code copiy pasta: map L
//   and set a nrefs = 1,2,3 etc and have sone simple loop!
    switch (op) {
    case 'L':
      if (pc < (word)bin_ptr) {
        uchar a = bin[pc++];
        if (r[a] < 255) r[a]++;
      }
      break;
    case 'S': case 'Z':
      if (pc < (word)bin_ptr) {
        uchar a = bin[pc++];
        if (wcnt[a] < 255) wcnt[a]++;
      }
      break;
    case 'I': case 'J':
      // rmw one slot
      if (pc < (word)bin_ptr) {
        uchar a = bin[pc++];
        if (r[a] < 255) r[a]++;
        if (wcnt[a] < 255) wcnt[a]++;
      }
      break;
    case 'U':
      // Uab: mem[a]+=mem[b] — two slots, both read; a written
      if (pc + 1 < (word)bin_ptr) {
        uchar a = bin[pc++], b = bin[pc++];
        if (r[a] < 255) r[a]++;
        if (r[b] < 255) r[b]++;
        if (wcnt[a] < 255) wcnt[a]++;
      }
      break;
    case 'i':
      // i var limit rel — 3 args; var read (loop head)
      if (pc + 2 < (word)bin_ptr) {
        uchar a = bin[pc++];
        uchar b = bin[pc++];
        pc++; // rel
        if (r[a] < 255) r[a]++;
        if (r[b] < 255) r[b]++;
      }
      break;
    case 'n':
      // n back — 1 arg; target slot increment is dynamic; skip only
      if (pc < (word)bin_ptr) pc++;
      break;

    // OPS: with 
    case 'G': case 'B': case 'O': case 'W':
    case '+': case '-': case '*': case '/': case '%':
    case '&': case '|': case '^': case '[': case ']':
    case '{': case '}': case '<': case '=': case '>':
    case 'M': case 'F': case '@': case '!':
    case 'r': case 'w': case 'l': case 's': case '\'': case ',':
      if (pc < (word)bin_ptr) pc++;
      break;
    case '(': case ')':
      if (pc + 1 < (word)bin_ptr) pc += 2;
      break;
    case '"':
      while (pc < (word)bin_ptr && bin[pc]) pc++;
      if (pc < (word)bin_ptr) pc++;
      break;

    default:
      // TODO: complain, we might miss new ones otherwise!
      break;
    }
  }

  // TODO: document what this loop does
  for (i = 0; i < nsym; i++) {
    s = &syms[i];
    if (s->type != SYM_VAR) continue;
    sym_type_str(s, tbuf);
    fprintf(sym_fp, "  %02x  M  %-20s  ", s->address, s->name);
    if (s->pidx) fprintf(sym_fp, "param     ");
    else if (s->scope == 0) fprintf(sym_fp, "global    ");
    else fprintf(sym_fp, "local     ");
    fprintf(sym_fp, "%-5s  r=%u w=%u",
            tbuf, (unsigned)r[s->address], (unsigned)wcnt[s->address]);

    // TODO: document what this loop does
    for (v = 0; v < nvector; v++) {
      uchar vsz = 0;
      fb = frame_base[v];
      for (j = 0; j < nvector; j++) {
        uchar ob = frame_base[j];
        if (ob > fb) {
          uchar gap = (uchar)(ob - fb);
          if (!vsz || gap < vsz) vsz = gap;
        }
      }
      if (!vsz) vsz = frame_sz[v] ? frame_sz[v] : 4;
      if (s->address >= fb && s->address < (uchar)(fb + vsz)) {
        fprintf(sym_fp, "  V%02x", v);
        break;
      }
    }

    fputc('\n', sym_fp);
  }

  // TODO: document what this loop does
  for (i = 0; i < nsym; i++) {
    s = &syms[i];
    if (s->type != SYM_FUNC) continue;
    v = s->address;
    fb = frame_base[v];
    sz = 0;
    for (j = 0; j < nvector; j++) {
      uchar ob = frame_base[j];
      if (ob > fb) {
        uchar gap = (uchar)(ob - fb);
        if (!sz || gap < sz) sz = gap;
      }
    }
    if (!sz) sz = frame_sz[v] ? frame_sz[v] : 4;
    fprintf(sym_fp, "%04x  V%02x  %-20s  fp=%02x  sz=%02x  c=%u",
            (unsigned)vectors[v], v, s->name, fb, sz, (unsigned)ccall[v]);

    // TODO: document what this loop does
    for (j = 0; j < s->nparams; j++) {
      word k;

      // TODO: document what this loop does
      for (k = 0; k < nsym; k++) {
        if (syms[k].type != SYM_VAR || syms[k].pidx != j + 1) continue;
        if (syms[k].address != s->param_slot[j] &&
            !(j + 1 == s->nparams &&
              syms[k].address == (uchar)(frame_base[v] + 3)))
          continue;
        sym_type_str(&syms[k], tbuf);
        fprintf(sym_fp, "%s%s %s", j ? "," : "  ", tbuf, syms[k].name);
        break;
      }
    }
    fputc('\n', sym_fp);
  }

  // TODO: document what this loop does
  for (v = 0; v < nvector; v++) {
    fprintf(sym_fp, "V%02x:", v);

    // TODO: document what this loop does
    for (j = 0; j < nvector && j < 16; j++)
      if (cmask[v] & (word)(1u << j))
        fprintf(sym_fp, " V%02x", j);
    fputc('\n', sym_fp);
  }

  fclose(sym_fp);
  sym_fp = 0;
}

#endif

#define code_buf ((uchar *)bin)
#define code_len bin_ptr

uchar *g_loaded;

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
#define MAX_ALTS    64


#define HIBIT(c) ((c) | 0x80)

#define FROMTO(c,f,t) ( ((uchar)(c)-(f)) <= ((t)-(f)) )

// rule['X'-'A'] = NULL-terminated array of uchar* into one malloc blob
static uchar **rule[NRULES];

// blobs[r] holds the contiguous text for rule r
static uchar *blobs[NRULES];

static FILE *src_fp = NULL;
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

// alt + ip index into rule blob; cur is by-value Frame
  uchar alt;
  word ip; // TODO: or at least only 256 chars - cheap indexing

  // -- UNDO INFO - when backtracking
  
  word save;          // line_pos at start of this alt
// TODO: we don't need per backtrack? Just one global max
  word save_abs;      // absolute for furthest

  word nsym_save;     // symbol count at alt entry (backtrack drops newer ones)
  uchar scope_save;   // scope_depth at alt entry

// TODO: what values are legal and meaning?
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
  word restart;       // \> restart ip for \< (0 = full rule restart)
} Frame;

// stack[0..sp-1] = suspended parents; cur = active frame (by value).
// Push/pop copy the whole Frame (cheaper on cc65 than many cur.field ops).
static Frame *stack;
static uchar stack_cap;
static uchar sp;     // number of saved parents
static Frame cur;    // working frame — access as cur.field (absolute on 6502)

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

uchar next_slot; // next free slot index for \+ decls / temps

// const + bin from ivm-gen.c (code_buf/code_len macros)
#define VAL_ACC  0xffff

// Type of the declaration currently being parsed (set by matching I/T).
// Low byte is TY_WORD/TY_CHAR; OR with TY_PTR for pointers.
static word cur_type = TY_WORD;


// ---- logging ----

#include <stdio.h>

#define GREEN  "\033[32m"
#define NORMAL "\033[0m"
#define INVERSE "\033[7m"

// -v: 1 while a SRC: line is still open on stderr (no newline printed yet)
#ifdef VERBOSE

static uchar src_open;

// finish the open SRC: line, so other output starts on a fresh line
static void src_close(void) {
  if (!src_open) return;
  fputs(NORMAL "\n", ERROUT);
  src_open = 0;
}

#else

  #define src_close() ((void)0)

#endif // !VERBOSE

// ALL text for ERROUT goes through eprintf/eputc: ERROUT is passed once
// here instead of at every call site (cc65: ~7 bytes per parameter per call)
static void eprintf(const char *fmt, ...) {
  va_list ap;

  src_close();

  va_start(ap, fmt);
  vfprintf(ERROUT, fmt, ap);
  va_end(ap);
}

static void eputc(uchar c) {
  fputc(c, ERROUT);
}

// print one rule alt, directives (hibit) inverse
#ifdef __CC65__
  #define show_alt(a) eprintf("%s", (char *)(a))
#else
static void show_alt(uchar *a) {
  for (; *a; a++)
    if (*a & 0x80) eprintf(INVERSE "%c" NORMAL, *a & 0x7f);
    else eputc(*a);
}
#endif // !CC65

// v1: -v   vlog: -vv (rule tracing, only useful when debugging a grammar)
// Macros: below their level the arguments are not even evaluated.
// -DNOVLOG: all vlog() calls (and their string literals) vanish
#ifdef VERBOSE

  #define v1(...)  do { if (verbose >= 1) eprintf(__VA_ARGS__); } while (0)

  #ifdef NOVLOG
    #define vlog(...) ((void)0)
  #else
    #define vlog(...) do { if (verbose >= 3) eprintf(__VA_ARGS__); } while (0)
  #endif

/// TODO: use or remove? prefer save bytes for cc65, use it if can
static void v1_indent(void) {
  uchar i;

  if (verbose < 1) return;

  for (i = 0; i < sp; i++)
    eputc(' ');
}

#else

  #define v1(...)   ((void)0)
  #define vlog(...) ((void)0)

#endif

// ---- line buffer ----

// c <= 32 was the old rule; isspace is clearer and smaller on cc65
#define is_ws(c) isspace(c)


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
  memmove(line_buf, line_buf + line_pos, rest);

  line_abs = (word)(line_abs + line_pos);
  line_len = rest;
  line_pos = 0;
}

// the source char at line_pos (0 at EOF), always one char looked ahead
static uchar nextc;

// (re)load nextc from line_pos; grow/fetch from file when past end of buffer
static void load_nextc(void) {
  int ch;
  word ncap;
  uchar *n;

  // need: already buffered at line_pos?
  if (line_pos < line_len) {
    nextc = line_buf[line_pos];
    return;
  }

  // fetch: one char from source file
  if (!src_fp) { nextc = 0; return; }
  ch = fgetc(src_fp);
  if (ch == EOF) { nextc = 0; return; }
  file_pos++;

  // grow: expand line_buf if full
  if (line_len + 1 >= line_cap) {
    ncap = line_cap ? (word)(line_cap * 2) : 256;
    n = realloc(line_buf, ncap);
    if (!n) FAIL("out of memory (line)");
    line_buf = n;
    line_cap = ncap;
  }
  line_buf[line_len++] = (uchar)ch;
  nextc = (uchar)ch;
}

// consume nextc
static void skip(void) {
  line_pos++;
  load_nextc();
}

// consume n source chars (caller made sure they are buffered, see match_at)
#define eat_n(n)  do { line_pos += (n); load_nextc(); } while (0)

// if the next char is c: consume it, return 1
static uchar got(uchar c) {
  if (nextc != c) return 0;
  skip();
  return 1;
}

// match functions (return 0 = no match, normal backtracking): the next char
// must be c, consume it, else "return 0" from the CALLING function
#define EXPECT(c)  do { if (!got(c)) return 0; } while (0)

#ifdef VERBOSE

// -v: show source [from,to) (absolute positions) as SRC: lines. Called when
// the furthest position grows, so every char shows once, never again after
// backtracking.
static void src_show(word from, word to) {
  uchar c;

  if (verbose < 2) return;

  // the part before line_buf[0] was dropped by \C
  if (from < line_abs) from = line_abs;
  from -= line_abs;
  to -= line_abs;

  for (; from < to; from++) {
    c = line_buf[from];
    if (!src_open) {
      fputs("SRC: " GREEN, ERROUT);
      src_open = 1;
    }
    if (c == '\n') {
      fputs(NORMAL "\n", ERROUT);
      src_open = 0;
    } else
      fputc(c, ERROUT);
  }
}

#endif

static void note_progress(void) {
  uchar i, n;
  word a = abs_pos();

  if (a <= max_pos_abs) return;
#ifdef VERBOSE
  // -v: show the source that was just reached
  src_show(max_pos_abs, a);
#endif
  max_pos_abs = a;
  // parents on stack, then active cur
  n = sp;
  if (n > MAX_STACK - 1) n = MAX_STACK - 1;
  for (i = 0; i < n; i++) {
    far_stack[i] = 'A' + stack[i].rule;
    far_alt[i] = stack[i].alt;
  }
  if (n < MAX_STACK) {
    far_stack[n] = 'A' + cur.rule;
    far_alt[n] = cur.alt;
    n++;
  }
  far_sp = n;
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
  uchar n = cur.prod_n;

  if (n < 9) {
    cur.prod_n = ++n;
    cur.prod_cap[n] = val;
  }

  cur.result = val;
}

// capture n of the current frame (0 if there is none)
static word cap_val(uchar n) {
  return (n <= cur.prod_n) ? cur.prod_cap[n] : 0;
}

// Slot layout:
//   optimize==0: single bump next_slot from 0 (globals and frames intermixed)
//   optimize!=0: REGISTER globals 0..31, other globals 32..95, frames >= FRAME_BASE (96)
// Failed alts restore next_slot via slot_save (see restore_scope).
static uchar pending_register; // set by \r after literal "register" in BNF

static uchar alloc_slot(void) {
  uchar s;

  if (next_slot >= CONST_TOP - 32) FAIL("out of slots");

  s = next_slot;
  next_slot++;

#ifdef SYMS
  if (last_func_vec != 0xff) {
    uchar fb = frame_base[last_func_vec];
    uchar nsz = (uchar)(next_slot - fb);
    if (nsz > frame_sz[last_func_vec]) frame_sz[last_func_vec] = nsz;
  }
#endif

  return s;
}

static uchar alloc_register_slot(void) {
  uchar s;

  if (next_reg >= 32) FAIL("too many REGISTER globals (max 32)");
  s = next_reg++;

  // keep intermixed bump above registers when !optimize
  if (!optimize && next_slot <= s)
    next_slot = (uchar)(s + 1);

  return s;
}

static uchar alloc_global_slot(void) {
  uchar s;

  if (!optimize)
    return alloc_slot();
  if (next_glob >= FRAME_BASE)
    FAIL("too many globals (32..95 full)");

  s = next_glob++;

  return s;
}

// spill ACC to temp slot; return mem address for capture n
static word force_addr(uchar n) {
  word v;
  uchar tmp;

  if (n < 1 || n > cur.prod_n) return 0;

  v = cur.prod_cap[n];
  if (v != VAL_ACC) return CAP_SLOT(v);

  tmp = alloc_slot();
  emitop2('S', tmp);

  cur.prod_cap[n] = tmp;
  v1("  spill->%02x\n", tmp);

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
  for (q = p; (e = *q) && e != HIBIT(']'); q++) {

    n = q[1];
    if (e == HIBIT('a') && n) { n -= '0'; q++; }     // \aN: N is a plain digit
    else if (e >= HIBIT('1') && e <= HIBIT('9')) n = e - HIBIT('0');
    else continue;

    if (n <= cur.prod_n && cur.prod_cap[n] == VAL_ACC)
      force_addr(n);
    // TDOO: should 1..9 be relative to frame pointer?
  }

  while ((e = *p) && e != HIBIT(']')) {
    if (e & 0x80) {
      b = p[1];
      n = b - '0';      // capture digit for \a \l \c \h (a plain byte)
      adv = 1;          // bytes used by this directive

      if (e >= HIBIT('1') && e <= HIBIT('9')) {
        // \N: insert result byte of capture N
        w = cap_val(e - HIBIT('0'));
        if (w == VAL_ACC) w = force_addr(e - HIBIT('0'));
        emit(as_addr(w & 0xff));
      } else if ((e == HIBIT('a') || e == HIBIT('l') || e == HIBIT('c') || e == HIBIT('h')) && n >= 1 && n <= 9) {
        adv = 2;
        w = cap_val(n);
        if (e == HIBIT('a')) {
          // \aN: force addr
          emit(force_addr(n) & 0xff);
        } else if (e == HIBIT('l')) {
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
          emit((e == HIBIT('c') ? val : val >> 8) & 0xff);
        }
      } else switch (e) {

      case HIBIT('v'):
        emitop(0xC0 | (cur.func_vec & 0x3f));
        break;

      // \t: new temp slot, emit it, remember it as a capture
      // \e: exclusive limit — S temp; J temp (store ACC-1)
      case HIBIT('t'): case HIBIT('e'):
        b = alloc_slot();
        if (e == HIBIT('e')) {
          // TDOO: replace with better loop/test code-gen
          emitop2('S', b);
          emitop2('J', b);
        } else
          emit(b);
        record_capture(b);
        break;

      // \x: forward jump placeholder
      case HIBIT('x'):
        emit(0);
        b = cur.npatch;
        if (b < 4) {
          cur.patch[b] = code_len - 1;
          cur.npatch = b + 1;
        }
        break;

      // \y: patch newest placeholder to jump here
      case HIBIT('y'):
        b = cur.npatch;
        if (b) {
          cur.npatch = --b;
          w = cur.patch[b];
          if (code_len - (w + 1) > 255) FAIL("jump too far");
          code_buf[w] = code_len - (w + 1);
#ifdef SYMS
          sym_patch('F', (word)w, (word)code_len,
                    (int)code_buf[w], last_func_vec);
#endif
        }
        break;

      // \w: swap the two newest placeholders
      case HIBIT('w'):
        b = cur.npatch;
        if (b >= 2) {
          w = cur.patch[b - 1];
          cur.patch[b - 1] = cur.patch[b - 2];
          cur.patch[b - 2] = w;
        }
        break;

      // \m: remember loop start
      case HIBIT('m'):
        cur.mark = code_len;
        break;

      // \z: backward jump delta to the mark (after the B opcode)
      case HIBIT('z'):
        w = code_len + 1 - cur.mark;
        if (w > 255) FAIL("loop body too large");
        emit(w);
#ifdef SYMS
        sym_patch('B', (word)(code_len - 1), (word)cur.mark,
                  (int)w, last_func_vec);
#endif
        break;

      // \k: param slot for the next call argument; frame above is the caller
      // (the rule that matched \i and called Z)
      case HIBIT('k'):
        cv = (sp >= 1) ? stack[sp - 1].func_vec : 0xff;
        f = (cv == 0xff) ? SYM_NONE : sym_func(cv);
        if (f == SYM_NONE) FAIL("call of unknown function");
        fs = &syms[f];
        b = cur.arg_i;
        if ((word)(b + 1) >= fs->nparams)
          FAIL("too many arguments");
        // (the "S" opcode is not missing: the BNF has [S\k], \k is only the slot)
        emit(fs->param_slot[b]);
        cur.arg_i = b + 1;
        break;

      // \#xx: emit raw hex byte (e.g. \#00 for NUL, \#20 for space)
      case HIBIT('#'):
        if (b && p[2]) {
          emit(hex2(p + 1));
          adv = 3;
        } else
          emit('#');
        break;

      // \g: global/local init from captures \1=dest slot, \2=src const
      //   scope 0: defer until main entry; else emit L+S now
      case HIBIT('g'):
        dest = cur.prod_cap[1] & 0xff;
        src  = as_addr(cur.prod_cap[2] & 0xff);
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
        emit(e & 0x7f);
      }

      p += adv;
      continue;
    }
    emit(e);
    p++;
  }

  if (*p == HIBIT(']')) p++;
  cur.result = VAL_ACC;
  return p;
}


static void skip_hspace(void) {
  while (nextc == ' ' || nextc == '\t' || nextc == '\r')
    skip();
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

// skip spaces and tabs of a text line
static char *skipsp(char *p) {
  char c;

  while ((c = *p) == ' ' || c == '\t') p++;
  return p;
}

// Parse one #= directive body (after the "#="). Returns 1 on success.
// Forms:  "{ }" | "/* */" | "//"   (whitespace separates start/end)
static int comments_add_from_line(char *p) {
  CommentStyle *cs;
  word i;
  char c;

  if (ncomments >= MAX_COMMENTS) return 0;
  p = skipsp(p);
  if (!*p) return 0;

  cs = &comments[ncomments];
  cs->start_len = 0;
  cs->end_len = 0;
  cs->start[0] = 0;
  cs->end[0] = 0;

  // start delimiter: non-space run
  i = 0;
  while ((c = *p) && c != ' ' && c != '\t' && i + 1 < COMM_START_MAX) {
    cs->start[i++] = c;
    p++;
  }
  cs->start[i] = 0;
  cs->start_len = i;
  if (!cs->start_len) return 0;

  p = skipsp(p);

  // optional end delimiter => block comment; none => line comment
  i = 0;
  while ((c = *p) && c != ' ' && c != '\t' && i + 1 < COMM_END_MAX) {
    cs->end[i++] = c;
    p++;
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

// True if source at line_pos starts with s[0..len). Does not consume.
// Uses skip/load_nextc so fetch/grow lives in one place only.
static int match_at(const char *s, uchar len) {
  uchar i;
  word save;

  if (!len) return 0;
  save = line_pos;
  for (i = 0; i < len; i++) {
    if (!nextc || nextc != (uchar)s[i]) {
      line_pos = save;
      load_nextc();
      return 0;
    }
    if (i + 1 < len)
      skip();
  }
  line_pos = save;
  load_nextc();
  return 1;
}

static void skip_ws(void) {
  uchar ci, n;
  CommentStyle *cs;

  for (;;) {

    // whitespace
    while (is_ws(nextc))
      skip();
    note_progress();
    if (!nextc) return;

    // try each configured comment style
    for (ci = 0; ci < ncomments; ci++) {
      cs = &comments[ci];
      n = cs->start_len;
      if (!match_at(cs->start, n)) continue;

      // consume start
      eat_n(n);

      n = cs->end_len;
      if (n == 0) {
        // line comment: to newline (keep the \n) or EOF
        while (nextc && nextc != '\n')
          skip();
      } else {
        // block comment: to end marker
        while (nextc) {
          if (match_at(cs->end, n)) {
            eat_n(n);
            break;
          }
          skip();
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
  while (got('\n')) {
    nnl++;
    note_progress();
    skip_hspace();
  }
  if (nnl == 0) return 0;
  skip_ws();
  return 1;
}


// match an identifier into id_buf
static int match_ident_raw(void) {
  word n;

  skip_ws();
  if (!isalpha(nextc))
    return 0;

  n = 0;
  // letter, digit, or underscore
  while (isalnum(nextc) || nextc == '_') {
    if (n + 1 < NAME_LEN)
      id_buf[n++] = nextc;
    skip();
  }
  id_buf[n] = 0;
  note_progress();
  return 1;
}

// next non-blank source char after line_pos; nothing consumed or captured
static uchar peek_char(void) {
  word sv = line_pos;
  uchar sc = nextc, c;

  while (is_ws(nextc))
    skip();
  c = nextc;
  line_pos = sv;
  nextc = sc;
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
  Symbol *s;

  if (n < 1 || n > cur.prod_n) return 0;
  expect = cur.prod_cap[n];
  if (expect == VAL_ACC) return 0;

  if (!match_ident_raw()) return 0;
  i = sym_find(id_buf, SYM_VAR, 0);
  if (i == SYM_NONE) return 0; // not defined: just fail this alt
  s = &syms[i];
  slot = s->address;
  if (slot != (expect & 0xff)) return 0;
  record_capture(MAKE_CAP(slot, sym_type(s)));
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
  cur.func_vec = last_func_vec;

  return 1;
}

// \+ — declare ident into current scope; result = new slot
static int match_ident_decl(void) {
  uchar slot;
  word i;
  uchar is_reg = pending_register;

  pending_register = 0;

  if (!match_ident_raw()) return 0;
  i = sym_find(id_buf, SYM_VAR, 1);
  if (i != SYM_NONE)
    slot = syms[i].address;
  else {
    if (scope_depth == 0) {
      if (is_reg)
        slot = alloc_register_slot();
      else
        slot = alloc_global_slot();
    } else
      slot = alloc_slot();
    set_decl_type(sym_add(id_buf, SYM_VAR, slot));
  }
  record_capture(MAKE_CAP(slot, cur_type));
  vlog("  decl '%s' scope %u slot %u%s type %c%s\n",
       id_buf, (unsigned)scope_depth, (unsigned)slot,
       is_reg ? " REGISTER" : "",
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
  // optimize: frames start at FRAME_BASE (96), after global bands 0..95
  if (optimize && next_slot < FRAME_BASE)
    next_slot = FRAME_BASE;
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
#ifdef SYMS
  frame_sz[vec] = 4; // ret/val/prev/ACC at minimum
#endif
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
  for (s = syms, i = 0; i < nsym; i++, s++)
    if (s->scope == scope_depth && s->pidx) k++;
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
  word i, f;
  Symbol *fs, *t, *lastp;

  v = last_func_vec;
  if (v == 0xff) return 0;
  f = sym_func(v);
  if (f == SYM_NONE) return 0;
  fs = &syms[f];        // nothing is added below: pointer stays valid

  n = 0;
  lastp = NULL;
  for (t = syms, i = 0; i < nsym; i++, t++) {
    if (t->scope == scope_depth && t->pidx) {
      fs->param_slot[n] = t->address;
      lastp = t;
      n++;
    }
  }
  if (n) {
    lastp->address = frame_base[v] + 3;
    fs->param_slot[n - 1] = lastp->address;
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

  if (!nextc) return 0;
  c = nextc;
  skip();
  if (c == '0') {
    *out = 0;
  } else if (c == 'x' || c == 'X') {
    v = 0;
    for (i = 0; i < 2 && isxdigit(nextc); i++) {
      v = (v << 4) + hexval(nextc);
      skip();
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
static int read_c_char(uchar *out) {
  uchar c;

  if (!nextc) return 0;

  c = nextc;
  skip();
  if (c == '\\')
    return decode_escape(out);
  *out = c;

  return 1;
}

// \s / \u — quoted "..." string
// emit_code=0 (\s): match "..." → data_pool + reloc pointer const (capture)
// emit_code=1 (\u): opening " already matched; emit body bytes + 0 into bytecode
static int match_str(uchar emit_code) {
  uchar c;
  word off = 0;
  uchar slot;

  if (!emit_code) {
    skip_ws();
    off = data_string_start();
    EXPECT('"');
  } else if (nextc == '"') {
    // opening " still in source (some rules only emit ["] without matching it)
    skip();
  }
  // else: opening " already matched as a literal before \u

  while (nextc && nextc != '"') {
    if (!read_c_char(&c)) return 0;
    if (emit_code)
      emit(c);
    else
      data_putc(c);
  }
  EXPECT('"');
  note_progress();

  if (emit_code) {
    emit(0);
    return 1;
  }

  data_putc(0);
  slot = add_string_reloc(off);
  record_capture(MAKE_CAP(slot, TY_CHAR | TY_PTR));
  return 1;
}

static int match_num(void) {
  word ty;
  uchar c;
  uchar base = 10;
  word v = 0;
  uchar n = 0;

  skip_ws();

  if (got('\'')) {

    // character literal 'x' / '\n' / ... → small integer const
    if (!read_c_char(&c)) return 0;
    EXPECT('\'');
    v = c;
    ty = TY_CHAR;

  } else {

    // base prefix 0x 0b 0o; a lone leading 0 is just the digit 0
    if (got('0')) {
      n = 1;
      // switch saves 6 bytes cmp if elif...
      switch (nextc) {
      case 'x': base = 16; break;
      case 'o': base =  8; break;
      case 'b': base =  2; break;
      }

      if (base != 10) {
        skip();
        n = 0;      // need at least one digit after the prefix
      }
    }

    // parse number (or cont of 0...)
    for (;;) {

      // TODO: negative!

      // Convert (HEX) character to its numeric value
      // (- 28827 28807) = saved 20 bytes
      c = toupper(nextc) - '0';
      if (c > 9) {
        c -= 'A' - '0' - 10;
        if (c < 10) break;
      }

      // Ensure the digit fits the current base (e.g., '8' in octal is invalid)
      if (c >= base) break;

      n++;
      v = v * base + c;
      skip();
    }

    // If no digits were successfully parsed, it's a failure
    if (n == 0) return 0;

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

// \e is a directive (stored with the hibit) or a quoted literal char e
// note: '>' is load-time only (not stored)
//       '<' becomes HIBIT(x), x=1..31 (alt jump) or HIBIT('<')
static int is_dir(uchar e) {
  return isdigit(e) || strchr("insfdprlxywmzkcahvgetou+{}#=", e) != NULL;
}

static void build_one_rule(uchar r, char *rhs) {
  char *p;
  uchar *blob, **altp, e, inprod, hex;
  word a, bi, na, alt_starts[MAX_ALTS];
  uchar taildest;   // 1-based alt index for \> / \< (0 = none yet)

  // directives (\i \n \s \o \< \d \+ \{ \}, rule calls, bare space, [ ]) get
  // the hibit, quoted \| \c (not specials) become the plain char
  // \> is not stored; it only updates taildest
  // \< becomes HIBIT(x) (x=1..31) = jump to alt x-1, or HIBIT('<') if no \> yet
  // build contiguous string with 0 separators between alts.
  blob = malloc(strlen(rhs) + 2);
  if (!blob) FAIL("out of memory (rule)");
  free(blobs[r]);
  free(rule[r]);
  blobs[r] = blob;

  bi = 0;
  na = 0;
  inprod = 0;
  hex = 0;
  taildest = 0;
  alt_starts[na++] = 0;
  for (p = rhs; (e = *p); p++) {
    if (hex) {
      // \#xx: the two hex digits are data (may be A-F), stored as they are
      hex--;
    } else if (e == '\\' && p[1]) {
      e = *++p;
      if (e == '>') {
        // \> : set jump target to current alt (1-based); do not store
        taildest = (uchar)na;   // na is already the current alt count
        continue;
      }
      if (e == '<') {
        // \< : with a \> target jump code 1..31, else full rule restart
        e = HIBIT(taildest && taildest < 32 ? taildest : '<');
      } else {
        if (is_dir(e)) e = HIBIT(e);
      }
    } else if (e == '|' && !inprod) {
      // '|' separates alts only outside a bare [ ... ] production
      // skip empty alts from || or line-join artifacts
      if (na > 0 && alt_starts[na - 1] == bi)
        continue;
      blob[bi++] = 0;
      if (na < MAX_ALTS)
        alt_starts[na++] = bi;
      continue;
    } else if (e == '[') {
      // bare [ ] = production (no nesting); | inside is a literal (\[ \] are handled above)
      inprod = 1;
      e = HIBIT('[');
    } else if (e == ']' && inprod) {
      inprod = 0;
      e = HIBIT(']');
    } else
      // rule calls and the ws-gap are directives, but not inside a production
      e = (!inprod && (isupper(e) || e == ' ')) ? HIBIT(e) : e;

    blob[bi++] = e;
    if (e == HIBIT('#')) hex = 2;
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
  char *nl, *eq, *p, c;
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

    c = *line;
    if (c == '.') break;
    if (c == 0) continue;

    // #= comment-style directive (must be before bare # skip)
    if (c == '#' && line[1] == '=') {
      comments_add_from_line(line + 2);
      continue;
    }
    if (c == '#') continue;

    p = skipsp(line);

    // continuation: line starts with |  (p stays, text is appended below)
    if (!(*p == '|' && have_cur)) {

      // new rule: flush previous
      if (have_cur)
        build_one_rule(cr, acc);
      have_cur = 0;

      if (!isupper(c)) continue;

      eq = strchr(line, '=');
      if (!eq) continue;

      cr = c - 'A';
      have_cur = 1;
      p = eq + 1;
      acc_len = 0;
    }

    while ((c = *p) && acc_len + 1 < MAX_ACCUM) {
      acc[acc_len++] = c;
      p++;
    }
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
  uchar **ap = rule[r];

  if (!ap) return 0;
  while (ap[n]) n++;
  return n;
}

// ---- stack ----

// snapshot the input/code/scope state of the newest frame as the
// start of an alt (used by push and by tail-recursion \<)
static void frame_mark(void) {
  cur.alt = 0;
  cur.ip = 0;
  cur.save = line_pos;
  cur.save_abs = abs_pos();
  cur.nsym_save = nsym;
  cur.scope_save = scope_depth;
  cur.slot_save = next_slot;
  cur.code_save = code_len;
  cur.prod_n = 0;
}

static void push(uchar r, uchar state) {
  if (sp >= MAX_STACK) FAIL("stack overflow");

  if (sp >= stack_cap) {
    Frame *ns;
    uchar ncap = stack_cap + STACK_GROW;

    if (ncap > MAX_STACK) ncap = MAX_STACK;
    ns = realloc(stack, ncap * sizeof(Frame));
    if (!ns) FAIL("out of memory (stack)");
    stack = ns;
    stack_cap = ncap;
  }

  // program main begin → blob.main
  if (r == 'B' - 'A' && sp > 0 && cur.rule == 'P' - 'A')
    main_offset = bin_ptr;

  // suspend active frame (memcpy via struct assignment), open a new cur
  stack[sp] = cur;
  sp++;
  anim_push(r);

  cur.rule = r;
  cur.func_vec = 0xff;
  cur.arg_i = 0;
  cur.npatch = 0;
  cur.mark = 0;
  cur.restart = 0;
  cur.result = 0;
  memset(cur.prod_cap, 0, sizeof(cur.prod_cap));
  frame_mark();
  cur.state = state;
}

static void restore_scope(void) {
  sym_truncate(cur.nsym_save);
  scope_depth = cur.scope_save;
  next_slot = cur.slot_save;
}

// back to the start of the current alt: input, code, productions
static void frame_rewind(void) {
  line_pos = cur.save;
  load_nextc();
  code_len = cur.code_save;
  cur.prod_n = 0;
  cur.npatch = 0;

  // First emit of a function clears pending_func_entry and may emit main's
  // deferred ginit. If this rewind drops that code, re-arm so the next emit
  // runs gen_func_entry again (otherwise globals stay 0 and P sees NULL).
  if (last_func_vec != 0xff) {

    if (last_func_vec < MAX_VECTORS && vectors[last_func_vec] > code_len)
      pending_func_entry = last_func_vec;
    else if (last_func_vec == main_vec && main_offset >= code_len)
      pending_func_entry = last_func_vec;
  }
}

// ---- one step ----
// estimated ~50-200us per step on 6502 @ 1MHz (dispatch + match)

static int step(void) {
  uchar *alt, *a;
  uchar c, e, rch, st;
  int ok;
  word nalt, adv;

  if (sp == 0) return 0;

  st = cur.state;

  // TRY ALTERNATIVES of CURRENT RULE
  if (st == ST_TRY) {

    // At end of RULE? => POP and continue caller
    nalt = rule_nalt(cur.rule);
    if (cur.alt >= nalt) {
      vlog("fail %c (no alt) @%u\n", 'A' + cur.rule, abs_pos());
      anim_pop();

      if (sp == 0) return -1;
      sp--;
      cur = stack[sp]; // restore parent Frame

      // parent's current alt failed: set ST_FAIL
      //   restores pos, capture, code, scope, moves next alt
      cur.state = ST_FAIL;

      return 1;
    }

    // undo and rest current ALT
    frame_rewind();
    cur.ip = 0;
    cur.state = ST_RUN;

    vlog("try %c alt %u @%u \"", 'A' + cur.rule, cur.alt, abs_pos());
    #ifdef VERBOSE
      if (verbose >= 3) show_alt(rule[cur.rule][cur.alt]);
    #endif
    vlog("\"\n");

    return 1;
  }


  if (st == ST_OK) {
    rch = 'A' + cur.rule;

    // Type rules I and T set cur_type for subsequent \+/\p decls
    if (rch == 'I') {
      // alts: int=0, word=1, void=2 (word sized), char=3 (order in c.bnf)
      cur_type = (cur.alt == 3) ? TY_CHAR : TY_WORD;
    }
    if (rch == 'T') {
      // T=I *|I^*|I  → alt 0/1 pointer, alt 2 plain
      if (cur.alt < 2)
        cur_type |= TY_PTR;
    }

    vlog("ok %c alt %u -> @%u\n", rch, cur.alt, abs_pos());
    #ifdef VERBOSE
      // -v: one GEN per statement (K), after alt commits
      if (verbose >= 2 && code_len > cur.code_save && rch == 'K') {
        eprintf(NORMAL "GEN:          [ ");
        ivm_print_code_range(ERROUT, code_buf, cur.code_save, code_len, 0);
        eprintf("]\n");
      }
    #endif

    // 
    {
      word child_res = cur.result;

      anim_pop();
      if (sp == 0) return 0;
      sp--;
      cur = stack[sp]; // restore parent Frame

      // parent records subrule result as next \n capture
      record_capture(child_res);
      cur.ip++;
      cur.state = ST_RUN;
    }
    return 1;
  }

  // current alt failed — next alt of same rule
  if (st == ST_FAIL) {
    frame_rewind();
    restore_scope();
    cur.alt++;
    cur.ip = 0;
    cur.state = ST_TRY;
    return 1;
  }

  // ST_RUN
  alt = rule[cur.rule][cur.alt];
  a = alt + cur.ip;
  if (!alt || (c = *a) == 0) {
    cur.state = ST_OK;
    return 1;
  }

  // literals (hibit clear) are matched right here: a whole run per step
  // (saved 67 bytes by c = *a)
  if (!(c & 0x80)) {

    for (;;) {

      if (nextc != c) {
        // not there: whitespace/comments may come first (not before a quoted
        // space/tab), or only the case differs (-I)
        if (c != ' ' && c != '\t') skip_ws();

        // lowercase if not match and ignore_case
        if (nextc != c &&
            !(ignore_case && isalpha(c) && tolower(nextc) == tolower(c))) {
          vlog("  fail lit '%c' @%u got '%c'\n", c, abs_pos(),
               nextc ? nextc : '?');
          cur.state = ST_FAIL;
          return 1;
        }
      }

      skip();
      note_progress();
      vlog("  ok lit '%c' @%u\n", c, abs_pos());

      c = *++a;
      if (!c || (c & 0x80)) break;
    }

    cur.ip = a - alt;

    return 1;
  }


  // space in BNF = required ws gap: skip comments/whitespace, no input char consumed as literal
  if (c == HIBIT(' ')) {
    skip_ws();
    cur.ip++;
    return 1;
  }

  // bare [ ... ] = production (use \[ \] in BNF to match bracket chars)
  if (c == HIBIT('[')) {
    cur.ip = run_production(a + 1) - alt;
    return 1;
  }

  // rule call
  if (c >= HIBIT('A') && c <= HIBIT('Z')) {
    uchar r = c - HIBIT('A');

    if (!rule_defined(r)) FAIL("rule not defined");
    vlog("  call %c from %c @%u\n", c & 0x7f, 'A' + cur.rule, abs_pos());
    push(r, ST_TRY);
    return 1;
  }

  ok = 1;
  adv = 1;      // bytes of the BNF alt used by this directive

  if (c == HIBIT('<')) {
    // plain \< : full rule restart (sequences, no \> in this rule)
    frame_mark();
    cur.state = ST_TRY;
    vlog("  tail %c @%u\n", 'A' + cur.rule, abs_pos());
    return 1;
  }
  if (c >= HIBIT(1) && c <= HIBIT(31)) {
    // jump code (x=1..31) from load-time \> : jump to alt x-1
    cur.alt = c - HIBIT(1);
    cur.ip = 0;
    cur.state = ST_TRY;
    vlog("  tail>%u %c @%u\n", (unsigned)cur.alt, 'A' + cur.rule, abs_pos());
    return 1;
  }

  if (c == HIBIT('o')) {
    // \o commit: drop buffer prefix; keep matching
    line_commit();
    // fix save positions relative to new buffer
    {
      uchar i;
      for (i = 0; i < sp; i++)
        stack[i].save = 0;
      cur.save = 0;
    }
  }
  else if (c == HIBIT('{')) scope_enter();
  else if (c == HIBIT('}')) scope_leave();

  // \#xx: result := hex xx, same as \d having matched that number (no input)
  else if (c == HIBIT('#')) {
    if (!a[1] || !a[2]) FAIL("bad \\# in grammar");
    record_capture(const_slot(hex2(a + 1)));
    adv = 3;
  }

  // \=n: result := capture n (no new capture recorded)
  else if (c == HIBIT('=')) {
    e = a[1] - '0';
    if (e >= 1 && e <= cur.prod_n)
      cur.result = cur.prod_cap[e];
    adv = 2;
  }

  else if (c >= HIBIT('0') && c <= HIBIT('9')) ok = match_ident_same(c - HIBIT('0'));
  else if (c == HIBIT('i')) ok = match_ident();
  else if (c == HIBIT('+')) ok = match_ident_decl();
  else if (c == HIBIT('f')) ok = match_ident_func_decl();
  else if (c == HIBIT('p')) ok = match_param_decl();
  else if (c == HIBIT('r')) {
    // global: after BNF "register\r" — mark next \+ as REGISTER (0..31)
    // function: end parameter list (existing)
    if (last_func_vec != 0xff)
      ok = params_done();
    else {
      pending_register = 1;
      ok = 1;
    }
  }
  else if (c == HIBIT('d')) ok = match_num();
  else if (c == HIBIT('n')) ok = match_newline();
  else if (c == HIBIT('s')) ok = match_str(0); // \s string const
  else if (c == HIBIT('u')) ok = match_str(1); // \u emit string inline
  else if (c == HIBIT('z')) {
    // sizeof value from cur_type (after matching T)
    record_capture(MAKE_CAP(const_slot(
      (!CAP_IS_PTR(cur_type) && CAP_TYPE(cur_type) == TY_CHAR) ? 1 : 2), TY_WORD));
  }
  // (other directives only mean something inside a production: no-op here)

  if (!ok) {
    vlog("  fail \\%c @%u\n", c & 0x7f, abs_pos());
    cur.state = ST_FAIL;
    return 1;
  }
  vlog("  ok \\%c @%u\n", c & 0x7f, abs_pos());
  cur.ip += adv;
  return 1;
}

// run up to n steps
// ~50-200 us/step on 6502 @ 1MHz → n=1000 ≈ 50-200 ms
static int run(word n) {
  int rc = 1;

  // 25961
  do {
    if (!step()) {
      emit('q');
      return 0;
    }
  } while (--n);
  
  return 1;
}

// "file:line: " prefix of an error line
static void err_prefix(word line) {
  eprintf("%s:%u: ", src_name ? src_name : "?", line);
}

// print buffered source line starting at i; returns index after its '\n'
static word show_line(word line, word i) {
  uchar c;

  err_prefix(line);
  while (i < line_len && (c = line_buf[i]) != '\n') {
    eputc(c);
    i++;
  }
  eputc('\n');
  if (i < line_len) i++;
  return i;
}

static void report_reject(void) {
  word i;

  eprintf("\n--- REJECT at %u (furthest %u) ---\n",
          abs_pos(), max_pos_abs);

  eprintf("STACK:");
  if (far_sp == 0) eprintf(" (empty)");

  for (i = 0; i < far_sp; i++)
    eprintf(" %c%u", far_stack[i], (unsigned)far_alt[i]);

  eprintf("\n");

  if (far_sp > 0) {
    uchar r = far_stack[far_sp - 1];
    uchar a = far_alt[far_sp - 1];

    eprintf("EXPECTED: rule %c", r);
    r -= 'A';
    if (rule_defined(r) && a < rule_nalt(r))
    { eprintf(" alt %u \"", a); show_alt(rule[r][a]); eputc('"'); }

    eprintf("\n");
  }

  if (had_undef && had_undef_func)
    eprintf("ERROR: undefined function: %s\n", last_undef);
  else if (had_undef)
    eprintf("ERROR: identifier \"%s\" not defined here\n", last_undef);

  // Show error from line_buf only — no rewind/fseek (cc65-friendly).
  // line_no_base tracks the line number of line_buf[0] across \C commits.
  eprintf("REJECT:\n");
  {
    word want = (had_undef && undef_pos_abs) ? undef_pos_abs : max_pos_abs;
    word start, off, col, line, endpos;
    word nlines;

    if (!line_buf || line_len == 0) {
      eprintf("  (no buffered source)\n");
    } else if (want < line_abs) {

      // furthest was discarded by \C; still report position
      eprintf("%s: (offset %u, before current buffer @%u)\n",
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
        eputc(' ');
      eprintf("^\n");

      // following lines still in buffer (no second open of the file)
      for (nlines = 0; nlines < ERR_LINES_CONTEXT && endpos < line_len; nlines++)
        endpos = show_line(line + 1 + nlines, endpos);
    }
  }

}


// init: load grammar + open source (errors: FAIL)
static void init(char *bnf_path, char *src_path) {

  src_name = src_path;
#ifdef SYMS
  sym_open(src_name);
#endif
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
  load_nextc();     // prime nextc (reads the first source char)
  max_pos_abs = 0;
  far_sp = 0;
  stack = NULL;
  stack_cap = 0;
  sp = 0;
  memset(&cur, 0, sizeof(cur));
  had_undef = 0;
  had_undef_func = 0;
  undef_pos_abs = 0;
  last_undef[0] = 0;
  next_slot = 0;
  pending_register = 0;
  gen_reset();
  if (optimize)
    next_slot = FRAME_BASE; // frames; globals use next_reg / next_glob
  nvector = 0;

  last_func_vec = 0xff;

  sym_reset();

  // get started with rule P
  if (!rule_defined('P' - 'A')) FAIL("rule P missing");
  push('P' - 'A', ST_TRY);
}

static void shutdown(void) {

  if (src_fp) fclose(src_fp);
  src_fp = NULL;

  free(line_buf);
  line_buf = NULL;

  free_rules();

  free(stack);
  stack = NULL;
  stack_cap = 0;
  sym_free();
}

int main(int argc, char **argv) {
#ifdef VERBOSE
  word a;
#endif
  char *bnf = NULL;
  char *srcpath = NULL;
  char *arg;
  word i;
  int rc;
  int err;
  int blen;
  uchar *blob;
  
  verbose = 0;
  ignore_case = 1;
  optimize = 0;

  // parse arguments
  for (i = 1; i < argc; i++) {
    arg = argv[i];
    if (strcmp(arg, "-v") == 0)
      verbose++;
    else if (strcmp(arg, "-O") == 0)
      optimize = 1;
    else if (strcmp(arg, "-I") == 0)
      ignore_case = 1;
    else if (strcmp(arg, "-C") == 0)
      ignore_case = 0;
    else if (!bnf)
      bnf = arg;
    else if (!srcpath)
      srcpath = arg;
  }

  if (!bnf || !srcpath) {
    eprintf("usage: %s [-v|-vv] [-O] [-I|-C] grammar.bnf source.p\n", argv[0]);
    return 1;
  }

  // FAIL
  // - longjmp value is error message pointer (or 1 on host: see fail_msg)
  err = setjmp(fail_jmp);
  if (err) {

    #ifdef __CC65__
      fail_msg = (char *)err;
    #endif

    anim_report();
    eprintf("\n%%ERROR: %s\n", fail_msg);
    if (fail_file) eprintf("  file: %s\n", fail_file);
    report_reject();
    shutdown();
    return 1;
  }

  // setup
  init(bnf, srcpath);

#ifdef VERBOSE
  // print rules
  if (verbose >= 2) {

    eprintf("--- rules ---\n");
    for (i = 0; i < NRULES; i++)

      if (rule_defined(i)) {
        eprintf("%c =", 'A' + (int)i);
        for (a = 0; rule[i][a]; a++) {
          if (a) eprintf("|");
          show_alt(rule[i][a]);
        }
        eprintf("\n");
      }
  }
  if (verbose) eprintf("--- parse ---\n");
#endif

  // run in chunks of 10000 steps until done
  do {
    rc = run(10000);
  } while (rc == 1);

  anim_report();
  
  // consistency check
  if (rc) FAIL("syntax error");

  // OK
  skip_ws();
  if (nextc) FAIL("trailing input");

  eprintf("--- ACCEPT ---\n");

#ifdef VERBOSE
  eprintf("--- BYTECODE (%u bytes) ---\n", (unsigned)code_len);

  ivm_print_code(ERROUT, code_buf, code_len);
  eputc('\n');

  eprintf("--- HEXDUMP (%u bytes) ---\n", (unsigned)code_len);
  // dump hex
  for (i = 0; i < code_len; i++) {
    eprintf("%02x", code_buf[i]);
    if ((i & 15) == 15) eputc('\n');
    else eputc(' ');
  }
  if (code_len & 15) eputc('\n');
#endif

  {
    blen = 0;
    // frame_base[] (set by \f) goes into the blob's fp table
    blob = build_blob(&blen);

    #ifdef VERBOSE
    eprintf("--- BLOB (%d bytes) main=%u nvector=%d ---\n",
      blen, main_offset, nvector);
    #endif

    if (blob) {

      // SAVE .ivm next to source (foo.c → foo.ivm)
      {
        char out[128];
        const char *base, *s, *dot;
        FILE *of;
        word n;
        char ch;

        // find base and last '.' position
        // TODO: strrchr ???
        base = src_name ? src_name : "out";
        for (s = base; (ch = *s); s++)
          if (ch == '/' || ch == '\\') base = s + 1;
        n = 0; dot = 0;
        for (s = base; (ch = *s) && n < sizeof(out) - 5; s++) {
          out[n++] = ch;
          if (ch == '.') dot = out + n - 1;
        }

        // change name test.p => test.ivm
        if (dot) n = (word)(dot - out);
        out[n++] = '.'; out[n++] = 'i'; out[n++] = 'v'; out[n++] = 'm';
        out[n] = 0;

        of = fopen(out, "wb");
        if (of) {
          fwrite(blob, 1, (size_t)blen, of);
          fclose(of);
          eprintf("--- SAVED %s (%d bytes) ---\n", out, blen);
#ifdef SYMS
          if (sym_fp) {
            sym_write_all();
            eprintf("--- SAVED .sym ---\n");
          }
#endif
        } else
          eprintf("--- SAVE failed: %s ---\n", out);
      }

#ifdef RUN
      {
        Process *proc;
        ivmvar ret;

      #ifdef VERBOSE
        if (verbose >= 1)
          for (i = 0; i < nvector; i++)
            eprintf("  frame V%02x fp=%u\n", i, frame_base[i]);
      #endif

        // Nevm installs consts (reversed pool) + host soft_heap from data
        proc = Nevm(blob);
        if (proc) {
          eprintf("--- RUN ---\n");
          ret = run_ivm(proc);
          eprintf("--- END acc=%u ---\n", ret);
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
