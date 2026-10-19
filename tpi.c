// tpi.c - Tiny Pascal → IVM bytecode
//
// Notice:
//  1) needs to follow c89 so can compie on cc65
//  2) only define varaibles at start of { scope
//  3) use // comment - do NOT: // no no no
//  4) don't change any lines if not needed
//  5) char is unsigned on cc65 & oscar64 & clang

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


#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

// --- options
#define TPDUMP

#define MAX_SRC_LEN   2048
#define MAX_SYM_LEN   64
#define MAX_BIN_LEN   4096
#define MAX_SYMBOLS   64
#define MAX_SLOTS     256
#define MAX_CONSTS    64
#define MAX_VECTORS   64 // opcodes 0xC0 .. 0xFF
#define MAX_PATCHES   128
#define MAX_PARAMS    4 // max arguments per function
#define GLOBAL_LIMIT  32 // preferred global area 0-31
#define FRAME_BASE    32 // frames start here

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
#define TOK_PROGRAM  'P'
#define TOK_VAR      'V'
#define TOK_BEGIN    'B'
#define TOK_END      'E'
#define TOK_INTEGER  'I'
#define TOK_WRITE    'W' // WRITELN
#define TOK_FUNCTION 'F'
#define TOK_FOR      'O' // F taken by FUNCTION
#define TOK_TO       'T'
#define TOK_DO       'D'

typedef struct {
  char *name;
  TokenType type;
} Keyword;

// Real keywords + "combi" names for specials (printer only).
// Names starting with * never match lexer (non-alpha won't enter IDENT).
Keyword keywords[] = {
  {"PROGRAM",  TOK_PROGRAM},
  {"VAR",      TOK_VAR},
  {"BEGIN",    TOK_BEGIN},
  {"END",      TOK_END},
  {"INTEGER",  TOK_INTEGER},
  {"WRITELN",  TOK_WRITE},
  {"FUNCTION", TOK_FUNCTION},
  {"FOR",      TOK_FOR},
  {"TO",       TOK_TO},
  {"DO",       TOK_DO},
  // TODO: insert a 0 entry to end matching during tokenization
  {"*EOF*",    TOK_EOF},
  {"*ERROR*",  TOK_ERROR},
  {"*IDENT*",  TOK_IDENT},
  {"*NUM*",    TOK_NUM},
  {"*STRING*", TOK_STRING},
  {":=",       TOK_ASSIGN},
};
#define NKEYWORDS ((int)(sizeof keywords / sizeof keywords[0]))

typedef struct {
  TokenType type;
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

char tok_name_buf[4];
char* tok_name(TokenType t) {
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
  if (bin_ptr >= MAX_BIN_LEN - 1) {
    printf("\nERROR: bytecode buffer overflow\n");
    longjmp(fail_jmp, 1);
  }
  bin[bin_ptr++] = c;
}

void emitop(char op) {
  emit(op);
  // TODO: make bitmask for detection
  if (op == 'I' || op == 'J' || op == '0' || op == '9' || op == '~')
    acc_invalidate();
}

void emitop2(char op, char arg) {
  emitop(op);
  emit(arg);
  if (op == 'S' || op == 'L')
    acc_set_slot(arg);
  // TODO: make bitmask for detection
  else if (op == '+' || op == '-' || op == '*' || op == '/' ||
           op == '&' || op == '|' || op == '^')
    acc_invalidate();
}

// Generate function return (pascal: exit)
//
// - If ACC already holds the return-value slot, emit 0 (return without reload).
// - Otherwise emit ';' (load ret slot, then return).
//
// Opcode \0  = return, keep ACC.
// Opcode ';' = ACC <- ret_val, then return
// Opcode 'Q' = quit program (C: exit).
void emitreturn(char ret_slot) {
  if (acc_valid && acc_slot == ret_slot) {
    // "poophole": drop END "S ret_slot" — ACC is == value already
    if (bin_ptr >= 2 && bin[bin_ptr - 2] == 'S' && bin[bin_ptr - 1] == ret_slot)
      bin_ptr -= 2;
    emit(0); // return, keep ACC
  } else
    emitop(';'); // ACC <- ret_val, then return
}

void add_patch(int offset, int kind, int value) {
  if (npatch >= MAX_PATCHES) {
    printf("\nERROR: too many patches\n");
    longjmp(fail_jmp, 1);
  }
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
int next_global = 0; // next candidate in 0-31
int next_frame  = FRAME_BASE; // next candidate from 32

// TODO: char
int alloc_global(void) {
  // Prefer 0-31; if exhausted continue from next_frame, skipping used slots
  while (next_global < GLOBAL_LIMIT) {
    if (!slot_used[next_global]) {
      slot_used[next_global] = 1;
      return next_global++;
    }
    next_global++;
  }
  // fall into frame area
  while (next_frame < MAX_SLOTS) {
    if (!slot_used[next_frame]) {
      slot_used[next_frame] = 1;
      return next_frame++;
    }
    next_frame++;
  }
  printf("\nERROR: out of slots\n");
  longjmp(fail_jmp, 1);
  return -1;
}

// TODO: char
int alloc_frame_slot(void) {
  while (next_frame < MAX_SLOTS) {
    if (!slot_used[next_frame]) {
      slot_used[next_frame] = 1;
      return next_frame++;
    }
    next_frame++;
  }
  printf("\nERROR: out of frame slots\n");
  longjmp(fail_jmp, 1);
  return -1;
}

// Temporaries for expression evaluation also come from the frame area

// TODO: char
int alloc_temp(void) {
  return alloc_frame_slot();
}

// ---------------------------------------------------------------------------
// Constants (separate array, referenced as 0x80+n)
// ---------------------------------------------------------------------------

unsigned int const_pool[MAX_CONSTS];

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


int nconst = 0;

int add_const(unsigned int v) {
  int i;
  for (i = 0; i < nconst; i++)
    if (const_pool[i] == v) return i;
  if (nconst >= MAX_CONSTS) {
    printf("\nERROR: constant pool full\n");
    longjmp(fail_jmp, 1);
  }
  const_pool[nconst] = v;
  return nconst++;
}

// Emit a load of a constant: L (0x80+idx)
void emit_load_const(unsigned int v) {
  int idx;
  if (v == 0) { emitop('0'); return; }
  if (v == (unsigned int)-1) { emitop('9'); return; }
  idx = add_const(v);
  emitop('L');
  emit((char)(0x80 | (idx & 0x7F)));
}

// ---------------------------------------------------------------------------
// Vectors (function entry points)
// ---------------------------------------------------------------------------
int vectors[MAX_VECTORS]; // bytecode offset during compile
char *vptr[MAX_VECTORS]; // resolved absolute pointers (finalise)
char vec_link[MAX_VECTORS]; // mem slot: return NEXT (ipc) per function
char vec_val[MAX_VECTORS]; // mem slot: return VALUE per function
int nvector = 0;
int main_offset = 0;
char *main_entry = 0;
char is_code_addr[MAX_CONSTS];

// threaded call chain: which function is active (0xFF = main)
#define MAX_CALL_DEPTH 8
#define FUNC_MAIN      ((char)0xFF)
char cur_func; // TODO: maybe register
char fstack[MAX_CALL_DEPTH];
char fsp;

int add_vector(int entry_offset) {
  if (nvector >= MAX_VECTORS) {
    printf("\nERROR: too many functions (max %d)\n", MAX_VECTORS);
    longjmp(fail_jmp, 1);
  }
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
int nsym = 0;

void add_symbol(const char* name, SymbolType type, int address) {
  if (nsym >= MAX_SYMBOLS) {
    printf("\nERROR: symbol table full\n");
    longjmp(fail_jmp, 1);
  }
  strcpy(syms[nsym].name, name);
  syms[nsym].type = type;
  syms[nsym].address = address;
  syms[nsym].nparams = 0;
  syms[nsym].ret_slot = -1;
  syms[nsym].ret_addr_slot = -1;
  nsym++;
}

int lookup_symbol(const char* name, SymbolType* type, int* address) {
  int i;
  for (i = nsym - 1; i >= 0; i--) {
    if (strcmp(syms[i].name, name) == 0) {
      *type = syms[i].type;
      *address = syms[i].address;
      return i;
    }
  }
  return -1;
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
int expression(void);
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

  if (!C) {
    curr_tok.type = TOK_EOF;
    strcpy(curr_tok.lexeme, "EOF");
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
    strcpy(curr_tok.lexeme, ":=");
    return;
  }

  // single-char: token type is the character itself
  curr_tok.lexeme[0] = C;
  curr_tok.lexeme[1] = 0;
  if (strchr(";:+-.,()*/", C)) {
    curr_tok.type = C;
    NEXT;
    return;
  }
  printf("\n%c <---- ERROR: unknown character\n", C);
  curr_tok.type = TOK_ERROR;
  longjmp(fail_jmp, 1);
}

void expect(TokenType expected) {
  if (curr_tok.type == expected) { next_token(); return; }
  printf("\n%s <---- ERROR: Expected %s but found %s\n",
         curr_tok.lexeme, tok_name(expected), tok_name(curr_tok.type));
  longjmp(fail_jmp, 1);
}

// ---------------------------------------------------------------------------
// Operand descriptors (in-situ codegen, no peephole pass)
// kind: 0=ACC 1=MEM 2=IMM
// Passed as separate scalar parameters to eliminate struct/pointer overhead on cc65.
// ---------------------------------------------------------------------------
#define OP_ACC 0
#define OP_MEM 1
#define OP_IMM 2

void force_acc(char kind, unsigned int val) {
  int idx;
  if (kind == OP_ACC)
    return;
  if (kind == OP_MEM) {
    if (acc_valid && acc_slot == (char)val)
      return;
    emitop2('L', (char)val);
    return;
  }
  // OP_IMM
  if (val == 0) { emitop('0'); return; }
  if (val == (unsigned int)-1) { emitop('9'); return; }
  idx = add_const(val);
  emitop('L');
  emit((char)(0x80 | (idx & 0x7F)));
  acc_invalidate();
}

void emit_add_to_acc(char kind, unsigned int val) {
  int idx;
  char slot;
  if (kind == OP_MEM) {
    slot = (char)val;
    // ACC holds this slot: need mem copy for +slot. S unless just stored.
    if (acc_valid && acc_slot == slot) {
      if (!(bin_ptr >= 2 && bin[bin_ptr-2] == 'S' && bin[bin_ptr-1] == slot))
        emitop2('S', slot);
    }
    emitop2('+', slot);
    return;
  }
  if (kind == OP_IMM) {
    if (val == 0) return;
    idx = add_const(val);
    emitop2('+', (char)(0x80 | (idx & 0x7F)));
    return;
  }
}

int primary(char *out_kind, unsigned int *out_val) {
  char name[MAX_SYM_LEN + 1];
  Symbol *sym;
  int i, pi;

  if (curr_tok.type == TOK_NUM) {
    *out_kind = OP_IMM;
    *out_val = (unsigned int)curr_tok.value;
    next_token();
    return 1;
  }

  if (curr_tok.type == TOK_IDENT) {
    sym = NULL;
    strcpy(name, curr_tok.lexeme);
    for (i = nsym - 1; i >= 0; i--) {
      if (strcmp(syms[i].name, name) == 0) {
        if (syms[i].type == SYM_FUNC) { sym = &syms[i]; break; }
        if (!sym) sym = &syms[i];
      }
    }
    if (!sym) {
      printf("\n%s <---- ERROR: Undeclared identifier\n", name);
      longjmp(fail_jmp, 1);
    }
    next_token();

    if (sym->type == SYM_VAR && curr_tok.type == '(') {
      for (i = nsym - 1; i >= 0; i--) {
        if (strcmp(syms[i].name, name) == 0 && syms[i].type == SYM_FUNC) {
          sym = &syms[i];
          break;
        }
      }
    }

    if (sym->type == SYM_FUNC) {
      EXPECT('(');
      // store all but last param; last stays in ACC for callee to S at entry
      for (pi = 0; pi < sym->nparams; pi++) {
        if (pi > 0) EXPECT(',');
        if (!expression()) return 0;
        if (pi < sym->nparams - 1)
          emitop2('S', sym->param_slot[pi]);
      }
      EXPECT(')');
      if (sym->address >= nvector) {
        printf("\nERROR: bad vector index for %s\n", name);
        longjmp(fail_jmp, 1);
      }
      emit((char)(0xC0 | (sym->address & 0x3F)));
      acc_invalidate(); // return value in ACC, not a known slot
      *out_kind = OP_ACC;
      *out_val = 0;
      return 1;
    }

    *out_kind = OP_MEM;
    *out_val = (unsigned int)sym->address;
    return 1;
  }

  printf("\n%s <---- ERROR: Expected expression\n", curr_tok.lexeme);
  longjmp(fail_jmp, 1);
  return 0;
}

// Expression → result in ACC
int expression(void) {
  char left_kind, right_kind;
  unsigned int left_val, right_val;
  char op;

  if (!primary(&left_kind, &left_val)) return 0;

  while (curr_tok.type == '+' || curr_tok.type == '-') {
    op = (curr_tok.type == '-') ? '-' : '+';
    next_token();

    // Spill a left-hand function result before evaluating the RHS.
    if (left_kind == OP_ACC) {
      int tmp = alloc_temp();
      emitop2('S', (char)tmp);
      left_kind = OP_MEM;
      left_val = (unsigned int)(unsigned char)tmp;
    }

    if (!primary(&right_kind, &right_val)) return 0;

    // Constant folding.
    if (left_kind == OP_IMM && right_kind == OP_IMM) {
      if (op == '-')
        left_val -= right_val;
      else
        left_val += right_val;
      continue;
    }

    // A function call on the right returns its value in ACC.
    // Spill it, restore the left operand, then apply the operator.
    if (right_kind == OP_ACC) {
      int tmp = alloc_temp();
      emitop2('S', (char)tmp);
      force_acc(left_kind, left_val);
      emitop2(op, (char)tmp);
      left_kind = OP_ACC;
      left_val = 0;
      continue;
    }

    force_acc(left_kind, left_val);

    if (op == '-')
      emitop2('-', (char)(right_kind == OP_MEM
        ? right_val
        : (0x80 | (add_const(right_val) & 0x7F))));
    else
      emit_add_to_acc(right_kind, right_val);

    left_kind = OP_ACC;
    left_val = 0;
  }

  force_acc(left_kind, left_val);
  return 1;
}



// ---------------------------------------------------------------------------
// Statement
// ---------------------------------------------------------------------------
int statement(void) {
  // assignment
  if (curr_tok.type == TOK_IDENT) {
    char target[MAX_SYM_LEN + 1];
    Symbol* sym = lookup_sym(curr_tok.lexeme); // ?? safe
    strcpy(target, curr_tok.lexeme);
    if (!sym || sym->type != SYM_VAR) {
      printf("\n%s <---- ERROR: Unknown variable\n", target);
      longjmp(fail_jmp, 1);
    }
    EXPECT(TOK_IDENT);
    EXPECT(TOK_ASSIGN);
    if (!expression()) return 0;
    emitop2('S', sym->address);
    EXPECT(';');
  }
  // WRITELN
  else if (curr_tok.type == TOK_WRITE) {
    int i;
    EXPECT(TOK_WRITE);
    EXPECT('(');
    if (curr_tok.type == TOK_STRING) {
      emitop('"');
      for (i = 0; curr_tok.lexeme[i]; i++)
        emit(curr_tok.lexeme[i]);
      emit(0);
      EXPECT(TOK_STRING);
    } else {
      if (!expression()) return 0;
      emitop('.');
    }
    emitop('N');
    EXPECT(')');
    EXPECT(';');
  }
  // FOR
  else if (curr_tok.type == TOK_FOR) {
    char ctrl_name[MAX_SYM_LEN + 1];
    int high_slot, loop_start, exit_patch;
    Symbol* ctrl;

    EXPECT(TOK_FOR);
    if (curr_tok.type != TOK_IDENT) {
      printf("\nERROR: expected control variable\n");
      longjmp(fail_jmp, 1);
    }
    strcpy(ctrl_name, curr_tok.lexeme);
    ctrl = lookup_sym(ctrl_name);
    if (!ctrl || ctrl->type != SYM_VAR) {
      printf("\n%s <---- ERROR: undeclared control variable\n", ctrl_name);
      longjmp(fail_jmp, 1);
    }
    EXPECT(TOK_IDENT);
    EXPECT(TOK_ASSIGN);

    if (!expression()) return 0;
    emitop2('S', ctrl->address);

    EXPECT(TOK_TO);
    high_slot = alloc_temp();
    if (!expression()) return 0;
    emitop2('S', high_slot);

    EXPECT(TOK_DO);
    loop_start = bin_ptr;

    // test: acc = ctrl - high;  if (acc > 0) exit
    emitop2('L', ctrl->address);
    emitop2('-', high_slot);
    exit_patch = bin_ptr;
    emitop('G'); emit(0); // G <delta> conditional forward (acc > 0)

    if (!statement()) return 0;

    // increment
    // TODO: make one op 'I'
    emitop2('L', ctrl->address);
    emitop('I');
    emitop2('S', ctrl->address);

    // backward jump B <delta>
    {
      int delta = (bin_ptr + 2) - loop_start;
      if (delta > 255) {
        printf("\nFOR body too large\n");
        longjmp(fail_jmp, 1);
      }
      emitop('B'); emit((char)delta);
    }
    // patch forward exit
    {
      int delta = bin_ptr - (exit_patch + 2);
      bin[exit_patch + 1] = (char)delta;
    }
  }
  else {
    printf("\n%s <---- ERROR: expected statement\n", curr_tok.lexeme);
    longjmp(fail_jmp, 1);
  }
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
void compile_function(void) {
  char func_name[MAX_SYM_LEN + 1];
  char param_name[MAX_SYM_LEN + 1];
  int ret_addr_slot, ret_val_slot;
  int entry_offset, vec_idx;
  int nparams, i;
  char param_slots[MAX_PARAMS];
  Symbol* fsym;

  // FUNCTION name (
  next_token(); // consume FUNCTION
  strcpy(func_name, curr_tok.lexeme);
  EXPECT(TOK_IDENT);
  EXPECT('(');

  // frame: ret_addr, ret_val, then params
  ret_addr_slot = alloc_frame_slot();
  ret_val_slot  = alloc_frame_slot();

  // parse parameter list: name : INTEGER [, name : INTEGER]*
  nparams = 0;
  if (curr_tok.type == TOK_IDENT) {
    while (1) {
      if (nparams >= MAX_PARAMS) {
        printf("\nERROR: too many parameters (max %d)\n", MAX_PARAMS);
        longjmp(fail_jmp, 1);
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

  // BEGIN
  EXPECT(TOK_BEGIN);

  // the entry point is right here
  entry_offset = bin_ptr;
  vec_idx = add_vector(entry_offset);
  vec_link[vec_idx] = ret_addr_slot; // NEXT pointer slot (compact, one per fn)
  vec_val[vec_idx]  = ret_val_slot; // return VALUE slot
  fsym->address = vec_idx;

  // last param arrives in ACC; store into its slot (call sites omit that S)
  if (nparams > 0) {
    emitop2('S', param_slots[nparams - 1]);
    acc_set_slot(param_slots[nparams - 1]);
  } else
    acc_invalidate();

  // body
  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) longjmp(fail_jmp, 1);
  }
  EXPECT(TOK_END);
  EXPECT(';');

  // return: 0 if ACC already is return value, else ';' (reload then return)
  emitreturn(ret_val_slot);
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
  nvector = 0;
  npatch = 0;
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
    int xslot = alloc_global();
    add_symbol("X", SYM_VAR, xslot);
  }

  next_token();
  EXPECT(TOK_PROGRAM);
  EXPECT(TOK_IDENT);
  EXPECT(';');

  // declarations
  while (curr_tok.type == TOK_VAR || curr_tok.type == TOK_FUNCTION) {
    if (curr_tok.type == TOK_VAR) {
      next_token();
      while (curr_tok.type == TOK_IDENT) {
        int slot = alloc_global();
        add_symbol(curr_tok.lexeme, SYM_VAR, slot);
        last_var_slot = slot;
        EXPECT(TOK_IDENT);
        EXPECT(':');
        EXPECT(TOK_INTEGER);
        EXPECT(';');
      }
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
    if (!statement()) return 0;
  }
  EXPECT(TOK_END);
  EXPECT('.');

  // leave last global in acc (compatibility)
  if (last_var_slot >= 0)
    emitop2('L', last_var_slot);

  emitop('Q'); // quit program
  return 1;
}

// ---------------------------------------------------------------------------
// Final layout: copy constants into mem[0x80..] and apply any patches
// ---------------------------------------------------------------------------
void finalise(void) {
  int i;

  // Constants at 0x80+idx (plain values and code offsets for return IPs).
  // Code offsets stay relative to bin[]; only vectors/main become direct
  // pointers — the code image does not move after this.
  for (i = 0; i < nconst; i++)
    mem[0x80 + i] = const_pool[i];

  // TODO: seems to have forgotten my memory layout design:
  // 
  // - all code "loaded from disk" should be a blob
  // - during loaded all vectors up to $0000 are relocated
  //   by adding actual start address. $0000 is left intact


// PROGRAM MEMORY LAYOUT
// ---------------------
// There may be several programs at any time and any
// program could have several instances. So, each
// execution of a process will depend on a few zero
// page globals. The interpretive loop runs for at most
// 4 ms. These are restored/saved by the multi-tasker.
//  
//   // - pure byte addressing program
//   char      *program;
//   // - program as vectors (this 
//   #define   VPROGRAM ((uint16_t*)program)
//
//   // - slots of vars/constants indexed by 0-255 max
//   uint16_t  *vars; 
//   
//   // - program running info
//   char      *ipc;


// PROGRAM VECTORS
// ---------------
// This is a vector interpretation of program bytes.
// Each is 2 bytes uint16_t relative to 0==program
// Patch at load time to actual addresses, until
// a final $0000 subroutine vector.

// 0: 1 byte index C=start of constants, N=number
// 1: P=start of actual program
// 2: subroutine code vectors... (max 64)
//    ..... relocated subs
// C: constant to copy start
//    ..... relocated constants! (like str ptr)
//    $0000 = end marker, relocate till here
//    .....
// P: program start
//    ...init...
//    jmp main (M)
//    ...subs...
// M: main program
//    rts
// S: strings/data referre to by relocated constants
//    ....


  // Function vectors → direct pointers (no bin+ at each CALL)
  for (i = 0; i < nvector; i++)
    vptr[i] = bin + vectors[i];

  // Main entry → direct pointer (replaces absolute G jump at program start)
  main_entry = bin + main_offset;
}

// ---------------------------------------------------------------------------
// Host-side IVM interpreter
// ---------------------------------------------------------------------------

unsigned int run_ivm(void) {
  // Classic single-switch + goto dispatch (ivm.c style).
  // - opcode 0   = halt (program end) — no explicit pre-test in the loop
  // - opcode ';' = return from function (following byte = slot with return IP)
  // - parameter ops use PAR then goto pnext (pnext skips the arg byte)
  // - zero-arg ops goto next

  // faster than ZP?
  register char *ipc;
  register unsigned int acc;
  register char op;

  // Start at resolved main entry
  ipc = main_entry;
  acc = 0;
  cur_func = FUNC_MAIN;
  fsp = 0;

#ifdef __CC65__
#define PAR mem[*ipc]
#else
#define PAR mem[(unsigned char)(*ipc)]
#endif

  goto next;

 pnext:
  ++ipc;
 next:
  op = *ipc++;
  switch (op) {

  case 'Q': // quit program (C exit)
    goto done;

    // 0 = return, keep ACC;  ';' = ACC <- ret_val, then return
  case 0:
  case ';': {
    char link;
    if (cur_func == FUNC_MAIN) {
      printf("\n[IVM] return from main\n");
      goto done;
    }
    if (op == ';')
      acc = mem[vec_val[cur_func]];
    link = vec_link[cur_func];
#ifdef __CC65__
    ipc = (char *)mem[link];
#else
    ipc = bin + mem[link];
#endif
    if (fsp <= 0) {
      printf("\n[IVM] call chain underflow\n");
      goto done;
    }
    cur_func = fstack[--fsp];
    goto next;
  }

    // ---- parameter ops ----
  case '+': acc += PAR; goto pnext;
  case '-': acc -= PAR; goto pnext;
  case '*': acc *= PAR; goto pnext;
  case '/': if (PAR) acc /= PAR; goto pnext;
  case '&': acc &= PAR; goto pnext;
  case '|': acc |= PAR; goto pnext;
  case '^': acc ^= PAR; goto pnext;
  case 'L': acc  = PAR; goto pnext;
  case 'S': PAR = acc;  goto pnext;

    // ---- no-arg ops ----
  case ' ': goto next;
  case '0': acc = 0;      goto next;
  case '9': acc = 0xffff; goto next;
  case 'I': ++acc;        goto next;
  case 'J': --acc;        goto next;
  case '~': acc = ~acc;   goto next;
  case '.': printf("%u", acc);  goto next;
  case 'N': putchar('\n');      goto next;
  case 'E': putchar((char)acc); goto next;

  case '"': // inline string ends at 0
    while (*ipc) putchar(*ipc++);
    goto pnext; // skip the terminating 0

    // ---- relative jumps (uppercase, single opcode each) ----
    // G <delta>  conditional forward if acc > 0
  case 'G':
    if ((int)acc > 0) ipc += (unsigned char)*ipc;
    goto pnext;
    // B <delta>  unconditional backward
  case 'B':
    ipc -= (unsigned char)*ipc;
    goto pnext;

    // ---- vectorised call (one byte) ----
    // push cur_func; cur_func=vec; mem[vec_link[vec]]=ipc; ipc=vptr[vec]
  default:
    if ((op & 0xc0) == 0xc0) {
      int vec = op & 0x3f;
      if (vec >= nvector) {
        printf("\n[IVM] bad vector %d\n", vec);
        longjmp(fail_jmp, 1);
      }
      if (fsp >= MAX_CALL_DEPTH) {
        printf("\n[IVM] call chain overflow\n");
        longjmp(fail_jmp, 1);
      }
      fstack[fsp++] = cur_func;
      cur_func = (char)vec;
#ifdef __CC65__
      mem[vec_link[vec]] = (unsigned int)ipc;
#else
      mem[vec_link[vec]] = (unsigned int)(ipc - bin);
#endif
      ipc = vptr[vec];
      goto next;
    }
    printf("\n[IVM] unknown opcode 0x%02X\n", op);
  }

 done:
  return acc;
#undef PAR
}

#ifdef TPDUMP
void dump_bytecode(void) {
  int i= 0, n= 0, wrap= 10;
  printf("\n--- BYTECODE (%d bytes) ---\n", bin_ptr);
  for (; i < bin_ptr; i++) {
    char c = bin[i];
    if (!c) { n= 0; printf("\\0  \n"); }
    else if (c >= 0xC0) printf("(%02x)", c - 0xC0);
    else if (c >= 0x80) printf("<%02x>", c - 0x80);
    else if (c == ' ')  printf("_   ", c);
    else if (c == '"') {
      putchar('\n'); do putchar(c); while((c=bin[++i]));
      printf("\\0\n"); n= 0; }
    else if (isprint(c) && c > ' ') {
      // single byte ops
      if (strchr("#$(),.0123456789;<=>?ADEKNPRU~", c)
        || islower(c)) {
        printf("%c   ", c);
        if (c == ';') { n= 0; putchar('\n'); }
      } else // takes at least one arg
        printf("%c%02x ", c, bin[++i]);
    }
    else printf("$%02x ", c);
    if (++n == wrap) { printf("\n"); n= 0; }
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
#else
#define dump_bytecode() (void)0
#endif // TPDUMP

// ---------------------------------------------------------------------------
int main(void) {
  int ch, ret, buf_idx = 0;

  // READ whole "file"
  memset(src_buf, 0, MAX_SRC_LEN);
  while ((ch = getchar()) != EOF && ch != 0xff && buf_idx < MAX_SRC_LEN - 1)
    src_buf[buf_idx++] = (char)ch;
  src_buf[buf_idx] = 0;
  if (!buf_idx) return 0;

  // COMPILE
  if (setjmp(fail_jmp) || !compile_pipeline()) {
    // ERROR: show line
    unsigned int err_pos = src_pos;
    printf(">> FAIL on line %d:\n", lnum);
    src_pos = cur_line;
    do {
      if (src_pos == err_pos) printf(" <<<ERROR<<< ");
      putchar(NEXT);
    } while (C && C != '\n');
    putchar('\n');
    return 1;
  }

  // "LOAD"
  finalise();
  dump_bytecode();

  // RUN
  printf("--- RUN IVM (%d bytes, %d vectors, %d consts) ---\n",
    bin_ptr, nvector, nconst);

  ret= run_ivm();

  printf("--- END ---\n");
  printf("acc = %u\n", ret);

  return 0;
}
