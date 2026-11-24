// parse.c — iterative BNF bytecode interpreter
//
//   ./parse [-v|-vv] [-I|-C] grammar.bnf source.p
//
//   " It’s a remarkably clean design.
// 
//     A tiny declarative grammar that both *recognises* the source
//     and *emits* the byte code in the same rules, an iterative
//     (non-recursive) matcher that stays small enough for 6502, and
//     just enough specialisation (loop special codes, the capture
//     machinery, the mark/patch jumps) to get real speedups
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
//   X=body     RULE X (A-Z, a-z)
//
//   a-z/punct  match LITERAL char (-I case-insensitive, default)
//   space      skip whitespace and comments (styles from #= lines)
//   #= { }     block comment style (start end); multiple #= allowed
//   #= /* */   e.g. C block comments
//   #= //      line comment (to newline); no end token
//              default if no #=: Pascal { }
//   $A-$Z      call rule
//   $a-$z
//
//   |             alternative (epsilon allowed)
//   newline '|'   continue previous rule (long alts)
//   newline '.'   end of rules section
//   [ ... ]       production: after match, emit bytes
//
// Match directives (stored with hibit):
//
// IDENTS:
//   \{ \}      enter / leave scope (\} drops names added in scope)
//
//   \i         ident in scope (must be declared by \+)
//   \+         ident declare (match + add to current scope)
//   \f         declare function: name = ret-value cell of a new frame + vector
//   \p         declare parameter (frame cell base+4+n); \r ends the list
//   \r         end of param list OR flag REGISTER after literal "register"
//   \m         member: .f or ->f of the struct of the last \i: const k
//   \q         end of a struct: "struct \f\{ { params } \q" (instead of \})
//
//   \&         a[i].f, an array of structs (SoA): const, f's array (striped:
//              its lo bytes; its hi bytes: the next const slot)
//   \b         flag REGISTER after literal "register" (use this, not \r)
//            (TODO: understand usage:)
//   \^         the next \i may name an array of structs alone (sizeof(a))
//
//   \_         a static declaration: as a global, the name local
//   \'X        set type high-byte of result to X ('c'/'w'/'C'/'W'; \'^ = uppercase)
//
//
// NUMBERS:
//   \d         parse number / char literal, sets type for guard
//
// STRINGS:
//   \s         string const "..." → data pool + reloc pointer (capture)
//   \u         emit string body + 0 into bytecode (opening " already matched)
//
// MATCH:
//   \n         match >=1 newlines; eats surrounding horizontal space
//   \1..\9     match same ident as capture N (match_ident_same)
//
// GUARDS:
//   \;c        fail if the next char is c (reads nothing: & not &&)
//   \?X        fail unless result type high-byte is X
//   \| \. etc  literal escaped char (not a directive)
//
// CONTROL:
//   \!         a statement that does not parse: report it, skip it (to its
//              ;, over a { } block), go on; fails at a } or the end
//   \%         a false #if: skip lines to its #else / #endif (nested ones
//              whole)
//
//   \=n        result := capture n (no new capture)
//   \#xx       result := hex const xx
//
//   \o         commit: drop line buffer up to current point
//
// (tail-recursive rules, instead of Kleenex!)
//   \>         (load-time) mark current alt as jump target for \<
//   \<         tail-recurse: to \> alt if any, else restart whole rule
//
// ================================================================
// Production directives: ONLY used inside [ ... ]:
//
//   [] alone   no code, result is ACC
//   \[ \]      literal [ ]
//
//   \1..\9     emit slot/result byte from nth value capture
//   \aN        force address of capture N
//   \cN \hN    lo / hi byte of const capture N -- \c3 is NOT \3
//              \3 emits the SLOT of capture 3 (where the value
//              lives, for "L xx"), \c3 the low byte of the VALUE of a const
//              capture (e.g. a loop step or a 'k' case byte inline)
//
//   \lN        load capture N if not ACC (emit Lnn if not)
//
//   \t         create a temp slot (becomes a slot result)
//   \e  ???    end/temp helper (see run_production)
//   \j         create a temp "only read"
//
//   \g         global init store
//
//   \v         creata a CALL opcode for matched function
//   \k         next call-arg param slot
//
// LOOPS / IFS / JUMPS
//   \( \- \)   loop open / continues land here / breaks land here (close)
//
//   \x \y \w   forward jump placeholder / patch / swap
//   \m \z      loop mark / relative back
//
//   \/ \~      break / continue: forward jump placeholder (0?G\/)
//
//
// Directive chars used (ASCII order, match + production):
//   ! # $ % & ' ( ) * + , - . / 0-9 : ; < = > ? @ ^ _ a-z (all) { } ~
//   (A-Z are rule names; space is the ws-gap directive)
//
// Free (not directives; ok as \X literal or future ops):
//   " `
//


// Compiling and Running
// =====================
// To use this to compile a program in PASCAL or C write this
// 
//   $ clang -g DSYMS -DVERBOSE -DRUN parse.c -o parse.x
//
//   $ ./parse.x -v c.bnf test.c              # ./pc test
//
//   $ ./parse.x -v tpi.bnf test.p            # ./p  test
//
//
//
// This will compile, show byte code, hexdump and some info.
// It'll also write out test.ivm and test.sym.
// 
//   $ ./ivm test                     # run using C interpreter
//
//   $ ./ivm-asm test                 # run using asm interpreter
//
//   $ ./ivm-jit test                 # jit code and run simulated
//
//   $ ./ivm-print test               # dump context and pp byte code
//
//
// -v -v (needs -DVERBOSE): SRC: shows the source text as the furthest parse
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
// 4. assume there is "unbounded input" read in chunks (fread fills the free buffer)
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
// HIBIT(' ') ws-gap, HIDIR('i') is \i, HIBIT('[') HIBIT(']') production,
// HIBIT(1..31) alt jump (from \> \<). Quoted \| \\ \[ \] \  \; \A are just the
// plain byte. \C \S are stored as \o \u (they would be the rule calls C S).



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
// TODO:
// A: (on the note below) with cc65 -Os, isalpha() etc. are inline table
//   lookups (lda c / tax / lda __ctype,x / and #mask), about as big as
//   ((c|32)-'a') < 26. Little to gain: measure one spot before changing all.

/*
7. tolower(c) / isalpha / isalnum / isxdigit — cc65 emulation overhead

These map to __ctype_* lookups. Each ≈ 8–10 bytes inline. In tight
inner loops (match_num, read_ident, match_lit, hexval) they cost
more than a hand-rolled check. Rewriting isalpha(c) as ((c|32)-'a' <
26) (≈5 bytes) in those 4 hot spots saves ~15–20 bytes and is
faster. Not a function call, but the same "tighten duplicated idiom"
idea.

#define isalpha(c) ((uchar)(((c)|32)-'a'))

*/

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

// ------------------------------------------------------------
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

// ------------------------------------------------------------
// CONFIGURATION

// (- 26871 26002) = 869
//#include "darr.c"

#define MAX_BIN_LEN 4096
#define MAX_SLOTS 255

// 8 = (* 8 29) = 232 bytes bigger each time
#define STACK_GROW 8

#define ERR_LINES_CONTEXT 3   // lines shown after the error line
#define ERR_LINES_BEFORE  2   // ...and before it, if still buffered

// errors and verbose output goes to:
#define ERROUT stdout



// Change .ext
// "dir/foo.c" -> "foo" + ext (".ivm", ".sym") into out[128]
static void change_ext(char *out, const char *path, const char *ext) {
  word n = 0;
  const char *base = path, *s;
  char *dot = 0;

  for (s = path; *s; s++)
    if (*s == '/' || *s == '\\') base = s + 1;

  for (s = base; *s && n < 128 - 5; s++) {
    out[n++] = *s;
    if (*s == '.') dot = out + n - 1;
  }

  if (dot) n = (word)(dot - out);
  strcpy(out + n, ext);
}

// shared emit + real const addresses
#ifdef SYMS
static FILE *sym_fp;
static uchar sym_r[256];
static uchar sym_w[256];

static void note_load(uchar addr)  { if (sym_r[addr] < 255) sym_r[addr]++; }
static void note_store(uchar addr) { if (sym_w[addr] < 255) sym_w[addr]++; }

#else

// TODO: use macros instead this take space
// A: ivm-gen.c declares them as functions ("void note_load(uchar);"), so a
//   macro here breaks that line. Needs #ifdef SYMS around the prototypes
//   in ivm-gen.c; then "#define note_load(a)" saves the calls.
static void note_load(uchar addr) { (void)addr; }
static void note_store(uchar addr) { (void)addr; }

#endif

// ------------------------------------------------------------
// FAIL: real (non-backtracking) errors. Message is printed ONCE where
// setjmp is caught in main(), together with the source position.
// 6502: pointer fits in int → longjmp(jb, msg).
// Host: int is 32-bit, pointers 64-bit → stash in fail_msg.
// (a failed *match* is NOT an error: that is normal backtracking, return 0)
static jmp_buf fail_jmp;
static const char *fail_msg;
static const char *fail_file;   // optional: file name for "cannot open"

// a function, not inline longjmp(): each FAIL() is then just "msg; jsr"
static void fail(const char *msg) {
#ifdef __CC65__
  longjmp(fail_jmp, (int)msg);
#else
  fail_msg = msg;
  longjmp(fail_jmp, 1);
#endif
}
#define FAIL(msg)  fail(msg)

// malloc/realloc that FAILs instead of returning NULL (p NULL: malloc)
static void *xrealloc(void *p, word n) {
  if (!(p = realloc(p, n))) FAIL("out of memory");
  return p;
}

// ivm-gen.c still uses the old name
#define IVM_GEN_FAIL(msg) FAIL(msg)

// ------------------------------------------------------------
#include "ivm-gen.c"



// .sym file writer (SYMS only)
#ifdef SYMS
  #include "parse-syms.c"
#endif


// ------------------------------------------------------------

#include <assert.h>
#include <string.h>

// Save bytes for poor on-device 6502 cc65
#ifdef RUN
  #include "ivm.c"
  static uchar compile_only;  // -c: write the .ivm, don't run it
#endif

// ------------------------------------------------------------

// A-Z
#define NRULES      ('z' - 'A' + 1)   // $A..$Z $a..$z (index: letter - 'A')

#define MAX_STACK   64
#define MAX_ALTS    64


#define HIBIT(c) ((c) | 0x80)

// Rule bytes: plain ASCII is a literal; with the hibit:
//
//   $81-$9A  letter directives \a..\z: HIDIR (so $A..$Z $a..$z are free)
//   $A0-$C0  ws gap, punctuation / digit directives \+ \# \< ..: HIBIT
//   $C1-$DA  rule calls $A..$Z, $E1-$FA $a..$z: HIBIT(letter)
//   $DB $DD  production [ ], $FB $FD \{ \}
//
// \< is 2 bytes: HIBIT('<'), ALT_JUMP + alt (1-based; 0: the rule again)
//
// A rule's index: its letter - 'A' (a..z: 32.., a few unused slots)

#define HIDIR(c)    (0x80 | ((c) & 31))  // a..z only (a case label: no ?:)
#define IS_RULE(c)  ((uchar)(((c) | 32) - HIBIT('a')) < 26)   // c: hibit
#define ALT_JUMP    0x20         // printable, not | [ ] \ \n: scans keep working
#define MAX_JUMP    31           // \< to alt 1..31


// rule['X'-'A'] = NULL-terminated array of uchar* into one malloc blob
static uchar **rule[NRULES];

// blobs[r] holds the contiguous text for rule r
static uchar *blobs[NRULES];

// ------------------------------------------------------------
static FILE *src_fp = NULL;
static char *src_name;

// input line buffer (unbounded source, fread into the slack; \o keeps the current line)
static uchar *line_buf;

// TODO: seriously 6 state variables, lol
// A: 6 now. They are:
//
//   line_buf/line_cap  the buffer and its size
//   line_len           bytes in it
//   line_pos           cursor (nextc is line_buf[line_pos])
//   line_abs           file offset of line_buf[0] (abs_pos = abs + pos)
//   line_no_base       line number of line_buf[0], for error messages
//
//   max_pos_abs        furthest position reached, for error messages
//
// The first 5 are the minimum for a sliding window. line_no_base could be
// counted at error time only if the buffer were never dropped by \o.

static word line_no_base;
static word line_len;
static word line_cap;
static word line_pos;       // cursor in line_buf
static word line_abs;       // absolute file offset of line_buf[0]
static word max_pos_abs;    // furthest absolute position

// ------------------------------------------------------------
// Snapshot of the matcher stack at the furthest source position reached.
// Used ONLY for error reporting (not during matching).
// far_stack[i] = rule letter ('A'+rule), far_alt[i] = alt index in that rule.
// Updated in note_progress when abs_pos advances.

static uchar far_stack[MAX_STACK];
static uchar far_alt[MAX_STACK];
static uchar far_sp;

// ------------------------------------------------------------
static uchar verbose;       // 0 off, 1 compact, 2+ detail
static uchar stripe;        // -S: all arrays of structs striped (the JIT)
static uchar ignore_case;   // default 1

// ------------------------------------------------------------
// FRAME struct storage

// TODO: too big!
// A: 49 bytes now The big ones:
//
//   prod_cap[10] 20 B (index 0 unused: [9] saves 2 B but needs n-1 code)
//   patch[4] 8 B. rip could be a uchar if no alt gets over 255 bytes.
//
// It's per stack level (up to MAX_STACK, grown by STACK_GROW), and push
// copies the whole Frame: smaller Frame = less RAM and faster push/pop.

typedef struct {
  uchar rule;

  // alts: pointer into the NULL-terminated rule[r] alt list
  // rip:  offset into *alts (rule instruction pointer)
  uchar **alts;
  word rip;

  // -- UNDO INFO - when backtracking
  word save_abs;      // abs file pos at alt start (input rewind / commit floor)

  word nsym_save;     // symbol count at alt entry (backtrack drops newer ones)
  uchar scope_save;   // scope_depth at alt entry

  // production value captures \1..\9; result = rule value (word)
  // 0..254 slot, 0x80|idx const, 0xFFFF = ACC
  word prod_cap[10];
  uchar prod_n;

  word result;

  word code_save; // bin_ptr at alt entry (backtrack)
  uchar ginit_save;   // ginit_len at alt entry (global init code, backtrack)
  uchar slot_save;    // next_slot at alt entry (backtrack frees slots)
  uchar glob_save;    // next_glob, next_reg the same (-O globals)
  uchar reg_save;
  uchar func_vec;     // vector of the function matched by \i in THIS frame (for \v)
  uchar arg_i;        // call args stored so far (rule Z, for \k)
  uchar rec;          // the symbol \i / \m matched here (0xff none): its
                      // struct (ret_slot) is \m's start (here or parent)
  uchar rec_save;     // rec at alt entry (a failed "\m\?c" alt: undone)
  uchar npatch;       // pending forward jumps (\x pushes, \y pops)
  word patch[4];
  word mark;          // \m loop start (for \z)
  uchar loop_save;    // nloop, nbrk at alt entry (break / continue)
  uchar brk_save;
} Frame;

// break / continue: not per frame like \x \y, the break is in the body,
// a rule (K) below the loop's. Loop k (\( .. \)) owns the pending jumps
// brk_at[loop_brk[k]..nbrk[ (the rel byte of a 0?G): \- patches its
// continues, \) its breaks. Alt entry saves nloop / nbrk (backtracking).
#define MAX_LOOP 8
#define MAX_BRK  16
static uchar nloop, loop_brk[MAX_LOOP];
static uchar nbrk, brk_cont[MAX_BRK];    // 1: continue, 0: break, 2: done
static word  brk_at[MAX_BRK];

// stack[0..sp-1] = suspended parents; cur = active frame (by value).
// Push/pop copy the whole Frame (cheaper on cc65 than many cur.field ops).

static Frame *stack;
static uchar stack_cap;
static uchar sp;     // number of saved parents
static Frame cur;    // working frame — access as cur.field (absolute on 6502)

// Symbols and scopes (\+ \f \p \i \{ \}) live in ivm-gen.c, allocated on demand:
//   sym_add / sym_find / sym_func / sym_truncate, scope_enter / scope_leave
//
// Function frames use the tpi.c layout, frame_base[vec] goes into the blob:
//   base+0 ret addr, +1 ret value (= function name), +2 prev fp,
//   +3 last param (JSR stores ACC there), +4.. other params, then locals

// longest identifier (stack buffers only)
#define NAME_LEN   32

// the identifier just matched by read_ident (shared, no arguments)
static char  id_buf[NAME_LEN];

// TODO: I'm sure when error it's in id_mmatch???
// A: last_undef is filled by note_undef() from id_buf, in match_ident (\i)
//   when the name isn't declared. Only report_reject() prints it.

// data for error reporting
static char  last_undef[NAME_LEN]; // furthest undefined ident (for reject msg)

static uchar undef_kind;           // sticky: 0 none, 1 undefined var, 2 function
static word  nerr;                 // \! errors reported (no .ivm then)
static word  err_at;               // the last one's position (+1): reported
static void  err_statement(word s0);
static void  check_consts(word from);
static void  err_here(const char *msg);
static void  err_skip(void);
static void  pp_skip(void);

static word  undef_pos_abs;        // sticky: position of that ident

// ------------------------------------------------------------

// current function
static uchar last_func_vec; // function being declared by \f (for \p \r)

uchar next_slot; // next free slot index for \+ decls / temps

// const + bin / bin_ptr from ivm-gen.c
#define VAL_ACC  0xffff

// Type of the declaration currently being parsed (set by matching I/T).
// High byte is type char: 'c'/'w' or uppercase 'C'/'W' for pointer.
static word cur_type = TY_WORD;


// ------------------------------------------------------------
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
    if (*a & 0x80)
      eprintf(INVERSE "%s%c" NORMAL, IS_RULE(*a) ? "$" : "\\",
        *a < 0xA0 ? *a - 0x80 + 'a' - 1 : *a & 0x7f);
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
// A: removed v1_indent(): nothing called it.

#else

  #define v1(...)   ((void)0)
  #define vlog(...) ((void)0)

#endif

// ------------------------------------------------------------
// LINE BUFFER

// c <= 32 was the old rule; isspace is clearer and smaller on cc65
#define is_ws(c) isspace(c)


static word abs_pos(void) {
  return (word)(line_abs + line_pos);
}

// \o commit: drop buffer before line_pos; window starts at line_abs.
// Does not touch frames. Rewind uses save_abs; if save_abs < line_abs, fail-fast.
static void line_commit(void) {
  word i, rest, drop;

  // drop only through the last '\n' before line_pos: the current line
  // stays whole in the buffer (error report shows it from its start)
  drop = 0;
  for (i = 0; i < line_pos; i++)
    if (line_buf[i] == '\n') {
      line_no_base++;
      drop = i + 1;
    }
  if (!drop) return;

  rest = line_len - drop;
  if (rest)
    memmove(line_buf, line_buf + drop, rest);

  line_abs = (word)(line_abs + drop);
  line_len = rest;
  line_pos -= drop;   // same source position (was = 0: re-parsed the line!)
}


// the source char at line_pos (0 at EOF), always one char looked ahead
static uchar nextc;

// (re)load nextc from line_pos; grow/fetch from file when past end of buffer
static void load_nextc(void) {
  int ch;
  word ncap;

  // need: already buffered at line_pos?
  if (line_pos < line_len) {
    nextc = line_buf[line_pos];
    return;
  }

  // grow: expand line_buf only if there is no slack left
  if (line_len >= line_cap) {
    ncap = line_cap ? (word)(line_cap * 2) : 256;
    line_buf = xrealloc(line_buf, ncap);
    line_cap = ncap;
  }

  // fetch: fill all the slack in one read
  nextc = 0;
  if (!src_fp) return;
  ch = (int)fread(line_buf + line_len, 1, line_cap - line_len, src_fp);
  if (ch <= 0) return;            // EOF
  line_len += ch;
  nextc = line_buf[line_pos];
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

// ------------------------------------------------------------
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
    far_alt[i] = (uchar)(stack[i].alts - rule[stack[i].rule]);
  }
  if (n < MAX_STACK) {
    far_stack[n] = 'A' + cur.rule;
    far_alt[n] = (uchar)(cur.alts - rule[cur.rule]);
    n++;
  }
  far_sp = n;
}

// ------------------------------------------------------------
// BYTECODE EMIT

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

// the forward jump placeholder (rel byte) at w: to here (\y \- \))
static void patch_here(word w) {
  if (bin_ptr - (w + 1) > 255) FAIL("jump too far");
  bin[w] = bin_ptr - (w + 1);
#ifndef IVM_NO_VN
  vn_label = bin_ptr;                  // a jump lands here
#endif
  #ifdef SYMS
    sym_patch('F', w, (word)bin_ptr, (int)bin[w], last_func_vec);
  #endif
}

// the last plain load emitted by \lN: [pl_at, pl_end[ loads slot pl_slot
static word  pl_at, pl_end = 0xffff;
static uchar pl_slot;

// record a value-producing match into \\1..\\9 of current frame
static void record_capture(word val) {
  uchar n = cur.prod_n;

  // captures are \1..\9 = prod_cap[1..9] (0 unused). n < 9: room for
  // one more. A 10th value only becomes the result. \N can't get it
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

// ------------------------------------------------------------
// SLOTS storage and memory index allocations

// Slot layout:
//   optimize==0: single bump next_slot from 0 (globals and frames intermixed)
//   optimize!=0: REGISTER globals 0..31, other globals 32..95, frames >= FRAME_BASE (96)
// Failed alts restore next_slot via slot_save (see restore_scope).

static uchar pending_register; // set by \r after literal "register" in BNF
static uchar pending_const;    // set by \c after literal "const"
static uchar static_decl;      // in a static declaration: \_ in rule V,
                               // cleared when that V ends or fails

// 3 functions doing the same thing? 
// TODO: one code, array of 3?
// A: they differ in more than the counter: alloc_slot is the frame/temp
//   bump (+ SYMS frame size), registers keep next_slot above them when
//   !optimize, globals fall back to alloc_slot when !optimize. A table
//   {counter, limit, msg} would cover the bump part only: little saved.

// Global variable allocation
static uchar alloc_slot(void) {
  uchar s;

  if (next_slot >= CONST_TOP - 32) FAIL("out of slots");

  s = next_slot;
  next_slot++;

#ifdef SYMS
  // TODO: make a function or macro this disturbes readabilty
  //   and doesn't cpature purpose of code give good name
  // A: it records the largest frame size for the .sym file (SYMS only,
  //   frame_size() reads it). As a function: note_frame_size().
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


// oh here it is
static uchar alloc_global_slot(void) {
  uchar s;

  if (!optimize)
    return alloc_slot();
  if (next_glob >= FRAME_BASE)
    FAIL("too many globals (32..95 full)");

  s = next_glob++;

  return s;
}




// TODO: I think we do this a lot, but in tpi.c
//   we had "elaborate" redordering as well as clever
//   associtiveity and other simple tricks -- peep hole?
// A: this is only the spill (S tmp). The peephole is gen_peephole() in
//   ivm-gen.c, run with -O on the final code; tpi.c's reordering would
//   go there (on bytecode), not here.

// Force a refernce to a physical slot needed for param to ops!
//
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



// ------------------------------------------------------------
// (BIG) PRODUCTION


// run production at p (first char after '['); returns after ']'
// quiet: GEN is printed only when the owning alt accepts (pop)
static uchar *run_production(uchar *p) {
  uchar n, b, e, adv, cv, dest, src, *q, is_char = 0;
  word w;
  unsigned int val;

  // ACC captures this production refers to are spilled FIRST, so the
  // spill code lands before the bytes that use it (\l takes ACC as is)

  // TODO: give examples, and "PROVE" this is is always correct
  //   or can we detect and give error when it's not?
  // A: e.g. "X=E<E [<\2]": when the production runs, the code for
  //   both E's is already emitted, and ACC holds the value of the LAST
  //   one. So at most one capture can really be in ACC: the last one
  //   matched. Spilling it (S tmp) before the production's bytes is
  //   correct, because nothing has touched ACC since. If two captures are
  //   VAL_ACC, the earlier one is already lost (see the E+E+E note below).
  //   Detect: count VAL_ACC captures here; more than one is a grammar
  //   bug, could FAIL("ACC clobbered") (debug builds only, costs bytes).
  for (q = p; (e = *q) && e != HIBIT(']'); q++) {

    n = q[1];
    if (e == HIDIR('a') && n) { n -= '0'; q++; }     // \aN: N is a plain digit
    else if (e >= HIBIT('1') && e <= HIBIT('9')) n = e - HIBIT('0');
    else continue;

    if (n <= cur.prod_n && cur.prod_cap[n] == VAL_ACC)
      force_addr(n);
  }

  // parse [ ... ]
  while ((e = *p) && e != HIBIT(']')) {

    if (e & 0x80) {

      // capture digit for \aN \lN \cN \hN (a plain byte) 
      b = p[1];
      n = b - '0';      
      adv = 1;          

      if (e >= HIBIT('1') && e <= HIBIT('9')) {

        // \N: insert result byte of capture N
        // - as if it's an address!
        w = cap_val(e - HIBIT('0'));
// TODO: how can we force an expression which is passed already if
//   we didn't save it "X=E+E+E [\1+\2+\3]" -- does this work???
// A: no. All three E's leave their value in ACC, so when the production
//   runs, only \3 is still in ACC; \1 and \2 were overwritten. Write it
//   left-recursive instead (E+E then +E), or spill each one as it's
//   matched: "X=E[S\t]+E[S\t]+E...".

        if (w == VAL_ACC) w = force_addr(e - HIBIT('0'));
        emit(as_addr(w & 0xff));

      } else if ((e == HIDIR('a') || e == HIDIR('l') || e == HIDIR('c') || e == HIDIR('h')) && n >= 1 && n <= 9) {

        adv = 2;
        w = cap_val(n);

        if (e == HIDIR('a')) {

          // \aN: force addr
          emit(force_addr(n) & 0xff);

        } else if (e == HIDIR('l')) {

          // \lN: load capture N into ACC unless it already is there
          if (w != VAL_ACC) {
            pl_at = bin_ptr;
            emitop2('L', as_addr(w & 0xff));
            pl_end = bin_ptr; pl_slot = as_addr(w & 0xff);
          }

        } else {

          // \cN: emit low byte of capture N's *value* (consts from \d / 'x')
          // \hN: emit high byte of that value
          val = 0;
          if (w != VAL_ACC) {
            b = w & 0xff;
            if (is_lit(b) || isconst(b))
              val = lit_val(b);
          }
          emit((e == HIDIR('c') ? val : val >> 8) & 0xff);

        }
      } else switch (e) {

      case HIDIR('v'):
        emitop(0xC0 | (cur.func_vec & 0x3f));
        break;

      // \t: new temp slot, emit it, remember it as a capture
      // \e: value - 1! "exclusive limit" (< and not <=)
      //     — S temp; J temp (store ACC-1)
      case HIDIR('t'): case HIDIR('e'): case HIDIR('j'):
        b = alloc_slot();

// TODO: this reads like gobblegock!
        
        // \j: a temp only read (O, an index): after a plain load of a var
        // / const (\l) right before the S, its slot has the value: no temp
        // ("x ^ (x << 7)": x itself). The L goes too: what follows \j
        // (R, E, '[') loads ACC itself. Not \t: a rule may write the
        // temp, or want a snapshot (a loop limit)
        if (e == HIDIR('j') && bin_ptr && bin[bin_ptr - 1] == 'S' &&
            pl_end == bin_ptr - 1 &&
            (bin[pl_at] == 'L' ? bin[pl_at + 1] : bin[pl_at]) == pl_slot) {
          bin_ptr = pl_at;                     // the L, the S
          acc_invalidate();
          pl_end = 0xffff;
          next_slot--;                         // the temp is not needed
          record_capture(pl_slot);
          break;
        }
#ifndef IVM_NO_VN
        // value numbering: ACC's value in a slot already? use that one
        if (e != HIDIR('e') && bin_ptr && bin[bin_ptr - 1] == 'S' &&
            (cv = vn_temp(cur.code_save, b)) != 0) {
          next_slot--;                         // the temp is not needed
          record_capture(cv);
          break;
        }
#endif
        if (e == HIDIR('e')) {
          // TODO: replace with better loop/test code-gen
          // A: \e makes "i < n" into "i <= n-1" for the t/n loop ops,
          //   at the cost of a temp + 2 ops. A loop op with < (exclusive)
          //   would remove it: then \e isn't needed at all.
          emitop2('S', b);
          emitop2('J', b);
        } else
          emit(b);
        record_capture(b);
        break;



      // \xHH: the byte HH; \x: forward jump placeholder
      case HIDIR('x'):
        if (isxdigit(p[1]) && isxdigit(p[2])) {
          emit(hex2(p + 1));
          adv = 3;
          break;
        }
        emit(0);
        b = cur.npatch;
        if (b < 4) {
          cur.patch[b] = bin_ptr - 1;
          cur.npatch = b + 1;
        }
        break;

      // \y: patch newest placeholder to jump here
      case HIDIR('y'):
        b = cur.npatch;
        if (b) {
          cur.npatch = --b;
          patch_here(cur.patch[b]);
        }
        break;

      // \( open a loop: the \/ \~ in its body are its
      case HIBIT('('):
        if (nloop == MAX_LOOP) FAIL("loops nested too deep");
        loop_brk[nloop++] = nbrk;
        break;

      // \/ break, \~ continue: forward jump placeholder, to the loop's
      //   \) / \-
      case HIBIT('/'):
      case HIBIT('~'):
        if (!nloop) FAIL(e == HIBIT('/') ? "break outside a loop"
                                         : "continue outside a loop");
        if (nbrk == MAX_BRK) FAIL("too many break / continue");
        emit(0);
        brk_at[nbrk] = bin_ptr - 1;
        brk_cont[nbrk++] = e == HIBIT('~');
        break;

      // \- continues land here; \) breaks land here, the loop is closed
      case HIBIT('-'):
      case HIBIT(')'):
        if (!nloop) FAIL("loop end without \\(");
        for (b = loop_brk[nloop - 1]; b < nbrk; b++) {
          if (brk_cont[b] == 1 && e == HIBIT(')'))
            FAIL("continue: the loop has no \\-");
          if (brk_cont[b] == (e == HIBIT('-'))) {
            patch_here(brk_at[b]);
            brk_cont[b] = 2;
          }
        }
        if (e == HIBIT(')')) nbrk = loop_brk[--nloop];
        break;

      // \w: swap the two newest placeholders
      case HIDIR('w'):
        b = cur.npatch;
        if (b >= 2) {
          w = cur.patch[b - 1];
          cur.patch[b - 1] = cur.patch[b - 2];
          cur.patch[b - 2] = w;
        }
        break;

      // \m: remember loop start - destination for B call
      case HIDIR('m'):
        cur.mark = bin_ptr;
#ifndef IVM_NO_VN
        vn_label = bin_ptr;
#endif
        break;

      // \z: backward jump delta to the mark (after the B opcode)
      case HIDIR('z'):
        w = bin_ptr + 1 - cur.mark;
        if (w > 255) FAIL("loop body too large");
        emit(w);
#ifdef SYMS
        sym_patch('B', (word)(bin_ptr - 1), (word)cur.mark,
                  (int)w, last_func_vec);
#endif
        break;


      // \k: param slot for the next call argument; frame above is the caller
      // (the rule that matched \i and called Z)
      case HIDIR('k'):
        cv = (sp >= 1) ? stack[sp - 1].func_vec : 0xff;
        if (cv == 0xff) FAIL("call of unknown function");
        // (the "S" opcode is not missing: the BNF has [S\k], \k is only the slot)
        emit(arg_slot(cv, cur.arg_i++));
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
      case HIDIR('g'):
        dest = cur.prod_cap[1] & 0xff;
        src  = as_addr(cur.prod_cap[2] & 0xff);
        if (pending_const) {
          // rebind the declared name to the const slot; no store emitted
          pending_const = 0;
          {
            word si;
            for (si = 0; si < nsym; si++)
              if (syms[si].address == dest && syms[si].type == SYM_VAR) {
                syms[si].address = src;
                syms[si].is_const = 1;
                vlog("  const rebind '%s' slot %u -> const %u\n",
                  syms[si].name, dest, src);
                break;
              }
          }
          break;
        }

        // char var: keep the hi byte 0 (i j in the JIT rely on it)
        {
          word si;
          for (si = 0; si < nsym; si++)
            if (syms[si].address == dest && syms[si].type == SYM_VAR) {
              if (syms[si].vtype == 'c' && !syms[si].is_ptr) {
                if (isconst(src)) src = const_slot(const_val(src) & 0xff);
                else is_char = 1;
              }
              break;
            }
        }

        // A global initializer ("int g = 5;" outside any function)
        // has no code to go into: nothing runs at scope 0. So it's
        // recorded in ginit_*, and gen_func_entry() emits Z/C stores
        // at the start of main. Inside a function it's a plain L+S.
        if (scope_depth == 0 || static_decl) {
          // (a static: main's prologue, if main is still to come)
          if (scope_depth && main_vec != 0xff)
            FAIL("static: initialized after main (move the function up)");
          gen_ginit(dest, src);    // code now, into main's prologue
          vlog("  ginit slot %u <- const %u\n", dest, src);
        } else {
          // TODO: we have better ways of generating this
          //   a) Z dd            dd := 0;
          //   b) 9 dd            dd := -1  or 0xffff;
          //   c) C dd ss         dd := ss;
          //   d) C d1 ss S d2... d2 := d1 := ss;
          emitop2('L', src);
          emitop2(is_char ? 's' : 'S', dest);
        }
        break;

        // LITERAL BYTE (like a op-letter)
      default:
        // TODO:O why strip bit? Is his for the "quoted"
        // A: a \X inside a production that isn't a production directive,
        //   e.g. [\d] or [\s]: it was stored with the hibit by
        //   build_one_rule (is_dir), so emit the plain letter. Plain
        //   letters in [ ] have no hibit and go to emit(e) below.
        // emit quoted \[ and /] basically
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



// ------------------------------------------------------------
// PARSING


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

// TODO: why we have 3 differnt ones
// 1. skip_hspace
// 2. skipsp
// 3. skip_ws
//
// They all sound the same, what's the SEMANTIC diffeerence
// What's there different USAGES?
// A: skip_hspace: SOURCE, spaces/tabs/CR only, never '\n' (for \n and
//      for '(' after a name: must not cross a newline)
//    skipsp: GRAMMAR text, a char* in load_bnf (not the source at all)
//    skip_ws: SOURCE, all whitespace incl. newlines, and comments: what
//      literals and the ws-gap use


// TODO: move to LINEBUF - why we have two?
// A: two source skippers because \n needs one that stops at '\n'
//   (skip_hspace) and everything else wants one that eats newlines and
//   comments (skip_ws). Both could move next to the LINE BUFFER code.
static void skip_hspace(void) {
  while (nextc == ' ' || nextc == '\t' || nextc == '\r')
    skip();
}

// skip spaces and tabs of a text line
static char *skipsp(char *p) {
  char c;

  while ((c = *p) == ' ' || c == '\t') p++;
  return p;
}

// copy a non-space run (max 3 chars) from p to dst, NUL-terminated; its length
static uchar copy_tok(char *dst, char *p) {
  uchar i = 0;
  char c;

  while ((c = *p++) && c != ' ' && c != '\t' && i + 1 < COMM_START_MAX)
    dst[i++] = c;
  dst[i] = 0;
  return i;
}

// Parse one #= directive body (after the "#="). Returns 1 on success.
// Forms:  "{ }" | "/* */" | "//"   (whitespace separates start/end)
static uchar comments_add_from_line(char *p) {
  CommentStyle *cs;

  if (ncomments >= MAX_COMMENTS) return 0;
  p = skipsp(p);

  cs = &comments[ncomments];

  // start delimiter: non-space run
  if (!(cs->start_len = copy_tok(cs->start, p))) return 0;

  // optional end delimiter => block comment; none => line comment
  cs->end_len = copy_tok(cs->end, skipsp(p + cs->start_len));

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
static uchar match_at(const char *s, uchar len) {
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

// Another one?
// the last run of white space / comments skipped: [ws_at, ws_end[
// (an error at a statement's end: before the gap after it)
static word ws_at, ws_end;

static void skip_ws(void) {
  uchar ci, n;
  CommentStyle *cs;
  word a = abs_pos();

  for (;;) {

    // whitespace
    while (is_ws(nextc))
      skip();
    note_progress();
    if (!nextc) break;

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
    if (ci >= ncomments) break;
  }
  if (abs_pos() != a) { ws_at = a; ws_end = abs_pos(); }
}


// ------------------------------------------------------------
// \OPS in rules


// \n in BNF: hspace, >=1 newline, hspace; then comments via skip_ws
static uchar match_newline(void) {
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

// read and identifier into id_buf
static uchar read_ident(void) {
  word n;

  skip_ws();
  if (!isalpha(nextc) && nextc != '_')    // C: _ starts one too (__MC65__)
    return 0;

  // letter, digit, or underscore
  n = 0;
  while (isalnum(nextc) || nextc == '_') {
    if (n + 1 < NAME_LEN)
      id_buf[n++] = nextc;
    skip();
  }
  id_buf[n] = 0;
  note_progress();

  return 1;
}

// \i — ident must already be in scope; result = its slot
//   name followed by ( is looked up as a function (like tpi.c), else a variable

// Record undefined ident (id_buf) for reject message
// (keep furthest; funcs win over vars)
static void note_undef(uchar is_func, word pos) {
  // TODO: 2 what? and you're going to match to err string
  //   why aren't we just assigning the err string? LOL
  if (!is_func && undef_kind == 2) return;
  if (undef_kind && pos < undef_pos_abs) return;

// TODO: just print error message, why are you copying things?
//   just FAIL already maybe with a printf?
// A: an undefined name is not an error yet: this alt fails, another one
//   may match (backtracking). Only when the whole parse fails do we know,
//   and by then id_buf holds some later name: so keep a copy of the
//   furthest one for the report.

  strncpy(last_undef, id_buf, NAME_LEN - 1);
  last_undef[NAME_LEN - 1] = 0;

  undef_kind = is_func + 1;        // 1: undef var NAME, 2: undef FUNCTION

  undef_pos_abs = pos;
}

static word last_sym = 0xffff;   // the symbol \i matched last (\$)

// type word of a symbol (for MAKE_CAP): uppercase high byte if pointer
static word sym_type(Symbol *s) {
#if 1
  // (- 26618 26584) = 34 bytes better?
  // TOUPPER for letters only
  return (s->is_ptr ? s->vtype & ~32 : s->vtype) << 8;   // pointer: upper
#else  
  uchar t = s->vtype;
  if (s->is_ptr) t = (uchar)toupper(t);
  return (word)t << 8;
#endif
}

// give a new symbol the type being declared (cur_type)
static void set_decl_type(Symbol *s) {
  uchar t = (uchar)(cur_type >> 8);
  s->is_ptr = isupper(t) ? 1 : 0; // haha: yes isupper can return any
  s->vtype = (uchar)tolower(t);
  s->ret_slot = (uchar)last_sym;   // struct \i: its struct (for \m)
}

// \1..\9 — like \i, but slot must equal capture n (same variable).
// Fail quietly (no undef_kind): rule should fall through to next alt.
static uchar match_ident_same(uchar n) {
  word i, expect;
  uchar slot;
  Symbol *s;

  if (n < 1 || n > cur.prod_n) return 0;

  expect = cur.prod_cap[n];
  if (expect == VAL_ACC) return 0;

  if (!read_ident()) return 0;

  i = sym_find(id_buf, SYM_VAR, 0);
  // not defined: just fail this alt
  if (i == SYM_NONE) return 0; 

  s = &syms[i];
  slot = s->address;
  if (slot != (expect & 0xff)) return 0;
  record_capture(MAKE_CAP(slot, sym_type(s)));
  return 1;
}

// Predefined names (#define'd consts), made when first seen: no const
// slot otherwise. __MC65__: the .ivm version, __TARGET_6502__: 1
static uchar predef(void) {
  word v;
  Symbol *s;

  if (!strcmp(id_buf, "__MC65__")) v = IVM_VERSION;
  else if (!strcmp(id_buf, "__TARGET_6502__")) v = 1;
  else return 0;
  s = sym_add(id_buf, SYM_VAR, add_const(v));
  s->is_const = 1;
  return 1;
}

// \^: the next \i may be an array of structs alone (sizeof(a))
static uchar any_name;

static uchar match_ident(void) {
  uchar c, slot, any;
  word start_abs, i;
  Symbol *s;

  start_abs = abs_pos();
  any = any_name;               // once: for this \i only
  any_name = 0;
  if (!read_ident()) return 0;

  // next char after the name, no newline crossed: '(' means a call
  skip_hspace();
  c = nextc;

  // TODO: rewrite sym_find to take c, if it's '(' it's function
  //   otherwise look for var...SYM_FUNC = '(' BTW will make things work!
  // TODO: we're doing many sym_find( SYM_VAR ) make it a function,
  //  saves passing an argument, I think? 
  i = sym_find(id_buf, (c == '(') ? SYM_FUNC : SYM_VAR, 0);
  if (i == SYM_NONE && c != '(' && predef()) i = (word)(nsym - 1);

  if (i == SYM_NONE) {
    note_undef(c == '(', start_abs);
    vlog("  ident '%s' not in scope\n", id_buf);
    return 0;
  }

  s = &syms[i];
  last_sym = i;
  if (s->type == SYM_FUNC) {

    // It's a function name? (in the frame: later \i matches, the call
    // arguments, must not change \v. Not last_func_vec: that is the one
    // being declared, "struct pt *q" in its params is an \i too)
    cur.func_vec = s->address;
    cur.rec = (uchar)(i + 1);         // the ret value: its struct, mk()->x
    slot = s->ret_slot;

  } else {

    // It's a variable name? (an array of structs: only a[i]. Its name
    // is the first field's array (SoA), as a pointer it would be wrong)
    if (s->ret_addr_slot != 0xff && c != '[' && !any) return 0;
    cur.func_vec = 0xff;
    cur.rec = (uchar)i;
    slot = s->address;
  }
  record_capture(MAKE_CAP(slot, sym_type(s)));

  return 1;
}

// \+ — read and declare ident into current scope; result = new slot
static uchar match_ident_decl(void) {
  uchar slot;
  word i;
  uchar is_reg = pending_register;

  pending_register = 0;
  if (!read_ident()) return 0;

  i = sym_find(id_buf, SYM_VAR, 1);
  if (i != SYM_NONE)
    slot = syms[i].address;
  else {
    if (scope_depth == 0 || static_decl) {   // static: as a global
      if (is_reg)
        slot = alloc_register_slot();
      else
        slot = alloc_global_slot();
    } else
      slot = alloc_slot();
    set_decl_type(sym_add(id_buf, SYM_VAR, slot));
    if (scope_depth && static_decl && slot < FRAME_BASE)   // (-O)
      static_slot[slot >> 3] |= (uchar)(1 << (slot & 7));  // gen_pack
  }
  record_capture(MAKE_CAP(slot, cur_type));

  vlog("  decl '%s' scope %u slot %u%s type %c%s\n",
    id_buf, (unsigned)scope_depth, (unsigned)slot,
    is_reg ? " REGISTER" : "",
    (char)(CAP_TYPE(cur_type) >> 8), CAP_IS_PTR(cur_type) ? "*" : "");

  return 1;
}



// The last declared ident as an array of cnt elements at const slot k:
// its own slot goes back (the last one handed out), the name is a const
// pointer of its declared type. 0: not a plain variable
static uchar array_sym(word cnt, uchar k) {
  Symbol *s;
  uchar slot;

  if (!nsym) return 0;
  s = &syms[nsym - 1];
  slot = s->address;
  if (s->type != SYM_VAR || !cnt) return 0;
  if ((scope_depth == 0 || static_decl) && optimize) {
    if (next_glob == slot + 1) next_glob--;
    if (slot < FRAME_BASE)                     // (a static: not kept)
      static_slot[slot >> 3] &= (uchar)~(1 << (slot & 7));
  } else if (next_slot == slot + 1) next_slot--;

  s->nelem = cnt;
  s->is_ptr = 1;
  s->is_const = 1;                     // the name: no stores
  s->address = k;
  vlog("  array '%s' [%u] %c at const slot %u\n", s->name, (unsigned)cnt,
    s->vtype, (unsigned)k);
  return 1;
}

// element size of the last declared ident
#define ESIZE() (syms[nsym - 1].vtype == 'c' ? 1 : 2)

// the element count: the product of the const captures, 0 none:
//   \+\[N\]: N   struct \i \+: its fields   struct \i \+\[N\]: fields * N
static word cap_count(void) {
  uchar i, k;
  word n = 0;

  for (i = 1; i <= cur.prod_n; i++) {
    k = CAP_SLOT(cur.prod_cap[i]);
    if (is_lit(k)) n = (word)((n ? n : 1) * lit_val(k));
  }
  return n;
}

// \@: a static array of cap_count() elements: zeroed (bss). Three captures
// (struct \i \+\[N\]): an array of structs, SoA (\&): N in ret_addr_slot
// (else 0xff; N < 255)
static uchar make_array(void) {
  word cnt = cap_count(), n = 0xff;

  if (cur.prod_n > 2 && (n = lit_val(CAP_SLOT(cur.prod_cap[3]))) >= 0xff)
    return 0;
  if (!nsym || !cnt || !array_sym(cnt, add_bss_array((word)(cnt * ESIZE()))))
    return 0;
  syms[nsym - 1].ret_addr_slot = (uchar)n;
  if (stripe && n != 0xff) syms[nsym - 1].vtype = 'z';   // -S: striped
  return 1;
}

// Initialized arrays: \: starts its data (capture 2: the size, if given),
// \, adds the last capture (a const) as an element, \. ends it: zeros up
// to the size, the name points to it (in the data: the file)
static word ia_start, ia_n;

static uchar init_start(void) {
  // set once (data): in a function C would set it on every call
  if (scope_depth && !static_decl)
    FAIL("an initialized array in a function needs static (set once)");
  ia_n = cap_count();
  ia_start = data_string_start();
  return nsym != 0;
}

static uchar init_elem(void) {
  uchar k = CAP_SLOT(cur.result);
  word v;
  if (!nsym || !is_lit(k)) return 0;
  v = (word)lit_val(k);
  data_putc((char)v);
  if (ESIZE() == 2) data_putc((char)(v >> 8));
  return 1;
}

static uchar init_end(void) {
  uchar k = CAP_SLOT(cur.result), es = ESIZE();
  word n = (word)(data_len - ia_start), want = (word)(ia_n * es);

  // [3] = "abc": no room for the 0, C drops it
  if (ia_n && n == want + 1 && es == 1 && !data_pool[data_len - 1]) {
    data_len--;
    n--;
  }
  if (ia_n && n > want) FAIL("too many initializers");
  while (n < want) { data_putc(0); n++; }

  // the name: a "string" in the data (that of \s itself, if it was one)
  if (!(is_lit(k) && const_val(k) == ia_start &&
        const_is_reloc[CONST_TOP - k] == 1))
    k = add_string_reloc(ia_start);
  return array_sym((word)(n / es), k);
}

// TODO: match_ident_decl and func_decl must be dduplicates....
// A: they only share read_ident + sym_find. \+ allocates one slot by
//   scope/register; \f allocates a 4-cell frame, a vector, a FUNC
//   symbol and the return-value variable, and sets main/pending entry.



// \f — declare function: new frame + vector (func_begin in ivm-gen.c)
static uchar match_ident_func_decl(void) {
  uchar vec;

  if (!read_ident()) return 0;
  vec = func_begin(id_buf);
  if (vec == 0xff) return 0;   // declared twice
  syms[nsym - 1].ret_slot = (uchar)last_sym;   // struct pt *f(): its struct

  #ifdef SYMS
    frame_sz[vec] = 4; // ret/val/prev/ACC at minimum
  #endif

  last_func_vec = vec;
  record_capture(frame_base[vec] + 1);

  vlog("  func '%s' slot %u vec %u frame %u\n", id_buf,
    (unsigned)(frame_base[vec] + 1), (unsigned)vec, (unsigned)frame_base[vec]);

  return 1;
}

// \p — declare the next parameter of the function just declared by \f
//   (param_add in ivm-gen.c: slot base+4+k, k from next_slot, no loops)
static uchar match_param_decl(void) {
  Symbol *s;

  if (!read_ident()) return 0;
  if (last_func_vec == 0xff) return 0;
  if (!(s = param_add(last_func_vec, id_buf))) return 0;
  set_decl_type(s);

  record_capture(MAKE_CAP(s->address, cur_type));

  vlog("  param '%s' #%u slot %u type %c%s\n",
       id_buf, (unsigned)(s->pidx - 1), (unsigned)s->address,
       (char)(CAP_TYPE(cur_type) >> 8), CAP_IS_PTR(cur_type) ? "*" : "");
  return 1;
}


// \q — the \f just declared is a struct, a prototype: its params are the
//   fields (k: param k). No code, no vector, no frame: given back. Its
//   name (the ret value variable) becomes a const, the field count (\@
//   \: its size, \$ sizeof). Leaves the scope, the params stay (\m)
static void struct_end(void) {
  uchar v = --nvector, b = frame_base[v], n = (uchar)(next_slot - b - 4);
  Symbol *s = syms + func_sym;

  s->type = SYM_STRUCT;                  // no calls
  ++s;
  s->nelem = n;
  s->address = const_slot(n);
  s->is_const = 1;
  s->scope = s[-1].scope;                // visible outside
  pending_func_entry = 0xff;
  next_slot = b;
  scope_leave();
}

// SoA: field k of array of structs a: its array at a + 2kN, a bss const
// (one per field, reused). Striped (vtype 'z'): that is its lo plane, the
// hi plane N after it: two consts made together (the a b ops read the hi
// one at the lo slot + 1, build_blob keeps it so)
static uchar soa_base(Symbol *a, uchar k) {
  uchar i, n = a->ret_addr_slot, z = a->vtype == 'z';
  word v = (word)(const_val(a->address) + 2 * k * n);
  uint16_t *c = const_pool;

  for (i = 0; i < nconst; i++, c++)
    if (c[0] == v && const_is_reloc[i] == 2 &&
        (uchar)(CONST_TOP - i) != bss_end_slot &&
        (!z || (i + 1 < nconst && c[1] == v + n && const_is_reloc[i + 1] == 2)))
      return (uchar)(CONST_TOP - i);
  i = add_const_raw(v, 2);
  if (z) add_const_raw(v + n, 2);
  return i;
}

// \m — member: .f or ->f, f a param of the struct (stub) of what \i or
//   \m matched last in this frame, else in the parent (a chain rule):
//   rec, its symbol, links (ret_slot) the struct's name. Searched as if
//   in its scope: the params right after it. Result = const k (the word
//   index for the ( ) ops); rec = f (a pointer field: the next ->). A
//   char field (type 'c'): 2k, the byte index of its cell's lo byte, for
//   the [ ] ops (a[i].f: the plane's first N bytes, the same address
//   striped or not: [ f i)
// \& — a[i].f, a an array of structs (SoA: f of all the N elements in a
//   row): the const a + 2 * k * N, f's array. Striped (vtype 'z', __striped
//   or -S: their N lo bytes, then their N hi bytes): its next slot is that
//   + N (index them with i: a b ops)
static uchar match_member(uchar soa) {
  Symbol *s, *a;
  uchar r = cur.rec, t, i;

  if (r == 0xff && sp) r = stack[sp - 1].rec;
  skip_ws();
  if (!got('.') && !(got('-') && got('>'))) return 0;
  if (!read_ident() || r >= nsym) return 0;
  a = syms + r;
  t = a->ret_slot;
  if (!t || t >= nsym || syms[t - 1].type == SYM_VAR) return 0;
  if (soa != (a->ret_addr_slot != 0xff)) return 0;         // a[i]. a.
  for (i = t + 1, s = syms + i; i < nsym && s->pidx; i++, s++)
    if (!strcmp(s->name, id_buf)) {
      cur.rec = i;
      t = s->vtype == 'c' && !s->is_ptr;     // a char field: [ ] bytes
      i = s->pidx - 1;
      record_capture(MAKE_CAP(soa ? soa_base(a, i) : const_slot(t ? 2 * i : i),
                              t ? TY_CHAR : TY_WORD));
      return 1;
    }
  return 0;
}


// ---------------------------------------------------------------------------
// String / char escapes — one decoder, shared by \S \s \c


// simple escapes: esc_in[i] decodes to esc_out[i]
static const char esc_in[]  = "ntrabfv";
static const char esc_out[] = "\n\t\r\a\b\f\v";

// After a leading '\\' has been consumed: decode one escape into *out.
// Advances line_pos. Returns 0 on failure (EOF / bad \x).
static uchar decode_escape(uchar *out) {
  uchar c, v, i;
  char *s;

  if (!nextc) return 0;
  c = nextc;
  skip();
  if (c == '0') {
    *out = 0;
  } else if (c == 'x' || c == 'X') {
    v = 0;

    // TODO: check if can use same as in match_digits?
    // A: could: match_num's digit conversion (toupper - '0' ...) works
    //   for hex too. Here it's limited to 2 digits; a shared digit(c)
    //   helper would also replace hexval(): a few bytes.

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
static uchar read_c_char(uchar *out) {
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
static uchar match_str(uchar emit_code) {
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
  record_capture(MAKE_CAP(slot, TY_CHAR_PTR));
  return 1;
}

static uchar match_num(void) {
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
      default:
        // 017: octal as in C (was decimal 17); 08 09: an error (the 8 is
        // left, nothing parses it)
        if (isdigit(nextc)) { base = 8; n = 0; }
      }

      if (base != 10 && n) {
        skip();
        n = 0;      // need at least one digit after the prefix
      }
    }

    // parse number (or cont of 0...)
    for (;;) {

      // TODO: negative!
      // A: leave it to the grammar: "-" then \d, emit 0 - x (or a
      //   neg op). Here "a -1" would read -1 and lose the minus.

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

    ty = (v < 256) ? TY_CHAR : TY_WORD;
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

// \X is a directive (stored with the hibit) or a quoted literal char e
// note: '>' is load-time only (not stored)
//       '<' becomes HIBIT('<') + ALT_JUMP + alt (1..31, 0: restart)
static uchar is_dir(uchar e) {
  return isdigit(e) || strchr("insfdprlxywmzkcahvgetoubjq+{}#='?*@$:,.!%&^()-/~;_", e) != NULL;
}

static void build_one_rule(uchar r, char *rhs) {
  char *p;
  uchar *blob, **altp, e, inprod, verbatim;
  word a, bi, na, alt_starts[MAX_ALTS];
  uchar taildest;   // 1-based alt index for \> / \< (0 = none yet)

  // directives (\i \n \s \o \< \d \+ \{ \}, rule calls, bare space, [ ]) get
  // the hibit
//
  // quoted \| \c (not specials) become the plain char
  //
  // \> is not stored; it only updates taildest
  // \< becomes HIBIT('<'), ALT_JUMP + x: jump to alt x-1, x = 0 if no \> yet
  // build contiguous string with 0 separators between alts.

  blob = xrealloc(NULL, strlen(rhs) + 2);

  free(blobs[r]);
  free(rule[r]);
  blobs[r] = blob;

  bi = 0;
  na = 0;
  inprod = 0;
  verbatim = 0;
  taildest = 0;
  alt_starts[na++] = 0;

  for (p = rhs; (e = *p); p++) {

    if (verbatim) {
      
      // Generic copy next "hex" bytes - rename
      // A: renamed to verbatim (only \# uses it so far)

      // \#xx: the two hex digits are data (may be A-F),
      // stored as they are
      verbatim--;

    } else if (e == '\\' && p[1]) {

      e = *++p;
      if (e == '>') {
        // \> : set jump target to current alt (1-based); do not store
        taildest = (uchar)na;   // na is already the current alt count
        continue;
      }

      if (e == '<') {

        // \< : HIBIT('<'), then the \> target alt (1..31), else 0: the
        //   whole rule again
        if (taildest > MAX_JUMP) {
          eprintf("\nrule %c: ", 'A' + r);
          FAIL("\\< to an alt past 31");
        }
        blob[bi++] = HIBIT('<');
        e = (uchar)(ALT_JUMP + taildest);

      } else {

        if (is_dir(e)) e = isalpha(e) ? HIDIR(e) : HIBIT(e);  // (a..z)
      }

    } else if (e == '|' && !inprod) {

      // '|' separates alts only outside a bare [ ... ] production
      // skip empty alts from || or line-join artifacts
      if (na > 0 && alt_starts[na - 1] == bi)
        continue;

      blob[bi++] = 0;

      if (na >= MAX_ALTS) {
        // TODO: don't repeat message
        // A: done: the rule letter here, the rest in the FAIL message
        eprintf("\nrule %c: ", 'A' + r);
        FAIL("too many alts (MAX_ALTS)");
      }
      alt_starts[na++] = bi;
      continue;

    } else if (e == '[') {

      // bare [ ] = production (no nesting); | inside is a literal (\[ \] are handled above)
      inprod = 1;
      e = HIBIT('[');

    } else if (e == ']' && inprod) {

      inprod = 0;
      e = HIBIT(']');

    } else {

      // rule calls $R $r and the ws-gap are directives, not in a production
      if (!inprod && e == '$' && isalpha(p[1])) e = HIBIT(*++p);
      else if (!inprod && e == ' ') e = HIBIT(e);

    }
    
    blob[bi++] = e;

    // \#xx \xHH => copy next two chars verbatim
    if (e == HIBIT('#') ||
        (e == HIDIR('x') && isxdigit(p[1]) && isxdigit(p[2]))) verbatim = 2;
    
  }

  blob[bi++] = 0;

  altp = xrealloc(NULL, sizeof(uchar *) * (na + 1));

  // create and fill an array of pointers to ALTs
  for (a = 0; a < na; a++)
    altp[a] = blob + alt_starts[a];

  // end with 0 -- TODO: just make all allocs clean/zero?
  // A: calloc would clear all of it for this one store: no gain

  // TODO: I mean if xrealloc always made new bytes allocated
  //   0 we wouldn't need to null things out later... like here!
  altp[na] = NULL;
  
  rule[r] = altp;
}


// ------------------------------------------------------------

// TODO: the idea was that several short alts wouldn't
//   be malloced separately, however these are big buffers?
//   do all ALTS need to fit inside a MAX_ACCUM?
//   why not just read and fill it fully?
//
// A: line and acc are only used while loading, then freed. acc collects
//   ONE rule: its first line plus all "|" continuation lines, so yes, all
//   alts of one rule must fit in MAX_ACCUM. build_one_rule then copies it
//   into one exact-size malloc. Reading lines straight into acc+acc_len
//   would save the 1 KB line buffer and the copy loop below.

#define MAX_LINE 1024
#define MAX_ACCUM 4096


static void load_bnf(const char *path) {
  FILE *f;
  char *line = xrealloc(NULL, MAX_LINE);
  char *acc = xrealloc(NULL, MAX_ACCUM);
  uchar cr;
  char *nl, *eq, *p, c;
  word acc_len, have_cur;
  
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

    // split a new line - use the rest
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

      if (!isalpha(c)) continue;

      eq = strchr(line, '=');
      if (!eq) continue;

      cr = c - 'A';
      have_cur = 1;
      p = eq + 1;
      acc_len = 0;
    }

    // TODO: why" it's just copying?
    // A: yes: append this line (or its part after "X=") to acc, because
    //   fgets reuses line for the next line
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


// ------------------------------------------------------------
// STACK

// Remember where the current alt starts (input, symbols, scope, slots,
// code), so a failing alt can be undone. Doesn't move alts/rip.
static void alt_mark(void) {
  cur.save_abs = abs_pos();
  cur.nsym_save = nsym;
  cur.scope_save = scope_depth;
  cur.slot_save = next_slot;
  cur.glob_save = next_glob;
  cur.reg_save = next_reg;
  cur.code_save = bin_ptr;
  cur.ginit_save = ginit_len;
  cur.rec_save = cur.rec;
  cur.loop_save = nloop;
  cur.brk_save = nbrk;
  cur.prod_n = 0;
}

// Switch to alt n of the current rule (0 = first) and snapshot undo.
// n == 255 means "step to the next alt" (after a failed one).
static void goto_alt(uchar n) {
  if (n == 255)
    cur.alts++;
  else
    cur.alts = rule[cur.rule] + n;
  cur.rip = 0;
  alt_mark();
}

static void push(uchar r) {
  // the STACK in the report is the furthest-position snapshot, not this
  // one: so say which rule is missing, and who called it
  if (!rule[r]) {
    eprintf("\nrule %c (called from %c): ", 'A' + r, sp ? 'A' + cur.rule : '-');
    FAIL("rule not defined");
  }
  if (sp >= MAX_STACK) FAIL("stack overflow");

  if (sp >= stack_cap) {
    uchar ncap = stack_cap + STACK_GROW;

    // TDOO: check that it minimizes the malloc() block?
    // A: it grows by STACK_GROW frames and never shrinks: the block is the
    //   deepest the parse got, rounded up to 8 frames
    
    if (ncap > MAX_STACK) ncap = MAX_STACK;
    stack = xrealloc(stack, ncap * sizeof(Frame));
    stack_cap = ncap;
  }

  // program main begin → blob.main

  // TDOO: what? code not clear
  // A: Pascal: "program x; ... begin ... end." The main block B is called
  //   straight from rule P, so its start is main's entry (main_offset).
  //   C doesn't get here: its main is a \f function (main_vec).

  if (r == 'B' - 'A' && sp > 0 && cur.rule == 'P' - 'A')
    main_offset = bin_ptr;

  // suspend active frame (memcpy via struct assignment), open a new cur
  stack[sp] = cur;
  sp++;
  anim_push(r);

  cur.rule = r;
  cur.func_vec = 0xff;
  cur.rec = 0xff;
  cur.arg_i = 0;
  cur.npatch = 0;
  cur.mark = 0;
  cur.result = 0;

  memset(cur.prod_cap, 0, sizeof(cur.prod_cap));

  // TODO: init (?) why ehere?
  // A: the new frame starts at its first alt, and goto_alt() also
  //   snapshots the undo info (alt_mark) for that alt
  goto_alt(0);
}

// ------------------------------------------------------------
 
static void restore_scope(void) {
  sym_truncate(cur.nsym_save);
  scope_depth = cur.scope_save;
  next_slot = cur.slot_save;
  next_glob = cur.glob_save;
  next_reg = cur.reg_save;
}

// frame_rewind: Undo the current alt before trying the next one.
// drop its code, captures and patches,
// rewind the input to where the alt started.

// Back to alt start, codegen undo:
// - input only if not committed away.
// Returns 0 if save_abs < line_abs (\o dropped that text)
// — caller fail-fasts.
static int frame_rewind(void) {
  bin_ptr = cur.code_save;
  if (pl_end > bin_ptr) pl_end = 0xffff;     // that load is gone
  if (ginit_len != cur.ginit_save) {
    ginit_len = cur.ginit_save;
    ginit_acc = 0xff;   // ACC after the kept init code: unknown
  }
  cur.prod_n = 0;
  cur.npatch = 0;
  cur.rec = cur.rec_save;
  nloop = cur.loop_save;
  nbrk = cur.brk_save;
  if (cur.rule == 'V' - 'A') static_decl = 0;   // (a static V failed)

  // Function entry code (and main's global inits) is emitted lazily,
  // by the first emit after \f (pending_func_entry). If this rewind
  // threw away that code, ask for it again.
  
  // re-arm function entry if rewind ate gen_func_entry / ginit bytes.
  // main: only if its global init code is gone (before main_offset +
  // ginit_len), not at main_offset itself: \j can take bin_ptr back to it
  // for a moment (main's first load cut), an outer rewind then restores
  // that load; armed, the next emit made main start after it ("x = 7 * 4"
  // as main's first statement gave 0)
  if (last_func_vec != 0xff) {
    if (last_func_vec < MAX_VECTORS && vectors[last_func_vec] > bin_ptr)
      pending_func_entry = last_func_vec;
    else if (last_func_vec == main_vec && bin_ptr < main_offset + ginit_len)
      pending_func_entry = last_func_vec;
  }

  if (cur.save_abs < line_abs)
    return 0;

  line_pos = (word)(cur.save_abs - line_abs);
  load_nextc();
  return 1;
}

// ------------------------------------------------------------
// ONE STEP

// estimated ~50-200us per step on 6502 @ 1MHz (dispatch + match)

// One unit of work.
// Flow: rule char vs source char → match / ws-case / hibit dispatch;
// end of alt → accept (pop); mismatch → next alt; no alts → fail (pop).
// State is only "where we are" (rule, ap, ip).
//
// Fail / pop lives only at fail_alt — normal path just follows *ap and *src.
//
// Returns 1 if more to do, 0 if parse finished (stack empty).

// TODO: is there a simple way to make this work, without a
//   wrapping function call?

//int run(uchar steps) {

int run() {
  uchar *alt, *a;
  uchar c, e, rch, ok;
  word adv;

  if (sp == 0) return 0;

  alt = *cur.alts;
  a = alt + cur.rip;
  c = *a;

  // TODO: why isn't this fixed up at the end before exit
  //   Write the code to ALWAYS leave it in  clear state!
  //   so we can drop into LITERAL directly!
  // A: the 0 is the alt's end marker, not leftover state: reaching it IS
  //   "alt matched". It has to be tested first, because a 0 would
  //   otherwise go into the literal path as a char to match.

  // TODO: I'm SAYING that it looks messy - at lest,
  //   it's more like an ACCEPT (which stops this rule fromm backtrack).
  //   maybe name it such and make it a function with clear purpose!

  // end of alt → accept (pop), parent advances past the call
  if (!c) {
    rch = 'A' + cur.rule;
    if (rch == 'K') check_consts(cur.code_save);

    // a global declaration made code: nothing runs it ("int g = 2 + 3;":
    // only consts, \g, at the top)
    if (rch == 'V' && (!scope_depth || static_decl) &&
        bin_ptr > cur.code_save) {
      bin_ptr = cur.code_save;
      err_here("global / static initializer: a constant only");
    }
    if (rch == 'V') static_decl = 0;
    vlog("ok %c alt %u -> @%u\n", rch, (unsigned)(cur.alts - rule[cur.rule]), abs_pos());

    #ifdef VERBOSE
      if (verbose >= 2 && bin_ptr > cur.code_save && rch == 'K') {
        eprintf(NORMAL "GEN:          [ ");
        ivm_print_code_range(ERROUT, bin, cur.code_save, bin_ptr, 0);
        eprintf("]\n");
      }
    #endif

    // ???
    // return to the caller: its frame comes back from the stack, the
    // child's result becomes the caller's next capture, and the caller
    // steps over its rule-call byte
    {
      word child_res = cur.result;
      anim_pop();
      if (sp == 0) return 0;
      sp--;
      cur = stack[sp];
      record_capture(child_res);
      cur.rip++;
    }
    return 1;
  }

  // --- literals (no hibit): rule char vs source ---

  // LITERAL
  if (!(c & 0x80)) {

    // rsa long as matching keep moving forward!
    for (;;) {
      if (nextc != c) {
        if (c != ' ' && c != '\t') skip_ws();
        if (nextc != c &&
            !(ignore_case && isalpha(c) && tolower(nextc) == tolower(c))) {
          vlog("  fail lit '%c' @%u got '%c'\n", c, abs_pos(),
               nextc ? nextc : '?');
          goto fail_alt;
        }
      }

      skip();
      note_progress();

      vlog("  ok lit '%c' @%u\n", c, abs_pos());
      c = *++a;
      if (!c || (c & 0x80)) break;

    }

    cur.rip = (word)(a - alt);
    return 1;
  }

  // --- hibit: space gap, production, rule call, escapes ---
  if (c == HIBIT(' ')) {
    skip_ws();
    note_progress();
    cur.rip++;
    return 1;
  }

  // RULE
  if (IS_RULE(c)) {
    uchar r = (c & 0x7f) - 'A';
    vlog("  call %c from %c @%u\n", c & 0x7f, 'A' + cur.rule, abs_pos());
    push(r);
    return 1;
  }

  // GENERATOR
  if (c == HIBIT('[')) {
    cur.rip = (word)(run_production(a + 1) - alt);
    return 1;
  }

  ok = 1;
  adv = 1;

  // TAIL-RECURSE rule ( go back beginning or previoius \>)
  //   HIBIT('<'), ALT_JUMP + n: alt n-1 (0: the first)
  if (c == HIBIT('<')) {
    e = a[1] > ALT_JUMP ? (uchar)(a[1] - ALT_JUMP - 1) : 0;
    goto_alt(e);
    vlog("  tail>%u %c @%u\n", (unsigned)e, 'A' + cur.rule, abs_pos());
    return 1;
  }

  // COMMIT (remove src not needed anymore)
       if (c == HIDIR('o')) line_commit();
  else if (c == HIBIT('{')) scope_enter();
  else if (c == HIBIT('}')) scope_leave();

  // \xHH: match the byte HH exactly (no ws skipped)
  else if (c == HIDIR('x') && isxdigit(a[1]) && isxdigit(a[2])) {
    ok = got(hex2(a + 1));
    adv = 3;
  }

  // \@: the ident just declared (\+, capture 1) is a static array of
  //   capture 2 (a number or const) elements: its slot goes back, its name
  //   is a const slot (the address, bss), a pointer of the declared type
  else if (c == HIBIT('@')) ok = make_array();

  // \: \, \.: an initialized array: start, an element, end
  else if (c == HIBIT(':')) ok = init_start();
  else if (c == HIBIT(',')) ok = init_elem();
  else if (c == HIBIT('.')) ok = init_end();

  // \$: the ident \i matched is a static array: the result is its size in
  //   bytes (sizeof), else fail
  else if (c == HIBIT('$')) {
    Symbol *sy = last_sym < nsym ? &syms[last_sym] : 0;
    ok = sy ? sy->nelem != 0 : 0;
    if (ok)
      record_capture(const_slot(sy->vtype == 'c' ? sy->nelem : sy->nelem * 2));
  }

  // SET # RETURN VALUE OF RULE
  else if (c == HIBIT('#')) {
    if (!a[1] || !a[2]) FAIL("bad \\# in grammar");
    record_capture(const_slot(hex2(a + 1)));
    adv = 3;
  }

  // SET other result than LAST
  else if (c == HIBIT('=')) {
    e = a[1] - '0';
    if (e >= 1 && e <= cur.prod_n)
      cur.result = cur.prod_cap[e];
    adv = 2;
  }

  // RESULT PICK
  else if (c >= HIBIT('0') && c <= HIBIT('9'))
    ok = match_ident_same(c - HIBIT('0'));

  // TYPE setting
  else if (c == HIBIT('\'')) {
    // \'X: set type high-byte of result (and cur_type) to X
    // \'^: uppercase current type (make it a pointer)
    if (!a[1]) FAIL("bad \\' in grammar");
    {
      uchar t = a[1];
      if (t == '^') t = (uchar)toupper((uchar)(cur.result >> 8));
      cur.result = (cur.result & 0xff) | ((word)t << 8);
      cur_type = (word)t << 8;
      last_sym = 0xffff;       // no struct: "int i" is not linked to one
                               // (struct\'W \i: \i links it again)
    }
    adv = 2;
  }

  // GUARD - ENSURE TYPE
  else if (c == HIBIT('?')) {
    // \?X: fail unless result type high-byte is X
    if (!a[1] || (cur.result >> 8) != a[1]) ok = 0;
    adv = 2;
  }

  // \_: a static declaration (rule V): as a global (\+ \g, V's end),
  //   the name local
  else if (c == HIBIT('_')) static_decl = 1;

  // \;c: fail if the next char is c, read nothing (& but not &&)
  else if (c == HIBIT(';')) {
    if (a[1] && nextc == a[1]) ok = 0;
    adv = 2;
  }

  else if (c == HIDIR('i')) ok = match_ident();
  else if (c == HIBIT('+')) ok = match_ident_decl();
  else if (c == HIDIR('f')) ok = match_ident_func_decl();
  else if (c == HIDIR('p')) ok = match_param_decl();
  else if (c == HIDIR('q')) struct_end();
  else if (c == HIDIR('m')) ok = match_member(0);
  else if (c == HIBIT('&')) ok = match_member(1);
  else if (c == HIBIT('^')) any_name = 1;
  else if (c == HIDIR('r')) {
    // end of params: params_end() (ivm-gen.c) moves the last to fp+3
    if (last_func_vec != 0xff) {
      params_end(last_func_vec);
      vlog("  params done vec %u n=%u\n", (unsigned)last_func_vec,
        (unsigned)frame_nparams[last_func_vec]);
    } else
      pending_register = 1;
  }
  // REGISTER flag SET ("register\b"): \r does it too, but only when
  // last_func_vec happens to be 0xff (see above)
  else if (c == HIDIR('b')) pending_register = 1;

  // CONSTANT flag SET
  else if (c == HIDIR('c')) {
    pending_const = 1;
    ok = 1;
  }

  // NUMBER
  else if (c == HIDIR('d')) ok = match_num();

  // NEWLINE
  else if (c == HIDIR('n')) ok = match_newline();

  // \!: a statement that does not parse (not a } / the end: no statement)
  else if (c == HIBIT('!')) {
    word s0 = abs_pos();                  // (an undefined name: from here)
    skip_ws();
    if ((ok = nextc && nextc != '}')) {
      err_statement(s0);
      err_skip();
    }
  }

  // \%: a false #if branch: skipped
  else if (c == HIBIT('%')) pp_skip();

  // \*c: skip input up to and including the next c (Kleene ".*c")
  //   \*\n: to end of line   \*>: through '>'   (\*\[ \*\| for [ |)
  else if (c == HIBIT('*')) {
    uchar stop = a[1] == HIDIR('n') ? '\n' : (uchar)(a[1] & 0x7f);
    if (!a[1]) FAIL("bad \\* in grammar");
    while (nextc && nextc != stop) skip();
    ok = got(stop);
    note_progress();
    adv = 2;
  }

  // "STRING" value
  else if (c == HIDIR('s')) ok = match_str(0);

  // "INLINE STRING\0
  else if (c == HIDIR('u')) ok = match_str(1);



  // OK
  // TODO: why not retun inside these rules instead?
  // A: then each would need its own "cur.rip += adv; return 1" or
  //   "goto fail_alt": shared here once is smaller

  // TODO: Since falling out of a if branch is a jump, we know 
  //   we're OK, so not need the flag, just goto OK: ???

  if (ok) {
    vlog("  ok \\%c @%u\n", c & 0x7f, abs_pos());
    cur.rip += adv;
    return 1;
  }

  // TODO: Funny how you make FAIL the default and OK the exception...

  // TODO: maybe fail earlier and just jump???
  // A: the failing ones only set ok = 0 and fall to here, which is the
  //   jump. The "if (!ok)" was redundant (ok is 0 here) and is gone.
  
  vlog("  fail \\%c @%u\n", c & 0x7f, abs_pos());

fail_alt:
  // next alt, or pop if none left / mark committed away
  if (frame_rewind()) {
    restore_scope();
    if (cur.alts[1]) {
      goto_alt(255);   // step to next alt and snapshot
      return 1;
    }
  } else {
    while (*cur.alts)
      cur.alts++;
  }

  // rule failed — pop to parent and fail that element's alt
  vlog("fail %c (no alt) @%u\n", 'A' + cur.rule, abs_pos());
  anim_pop();
  if (sp == 0 || !stack[sp - 1].alts) return 0;   // the root rule failed
  sp--;
  cur = stack[sp];
  goto fail_alt;
}



// ------------------------------------------------------------
// ERROR SUPPORT


// "file:line: " prefix of an error line
static void err_prefix(word line) {
  eprintf("%s:%u: ", src_name ? src_name : "?", line + 1); // line is 0-based
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

// \!: the statement at the current position does not parse. Its line, a
// caret where parsing got furthest in it (if on that line). Once per
// statement: backtracking comes back here (err_at). s0: before its white
// space (an undefined name in it: noted from there)
// the line of position pos (in line_buf): its number, *start: where it
// starts in line_buf
static word err_line(word pos, word *start) {
  word i, line = line_no_base;

  *start = 0;
  for (i = 0; i < pos; i++)
    if (line_buf[i] == '\n') { line++; *start = (word)(i + 1); }
  return line;
}

static void err_statement(word s0) {
  word at = abs_pos(), i, start, line, col;

  // once
  if (at < err_at) return;
  err_at = (word)(at + 1);
  nerr++;

  // its line in the buffer
  line = err_line(line_pos, &start);
  err_prefix(line);
  eprintf("error: cannot parse");
  if (undef_kind && undef_pos_abs >= s0)
    eprintf(" (%s not defined: %s)", undef_kind == 2 ? "function" :
      "identifier", last_undef);
  eputc('\n');
  show_line(line, start);

  // caret: the furthest point, if still on this line
  col = (word)(max_pos_abs - line_abs);
  for (i = start; i < col && i < line_len && line_buf[i] != '\n'; i++) ;
  if (i == col) {
    err_prefix(line);
    for (i = start; i < col; i++) eputc(line_buf[i] == '\t' ? '\t' : ' ');
    eprintf("^\n");
  }
}

// A statement (K) accepted: no op of its code [from, bin_ptr[ may write a
// const slot: a #define / const, an array's or a static struct's address
// ("a = b"). By the slot, not the symbol: also what the grammar emits
// itself ([S\1]), and the locals' symbols are gone at the end (a const
// slot has many names: none printed, the line is). Each write once
// (err_code: a block's K sees its statements' code again)
static word err_code;

// an error at the end of what was just accepted: its line (its last
// token's, not after the gap: "$K [n..]"), msg
static void err_here(const char *msg) {
  word start, line, at = abs_pos();

  if (at == ws_end) at = ws_at;
  at = at > line_abs ? (word)(at - line_abs) : 0;
  nerr++;
  line = err_line(at, &start);
  err_prefix(line);
  eprintf("error: %s\n", msg);
  show_line(line, start);
}

static void check_consts(word from) {
  word n;
  uchar *p;

  for (; from < (word)bin_ptr; from += n) {
    p = bin + from;
    if (!(n = ivm_oplen(p))) return;
    if (n < 2 || !WRITES_VAR(*p) || !is_lit(p[1]) || from < err_code)
      continue;
    err_code = (word)(from + 1);
    err_here("const modified");
  }
}

// \!: skip the statement after its error: to its ; or over a { } block,
// not over the } of the block it is in. Strings, chars, comments: not
// looked into. After a block: an "else" goes on, a ; on the same line
// ends it too (struct { } x;  do { } while (x);)
static void err_skip(void) {
  uchar depth = 0, q;
  word save;

  for (;;) {
    skip_ws();
    if (!nextc) return;

    // "..." '...': to the closing one (\ escapes)
    if (nextc == '"' || nextc == '\'') {
      q = nextc;
      skip();
      while (nextc && nextc != q) {
        if (nextc == '\\') skip();
        skip();
      }
      skip();
      continue;
    }

    // ; ends it, a { } block too (the rest of its line: up to a ;)
    if (nextc == ';' && !depth) { skip(); return; }
    if (nextc == '{') depth++;
    if (nextc == '}') {
      if (!depth) return;                       // the enclosing block's
      skip();
      if (--depth) continue;
      skip_hspace();
      if (match_at("else", 4)) continue;
      save = line_pos;
      while (nextc && nextc != '\n' && nextc != ';' && nextc != '{' &&
             nextc != '}')
        skip();
      if (nextc == ';') skip();
      else { line_pos = save; load_nextc(); }
      return;
    }
    skip();
  }
}

// \%: skip the lines of a false #if branch (at a line start): up to its
// #else (the rest is parsed) or #endif, both consumed; a nested #if..
// #endif inside: skipped whole
static void pp_skip(void) {
  uchar nest = 0, done = 0;

  while (nextc && !done) {
    // a # line: #if.. opens, #else / #endif ends ours
    skip_hspace();
    if (nextc == '#') {
      skip();
      skip_hspace();
      if (match_at("if", 2)) nest++;
      else if (match_at("endif", 5)) { if (nest) nest--; else done = 1; }
      else if (!nest && match_at("else", 4)) done = 1;
    }

    // the rest of the line
    while (nextc && nextc != '\n') skip();
    if (nextc) skip();
  }
}

static void report_reject(void) {
  word i;

  eprintf("\n--- REJECT at %u (furthest %u) ---\n",
          abs_pos(), max_pos_abs);

  eprintf("STACK:");
  if (far_sp < 2) eprintf(" (empty)");

  // [0] is the empty frame init() pushes P over: not a rule (the "A80")
  for (i = 1; i < far_sp; i++)
    eprintf(" %c%u", far_stack[i], (unsigned)far_alt[i]);

  eprintf("\n");

  if (far_sp > 0) {
    uchar r = far_stack[far_sp - 1];
    uchar a = far_alt[far_sp - 1];

    eprintf("EXPECTED: rule %c", r);
    r -= 'A';
    {
      uchar **alts = rule[r], k;
      for (k = 0; alts && k < a && alts[k]; k++)
        ;  // alts[] ends at NULL: a may be stale
      if (alts && k == a && alts[a])
      { eprintf(" alt %u \"", a); show_alt(alts[a]); eputc('"'); }
    }

    eprintf("\n");
  }

  // TODO: name the error messages and assign them?
  //   regarding last_undef maybe just always print it as context?
  if (undef_kind == 2)
    eprintf("ERROR: undefined function: %s\n", last_undef);
  else if (undef_kind)
    eprintf("ERROR: identifier \"%s\" not defined here\n", last_undef);

  // Show error from line_buf only — no rewind/fseek (cc65-friendly).
  // line_no_base tracks the line number of line_buf[0] across \C commits.
  eprintf("REJECT:\n");
  {
    word want = (undef_kind && undef_pos_abs) ? undef_pos_abs : max_pos_abs;
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

      word k;

      // one more refill, in case the buffer ends at the error
      line_pos = line_len;
      load_nextc();

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

      // up to ERR_LINES_BEFORE lines before it, as far as the buffer goes
      for (i = start, k = 0; k < ERR_LINES_BEFORE && i; k++)
        for (i--; i && line_buf[i - 1] != '\n'; i--)
          ;
      for (; k; k--)
        i = show_line(line - k, i);

      // error line text
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


// TODO: a line like this is 22 bytes!
// A: FAIL is a function now: fail(msg) instead of an inline longjmp,
//   so each FAIL is about the message pointer plus one jsr.

  if (!src_fp) FAIL("cannot open source");

  // & of a variable anywhere (&x, not && &=): stores through pointers may
  // hit mem[], value numbering then keeps no slot over them (vn_alias).
  // The whole source first: the & may come after the code it affects
  {
    int c, last = 0;
    uchar amp = 0;                         // after a lone & (and spaces)
    while ((c = fgetc(src_fp)) != EOF) {
      if (c == '&') amp = last != '&';      // && : no
      else if (amp && (isalpha(c) || c == '_')) vn_alias = 1;
      else if (c != ' ') amp = 0;           // &= &( &1 ...
      last = c;
    }
    rewind(src_fp);
  }

  fail_file = NULL;


// TODO: do a memset then set needd values?
//   if theses were on static struct, just bzero!
// A: right: statics start at 0 and init() runs once per program run, so
//   every "= 0" below is only needed if init() is ever called again. Put
//   the line/parse state in one static struct: then one memset.

// TODO: do it! That state should have been one struct to start with
//   then rewind is just memmove or struct assign!

  line_buf = NULL;
  line_no_base = 0;
  line_cap = 0;
  line_len = 0;
  line_pos = 0;
  line_abs = 0;

  load_nextc();     // prime nextc (reads the first source char)
  max_pos_abs = 0;
  far_sp = 0;
  stack = NULL;
  stack_cap = 0;
  sp = 0;

  memset(&cur, 0, sizeof(cur));

  undef_kind = 0;
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
  push('P' - 'A');
}

static void shutdown(void) {

  if (src_fp) fclose(src_fp);
  src_fp = NULL;

  #if 0
    printf("\nLINEBUF: no_base=%u len=%u cap=%u pos=%u abs=%u\n",
      line_no_base, line_len, line_cap, line_pos, line_abs);
    // shows garbage after valid
    printf("\n>>>>>LINEBUF>>>>\n%s\n<<<<<\n", line_buf);
  #endif

  free(line_buf);
  line_buf = NULL;


  free_rules();

  free(stack);
  stack = NULL;
  stack_cap = 0;

  sym_free();
}


/// ------------------------------------------------------------
int main(int argc, char **argv) {
  #ifdef VERBOSE
  word a;
  #endif
  char *bnf = NULL;
  char *srcpath = NULL;
  char *arg;
  word i;
  int err;
  int blen;
  uchar *blob;
  
  verbose = 0;
  ignore_case = 1;
  optimize = 0;
#ifdef RUN
  compile_only = 0;
#endif

  // parse arguments
  for (i = 1; i < argc; i++) {
    arg = argv[i];

    if (arg[0] == '-' && arg[1] && !arg[2])
      switch (arg[1]) {
      case 'v': verbose++;       continue;
      case 'O': optimize = 1;    continue;
      case 'S': stripe = 1;      continue;
      case 'i': ignore_case = 1; continue; // LOL
#ifdef RUN
      case 'c': compile_only = 1; continue;  // just the .ivm, don't run it
#endif
      default: ;// TODO: give error
      }

    // Fill in args in order (default)
         if (!bnf)                   bnf = arg;
    else if (!srcpath)               srcpath = arg;
  }

  if (!bnf || !srcpath) {
    eprintf("usage: %s [-v ...] [-O] [-S] [-i] [-c] c.bnf src.c\n", argv[0]);
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
    // TODO: move to extra module
    // A: it's VERBOSE only (no bytes in the cc65 build): fine either way
    //
    // print rules
    if (verbose >= 2) {
   
      eprintf("--- rules ---\n");
      for (i = 0; i < NRULES; i++)
  
        if (rule[i]) {
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

  // run until done
  while (run())
    ;

  // TODO: just to be sure? lol
  // A: not needed for C: main's X returns into the _start trampoline's q.
  //   Kept: a Pascal program body (no FUNC, no X) probably relies on it to
  //   stop. One byte, and it never runs for C.
  emit('q');

  anim_report();

  // OK
  skip_ws();
  if (nextc) FAIL("trailing input");

  // statements \! skipped: no .ivm
  if (nerr) {
    eprintf("--- %u error%s ---\n", nerr, nerr > 1 ? "s" : "");
    shutdown();
    return 1;
  }

  eprintf("--- ACCEPT ---\n");

#ifdef VERBOSE
  eprintf("--- BYTECODE (%u bytes) ---\n", (unsigned)bin_ptr);

  ivm_print_code(ERROUT, bin, bin_ptr);
  eputc('\n');

  eprintf("--- HEXDUMP (%u bytes) ---\n", (unsigned)bin_ptr);
  // dump hex
  for (i = 0; i < bin_ptr; i++) {
    eprintf("%02x", bin[i]);
    if ((i & 15) == 15) eputc('\n');
    else eputc(' ');
  }
  if (bin_ptr & 15) eputc('\n');
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
        FILE *of;

        // find base and last '.' position
        // TODO: strrchr ???
        // A: strrchr needs two calls ('/' and '\\') plus one for '.', and the
        //   copy is still needed: the loop in out_name() is as small. Shared
        //   with sym_open() now.
        // change name test.p => test.ivm
        change_ext(out, src_name ? src_name : "out", ".ivm");
        
        of = fopen(out, "wb");
        if (!of) { eprintf("--- SAVE file fail: %s ---\n", out); exit(1); }

        fwrite(blob, 1, (size_t)blen, of);

        fclose(of);

        eprintf("--- SAVED %s (%d bytes) ---\n", out, blen);
        #ifdef SYMS
          if (sym_fp) {
            sym_write_all();
            eprintf("--- SAVED .sym ---\n");
          }
        #endif
      }

      #ifdef RUN
      if (!compile_only) {
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
