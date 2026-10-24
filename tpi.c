// tpi.c - Tiny Pascal → IVM bytecode
//
// Notice:
//  1) needs to follow c89 so can compie on cc65
//  2) only define varaibles at start of { scope
//  3) use // comment - do NOT: // no no no
//  4) don't change any lines if not needed
//  5) char is unsigned on cc65 & oscar64 & clang
//  6) sizeof needs sizeof(xxx) on oscar64

// Target model (as specified):
//
// a) Globals allocated preferentially in slots 0-31
// b) Function frames (ret-addr, ret-val, params, locals) from 32 upward
// c) When 0-31 are full, globals may continue into the frame area (skip used)
// d) Vectors kept in a separate array during compilation
// e) Constants in their own array; referenced as 0x80+n in the code stream
// f) Patch list of program offsets that need final fix-ups
// g) Vectorised calls use opcodes 192-255 (0xC0-0xFF)


// asm: Test program generates 574 bytes
// ivm: Test program generates 167 bytes
// asm: 1000: (/ (- 1418227 1290832) 1000) =  127c/loop

// ivm: 1000: (/ (- 9734015 3970047) 1000) = 5763c/loop
// ivm/asm = (/ 5763 127) = 45x slower, lol            

// ivm: 1000: (/ (- 6142119 3989127) 1000) = 2152 start
// ivm/asm = (/ 2152 127.0) = 16.95x slower, but improved 3x!

// - moving a print out... lolshoulnd' ffect any 
// ivm: 1000: (/ (- 6150038 3985079) 1000) = 2164 start

// 'g' 'b' no sublevel
// ivm: 1000: (/ (- 5899169 3976177) 1000) = 1922 start
// ivm/asm = (/ 1922 127.0) = 15.13x slower, but improved 3x!

// ivm/asm = (/ 1922 127.0) = 15.13x slower, but improved 1%
 
// ivm: 1000: (/ (- 5593710 3707529) 1000) = 1886 start
// ivm/asm = (/ 1886 127.0) = 14.85x slower, but improved 0.5%

// - fixing return byte wasted (save one byte per function call)
// ivm: 1000: (/ (- 5273425 3384263) 1000) = 1889 start
// ivm/asm = (/ 1889 127.0) = 14.87x slower, slightly slower

// - better single byte return, and mini stack, lol  => 239 => 225
// ivm: 1000: (/ (- 5201162 3311988) 1000) = 1889 start
//  => same speed

// - optimize operator apply
// ivm: 1000: (/ (- 4791507 3247259) 1000) = 1544 start
// ivm/asm = (/ 1544 127.0) = 12.16x ... 18% faster!
//  => same speed => 225 => 211 bytesp (/ 211 239.0) ... 12% smaller

// - optimize operator apply
// ivm: 1000: (/ (- 4782032 3245844) 1000) = 1536 start
// ivm/asm = (/ 1536 127.0) = 12.09x ... (/ 12.09 12.16) 0.5 faster
// code (- 14800 14705) = 95 bytes saved

// - "poophole" (not peep as this is at the "end") return optimization
// ivm: 1000: (/ (- 8418438 8140443) 1000) = 277 c less
// (/ 8418438 8140443.0) => 3.4 % faster function return
// 339 bytes => 333 bytes, saved 6 bytes! 

// - function 2 parameter overhead:
// ivm: 1000: (/ (- 8140443 6778897) 1000) = 1361 cycles/call
// 9 ops instead of 3, so 6 => (/ 1361 6) = 226 cycle/op

// - last parameter in ACC when call function
// ivm: 1000: (/ (- 6789427 5236649) 1000) = 1552 cycles / loop
// savee: (- 333 317) = 16 bytes! (2 bytes per >=2 calls same fun!)

// - Reverses communitative operands (+) if better code
// ivm: 1000: (/ (- 8086719 5182360) 1000) = 2904 cycles / loop
// savee: (- 317 309) = 8 bytes 
// (/ (- 8156974 8086719) 1000) saves 70c / Two-par-function call

// - "Poophole" optimization: sum:= sum+i; sum:= i+sum;
// "Iss Jss Ussdd": ops modified, 9 bytes saved
// (/ (- 6590838 5178079) 1000) = 1412 with 'U'
// (/ (- 6561061 5138975) 1000) = 1422 no 'U' opt!!! ?????
// (/ (- 6568864 5194953) 1000) = 1373 'Ussdd' 300 bytes!
// (/ 1373 127.0)  = 10.81x 300 bytes from 303.
// (/ 10.81 12.09) = 11% faster sum- loop
// 6 ops / loop with new Iss and Ussdd
// (/ 1373 6) = 228c / op

// -- cc65 1516 bytes
// (/ (- 6568864 5194953) 1000) = 1373 ksum
// (/ (- 7720424 5213578) 1000) = 2506 fsum

// -- oscar 15K (no explicit opt) (-O3: 18K nearly, but 20% faster)
// (/ (- 4294849 3357946) 1000) =  936 sum
// (/ (- 5466849 3357946) 1000) = 2108 fsum

// -- oscar  -Os => 14386 bytes
// (/ (- 3880227 2927271) 1000)  952 sum
// (/ (- 5066227 2927271) 1000) 2138 fsum

// - "better smaller/cleaner compiler" - but SLOWER!
// - VERY SLOW???
// (/ (- 10586159 9111306) 1000) -> 1474 sum  from 1373
// (/ (- 11638157 9111306) 1000) -> 2526 fsum from 2506

// - oscar -Os 13081 bytes - FASTER fsum
// (/ (- 3647717 2694535) 1000)  953 sum  from  952
// (/ (- 4692714 2694535) 1000) 1998 fsum from 2138

// - compile to file, loader/Nevm/run_ivm
// (/ (- 2570914 752081) 1000) 1818c/loop sum, was 1474 from 1373
// (/ (- 4844460 752081) 1000) 4092c/loop fsum, was 2526 from 2506


#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdint.h>

typedef unsigned char uchar; // plain char is unsigned on cc65/oscar64/clang*-unsigned
typedef uint16_t word; // blob / vector cell width (6502)

void note_load(uchar addr);
void note_store(uchar addr);



extern word slot_refs[]; // def near slot_used
#include <assert.h>

// --- options
#define TPDUMP

// --- config
#define MAX_SRC_LEN   8192
#define MAX_BIN_LEN   2048

#define MAX_PATCHES    128

// TODO: this is used for strings too? do NOT!

#define MAX_SYM_LEN     64 // TODO: static do strdup
#define MAX_SYMBOLS     64

// TOOO: this should not be hardcoded, just walk n items in table?
#define MAX_PARAMS       4 // max arguments per function

// --- limits
#define MAX_SLOTS      255 // Last one not used, is error
#define MAX_CONSTS      64

#define MAX_VECTORS     64 // # subroutines: opcodes 0xC0 .. 0xFF

// TODO: "REGISTER" prefix" or special loop-index variables?
#define GLOBAL_LIMIT    96 // globals prefer low slots
#define FRAME_BASE      96 // frames before pack; -O may lower into 0-31 band
#define CONST_TOP      253 // consts grow down 253,252,..; IMM0/1 use 254/255

// ---------------------------------------------------------------------------
// Tokens
// ---------------------------------------------------------------------------
// Token type is char:
//   single-char tokens are that char (';', '+', '('…) — compare with ';')
//   keywords use 'A'..'Z' codes
//   specials 0..5 for EOF/IDENT/NUM/…
// ---------------------------------------------------------------------------
typedef char TokenType;

#define TOK_EOF      ((TokenType)0)
#define TOK_ERROR    ((TokenType)1)
#define TOK_IDENT    ((TokenType)2)
#define TOK_NUM      ((TokenType)3)
#define TOK_STRING   ((TokenType)4)
#define TOK_ASSIGN   ((TokenType)5) // :=

// keywords — letters, unique
// free: A C H J M O QRS U XYZ
#define TOK_PROGRAM  ((TokenType)'P')
#define TOK_VAR      ((TokenType)'V')
#define TOK_BEGIN    ((TokenType)'B')
#define TOK_END      ((TokenType)'E')
#define TOK_INTEGER  ((TokenType)'N')
#define TOK_WRITE    ((TokenType)'w') // WRITE  (no newline)
#define TOK_WRITELN  ((TokenType)'W') // WRITELN
#define TOK_FUNCTION ((TokenType)'F')

#define TOK_IF       ((TokenType)'I')
#define TOK_THEN     ((TokenType)'T')
#define TOK_ELSE     ((TokenType)'L')

#define TOK_FOR      ((TokenType)'O') // F taken by FUNCTION
#define TOK_TO       ((TokenType)'G')
#define TOK_DO       ((TokenType)'D')

typedef struct {
  const char *name;
  const TokenType type;
} Keyword;

// Real keywords + "combi" names for specials (printer only).
// Names starting with * never match lexer (non-alpha won't enter IDENT).
Keyword keywords[] = {
  {"PROGRAM",  TOK_PROGRAM},
  {"VAR",      TOK_VAR},
  {"BEGIN",    TOK_BEGIN},
  {"END",      TOK_END},
  {"INTEGER",  TOK_INTEGER},
  {"WRITE",    TOK_WRITE},
  {"WRITELN",  TOK_WRITELN},
  {"FUNCTION", TOK_FUNCTION},

  {"IF",       TOK_IF},
  {"THEN",     TOK_THEN},
  {"ELSE",     TOK_ELSE},
  
  {"FOR",      TOK_FOR},
  {"TO",       TOK_TO},
  {"DO",       TOK_DO},

  {"NOT",      (TokenType)'?'},  // logical not → opcode '?'
  {"SHL",      (TokenType)'['},  // acc << PAR
  {"SHR",      (TokenType)']'},  // acc >> PAR

  // TODO: insert a 0 entry to end matching during tokenization
  {"*EOF*",    TOK_EOF},
  {"*ERROR*",  TOK_ERROR},
  {"*IDENT*",  TOK_IDENT},
  {"*NUM*",    TOK_NUM},
  {"*STRING*", TOK_STRING},
  {":=",       TOK_ASSIGN},
};
#define NKEYWORDS ((int)(sizeof(keywords) / sizeof(keywords[0])))

typedef struct {
  TokenType type;
  // TODO: this also used for strings, maybe strdup?
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

char tok_name_buf[4];
const char* tok_name(TokenType t) {
  int i;
  for (i = 0; i < NKEYWORDS; i++)
    if (keywords[i].type == t) return keywords[i].name;
  if (t > 32 && t < 127) {
    tok_name_buf[0] = t;
    tok_name_buf[1] = 0;
    return tok_name_buf;
  }
  return "?";
}

jmp_buf fail_jmp;

// FAIL: message printed once at setjmp — no printf at each site.
// 6502: pointer fits in int → longjmp(jb, msg).
// Host: int is 32-bit, pointers 64-bit → stash in fail_msg.
const char *fail_msg;

#ifdef __CC65__
  #define FAIL(msg)  longjmp(fail_jmp, (int)(msg))
#else
  #define FAIL(msg)  do { fail_msg = (msg); longjmp(fail_jmp, 1); } while (0)
#endif

// ---------------------------------------------------------------------------
// Source / token state
// ---------------------------------------------------------------------------
char src_buf[MAX_SRC_LEN];
int src_pos = 0;
int lnum = 1;
int last_lnum = 0;
Token curr_tok;

int cur_line = 0;
char cur_c;
#define C cur_c
#define NEXT next()
char next(void) {
  if ((cur_c = src_buf[++src_pos]) == '\n')
    cur_line = src_pos;
  return cur_c;
}

// ---------------------------------------------------------------------------
// Bytecode buffer + patch list
// ---------------------------------------------------------------------------
char bin[MAX_BIN_LEN];
int bin_ptr = 0;

typedef struct {
  int offset; // location inside bin[] that needs a value
  int kind; // 0 = const index, 1 = vector index, …
  int value; // the value to write (or index)
} Patch;

Patch patches[MAX_PATCHES];
int npatch = 0;

// ACC mirror: which mem slot ACC currently holds (-1 = none/unknown)
char acc_slot;
char acc_valid;

void acc_invalidate(void) {
  acc_valid = 0;
  acc_slot = (char)-1;
}

void acc_set_slot(char slot) {
  acc_slot = slot;
  acc_valid = 1;
}

void emit(char c) {
  if (bin_ptr >= MAX_BIN_LEN - 1) FAIL("bytecode buffer overflow");
  bin[bin_ptr++] = c;
}

void emitop(char op) {
  emit(op);
  // control-flow / ACC-clobbering ops — mirror is invalid after these
  if (op == '0' || op == '9' || op == '~' || op == '?' || op == 'G' || op == 'B')
    acc_invalidate();
}

void emitop2(char op, char arg) {
  emitop(op);
  emit(arg);
  if (op == 'S' || op == 'L' || op == 'U') {
    acc_set_slot(arg);
    if (op == 'L') note_load(arg);
    if (op == 'S') note_store(arg);
  }
  else if (strchr("+-*/%&|^IJ[]{}MF@", op))
    acc_invalidate();
}

// Generate function return (pascal: exit)

// TODO: optimization INVALID if code part of
//        IF THEN ELSE / GOTO / LOOP !!!!

//
// - If ACC already holds the return-value slot, emit 0 (return without reload).
// - Otherwise emit ';' (load ret slot, then return).
//
// Opcode \0  = return, keep ACC.
// Opcode ';' = ACC <- ret_val, then return
// Opcode 'q' = quit program (C: exit).
void emitreturn(char ret_slot) {
  if (0 && acc_valid && acc_slot == ret_slot) {
    // TODO: IF == NOT CORRECT!
    // if "Gxx ... 0 (here) ;" canNOT!!
    // only for return can "; ... 0 ;" lol, 

    // "poophole": remove save to return slot before return
    //                      "S ret_slot" RETURN    => "\0"
    if (bin_ptr >= 2 && bin[bin_ptr - 2] == 'S' && bin[bin_ptr - 1] == ret_slot)
      bin_ptr -= 2;

    emitop('X'); // return, keep ACC

  } else if (0 && bin_ptr >= 2 && bin[bin_ptr - 2] == 'Z' && bin[bin_ptr - 1] == ret_slot) {
    // TODO: IF == NOT CORRECT!
    // if "Gxx ... 0 (here) ;" canNOT!!
    // only for return can "; ... 0 ;" lol, 

    // TODO: keep track of last patch point DESTINATION!
    //   cannot poophole before that pos...

    // "poophole": remove zero save to return slot before return
    //             "0 S ret_slot" became
    //             "Z ret_slot" RETURN    => "0 \0"
    // LOL: restore '0'
    bin_ptr -= 2;
    // TODO: 0 return, 1 return, -1 return?
    emitop('Q'); // ACC=0; return

  } else if (acc_valid && acc_slot == (char)0xfe) {
    emitop('Q'); // return 0
  } else
    // Normal return: ACC <- ret_val, then return
    emitop(';');
}

void add_patch(int offset, int kind, int value) {
  if (npatch >= MAX_PATCHES) FAIL("too many patches");
  patches[npatch].offset = offset;
  patches[npatch].kind   = kind;
  patches[npatch].value  = value;
  npatch++;
}

// ---------------------------------------------------------------------------
// Slot allocation (globals 0-31, frames from 32)
// ---------------------------------------------------------------------------
unsigned int mem[MAX_SLOTS]; // runtime memory
char slot_used[MAX_SLOTS]; // allocation map
word slot_loads[MAX_SLOTS];  // global loads  (L / fused) — want low indices
word slot_stores[MAX_SLOTS]; // global stores (S) — future S# benefit

// Only globals (below FRAME_BASE). Frames/params are packed as units.
void note_load(uchar addr) {
  if (addr < FRAME_BASE)
    slot_loads[addr]++;
}
void note_store(uchar addr) {
  if (addr < FRAME_BASE)
    slot_stores[addr]++;
}


char next_global = 0; // next candidate for globals
char next_frame  = FRAME_BASE; // next candidate for frames

char alloc_global(void) {
  while (next_global < GLOBAL_LIMIT) {
    if (!slot_used[next_global]) {
      slot_used[next_global] = 1;
      return (char)next_global++;
    }
    next_global++;
  }
  while (next_frame < MAX_SLOTS) {
    if (!slot_used[next_frame]) {
      slot_used[next_frame] = 1;
      return (char)next_frame++;
    }
    next_frame++;
  }
  FAIL("out of slots");
  return 0xff;
}

char alloc_frame_slot(void) {
  // contiguous (packed bases may overlap across functions)
  if (next_frame >= MAX_SLOTS) FAIL("out of frame slots");
  slot_used[next_frame] = 1;
  return (char)next_frame++;
}

char alloc_temp(void) {
  return alloc_frame_slot();
}

// ---------------------------------------------------------------------------
// Constants (separate array, referenced as 0x80+n)
// ---------------------------------------------------------------------------

uint16_t const_pool[MAX_CONSTS];

// NO NO NO NO NO ! ! !

// TODO: the const pool separate array is just temporary during
//   compilation, the finlize() should add it at the end of the
//   runtime vars block!
//
//   The whole point was to store constants in memory so 
//   that we wouldn't have a separate code path for it,
//   now everything is duplicated! LOL
//
//   I guess this makes another one of those to "relocate", lol
//   
//   finalising(): before executing, we relocate all references
//   to constants in the code, by moving them to a base at end
//   of final, that is, if we're copying the constants...
// 
//   Then during execution there would be one code path.
//
//   ALT: hi-bit set (== too limited), or idx >= startconstidx:
//     load using different base pointer. (complicated)


char nconst = 0;
uchar constidx = 255; // lowest const slot; 255 = no constants

// Constants grow downward from CONST_TOP.
// const_pool[0] @ CONST_TOP, [1] @ CONST_TOP-1, ...
uchar add_const(unsigned int v) {
  uchar i, addr;
  for (i = 0; i < nconst; i++)
    if (const_pool[i] == v)
      return CONST_TOP - i;
  if (nconst >= MAX_CONSTS) FAIL("constant pool full");
  if (CONST_TOP - nconst < FRAME_BASE) FAIL("constants collided with frames");
  const_pool[nconst] = v;
  addr = CONST_TOP - nconst;
  nconst++;
  constidx = addr;
  return addr;
}

#define isconst(a) ((a) >= constidx && (a) <= CONST_TOP)
#define const_val(a) (const_pool[CONST_TOP - (a)])

// Literals 0 / -1: not pooled until needed as an op address (RHS of + etc.)
// outside const band (CONST_TOP=253); never emitted as L/S operands
#define IMM0 ((uchar)0xfe)
#define IMM1 ((uchar)0xff)

int is_lit(char a) {
  return a == IMM0 || a == IMM1 || isconst(a);
}

unsigned int lit_val(char a) {
  if (a == IMM0) return 0;
  if (a == IMM1) return (unsigned int)-1;
  return const_val(a);
}

// Address usable as emitop2 arg (intern IMM0/IMM1 if needed)
char as_addr(char a) {
  if (a == IMM0) return add_const(0);
  if (a == IMM1) return add_const((unsigned int)-1);
  return a;
}

// Map a numeric value to IMM0 / IMM1 / pooled const address
char lit_addr(unsigned int v) {
  if (v == 0) return IMM0;
  if (v == (unsigned int)-1) return IMM1;
  return add_const(v);
}

// ---------------------------------------------------------------------------
// Vectors (function entry points)
// ---------------------------------------------------------------------------
uint16_t vectors[MAX_VECTORS]; // bytecode offset during compile
char *vptr[MAX_VECTORS]; // resolved entry pointers (finalise)
char frame_base[MAX_VECTORS]; // subr# → fp base in mem (fixed per program)
char func_high[MAX_VECTORS];  // highest slot used by function
char func_slots[MAX_VECTORS];
char func_names[MAX_VECTORS][MAX_SYM_LEN+1];
char func_ncalls[MAX_VECTORS]; // unique callees listed
char func_calls[MAX_VECTORS][MAX_VECTORS];
char func_nsites[MAX_VECTORS]; // how many times THIS func is called
int  func_code0[MAX_VECTORS];  // bin_ptr at entry
int  func_code_bytes[MAX_VECTORS];
int  func_code_prev[MAX_VECTORS]; // previous pass size for -O delta
char opt_base[MAX_VECTORS]; // 0 = sequential; else forced base
char cur_vec; // function being compiled (0xff = none)
char nvector = 0;
int opt_level;
int opt_pass; // 0 first, 1 second...

int main_offset = 0;
char *main_entry = 0;
char is_code_addr[MAX_CONSTS];

// Frame layout at fp (contiguous slots from alloc_frame_slot):
//   fp+0  RETURN ipc
//   fp+1  return value
//   fp+2  previous fp
//   fp+3… params / locals
#define FR_RET  0
#define FR_VAL  1
#define FR_PREV 2
#define FP_MAIN ((char)0xFF)
char fp; // current frame pointer (index into mem), FP_MAIN = main

int add_vector(int entry_offset) {
  if (nvector >= MAX_VECTORS) FAIL("too many functions");
  vectors[nvector] = entry_offset;
  return nvector++;
}

// ---------------------------------------------------------------------------
// Symbol table
// ---------------------------------------------------------------------------
typedef enum { SYM_VAR, SYM_FUNC } SymbolType;

typedef struct {
  char name[MAX_SYM_LEN + 1];
  SymbolType type;
  char address; // slot number (var) or vector index (func)
  char nparams; // number of parameters
  char param_slot[MAX_PARAMS]; // slots for each parameter
  char ret_slot; // for functions: slot of the return value
  char ret_addr_slot; // for functions: slot that holds return ipc
} Symbol;

Symbol syms[MAX_SYMBOLS];
char nsym = 0;

void add_symbol(const char* name, SymbolType type, char address) {
  if (nsym >= MAX_SYMBOLS) FAIL("symbol table full");
  strcpy(syms[nsym].name, name);
  syms[nsym].type = type;
  syms[nsym].address = address;
  syms[nsym].nparams = 0;
  syms[nsym].ret_slot = 0xff;
  syms[nsym].ret_addr_slot = 0xff;
  nsym++;
}

Symbol* lookup_sym(const char* name) {
  int i;
  for (i = nsym - 1; i >= 0; i--)
    if (strcmp(syms[i].name, name) == 0) return &syms[i];
  return NULL;
}

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------
void next_token(void);
void expect(TokenType expected);
void expression(void);
void expressionsave(char addr);
int statement(void);
#define EXPECT(expected) expect(expected)

void next_token(void) {
  int i, len;
  char quote_char;

  while (C) {

    // SPACE
    if (isspace(C)) {
      if (C == '\n') lnum++;
      NEXT;
      continue;
    }

    // COMMENT
    if (C == '{') {
      NEXT;
      while (C && C != '}') {
        if (C == '\n') lnum++;
        NEXT;
      }
      if (C == '}') { NEXT; continue; }
      printf("\n} <---- ERROR: Unclosed comment\n");
      curr_tok.type = TOK_ERROR;
      return;
    }
    break;
  }

  // *EOF*
  if (!C) {
    // TODO: is failure? maybe not in script?
    curr_tok.type = TOK_EOF;
    return;
  }

  // STRING
  if (C == '"' || C == '\'') {
    quote_char = C;
    NEXT;
    len = 0;
    while (C && C != quote_char && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = C;
      NEXT;
    }
    if (C == quote_char) NEXT;
    curr_tok.lexeme[len] = 0;
    curr_tok.type = TOK_STRING;
    return;
  }

  // IDENT
  if (isalpha(C)) {
    len = 0;
    while (isalnum(C) && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = C;
      NEXT;
    }
    curr_tok.lexeme[len] = 0;
    for (i = 0; i < len; i++)
      curr_tok.lexeme[i] = toupper(curr_tok.lexeme[i]);

    curr_tok.type = TOK_IDENT;
    for (i = 0; i < NKEYWORDS; i++) {
      if (strcmp(curr_tok.lexeme, keywords[i].name) == 0) {
        curr_tok.type = keywords[i].type;
        break;
      }
    }
    return;
  }

  // NUMBER
  if (isdigit(C)) {
    curr_tok.type = TOK_NUM;
    curr_tok.value = 0;
    while (isdigit(C)) {
      curr_tok.value = curr_tok.value * 10 + (C - '0');
      NEXT;
    }
    sprintf(curr_tok.lexeme, "%d", curr_tok.value);
    return;
  }

  // ASSIGN :=
  if (C == ':' && src_buf[src_pos + 1] == '=') {
    NEXT; NEXT;
    curr_tok.type = TOK_ASSIGN;
    // TODO: remove, why needed? for error message?
    strcpy(curr_tok.lexeme, ":=");
    return;
  }

  // single-char: token type is the character itself
  curr_tok.lexeme[0] = C;
  curr_tok.lexeme[1] = 0;
  if (strchr(";:.,()+-*'*/&|^<>=", C)) {
    curr_tok.type = C;
    NEXT;
    return;
  }

  FAIL("unknown character");
}

void expect(TokenType expected) {
  if (curr_tok.type == expected) { next_token(); return; }
  printf("\n%%Expected %s but found '%s'",
    tok_name(expected), tok_name(curr_tok.type));
  FAIL("expected");
}

// ---------------------------------------------------------------------------
// Operands: value is in ACC, or at a mem address (var slot or const 0x80+n).
// Constants and variables are the same — isconst(addr) distinguishes for folding.
// ---------------------------------------------------------------------------
#define is_acc(k) ((k) == 0)

// Ensure ACC holds value at addr (or already holds it).
// Encoding (smallest first):
//   IMM0/IMM1     -> '0' / '9'
//   slot 0..31    -> single byte 0x00..0x1F  (fused load)
//   frame+1..+8   -> '1'..'8'  (fp-relative local)
//   else          -> 'L' + slot
void emitload(char in_acc, uchar addr) {
  uchar base, rel;
  if (in_acc) return;
  if (acc_valid && acc_slot == addr) return;

  if (addr == IMM0 || (isconst(addr) && const_val(addr) == 0)) {
    emitop('0');
    acc_set_slot(IMM0);
    return;
  }
  if (addr == IMM1 || (isconst(addr) && const_val(addr) == (unsigned int)-1)) {
    emitop('9');
    acc_set_slot(IMM1);
    return;
  }

  // fused absolute load slots 0..31 (globals / low mem)
  if (addr < 32) {
    emit(addr);
    acc_set_slot(addr);
    note_load(addr);
    return;
  }

  // frame-relative locals '1'..'8' — not register-global candidates
  if (cur_vec != (char)0xff) {
    base = frame_base[cur_vec];
    rel = addr - base;
    if (rel >= 1 && rel <= 8) {
      emit('0' + rel);
      acc_set_slot(addr);
      return;
    }
  }

  emitop2('L', addr);
}

int lastv= -666;

#ifdef OPTMOREPARAMS
void emitstore(char addr) {
  if (addr == lastv+1) {
    ++lastv;
    emitop(',');
  } else 
    emitop2('S', addr);
}
#endif // OPTMOREPARAMS

// Parse a primary simple type
void primary(char *out_acc, char *out_addr) {
  char name[MAX_SYM_LEN + 1];
  Symbol *sym;
  int i, pi;

  // NOT expr  (opcode '?')
  if (curr_tok.type == (TokenType)'?') {
    next_token();
    primary(out_acc, out_addr);
    emitload(*out_acc, *out_addr);
    emitop('?');
    *out_acc = 1;
    *out_addr = 0;
    return;
  }

  // ( expression )
  if (curr_tok.type == '(') {
    next_token();
    expression();
    EXPECT(')');
    *out_acc = 1;
    *out_addr = 0;
    return;
  }

  // NUMBER
  if (curr_tok.type == TOK_NUM) {
    *out_acc = 0;
    *out_addr = lit_addr((unsigned int)curr_tok.value);
    next_token();
    return;
  }

  // IDENT
  if (curr_tok.type == TOK_IDENT) {
    char type= SYM_VAR;
    sym = NULL;
    strcpy(name, curr_tok.lexeme);

    // ? FUNCALL ( ... )
    next_token();
    if (curr_tok.type == '(') type= SYM_FUNC;
    
    // search backwards (lexical!)
    for (i = nsym - 1; i >= 0; i--) {
      if (strcmp(syms[i].name, name) == 0 && syms[i].type == type) {
        sym= syms+i; break;
      }
    }
    if (!sym) FAIL("Undeclared identifier");

    // FUNCALL ( ... )
    // TODO: WARNING: foo(2, foo(3,4)) will do wrong!
    if (type == SYM_FUNC) {

      EXPECT('(');
      for (pi = 0; pi < sym->nparams; pi++) {
        if (pi > 0) EXPECT(',');

        if (pi < sym->nparams - 1)
          // opt for 0 Spp => Zpp
          expressionsave(sym->param_slot[pi]);
        else
          // last param in ACC
          expression();
      }
      EXPECT(')');

      emitop((char)(0xC0 | (sym->address & 0x3F)));
      {
        char cv = (char)(sym->address & 0x3F);
        uchar uc = cv;
        func_nsites[uc]++;
        if (cur_vec != (char)0xff) {
          char k, found = 0;
          uchar uv = cur_vec;
          for (k = 0; k < func_ncalls[uv]; k++)
            if (func_calls[uv][k] == cv) { found = 1; break; }
          if (!found && func_ncalls[uv] < MAX_VECTORS)
            func_calls[uv][func_ncalls[uv]++] = cv;
        }
      }

      acc_invalidate();
      *out_acc = 1;
      *out_addr = 0;
      return;
    }

    *out_acc = 0;
    *out_addr = sym->address;
    return;
  }

  FAIL("Expected expression");
}

void expression(void) {
  char left_acc, right_acc;
  char left_addr, right_addr;
  char op, comm;
  unsigned int lv, rv;

  primary(&left_acc, &left_addr);

  while (strchr("+-*/&|^<>=[]", curr_tok.type)) {
    op = curr_tok.type;
    next_token();
    comm = strchr("-/<>[]", op) == NULL;

    // save if result of primary is in acc
    if (left_acc) {
      char tmp = alloc_temp();
      emitop2('S', tmp);
      left_acc = 0;
      left_addr = tmp;
    }

    primary(&right_acc, &right_addr);

    // FOLD CONSTANTS
    if (!left_acc && !right_acc && is_lit(left_addr) && is_lit(right_addr)) {

      lv = lit_val(left_addr);
      rv = lit_val(right_addr);
      switch (op) {
        case '+': lv += rv; break;
        case '-': lv -= rv; break;
        case '*': lv *= rv; break;
        case '/': if (rv) lv /= rv; break;
        case '&': lv &= rv; break;
        case '|': lv |= rv; break;
        case '^': lv ^= rv; break;
        case '<': lv = lv < rv; break;
        case '=': lv = lv == rv; break;
        case '>': lv = lv > rv; break;
        case '[': lv = lv << rv; break;
        case ']': lv = lv >> rv; break;
      }

      left_acc = 0;
      left_addr = lit_addr(lv);
      continue;
    }

    // right in ACC (call result)
    if (right_acc) {

      if (comm)
        emitop2(op, as_addr(left_addr));
      else {
        char tmp = alloc_temp();
        emitop2('S', tmp);
        emitload(left_acc, left_addr);
        emitop2(op, tmp);
      }
      left_acc = 1;
      continue;
    }

    // ACC already holds right (e.g. last param)
    if (comm && acc_valid && acc_slot == right_addr) {
      emitop2(op, as_addr(left_addr));
      left_acc = 1;
      continue;
    }

    // standard: left → ACC, ACC op= right
    emitload(left_acc, left_addr);
    if (op == '+' && acc_valid && acc_slot == right_addr) {
      if (!(bin_ptr >= 2 && bin[bin_ptr-2] == 'S' && bin[bin_ptr-1] == right_addr))
        emitop2('S', right_addr);
    }
    emitop2(op, as_addr(right_addr));
    left_acc = 1;
  }

  emitload(left_acc, left_addr);
}

void expressionsave(char addr) {
  int ptr= bin_ptr;

  expression();

  // TODO: only valid if no jump dest near!

  // Poophole optimization 0 Sxx => Zxx
  if (bin_ptr - ptr == 1 && bin[ptr] == '0') {
    bin[ptr]= 'Z'; emit(addr);
  } else
    emitop2('S', addr);
}

// ---------------------------------------------------------------------------
// Statement
// ---------------------------------------------------------------------------
void beginend() {
  EXPECT(TOK_BEGIN);

  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    // TODO: doesn't statement do this already
    statement();
  }

  EXPECT(TOK_END);
}

int statement(void) {

  // ASSIGNMENT
  if (curr_tok.type == TOK_IDENT) {
    int jj= src_pos - 5;
    char target[MAX_SYM_LEN + 1];
    Symbol* sym = lookup_sym(curr_tok.lexeme); // ?? safe
    strcpy(target, curr_tok.lexeme);
    if (!sym || sym->type != SYM_VAR) {
      FAIL("Unknown variable");
    }

    // "Poophole" optimization: sum:= sum+i; sum:= i+sum;
    { int i= bin_ptr;

      EXPECT(TOK_IDENT);
      EXPECT(TOK_ASSIGN);
      expressionsave(sym->address);
      EXPECT(';');

      // TODO: only valid if no jumps "near"

      if (bin_ptr-i==6
        && bin[i]=='L' && bin[i+2]=='+' && bin[i+4]=='S'
        && bin[i+1]==bin[i+5]
      ) {
        #if 0
        printf("====> poophole 'U' %d: %c%02x %c%02x %c%02x   >> %.30s\n"
          , bin_ptr - i
          , bin[i + 0]
          , bin[i + 1]
          , bin[i + 2]
          , bin[i + 3]
          , bin[i + 4]
          , bin[i + 5]
          , src_buf + jj
        );
        #endif
        
        {
          char d= bin[i+3], s= bin[i+1];
          bin_ptr-= 6; // rewind
          emitop2('U', s); emit(d);
        }
      }
    }
  }

  // WRITE / WRITELN ( 0..n args, comma-separated )
  // WRITELN: newline — if last arg is an inline string, append \n before 0;
  //          otherwise emit opcode 'N' after all args (also for zero args).
  // WRITE:   never adds newline.
  else if (curr_tok.type == TOK_WRITE || curr_tok.type == TOK_WRITELN) {
    int i, nargs, last_was_str, nl;
    nl = (curr_tok.type == TOK_WRITELN);
    next_token(); // WRITE / WRITELN
    EXPECT('(');

    nargs = 0;
    last_was_str = 0;
    if (curr_tok.type != ')') {
      while (1) {
        last_was_str = 0;
        if (curr_tok.type == TOK_STRING) {
          // inline string
          emitop('"');
          for (i = 0; curr_tok.lexeme[i]; i++)
            emit(curr_tok.lexeme[i]);
          // WRITELN + last arg decided after loop; stash string open, close later
          // emit terminator after optional \n once we know if this is last
          EXPECT(TOK_STRING);
          // temporarily close without \n; fixed if last + nl below
          emit(0);
          last_was_str = 1;
        } else {
          int pos = bin_ptr;
          expression();
          // "poophole" Lxx. => Wxx
          if (bin_ptr - pos == 2 && bin[pos] == 'L')
            bin[pos] = 'W';
          else if (bin_ptr - pos == 1 && bin[pos] < 32)
            emitop('.'); // fused load already in ACC
          else if (bin_ptr - pos == 1 && bin[pos] >= '1' && bin[pos] <= '8')
            emitop('.');
          else
            emitop('.');
        }
        nargs++;
        if (curr_tok.type != ',')
          break;
        next_token(); // ,
      }
    }

    // WRITELN newline policy
    if (nl) {
      if (last_was_str && nargs > 0) {
        // rewrite trailing 0 of last string to \n 0
        if (bin_ptr >= 1 && bin[bin_ptr - 1] == 0) {
          bin[bin_ptr - 1] = '\n';
          emit(0);
        }
      } else {
        emitop('N');
      }
    }

    EXPECT(')');
    EXPECT(';');
  }

  // FOR
  else if (curr_tok.type == TOK_FOR) {
    char high_slot, ctrl_name[MAX_SYM_LEN + 1];
    int loop_start, exit_patch;
    Symbol* ctrl;

    EXPECT(TOK_FOR);
    if (curr_tok.type != TOK_IDENT) FAIL("expected control variable");
    strcpy(ctrl_name, curr_tok.lexeme);
    ctrl = lookup_sym(ctrl_name);
    if (!ctrl || ctrl->type != SYM_VAR) FAIL("undeclared control variable");
    EXPECT(TOK_IDENT);
    EXPECT(TOK_ASSIGN);

    expressionsave(ctrl->address);

    // PASCAL FOR i:=1 TO 4711+39
    // : it saves the limit so evaluated only once
    EXPECT(TOK_TO);
    high_slot = alloc_temp();
    expressionsave(high_slot);

    EXPECT(TOK_DO);
    loop_start = bin_ptr;

    // test: acc = ctrl - high;  if (acc > 0) exit
// TODO: fuse? 
    emitop2('L', ctrl->address);
    emitop2('-', high_slot);
    exit_patch = bin_ptr;
    // TDOO: emitop2?
    emitop('G'); emit(0); // G <delta> conditional forward (acc > 0)

    statement();

    // increment
    emitop2('I', ctrl->address);

    // backward jump B <delta>
    {
      int delta = (bin_ptr + 2) - loop_start;
      if (delta > 255) FAIL("FOR body too large");
      // TODO: emitop2?
      emitop('B'); emit((char)delta);

      // patch forward exit
      delta = bin_ptr - (exit_patch + 2);
      bin[exit_patch + 1] = (char)delta;
    }
    acc_invalidate();
  }

  // IF expr THEN stmt [ELSE stmt]
  else if (curr_tok.type == TOK_IF) {
    int patch_then;
    int patch_else;

    EXPECT(TOK_IF);

    expression();

    // Condition in ACC (0 = false, nonzero = true).
    // G jumps if ACC > 0 — invert so we skip THEN when false.
    emitop('?');
    emitop('G');
    patch_then = bin_ptr;
    emit(0); // Placeholder for forward jump delta

    EXPECT(TOK_THEN);

    statement();

    // Check for optional ELSE branch
    if (curr_tok.type == TOK_ELSE) {
      EXPECT(TOK_ELSE);

      // If the THEN body executed, we must unconditionally jump past the ELSE body.
      /// emitop('1'); // TODO: ???


// TODO:!!!!


//      emitop('9'); // TRUE (but its neg!) lol
//      emitop('?'); // ???
      emitop('0');
      emitop('?'); // ???

      // TODO: emitop2?
      emitop('G'); // Forward unconditional jump
      patch_else = bin_ptr;
      emit(0); // Placeholder for forward jump delta

      // Patch the initial false jump to target this ELSE entry point
      bin[patch_then] = bin_ptr - (patch_then + 1);

      statement();

      // Patch the ELSE exit jump to target right after the ELSE body
      bin[patch_else] = bin_ptr - (patch_else + 1);
    } else {
      // No ELSE branch: patch the false jump to land right here
      bin[patch_then] = bin_ptr - (patch_then + 1);
    }
    // both branches merge — ACC content unknown
    acc_invalidate();
  }

  // BEGIN ... END
  else if (curr_tok.type == TOK_BEGIN) beginend();

  // ; (empty statement)
  else if (curr_tok.type == ';') EXPECT(';');
  
  // --- ERROR
  else FAIL("expected statement");

  return 1;
}

// ---------------------------------------------------------------------------
// Compile a whole function (called from the declaration loop)
// Layout of a function frame (allocated from FRAME_BASE upward):
//   ret_addr_slot
//   ret_val_slot
//   param_slot          (single argument for now)
//   … possible locals later
// The vector entry points at the first instruction of the body.
// ---------------------------------------------------------------------------
// VAR name : INTEGER; ...
// local=0 globals; local=1 frame slots (same as params for now)
void parsevars(char local) {
  EXPECT(TOK_VAR);
  while (curr_tok.type == TOK_IDENT) {
    char slot;
    if (local)
      slot = alloc_frame_slot();
    else
      slot = alloc_global();
    add_symbol(curr_tok.lexeme, SYM_VAR, slot);
    EXPECT(TOK_IDENT);
    EXPECT(':');
    EXPECT(TOK_INTEGER);
    EXPECT(';');
  }
}

void compile_function(void) {
  char func_name[MAX_SYM_LEN + 1];
  char param_name[MAX_SYM_LEN + 1];
  char ret_addr_slot, ret_val_slot, caller_slot;
  int entry_offset, vec_idx;
  int nparams, i;
  char param_slots[MAX_PARAMS];
  Symbol* fsym;

  // FUNCTION name (
  next_token(); // consume FUNCTION
  strcpy(func_name, curr_tok.lexeme);
  EXPECT(TOK_IDENT);
  EXPECT('(');

  if (opt_base[nvector])
    next_frame = opt_base[nvector];

  // frame: RETURN (ipc), ret_val, CALLER (prev cur_func), then params
  ret_addr_slot = alloc_frame_slot();
  ret_val_slot  = alloc_frame_slot();
  caller_slot   = alloc_frame_slot();

  // parse parameter list: name : INTEGER [, name : INTEGER]*
  nparams = 0;
  if (curr_tok.type == TOK_IDENT) {
    while (1) {
      if (nparams >= MAX_PARAMS) {
        FAIL("too many parameters");
      }
      strcpy(param_name, curr_tok.lexeme);
      param_slots[nparams] = alloc_frame_slot();
      add_symbol(param_name, SYM_VAR, param_slots[nparams]);
      nparams++;
      EXPECT(TOK_IDENT);
      EXPECT(':');
      EXPECT(TOK_INTEGER);
      // accept both "," and ";" as parameter separators
      // (comma is convenient; semicolon is classic Pascal)
      if (curr_tok.type == ',' || curr_tok.type == ';') {
        next_token();
        // trailing separator before ')' is not allowed – loop continues
        // only if another IDENT follows; otherwise fall through to RPAREN
        if (curr_tok.type != TOK_IDENT)
          break;
        continue;
      }
      break;
    }
  }
  EXPECT(')');
  EXPECT(':');
  EXPECT(TOK_INTEGER);
  EXPECT(';');

  // register the function symbol (vector index filled in a moment)
  add_symbol(func_name, SYM_FUNC, 0); // temporary
  fsym = lookup_sym(func_name);
  fsym->nparams = nparams;
  for (i = 0; i < nparams; i++)
    fsym->param_slot[i] = param_slots[i];
  fsym->ret_slot      = ret_val_slot;
  fsym->ret_addr_slot = ret_addr_slot;

  // Also expose the function name as a variable that writes the return slot
  // (classic Pascal:  FuncName := expression;  )
  add_symbol(func_name, SYM_VAR, ret_val_slot);

  // Expose RETURN as a local name for the return-address slot
  add_symbol("RETURN", SYM_VAR, ret_addr_slot);

  if (curr_tok.type == TOK_VAR)
    parsevars(1);

  // the entry point is right here
  entry_offset = bin_ptr;
  vec_idx = add_vector(entry_offset);
  // subr# → frame base (== RETURN slot index); FR_VAL/FR_PREV are +1/+2
  frame_base[vec_idx] = ret_addr_slot;
  fsym->address = vec_idx;
  cur_vec = (char)vec_idx;
  func_ncalls[vec_idx] = 0;
  func_code0[vec_idx] = entry_offset;

  // TODO: mustbe after previous section. WHY?
  
  // last param arrives in ACC; store into its slot (call sites omit that S)
  if (nparams > 0) {
    emitop2('S', param_slots[nparams - 1]);
    acc_set_slot(param_slots[nparams - 1]);
  } else
    acc_invalidate();

  beginend();
  EXPECT(';');

  // return: 0 if ACC already is return value, else ';' (reload then return)
  emitreturn(ret_val_slot);

  // frame high = last frame slot allocated for this function
  {
    char high = (char)(next_frame - 1);
    char slots, k;
    uchar uv = vec_idx;
    int nbytes, delta;
    func_high[uv] = high;
    slots = (char)(high - ret_addr_slot + 1);
    func_slots[uv] = slots;
    strcpy(func_names[uv], func_name);
    nbytes = bin_ptr - func_code0[uv];
    func_code_bytes[uv] = nbytes;
    printf("%-16s nparams=%d slots=%d base=%02x code=%d sites=%u calls:",
      func_name, nparams, slots, ret_addr_slot, nbytes,
      (unsigned)func_nsites[uv]);
    for (k = 0; k < func_ncalls[uv]; k++) {
      uchar c = func_calls[uv][k];
      printf(" %02x:%u", c, func_high[c]);
    }
    if (func_ncalls[uv] == 0) printf(" -");
    if (opt_pass > 0) {
      delta = func_code_prev[uv] - nbytes;
      if (delta > 0)
        printf("  (%s -%d bytes)", func_name, delta);
      else if (delta < 0)
        printf("  (%s +%d bytes)", func_name, -delta);
    }
    putchar('\n');
  }
  cur_vec = 0xff;
}



int pack_frames(void) {
  int i, j, changed, iter;
  int seq_top, pack_top, saved;
  uchar b, h, c;

  for (i = 0; i < nvector; i++)
    opt_base[i] = FRAME_BASE;
  for (iter = 0; iter < nvector + 2; iter++) {
    changed = 0;
    for (i = 0; i < nvector; i++) {
      b = FRAME_BASE;
      for (j = 0; j < func_ncalls[i]; j++) {
        c = func_calls[i][j];
        h = func_high[c];
        if ((int)h + 1 > (int)b)
          b = (h + 1);
      }
      if (opt_base[i] != (char)b) {
        opt_base[i] = (char)b;
        changed = 1;
      }
      func_high[i] = (char)(opt_base[i] + func_slots[i] - 1);
    }
    if (!changed) break;
  }
  seq_top = FRAME_BASE - 1;
  pack_top = FRAME_BASE - 1;
  for (i = 0; i < nvector; i++) {
    h = (frame_base[i] + func_slots[i] - 1);
    if ((int)h > seq_top) seq_top = h;
    h = func_high[i];
    if ((int)h > pack_top) pack_top = h;
  }
  saved = seq_top - pack_top;
  if (saved < 0) saved = 0;
  return saved;
}

int estimate_s12_save(void) {
  int i, n = 0;
  for (i = 0; i < bin_ptr - 1; i++)
    if ((bin[i] == 'L' || bin[i] == 'S') && bin[i + 1] < 16)
      n++;
  return n;
}

int estimate_slot32_save(void) {
  int i, n = 0;
  for (i = 0; i < bin_ptr - 1; i++)
    if ((bin[i] == 'L' || bin[i] == 'S') && bin[i + 1] < 32)
      n++;
  return n;
}

void print_reg_stats(void) {
  int i, j;
  int idx[64], nidx = 0;
  printf("REGISTER candidates (globals < FRAME_BASE):\n");
  for (i = 0; i < FRAME_BASE && i < MAX_SLOTS; i++)
    if ((slot_loads[i] || slot_stores[i]) && nidx < 64)
      idx[nidx++] = i;
  // sort by loads+stores descending
  for (i = 0; i < nidx; i++)
    for (j = i + 1; j < nidx; j++)
      if (slot_loads[idx[j]] + slot_stores[idx[j]] >
          slot_loads[idx[i]] + slot_stores[idx[i]]) {
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
      }
  for (i = 0; i < nidx; i++) {
    int s = idx[i];
    const char *name = "?";
    for (j = 0; j < nsym; j++)
      if (syms[j].type == SYM_VAR && syms[j].address == s) {
        name = syms[j].name;
        break;
      }
    printf("  slot %02x  L=%u S=%u  %s\n",
      s, (unsigned)slot_loads[s], (unsigned)slot_stores[s], name);
  }
  if (!nidx)
    printf("  (none)\n");
  printf("CALL sites:\n");
  for (i = 0; i < nvector; i++)
    printf("  %-16s sites=%u code=%d slots=%d\n",
      func_names[i], (unsigned)func_nsites[i],
      func_code_bytes[i], func_slots[i]);
}

void opt_phase_report(int slot_saved) {
  int s12, s32, code_saved, i;
  s12 = estimate_s12_save();
  s32 = estimate_slot32_save();
  code_saved = 0;
  if (opt_pass > 0)
    for (i = 0; i < nvector; i++)
      code_saved += func_code_prev[i] - func_code_bytes[i];
  printf("--- opt:%d  saved %d frame slots, ~%d S12 bytes, ~%d slot<32 L/S, code %+d bytes, %d vectors ---\n",
    opt_pass, slot_saved, s12, s32, code_saved, nvector);
}

// ---------------------------------------------------------------------------
// Top-level compilation
// ---------------------------------------------------------------------------
int compile_pipeline(void) {
  int last_var_slot = -1;

  src_pos = -1;
  NEXT;
  lnum = 1;
  bin_ptr = 0;
  nsym = 0;
  nconst = 0;
  constidx = 255;
  memset(slot_loads, 0, sizeof(slot_loads));
  memset(slot_stores, 0, sizeof(slot_stores));
  nvector = 0;
  npatch = 0;
  cur_vec = (char)0xff;
  memset(func_ncalls, 0, sizeof(func_ncalls));
  memset(func_high, 0, sizeof(func_high));
  memset(func_nsites, 0, sizeof(func_nsites));
  main_offset = 0;
  next_global = 0;
  next_frame  = FRAME_BASE;
  memset(slot_used, 0, sizeof(slot_used));
  memset(mem, 0, sizeof(mem));
  memset(is_code_addr, 0, sizeof(is_code_addr));

  // Functions are emitted first; main_entry is set when we reach BEGIN.
  // finalise() resolves main_entry to a direct pointer — no absolute G opcode.

  // implicit X (global)
  {
    char xslot = alloc_global();
    add_symbol("X", SYM_VAR, xslot);
  }

  next_token();
  EXPECT(TOK_PROGRAM);
  EXPECT(TOK_IDENT);
  EXPECT(';');

  // declarations
  while (curr_tok.type == TOK_VAR || curr_tok.type == TOK_FUNCTION) {
    if (curr_tok.type == TOK_VAR) {
      parsevars(0);
      if (nsym > 0 && syms[nsym - 1].type == SYM_VAR)
        last_var_slot = syms[nsym - 1].address;
    }
    else { // FUNCTION
      compile_function();
    }
  }

  // main program
  EXPECT(TOK_BEGIN);

  // record main entry offset (resolved to a pointer in finalise)
  main_offset = bin_ptr;

  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    statement();
  }
  EXPECT(TOK_END);
  EXPECT('.');

  // leave last global in acc (compatibility)
  if (last_var_slot >= 0)
    emitop2('L', last_var_slot);

  emitop('q'); // quit program

  // pack + phase report after main (call sites complete)
  {
    int slot_saved = 0;
    int i;
    if (nvector)
      slot_saved = pack_frames();
    // refresh sites on function lines already printed — summary:
    print_reg_stats();
    opt_phase_report(slot_saved);
    if (slot_saved > 0 && opt_level <= 0)
      printf("  hint: re-run with -O to place frames (save %d slots)\n", slot_saved);
  }
  return 1;
}

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
  word main;
  word code;
  word call;
  word fp;
  word con;
  word data;
  word memsz;
  word constidx; // lowest const slot (not relocated)
} BlobHeader;

// Process head — layout of ZP image (memcpy in/out of run_ivm)
typedef struct ProcessHead {
  BlobHeader hdr;
  uchar *ipc;
  char fp;
  unsigned int *mem;
  unsigned int acc;
} ProcessHead;

typedef struct Process {
  ProcessHead h;
  unsigned int mem[1];
} Process;

#define MAX_BLOB_LEN 4096
uchar blob[MAX_BLOB_LEN];
int blob_len;
uchar *g_loaded;

// ZP global (same layout as ProcessHead)
ProcessHead zp;
word *consts; // unused if PAR is mem-only
uchar op;

// append n bytes of data at *pp; return offset from base
static word memadd(uchar *base, uchar **pp, void *data, word n) {
  word off = (word)(*pp - base);
  if (data && n)
    memcpy(*pp, data, n);
  *pp += n;
  return off;
}


// Process memory layout (after compile, before finalise):
//   [hot globals by L][packed frames][cold globals][constants]
// Remap bytecode operands; memsz = total cells. Nevm copies const words into mem[].
// PAR is always mem[idx] — no separate const path in the dispatch loop.
uchar slot_map[256];
uchar slot_mapped[256];
uchar layout_order[256]; // global: cc65 stack too small for local[256]
word layout_memsz;
uchar layout_next;

void layout_process_mem(void) {
  uchar i, j, norder, oldb, slots, k, base;

  layout_next = 0;
  norder = 0;
  for (i = 0; i < 255; i++) {
    slot_map[i] = i;
    slot_mapped[i] = 0;
  }

  // 1) globals below FRAME_BASE, hottest loads first
  for (i = 0; i < FRAME_BASE; i++)
    if (slot_used[i] || slot_loads[i] || slot_stores[i])
      layout_order[norder++] = i;
  for (i = 0; i < norder; i++)
    for (j = i + 1; j < norder; j++)
      if (slot_loads[layout_order[j]] > slot_loads[layout_order[i]]) {
        uchar t = layout_order[i];
        layout_order[i] = layout_order[j];
        layout_order[j] = t;
      }
  for (i = 0; i < norder; i++) {
    slot_map[layout_order[i]] = layout_next;
    slot_mapped[layout_order[i]] = 1;
    layout_next++;
  }

  // 2) frames at emit-time bases
  for (i = 0; i < nvector; i++) {
    oldb = frame_base[i];
    slots = func_slots[i];
    if (!slots) continue;
    for (k = 0; k < slots; k++) {
      slot_map[oldb + k] = layout_next + k;
      slot_mapped[oldb + k] = 1;
    }
    frame_base[i] = layout_next;
    layout_next += slots;
  }

  // 3) leftover used non-const slots
  for (i = 0; i < MAX_SLOTS; i++) {
    if (slot_used[i] && !slot_mapped[i] && (i < constidx || constidx == 255)) {
      slot_map[i] = layout_next++;
      slot_mapped[i] = 1;
    }
  }

  // 4) constants last: pool[i] at base+i
  if (nconst) {
    base = layout_next;
    for (i = 0; i < nconst; i++) {
      oldb = CONST_TOP - i;
      slot_map[oldb] = base + i;
      slot_mapped[oldb] = 1;
    }
    constidx = base;
    layout_next += nconst;
  }

  layout_memsz = layout_next;
  printf("layout: memsz=%u constidx=%u nconst=%d frames=%d\n",
    (unsigned)layout_memsz, (unsigned)constidx, nconst, nvector);
}

void remap_bytecode(void) {
  int i;
  uchar c, a;
  i = 0;
  while (i < bin_ptr) {
    c = bin[i++];
    if (c < 32) {
      if (slot_mapped[c] && slot_map[c] < 32)
        bin[i - 1] = slot_map[c];
    } else if (c == '"') {
      while (i < bin_ptr && bin[i]) i++;
      if (i < bin_ptr) i++;
    } else if (c >= 0xC0) {
      ;
    } else if (c >= 0x80) {
      ;
    } else if (c == 'G' || c == 'B') {
      if (i < bin_ptr) i++;
    } else if (c == 'U') {
      if (i + 1 < bin_ptr) {
        a = bin[i];
        if (slot_mapped[a]) bin[i] = slot_map[a];
        a = bin[i + 1];
        if (slot_mapped[a]) bin[i + 1] = slot_map[a];
        i += 2;
      }
    } else if (strchr("LS+-*/%&|^[]{}@!IMFWZ=<>?", c)) {
      if (i < bin_ptr) {
        a = bin[i];
        if (slot_mapped[a]) bin[i] = slot_map[a];
        i++;
      }
    }
  }
}

void finalise(void) {
  BlobHeader h;
  uchar *p;
  word zero;
  int i;

  p = blob + sizeof(BlobHeader);
  zero = 0;

  // call vectors (blob-absolute entries filled after code offset known)
  h.call = memadd(blob, &p, 0, (word)(nvector * sizeof(word)));
  // 0000 ends relocatable vector region (loader stops here)
  memadd(blob, &p, &zero, sizeof(zero));

  // constants — after 0000, not relocated
  h.con = memadd(blob, &p, const_pool, (word)(nconst * sizeof(word)));

  // frame bases
  h.fp = memadd(blob, &p, frame_base, (word)nvector);

  // code
  h.code = memadd(blob, &p, bin, (word)bin_ptr);
  h.main = h.code + main_offset;

  h.data = (word)(p - blob);
  if (layout_memsz)
    h.memsz = layout_memsz;
  else {
    int top = 0;
    for (i = 0; i < MAX_SLOTS; i++)
      if (slot_used[i])
        top = i;
    h.memsz = (word)(top + 1);
  }
  h.constidx = (word)constidx;

  // fill call table: code entry offsets (blob-absolute before loader +base)
  for (i = 0; i < nvector; i++)
    ((word *)(blob + h.call))[i] = vectors[i] + h.code;

  memcpy(blob, &h, sizeof(h));
  blob_len = (int)(p - blob);
  if (blob_len >= MAX_BLOB_LEN) FAIL("blob overflow");

  // dump helpers only
  for (i = 0; i < nvector; i++)
    vptr[i] = bin + vectors[i];
  main_entry = bin + main_offset;
}

// relocating first vectors till 0000
// Intent: every word from blob start through the call table is a blob-relative
// offset; add base so they become absolute pointers (6502 address space).
// memsz sits in the header but is a *count*, not an offset — preserve it.
void loader(uchar *base) {
  word *v;
  word save_memsz;
  BlobHeader *h;

  h = (BlobHeader *)base;
  save_memsz = h->memsz; // NOT an offset
  {
    word save_ci = h->constidx;

  v = (word *)base;
  while (*v) {
    #ifdef __CC65__
    *v = *v + (word)base;
    #endif
    v++;
  }

  h->memsz = save_memsz;
  h->constidx = save_ci;
  }
}

// loaded = output of loader; does not call loader
Process *Nevm(uchar *loaded) {
  Process *p;
  unsigned int cells, bytes;
  BlobHeader *bh;

  bh = (BlobHeader *)loaded;
  cells = bh->memsz;
  if (!cells) cells = MAX_SLOTS;

  bytes = (unsigned int)(sizeof(ProcessHead) + cells * sizeof(unsigned int));
  p = (Process *)calloc(1, bytes);
  if (!p) {
    printf("calloc fail %u bytes (%u slots)\n", bytes, cells);
    FAIL("Nevm calloc");
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

// Oscar doesn't have
// TODO: maybe _zoropage ? but on parameter?
#ifndef __CC65__
#define register
#endif

// Byte codes used:
// 

// 1-31: fast var, TODO: 0 get it back!
// #$ ' , () : \ _ `
// 12345678 - actual numbers? lol
// ACD H R T VY
//  cAr Cons cDr  Heap/Here Recurse Tail-call Vector/index? Yndex?
// 26: a-z  : OrwinOS API
// 64: 00-3f: conditional relative jump forward
// 64: (xx) : vector calls   
unsigned int run_ivm(Process *p) {
  // Classic single-switch + goto dispatch (ivm.c style).
  // - opcode 'X' = return keep ACC; 'Q' = return 0; ';' = load ret then return
  // - 0x00..0x1F = fused load mem[n]; '1'..'8' = load mem[fp+k]
  // - opcode ';' = ACC ← ret_val at fp, then return
  // - parameter ops use PAR then goto pnext
  // - zero-arg ops goto xnext
  //
  // ZP: memcpy Process head ↔ globals (ivm_hdr, ipc, fp, mem, acc)

  // load process head → ZP global
  memcpy(&zp, &p->h, sizeof(zp));
  zp.mem = p->mem;
  #ifdef __CC65__
  /* pool only used if needed elsewhere */
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

  // fused load slots 0..31 (one byte)
  if (op < 32) {
    zp.acc = zp.mem[op];
    goto xnext;
  }

  switch (op) {

    // OS: quit program (C: exit)
  case 'q':
    goto done;

  // 'Q' = return 0 (ACC=0 then return)
  case 'Q':
    zp.acc = 0;
    // fall-through to X

  // 'X' = return, keep ACC (was opcode 0)
  case 'X':
    if (zp.fp == FP_MAIN) goto done;
    #ifdef __CC65__
      zp.ipc = (uchar *)zp.mem[zp.fp + FR_RET];
    #else
      zp.ipc = g_loaded + zp.mem[zp.fp + FR_RET];
    #endif
    zp.fp = (char)zp.mem[zp.fp + FR_PREV];
    goto xnext;

    // ';' = ACC <- ret_val, then return
  case ';':
    if (zp.fp == FP_MAIN) goto done;
    zp.acc = zp.mem[zp.fp + FR_VAL];
    // fall-through was to 0; now jump like X
    #ifdef __CC65__
      zp.ipc = (uchar *)zp.mem[zp.fp + FR_RET];
    #else
      zp.ipc = g_loaded + zp.mem[zp.fp + FR_RET];
    #endif
    zp.fp = (char)zp.mem[zp.fp + FR_PREV];
    goto xnext;

  // frame-relative load fp+1 .. fp+8
  case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8':
    zp.acc = zp.mem[zp.fp + (op - '0')];
    goto xnext;

    // ---- parameter ops ----

    // TODO: cc65 '+' becomes 72 c include goto pnext
    // ~150++ cycles per instruction 
    // (/ 1000000 150) = 6666 ops/s (18K ok for asm 50c)
  case '+': zp.acc += PAR; goto pnext;
  case '-': zp.acc -= PAR; goto pnext;
  case '*': zp.acc *= PAR; goto pnext;
  case '/': if (PAR) zp.acc /= PAR; goto pnext;
  case '%': if (PAR) zp.acc %= PAR; goto pnext;

  case '&': zp.acc &= PAR; goto pnext;
  case '|': zp.acc |= PAR; goto pnext;
  case '^': zp.acc ^= PAR; goto pnext;

  case '[': zp.acc <<= PAR; goto pnext;
  case ']': zp.acc >>= PAR; goto pnext;
  case '{': zp.mem[*zp.ipc] <<= zp.acc; goto pnext;
  case '}': zp.mem[*zp.ipc] >>= zp.acc; goto pnext;

  case 'L': zp.acc = PAR;             goto pnext;
  case 'S': zp.mem[*zp.ipc] = zp.acc; goto pnext;
  case 'Z': zp.mem[*zp.ipc] = 0;      goto pnext;
    // TODO: char? and then explicit for indexing : _ ???

  #ifdef __CC65__
  case 'M': zp.acc = zp.mem[PAR] = (int)calloc(zp.acc, 1);    goto pnext;
  case 'F': free((void*)zp.mem[*zp.ipc]); zp.mem[*zp.ipc]= 0; goto pnext;
  #endif
  case '@': zp.acc = zp.mem[PAR]; goto pnext; // read  pointer
  case '!': zp.mem[PAR] = zp.acc; goto pnext; // write pointer

  #ifdef OPTMOREPARAMS
    // TODO: only useful for 3 or moe parameters...
    //   in linux/kernel/big projects
    //   7-10% take 3 parameters, and <5% take 4+ ...
  // case 'S': PAR = zp.acc;  lastv= *zp.ipc; goto pnext;
  //case 'V': zp.mem[++lastv]= PAR; goto pnext;
  //case ',': zp.mem[++lastv]= zp.acc; goto xnext;
  #endif

  case 'I': zp.acc= ++zp.mem[*zp.ipc]; goto pnext;
  case 'J': zp.acc= --zp.mem[*zp.ipc]; goto pnext;
//  case 'I': zp.acc= PAR++; goto pnext;
//  case 'J': zp.acc= PAR--; goto pnext;

  case 'U': zp.acc= (zp.mem[zp.ipc[0]]+= zp.mem[zp.ipc[1]]); zp.ipc+=2; goto xnext;

  case '<': zp.acc= zp.acc < PAR; goto pnext;
  case '=': zp.acc= zp.acc == PAR; goto pnext;
  case '>': zp.acc= zp.acc > PAR; goto pnext;

    // ---- no-arg ops ----
  case ' ':                  goto xnext;
  case '0': zp.acc = 0;      goto xnext;
  case '9': zp.acc = 0xffff; goto xnext;

// TODO: add keyword NOT !
  case '?': zp.acc = !zp.acc; goto xnext;
  case '~': zp.acc = ~zp.acc; goto xnext;

    // printers
  case 'K': zp.acc= getchar();    goto xnext; // key
  case 'E': putchar(zp.acc);      goto xnext; // putchar
  case 'N': putchar('\n');        goto xnext; // newline
  case 'P': printf("%s", zp.acc); goto xnext; // print strptr
  case 'O': putchar(PAR);         goto pnext; // out letter
  case 'W': printf("%u", PAR);    goto pnext; // write var
  case '.': printf("%u", zp.acc); goto xnext; // print num
  case '"': // inline string ends at 0      // print "const"
    while (*zp.ipc) putchar(*zp.ipc++);
    goto pnext; // skip the terminating 0

    // ---- relative jumps (uppercase, single opcode each) ----
    // B  <delta>  2 byte: AGAIN= unconditional backward
    // G  <delta>  2 byte: LOOP = conditional zp.acc>0 forward
    // 00-63       1 byte: IF   = conditional zp.acc=0 forward
    // relative jumps (byte code stats: jvm p-code etc)
    //     0-15: 70-75%, 0-31: 85-90%, 32+: 10-15

  case 'G': if ((int)zp.acc > 0) zp.ipc += *zp.ipc;    goto pnext;
  case 'B':                      zp.ipc -= *zp.ipc;    goto pnext;
    
  default:
    if (op >= 0x80) {
      if (op < 0xc0) { // 0b10

        // relative jumps
        if (!zp.acc)   zp.ipc += op & 0x3f;    goto xnext;

      } else { // 0b11

        // ---- vectorised call (one byte) ----
        // push cur_func; cur_func=vec; zp.mem[vec_link[vec]]=zp.ipc; zp.ipc=vptr[vec]

        // JSR: zp.fp chain in callee frame; fptab/call from loaded blob
        {
          char v = op & 0x3f;
          char nfp;
          word *callv;
          #ifdef __CC65__
          nfp = ((uchar *)(unsigned)zp.hdr.fp)[v];
          callv = (word *)(unsigned)zp.hdr.call;
          zp.mem[nfp + FR_PREV] = zp.fp;
          zp.mem[nfp + FR_RET] = (unsigned int)zp.ipc;
          zp.fp = nfp;
          zp.ipc = (uchar *)(unsigned)callv[v];
          #else
          nfp = g_loaded[zp.hdr.fp + v];
          callv = (word *)(g_loaded + zp.hdr.call);
          zp.mem[nfp + FR_PREV] = zp.fp;
          zp.mem[nfp + FR_RET] = (unsigned int)(zp.ipc - g_loaded);
          zp.fp = nfp;
          zp.ipc = g_loaded + callv[v];
          #endif
        }
        goto xnext;
      }
    }

    // ZJMP [op] (forward)
    // TODO: (op & 0xc0) == 0x80 for 0x80..0xbf
    printf("\n[IVM] unknown opcode 0x%02X '%c'\n", op, isprint(op)?op:0);
  }

 done:
  // save ZP global → process head
  memcpy(&p->h, &zp, sizeof(zp));
  return zp.acc;

#undef PAR
}

#ifdef TPDUMP
void dump_bytecode(void) {
  int i= 0, n= 0, wrap= 10;
  printf("\n--- BYTECODE (%d bytes) ---\n", bin_ptr);
  for (; i < bin_ptr; i++) {
    char c = bin[i];
    if (c < 32) {
      printf("#%02x ", c);
    }
    else if (c >= 0xC0) printf("(%02x)", c - 0xC0);
    else if (c >= constidx && c <= CONST_TOP) printf("C%02x ", c);

    else if (c == ' ')  printf("_   ");
    else if (c == '"') {
      putchar('\n'); do putchar(c); while((c=bin[++i]));
      printf("\\0\n"); n= 0; }
    else if (isprint(c) && c > ' ') {

      // single byte ops (incl. X Q return; 1-8 frame loads)
      if (strchr("#$(),.0123456789;<=>?ADEKNPQR~X", c)
        || islower(c)) {
        printf("%c   ", c);
        if (c == ';' || c == 'X' || c == 'Q') { n= 0; putchar('\n'); }
      }

      // takes at least one arg
      else 
        if (c == 'U') { // two args
          printf("U%02x%02x   ", bin[i+1], bin[i+2]); i+= 2;
        } else
          printf("%c%02x ", c, bin[++i]);
    }
    else printf("$%02x ", c);
    if (c == 'N' || ++n == wrap) { printf("\n"); n= 0; }
  }

  if (nvector) {
    printf("\n\n--- Vectors:\n");
    for (i = 0; i < nvector; i++)
      printf("  V%02x → offset %d\n", i, vectors[i]);
  }

  if (nvector) {
    printf("\n--- Symbols:\n");
    for (i = 0; i < nsym; i++) {
      Symbol *s = syms + i;
      printf("   %2d @ %02x nparams=%d =>%02x Ret:%02x :: %s\n", i, s->address,
        s->nparams, s->ret_slot, s->ret_addr_slot,
        s->name);
    }
  }

  if (nconst) {
    printf("--- Constants:\n");
    for (i = 0; i < nconst; i++)
      printf("  C%02x =%5d ($%04x)\n", 0x80 + i, const_pool[i], const_pool[i]);
  }

  printf("---------------------------\n\n");
}

void dump_blob(void) {
  BlobHeader *h;
  uchar *p;
  uint16_t *w;
  int i, j, ncall, nbytes;
  Symbol *s;

  h = (BlobHeader *)blob;
  ncall = nvector;
  nbytes = blob_len;

  printf("\n--- BLOB (%d bytes) ---\n", nbytes);
  printf("HEADER  overview\n");
  printf("  section     offset  bytes  slots\n");
  printf("  header      %5u  %5u  %5u\n", 0, (unsigned)sizeof(BlobHeader), (unsigned)(sizeof(BlobHeader) / 2));
  printf("  call        %5u  %5u  %5u\n", h->call, (unsigned)((ncall + 1) * 2), ncall + 1);
  printf("  const       %5u  %5u  %5u\n", h->con, (unsigned)(nconst * 2), nconst);
  printf("  fp          %5u  %5u  %5u\n", h->fp, nvector, nvector);
  printf("  code        %5u  %5u\n", h->code, bin_ptr);
  printf("  data        %5u\n", h->data);
  printf("  main        %5u  (code+%u)\n", h->main, h->main - h->code);
  printf("  memsz       %5u  cells\n", h->memsz);

  printf("CALL VECTORS  (%d + 0000)\n", ncall);
  w = (uint16_t *)(blob + h->call);
  for (i = 0; ; i++) {
    if (w[i] == 0) {
      printf("  [%2d]  %5u  end reloc\n", i, 0);
      break;
    }
    s = 0;
    for (j = 0; j < nsym; j++)
      if (syms[j].type == SYM_FUNC && syms[j].address == i) {
        s = &syms[j];
        break;
      }
    printf("  [%2d]  %5u  code+%-5u", i, w[i], w[i] - h->code);
    if (s)
      printf("  %s", s->name);
    printf("\n");
  }

  printf("CONSTANTS  (%d)\n", nconst);
  w = (uint16_t *)(blob + h->con);
  for (i = 0; i < nconst; i++)
    printf("  C%02x   %5u  ($%04x)\n", 0x80 + i, w[i], w[i]);

  printf("FRAME BASES  (%d)\n", nvector);
  p = blob + h->fp;
  for (i = 0; i < nvector; i++) {
    s = 0;
    for (j = 0; j < nsym; j++)
      if (syms[j].type == SYM_FUNC && syms[j].address == i) {
        s = &syms[j];
        break;
      }
    printf("  V%02x   fp=%-3u", i, p[i]);
    if (s) {
      printf("  %-12s", s->name);
      if (s->ret_addr_slot != 0xff)
        printf("  RETURN@%-3u", s->ret_addr_slot);
      if (s->ret_slot != 0xff)
        printf("  RETVAL@%-3u", s->ret_slot);
      for (j = 0; j < s->nparams; j++)
        printf("  p%d@%-3u", j, s->param_slot[j]);
    }
    printf("\n");
  }

  printf("MEM SLOTS  (all used, memsz=%u)\n", h->memsz);
  {
    int tmpn = 0;
    uchar *fb = blob + h->fp;
    for (i = 0; i < (int)h->memsz; i++) {
      int any = 0;
      int role = 0; // 1=RETURN 2=RETVAL 3=FP
      if (!slot_used[i])
        continue;
      printf("  %3u ", i);
      for (j = 0; j < nsym; j++) {
        if (syms[j].type == SYM_VAR && syms[j].address == i) {
          printf(" %s", syms[j].name);
          any = 1;
        }
      }
      for (j = 0; j < nvector; j++) {
        uchar base = fb[j];
        if (i == base + FR_RET)
          role = 1;
        else if (i == base + FR_VAL)
          role = 2;
        else if (i == base + FR_PREV)
          role = 3;
      }
      if (role == 1 && !any)
        printf(" *RETURN*");
      else if (role == 2 && !any)
        printf(" *RETVAL*");
      else if (role == 3)
        printf(" *FP*");
      else if (!any) {
        tmpn++;
        printf(" *TMP%d*", tmpn);
      }
      printf("\n");
    }
  }

  printf("CODE   @%5u  %5d bytes\n", h->code, bin_ptr);
  printf("DATA   @%5u\n", h->data);
  printf("---------------------------\n\n");
}

#else
#define dump_bytecode() (void)0
#define dump_blob() (void)0
#endif // TPDUMP

// ---------------------------------------------------------------------------
// file buffers global — cc65 stack cannot hold large locals
static char outname[256];

// ends_with(name, ".ivm") etc — C89 / cc65 friendly
static int ends_with(const char *s, const char *suf) {
  int n, m;
  n = (int)strlen(s);
  m = (int)strlen(suf);
  if (n < m) return 0;
  return strcmp(s + n - m, suf) == 0;
}

// foo.p / foo.pas → foo.ivm
static void ivm_name_from(char *out, const char *in) {
  int n, i;
  n = (int)strlen(in);
  strcpy(out, in);
  if (ends_with(in, ".pas")) {
    out[n - 3] = 'i';
    out[n - 2] = 'v';
    out[n - 1] = 'm';
  } else if (ends_with(in, ".p")) {
    out[n - 1] = 0;
    strcat(out, "ivm");
  } else {
    strcat(out, ".ivm");
  }
  (void)i;
}

static int load_file(const char *path, char *buf, int maxn) {
  FILE *f;
  int n;
  f = fopen(path, "rb");
  if (!f) {
    printf("cannot open %s\n", path);
    return -1;
  }
  n = (int)fread(buf, 1, (unsigned)maxn - 1, f);
  if (n == maxn-1) {
    printf("too large source - truncated\n");
    return -1;
  }
  fclose(f);
  buf[n] = 0;
  return n;
}

static int save_blob(const char *path) {
  FILE *f;
  f = fopen(path, "wb");
  if (!f) {
    printf("cannot write %s\n", path);
    return 0;
  }
  if ((int)fwrite(blob, 1, (unsigned)blob_len, f) != blob_len) {
    printf("write failed %s\n", path);
    fclose(f);
    return 0;
  }
  fclose(f);
  printf("wrote %s (%d bytes)\n", path, blob_len);
  return 1;
}

static int load_blob(const char *path) {
  FILE *f;
  int n;
  f = fopen(path, "rb");
  if (!f) {
    printf("cannot open %s\n", path);
    return 0;
  }
  n = (int)fread(blob, 1, MAX_BLOB_LEN, f);
  fclose(f);
  if (n <= 0) {
    printf("empty %s\n", path);
    return 0;
  }
  blob_len = n;
  printf("loaded %s (%d bytes)\n", path, blob_len);
  return 1;
}

static int run_loaded_blob(void) {
  Process *proc;
  unsigned int ret;

  loader(blob);
  proc = Nevm(blob);
  printf("--- RUN IVM (blob %d) ---\n", blob_len);
  ret = run_ivm(proc);
  printf("\n--- END ---\n");
  printf("acc = %u\n", ret);
  return (int)ret;
}

int main(int argc, char **argv) {
  const char *filename;
  int buf_idx;

  opt_level = 0;
  opt_pass = 0;
  filename = 0;
  {
    int ai;
    for (ai = 1; ai < argc; ai++) {
      if (argv[ai][0] == '-' && argv[ai][1] == 'O') {
        opt_level = argv[ai][2] ? argv[ai][2] - '0' : 1;
        if (opt_level < 1) opt_level = 1;
      } else if (!filename)
        filename = argv[ai];
    }
  }
  if (!filename) {
    printf("usage: %s [-O] file.p|file.pas|file.ivm\n", argv[0]);
    printf("  -O  second pass with packed frame bases\n");
    return 1;
  }

  // ----- run mode: pure .ivm -----
  if (ends_with(filename, ".ivm")) {
    if (!load_blob(filename))
      return 1;
    run_loaded_blob();
    return 0;
  }

  // ----- compile mode: .p / .pas -----
  if (!ends_with(filename, ".p") && !ends_with(filename, ".pas")) {
    printf("need .p, .pas, or .ivm: %s\n", filename);
    return 1;
  }

  buf_idx = load_file(filename, src_buf, MAX_SRC_LEN);
  if (buf_idx < 0) return 1;
  if (!buf_idx) {
    printf("empty source %s\n", filename);
    return 1;
  }

  // COMPILE — longjmp value is error message pointer (or 1 if bare)
  {
    int err = setjmp(fail_jmp);
    if (err) {
      #ifdef __CC65__
      fail_msg = (char*) err;
      #endif
      buf_idx = (int)src_pos;

      printf("\n%%ERROR: %s\n%s:%d.%d: ", fail_msg, filename,
        lnum, buf_idx-cur_line);
      src_pos = cur_line;
      do {
        if ((int)src_pos == buf_idx) printf(" >>> %s <<<ERROR<<< ", curr_tok.lexeme);
        putchar(NEXT);
      } while (C && C != '\n');
      printf("\n\n");
      return 1;
    }
  }
  if (!opt_level)
    memset(opt_base, 0, sizeof(opt_base));
  opt_pass = 0;
  if (!compile_pipeline()) {
    printf("\n>> FAIL on line %d\n", lnum);
    return 1;
  }
  if (opt_level > 0) {
    int i;
    for (i = 0; i < nvector; i++)
      func_code_prev[i] = func_code_bytes[i];
    opt_pass = 1;
    printf("--- opt:%d applying packed bases ---\n", opt_pass);
    if (!compile_pipeline()) {
      printf("\n>> FAIL on line %d\n", lnum);
      return 1;
    }
  }

  layout_process_mem();
  remap_bytecode();
  finalise();
  dump_bytecode();
  dump_blob();

  ivm_name_from(outname, filename);
  if (!save_blob(outname))
    return 1;

  // true round-trip: clear and reload from disk
  memset(blob, 0, sizeof(blob));
  blob_len = 0;
  if (!load_blob(outname))
    return 1;
  run_loaded_blob();
  return 0;
}


