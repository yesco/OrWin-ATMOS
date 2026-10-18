// tpi.c - Tiny Pascal → IVM bytecode
//
// Notice:
//  1) needs to follow c89 so can compie on cc65
//  2) only define varaibles at start of { scope
//  3) use // comment - do NOT: /* no no no */
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
// ivm/asm = (/ 2152 127) = 16x slower, but improved 3x!

// - moving a print out... lolshoulnd' ffect any 
// ivm: 1000: (/ (- 6150038 3985079) 1000) = 2164 start
//                  


#include <stdio.h>
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
#define MAX_VECTORS   64          // opcodes 0xC0 .. 0xFF
#define MAX_PATCHES   128
#define MAX_PARAMS    4           // max arguments per function
#define GLOBAL_LIMIT  32          // preferred global area 0-31
#define FRAME_BASE    32          // frames start here

// ---------------------------------------------------------------------------
// Tokens
// ---------------------------------------------------------------------------
typedef enum {
  TOK_EOF, TOK_ERROR, TOK_PROGRAM, TOK_VAR, TOK_BEGIN, TOK_END,
  TOK_INTEGER, TOK_IDENT, TOK_NUM, TOK_ASSIGN, TOK_SEMI,
  TOK_COLON, TOK_PLUS, TOK_DOT, TOK_WRITE, TOK_STRING,
  TOK_LPAREN, TOK_RPAREN, TOK_FUNCTION,
  TOK_FOR, TOK_TO, TOK_DO, TOK_COMMA
} TokenType;

const char* TOKEN_NAMES[] = {
  "EOF", "ERROR", "PROGRAM", "VAR", "BEGIN", "END",
  "INTEGER", "IDENT", "NUM", "ASSIGN", "SEMI", "COLON",
  "PLUS", "DOT", "WRITELN", "STRING", "LPAREN", "RPAREN", "FUNCTION",
  "FOR", "TO", "DO", "COMMA"
};

typedef struct {
  TokenType type;
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

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
  int offset;          // location inside bin[] that needs a value
  int kind;            // 0 = const index, 1 = vector index, …
  int value;           // the value to write (or index)
} Patch;

Patch patches[MAX_PATCHES];
int npatch = 0;

void emit(char c) {
  if (bin_ptr >= MAX_BIN_LEN - 1) {
    printf("\nERROR: bytecode buffer overflow\n");
    longjmp(fail_jmp, 1);
  }
  bin[bin_ptr++] = c;
}

void emit2(char op, char arg) {
  emit(op);
  emit((char)arg);
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
unsigned int mem[MAX_SLOTS];          // runtime memory
char slot_used[MAX_SLOTS];            // allocation map
int next_global = 0;                  // next candidate in 0-31
int next_frame  = FRAME_BASE;         // next candidate from 32

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
int alloc_temp(void) {
  return alloc_frame_slot();
}

// ---------------------------------------------------------------------------
// Constants (separate array, referenced as 0x80+n)
// ---------------------------------------------------------------------------
unsigned int const_pool[MAX_CONSTS];
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
  if (v == 0) { emit('0'); return; }
  if (v == (unsigned int)-1) { emit('9'); return; }
  idx = add_const(v);
  emit('L');
  emit((char)(0x80 | (idx & 0x7F)));
}

// ---------------------------------------------------------------------------
// Vectors (function entry points)
// ---------------------------------------------------------------------------
int vectors[MAX_VECTORS];             // bytecode offset of each function
int nvector = 0;

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
  char address;          // slot number (var) or vector index (func)
  char nparams;          // number of parameters
  char param_slot[MAX_PARAMS]; // slots for each parameter
  char ret_slot;         // for functions: slot of the return value
  char ret_addr_slot;    // for functions: slot that holds return ipc
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
    if (isspace(C)) {
      if (C == '\n') lnum++;
      NEXT;
      continue;
    }
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

  if (isalpha(C)) {
    len = 0;
    while (isalnum(C) && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = C;
      NEXT;
    }
    curr_tok.lexeme[len] = 0;
    for (i = 0; i < len; i++)
      curr_tok.lexeme[i] = toupper(curr_tok.lexeme[i]);

    if      (strcmp(curr_tok.lexeme, "PROGRAM")  == 0) curr_tok.type = TOK_PROGRAM;
    else if (strcmp(curr_tok.lexeme, "VAR")      == 0) curr_tok.type = TOK_VAR;
    else if (strcmp(curr_tok.lexeme, "BEGIN")    == 0) curr_tok.type = TOK_BEGIN;
    else if (strcmp(curr_tok.lexeme, "END")      == 0) curr_tok.type = TOK_END;
    else if (strcmp(curr_tok.lexeme, "INTEGER")  == 0) curr_tok.type = TOK_INTEGER;
    else if (strcmp(curr_tok.lexeme, "WRITELN")  == 0) curr_tok.type = TOK_WRITE;
    else if (strcmp(curr_tok.lexeme, "FUNCTION") == 0) curr_tok.type = TOK_FUNCTION;
    else if (strcmp(curr_tok.lexeme, "FOR")      == 0) curr_tok.type = TOK_FOR;
    else if (strcmp(curr_tok.lexeme, "TO")       == 0) curr_tok.type = TOK_TO;
    else if (strcmp(curr_tok.lexeme, "DO")       == 0) curr_tok.type = TOK_DO;
    else curr_tok.type = TOK_IDENT;
    return;
  }

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

  if (C == ':' && src_buf[src_pos + 1] == '=') {
    NEXT; NEXT;
    curr_tok.type = TOK_ASSIGN;
    strcpy(curr_tok.lexeme, ":=");
    return;
  }

  curr_tok.lexeme[0] = C;
  curr_tok.lexeme[1] = 0;
  switch (C) {
    case ';': curr_tok.type = TOK_SEMI; break;
    case ':': curr_tok.type = TOK_COLON; break;
    case '+': curr_tok.type = TOK_PLUS; break;
    case '.': curr_tok.type = TOK_DOT; break;
    case '(': curr_tok.type = TOK_LPAREN; break;
    case ')': curr_tok.type = TOK_RPAREN; break;
    case ',': curr_tok.type = TOK_COMMA; break;
    default:
      curr_tok.type = TOK_ERROR;
      printf("\n%c <---- ERROR: unknown character\n", C);
      longjmp(fail_jmp, 1);
  }
  NEXT;
}

void expect(TokenType expected) {
  if (curr_tok.type == expected) { next_token(); return; }
  printf("\n%s <---- ERROR: Expected %s but found %s\n",
         curr_tok.lexeme, TOKEN_NAMES[expected], TOKEN_NAMES[curr_tok.type]);
  longjmp(fail_jmp, 1);
}

// ---------------------------------------------------------------------------
// Expression → result in acc
// ---------------------------------------------------------------------------
int expression(void) {
  char name[MAX_SYM_LEN + 1];

  if (curr_tok.type == TOK_NUM) {
    emit_load_const((unsigned int)curr_tok.value);
    next_token();
  }
  else if (curr_tok.type == TOK_IDENT) {
    Symbol* sym = NULL;
    strcpy(name, curr_tok.lexeme);
    // Peek: if next token will be '(', prefer the function symbol
    {
      int i;
      int want_func = 0;
      // crude peek at source for '(' after the identifier
      // (the real next_token has not been called yet)
      // Better: look up both and decide after seeing the next token.
      for (i = nsym - 1; i >= 0; i--) {
        if (strcmp(syms[i].name, name) == 0) {
          if (syms[i].type == SYM_FUNC) { sym = &syms[i]; break; }
          if (!sym) sym = &syms[i];   // keep first (most recent) var
        }
      }
    }
    if (!sym) {
      printf("\n%s <---- ERROR: Undeclared identifier\n", name);
      longjmp(fail_jmp, 1);
    }
    next_token();

    // If we found a var but the next token is '(', look again for a function
    if (sym->type == SYM_VAR && curr_tok.type == TOK_LPAREN) {
      int i;
      for (i = nsym - 1; i >= 0; i--) {
        if (strcmp(syms[i].name, name) == 0 && syms[i].type == SYM_FUNC) {
          sym = &syms[i];
          break;
        }
      }
    }

    if (sym->type == SYM_FUNC) {
      // Stackless multi-arg call:
      //   1. eval each arg → S param_slot[i]
      //   2. store return-IP into callee ret_addr_slot
      //   3. vector call 0xC0|V
      //   4. L ret_slot
      int ret_ip_const, pi;
      EXPECT(TOK_LPAREN);

      for (pi = 0; pi < sym->nparams; pi++) {
        if (pi > 0) EXPECT(TOK_COMMA);
        if (!expression()) return 0;             // arg → acc
        emit2('S', sym->param_slot[pi]);
      }
      EXPECT(TOK_RPAREN);

      // fresh constant for the return IP (no dedup)
      if (nconst >= MAX_CONSTS) {
        printf("\nERROR: constant pool full\n");
        longjmp(fail_jmp, 1);
      }
      ret_ip_const = nconst;
      const_pool[nconst++] = 0;
      emit('L');
      emit((char)(0x80 | (ret_ip_const & 0x7F)));
      emit2('S', sym->ret_addr_slot);

      // vectorised call
      if ((unsigned char)sym->address >= (unsigned char)nvector) {
        printf("\nERROR: bad vector index for %s\n", name);
        longjmp(fail_jmp, 1);
      }
      emit((char)(0xC0 | (sym->address & 0x3F)));

      // patch return IP
      const_pool[ret_ip_const] = (unsigned int)bin_ptr;

      // result
      emit2('L', sym->ret_slot);
    }
    else {
      // ordinary variable
      emit2('L', sym->address);
    }
  }
  else {
    printf("\n%s <---- ERROR: Expected expression\n", curr_tok.lexeme);
    longjmp(fail_jmp, 1);
  }

  // binary +
  if (curr_tok.type == TOK_PLUS) {
    int tmp = alloc_temp();
    next_token();
    emit2('S', tmp);   // save LHS
    if (!expression()) return 0;            // RHS → acc
    emit2('+', tmp);   // acc += mem[tmp]
  }
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
    emit2('S', sym->address);
    EXPECT(TOK_SEMI);
  }
  // WRITELN
  else if (curr_tok.type == TOK_WRITE) {
    int i;
    EXPECT(TOK_WRITE);
    EXPECT(TOK_LPAREN);
    if (curr_tok.type == TOK_STRING) {
      emit('"');
      for (i = 0; curr_tok.lexeme[i]; i++)
        emit(curr_tok.lexeme[i]);
      emit(0);
      EXPECT(TOK_STRING);
    } else {
      if (!expression()) return 0;
      emit('.');
    }
    emit('N');
    EXPECT(TOK_RPAREN);
    EXPECT(TOK_SEMI);
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
    emit2('S', ctrl->address);

    EXPECT(TOK_TO);
    high_slot = alloc_temp();
    if (!expression()) return 0;
    emit2('S', high_slot);

    EXPECT(TOK_DO);
    loop_start = bin_ptr;

    // test: acc = ctrl - high;  if (acc > 0) exit
    emit2('L', ctrl->address);
    emit2('-', high_slot);
    exit_patch = bin_ptr;
    emit('g'); emit('f'); emit(0);          // placeholder

    if (!statement()) return 0;

    // increment
    emit2('L', ctrl->address);
    emit('I');
    emit2('S', ctrl->address);

    // backward jump
    {
      int delta = (bin_ptr + 3) - loop_start;
      if (delta > 255) {
        printf("\nFOR body too large\n");
        longjmp(fail_jmp, 1);
      }
      emit('g'); emit('b'); emit((char)delta);
    }
    // patch forward exit
    {
      int delta = bin_ptr - (exit_patch + 3);
      bin[exit_patch + 2] = (char)delta;
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
  next_token();                       // consume FUNCTION
  strcpy(func_name, curr_tok.lexeme);
  EXPECT(TOK_IDENT);
  EXPECT(TOK_LPAREN);

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
      EXPECT(TOK_COLON);
      EXPECT(TOK_INTEGER);
      // accept both "," and ";" as parameter separators
      // (comma is convenient; semicolon is classic Pascal)
      if (curr_tok.type == TOK_COMMA || curr_tok.type == TOK_SEMI) {
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
  EXPECT(TOK_RPAREN);
  EXPECT(TOK_COLON);
  EXPECT(TOK_INTEGER);
  EXPECT(TOK_SEMI);

  // register the function symbol (vector index filled in a moment)
  add_symbol(func_name, SYM_FUNC, 0);   // temporary
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
  fsym->address = vec_idx;              // now the symbol holds the vector index

  // body
  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) longjmp(fail_jmp, 1);
  }
  EXPECT(TOK_END);
  EXPECT(TOK_SEMI);

  // return sequence (stackless):
  //   The return address was stored by the caller into ret_addr_slot.
  //   Opcode ';' takes that slot and jumps to the address held there.
  //   (Pascal-style  FuncName := expr;  already wrote the result into
  //    ret_val_slot.)
  emit(';');
  emit((char)ret_addr_slot);
}


// ---------------------------------------------------------------------------
// Top-level compilation
// ---------------------------------------------------------------------------
int compile_pipeline(void) {
  int last_var_slot = -1;
  int main_entry_patch;

  src_pos = -1;
  NEXT;
  lnum = 1;
  bin_ptr = 0;
  nsym = 0;
  nconst = 0;
  nvector = 0;
  npatch = 0;
  next_global = 0;
  next_frame  = FRAME_BASE;
  memset(slot_used, 0, sizeof(slot_used));
  memset(mem, 0, sizeof(mem));

  // Absolute jump to main (patched once we know the offset)
  // Encoding:  'G'  <lo> <hi>
  emit('G');
  main_entry_patch = bin_ptr;
  emit(0); emit(0);

  // implicit X (global)
  {
    int xslot = alloc_global();
    add_symbol("X", SYM_VAR, xslot);
  }

  next_token();
  EXPECT(TOK_PROGRAM);
  EXPECT(TOK_IDENT);
  EXPECT(TOK_SEMI);

  // declarations
  while (curr_tok.type == TOK_VAR || curr_tok.type == TOK_FUNCTION) {
    if (curr_tok.type == TOK_VAR) {
      next_token();
      while (curr_tok.type == TOK_IDENT) {
        int slot = alloc_global();
        add_symbol(curr_tok.lexeme, SYM_VAR, slot);
        last_var_slot = slot;
        EXPECT(TOK_IDENT);
        EXPECT(TOK_COLON);
        EXPECT(TOK_INTEGER);
        EXPECT(TOK_SEMI);
      }
    }
    else { // FUNCTION
      compile_function();
    }
  }

  // main program
  EXPECT(TOK_BEGIN);

  // patch the initial jump so it lands here
  {
    int main_off = bin_ptr;
    bin[main_entry_patch]     = main_off & 0xFF;
    bin[main_entry_patch + 1] = (main_off >> 8) & 0xFF;
  }

  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) return 0;
  }
  EXPECT(TOK_END);
  EXPECT(TOK_DOT);

  // leave last global in acc (compatibility)
  if (last_var_slot >= 0)
    emit2('L', last_var_slot);

  emit(0);                              // halt
  return 1;
}

// ---------------------------------------------------------------------------
// Final layout: copy constants into mem[0x80..] and apply any patches
// ---------------------------------------------------------------------------
void finalise(void) {
  int i;
  // Constants live at logical addresses 0x80 + idx.
  // At runtime the interpreter treats an argument byte with the high bit set
  // as a constant-pool index.
  for (i = 0; i < nconst; i++)
    mem[0x80 + i] = const_pool[i];

  // (Future: write the vector table into the program image, relocate, etc.)
  // For the host runner the vectors[] array is already sufficient.
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
  register unsigned char op;

  ipc = bin;
  acc = 0;

  // PAR = memory cell addressed by the byte currently at ipc (not consumed yet)
#define PAR mem[(unsigned char)(*ipc)]

  goto next; // enter at next, not pnext

 pnext:
  ++ipc; // skip the parameter byte
 next:
  op = (unsigned char)*ipc++;
  switch (op) {

    // ---- return from function: next byte is slot holding return IP ----
  case 0: goto done;

  case ';': {
    unsigned char slot = (unsigned char)*ipc++;
    ipc = bin + mem[slot];
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

    // ---- absolute goto  G <lo> <hi> ----
  case 'G': {
    unsigned int addr;
    addr  = (unsigned char)*ipc++; // lo
    addr |= ((unsigned int)(unsigned char)*ipc++) << 8; // hi
    ipc = bin + addr;
    goto next; // already skipped both bytes 
  }

    // ---- relative jumps  g f|b <delta> ---- 
    // TODO: bad use the 0x80-0x8f
  case 'g': {
    char kind = *ipc++; // f or b
    // peek, pnext will skip
    unsigned char delta = (unsigned char)*ipc;
    if (kind == 'f') {
      if ((int)acc > 0) ipc += delta;
    } else {
      ipc -= delta;
    }
    goto pnext;
  }

    // ---- vectorised call 0xC0..0xFF ----
  default:
    if (op >= 0xC0) {
      int vec = op - 0xC0;
      if (vec >= nvector) {
        printf("\n[IVM] bad vector %d\n", vec);
        longjmp(fail_jmp, 1);
      }
      ipc = bin + vectors[vec];
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
    if      (c == 0)    printf("\\0  ");
    else if (c >= 0xC0) printf("V%02x ", c - 0xC0);
    else if (c >= 0x80) printf("J%02x ", c - 0x80);
    else if (c == ' ')  printf("_   ", c);
    else if (c == '"') {
      putchar('\n'); do putchar(c); while((c=bin[++i]));
      printf("\\0\n"); n= 0; }
    else if (isprint(c) && c > ' ') {
      // single byte ops
      if (strchr("#$(),.0123456789;<=>?ADEKNPRU~", c)
        || islower(c))
        printf("%c   ", c);
      else // takes at least one arg
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
