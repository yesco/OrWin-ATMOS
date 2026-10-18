// tpi.c - Tiny Pascal → IVM (Indirect / Accumulator Alphabetical Virtual Machine)
// Based on tp.c (6502 native) but retargeted to the alpha bytecode VM in ivm.c
//
// Design goals for this first version:
//   - Keep the same Pascal subset (PROGRAM, VAR, FUNCTION, BEGIN/END,
//     assignment, +, WRITELN, FOR)
//   - Emit readable ASCII bytecode using the opcodes defined in ivm.c
//   - Provide a minimal host-side interpreter so programs can run today
//   - Variables and temporaries live in a 256-slot memory array (the PAR space)
//   - Result of every expression is left in the accumulator (acc)
//
// Missing / incomplete in upstream ivm.c (we add minimal support here):
//   - Real memory array (MEM / PAR)
//   - Conditional relative jumps (needed for FOR)
//   - Comparison that sets acc to 0 / non-zero

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

//#define TPPRINT

#define MAX_SRC_LEN  2048
#define MAX_SYM_LEN  64
#define MAX_BIN_LEN  2048          // bytecode can be larger than 6502 binary
#define MAX_SYMBOLS  64
#define MAX_SLOTS    256           // IVM memory / PAR space

// ---------------------------------------------------------------------------
// Tokens (same as tp.c + FOR)
// ---------------------------------------------------------------------------
typedef enum {
  TOK_EOF, TOK_ERROR, TOK_PROGRAM, TOK_VAR, TOK_BEGIN, TOK_END,
  TOK_INTEGER, TOK_IDENT, TOK_NUM, TOK_ASSIGN, TOK_SEMI,
  TOK_COLON, TOK_PLUS, TOK_DOT, TOK_WRITE, TOK_STRING,
  TOK_LPAREN, TOK_RPAREN, TOK_FUNCTION,
  TOK_FOR, TOK_TO, TOK_DO
} TokenType;

const char* TOKEN_NAMES[] = {
  "EOF", "ERROR", "PROGRAM", "VAR", "BEGIN", "END",
  "INTEGER", "IDENT", "NUM", "ASSIGN", "SEMI", "COLON",
  "PLUS", "DOT", "WRITELN", "STRING", "LPAREN", "RPAREN", "FUNCTION",
  "FOR", "TO", "DO"
};

typedef struct {
  TokenType type;
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

jmp_buf fail_jmp;

// ---------------------------------------------------------------------------
// Compiler workspaces
// ---------------------------------------------------------------------------
char src_buf[MAX_SRC_LEN];
int src_pos = 0;
int lnum = 1;
int last_lnum = 0;
Token curr_tok;

char bin[MAX_BIN_LEN];             // emitted bytecode (ASCII opcodes + args)
int bin_ptr = 0;

// IVM memory model (the $ / PAR space)
unsigned int mem[MAX_SLOTS];
int next_slot = 16;                // leave 0..15 free for future OS / globals

// Symbol table – address is now a slot index (0-255)
typedef enum { SYM_VAR, SYM_FUNC } SymbolType;
typedef struct {
  char name[MAX_SYM_LEN + 1];
  SymbolType type;
  int address;                     // slot number
} Symbol;

Symbol syms[MAX_SYMBOLS];
int nsym = 0;

void next_token(void);
void expect(TokenType expected);
int expression(void);
int statement(void);

#define EXPECT(expected) expect(expected)

// ---------------------------------------------------------------------------
// Bytecode emission
// ---------------------------------------------------------------------------
void emit(char c) {
  if (bin_ptr < MAX_BIN_LEN - 1)
    bin[bin_ptr++] = c;
}

void emit_op_par(char op, unsigned char slot) {
  emit(op);
  emit((char)slot);
}

// Allocate a fresh slot (for vars or temporaries / constants)
int alloc_slot(void) {
  if (next_slot >= MAX_SLOTS) {
    printf("\nERROR: out of IVM memory slots\n");
    longjmp(fail_jmp, 1);
  }
  return next_slot++;
}

void add_symbol(const char* name, SymbolType type, int address) {
  if (nsym < MAX_SYMBOLS) {
    strcpy(syms[nsym].name, name);
    syms[nsym].type = type;
    syms[nsym].address = address;
    nsym++;
  }
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

#ifdef TPPRINT
void log_token() {
  if (lnum != last_lnum) {
    if (last_lnum != 0) printf("\n");
    printf("%05d: %s", lnum, curr_tok.lexeme);
    last_lnum = lnum;
  } else {
    printf(" %s", curr_tok.lexeme);
  }
}
#else
#define log_token() (void)0
#endif

char cur_line = 0;
char cur_c;
#define C cur_c
#define NEXT next()
char next(void) {
  if ((cur_c = src_buf[++src_pos]) == '\n')
    cur_line = src_pos;
  return cur_c;
}

// ---------------------------------------------------------------------------
// Lexer (identical to tp.c)
// ---------------------------------------------------------------------------
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
      if (C == '}') {
        NEXT;
        continue;
      } else {
        printf("\n} <---- ERROR: Unclosed comment block\n");
        curr_tok.type = TOK_ERROR;
        return;
      }
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
    log_token();
    return;
  }

  if (isalpha(C)) {
    len = 0;
    while (isalnum(C) && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = C;
      NEXT;
    }
    curr_tok.lexeme[len] = 0;
    for (i = 0; i < len; i++) curr_tok.lexeme[i] = toupper(curr_tok.lexeme[i]);

    if (strcmp(curr_tok.lexeme, "PROGRAM") == 0) curr_tok.type = TOK_PROGRAM;
    else if (strcmp(curr_tok.lexeme, "VAR") == 0) curr_tok.type = TOK_VAR;
    else if (strcmp(curr_tok.lexeme, "BEGIN") == 0) curr_tok.type = TOK_BEGIN;
    else if (strcmp(curr_tok.lexeme, "END") == 0) curr_tok.type = TOK_END;
    else if (strcmp(curr_tok.lexeme, "INTEGER") == 0) curr_tok.type = TOK_INTEGER;
    else if (strcmp(curr_tok.lexeme, "WRITELN") == 0) curr_tok.type = TOK_WRITE;
    else if (strcmp(curr_tok.lexeme, "FUNCTION") == 0) curr_tok.type = TOK_FUNCTION;
    else if (strcmp(curr_tok.lexeme, "FOR") == 0) curr_tok.type = TOK_FOR;
    else if (strcmp(curr_tok.lexeme, "TO") == 0) curr_tok.type = TOK_TO;
    else if (strcmp(curr_tok.lexeme, "DO") == 0) curr_tok.type = TOK_DO;
    else curr_tok.type = TOK_IDENT;

    log_token();
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
    log_token();
    return;
  }

  if (C == ':' && src_buf[src_pos + 1] == '=') {
    NEXT; NEXT;
    curr_tok.type = TOK_ASSIGN;
    strcpy(curr_tok.lexeme, ":=");
    log_token();
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
    default:
      curr_tok.type = TOK_ERROR;
      printf("\n%c <---- ERROR: Unknown character syntax error: $%02x\n", C, C);
      longjmp(fail_jmp, 1);
  }
  NEXT;
  log_token();
}

void expect(TokenType expected) {
  if (curr_tok.type == expected) { next_token(); return; }
  printf("\n%s <---- ERROR: Expected %s but found %s\n",
         curr_tok.lexeme, TOKEN_NAMES[expected], TOKEN_NAMES[curr_tok.type]);
  longjmp(fail_jmp, 1);
}

// ---------------------------------------------------------------------------
// Expression → leaves result in acc
// ---------------------------------------------------------------------------
int expression(void) {
  SymbolType stype;
  int saddr;
  char name[MAX_SYM_LEN + 1];

  if (curr_tok.type == TOK_NUM) {
    // Special-case the two constants the VM has direct opcodes for
    if (curr_tok.value == 0) {
      emit('0');
    } else if (curr_tok.value == -1 || curr_tok.value == 65535) {
      emit('9');
    } else {
      // Allocate a constant slot and load it
      int slot = alloc_slot();
      mem[slot] = (unsigned int)curr_tok.value;   // initialise at compile time
      emit_op_par('L', (unsigned char)slot);
    }
    next_token();

  } else if (curr_tok.type == TOK_IDENT) {
    strcpy(name, curr_tok.lexeme);
    if (lookup_symbol(name, &stype, &saddr) == -1) {
      printf("\n%s <---- ERROR: Undeclared identifier\n", curr_tok.lexeme);
      longjmp(fail_jmp, 1);
    }
    next_token();

    if (stype == SYM_FUNC) {
      // Very simple function call support (static param block style)
      // For now we only support the call pattern that stores the argument
      // into the function's parameter slot and then jumps to the body.
      // Full call/return is still incomplete in the alpha VM.
      EXPECT(TOK_LPAREN);
      if (!expression()) return 0;          // arg → acc
      // Store into the parameter slot that was allocated for the function
      // (we stored it as address-2 relative to the function entry in tp.c;
      //  here we simply use the address that was registered for the param)
      // For the first version we just evaluate the argument and leave it
      // in acc; a real call would need the vectorised call mechanism.
      EXPECT(TOK_RPAREN);
      // TODO: real call once IVM subroutine vectors are finished
      printf("\nWARNING: FUNCTION calls are not yet fully supported in IVM backend\n");
    } else {
      // Variable load
      emit_op_par('L', (unsigned char)saddr);
    }
  } else {
    printf("\n%s <---- ERROR: Expected expression\n", curr_tok.lexeme);
    longjmp(fail_jmp, 1);
  }

  // Binary +
  if (curr_tok.type == TOK_PLUS) {
    int tmp = alloc_slot();
    next_token();

    // acc currently holds left-hand side → store it
    emit_op_par('S', (unsigned char)tmp);

    // evaluate right-hand side → acc
    if (!expression()) return 0;

    // acc = acc + mem[tmp]
    emit_op_par('+', (unsigned char)tmp);
  }

  return 1;
}

// ---------------------------------------------------------------------------
// Statement
// ---------------------------------------------------------------------------
int statement(void) {

  // assignment: ident := expr ;
  if (curr_tok.type == TOK_IDENT) {
    char target_var[MAX_SYM_LEN + 1];
    int addr = -1;
    int i;

    strcpy(target_var, curr_tok.lexeme);
    for (i = nsym - 1; i >= 0; i--) {
      if (strcmp(syms[i].name, target_var) == 0 && syms[i].type == SYM_VAR) {
        addr = syms[i].address;
        break;
      }
    }
    if (addr == -1) {
      printf("\n%s <---- ERROR: Unknown variable identifier\n", target_var);
      longjmp(fail_jmp, 1);
    }

    EXPECT(TOK_IDENT);
    EXPECT(TOK_ASSIGN);
    if (!expression()) return 0;
    emit_op_par('S', (unsigned char)addr);
    EXPECT(TOK_SEMI);
  }

  // WRITELN ( string | expr )
  else if (curr_tok.type == TOK_WRITE) {
    EXPECT(TOK_WRITE);
    EXPECT(TOK_LPAREN);

    if (curr_tok.type == TOK_STRING) {
      // Inline string print: " .... \0
      emit('"');
      {
        int idx = 0;
        while (curr_tok.lexeme[idx]) {
          emit(curr_tok.lexeme[idx]);
          idx++;
        }
      }
      emit(0);                     // terminator
      EXPECT(TOK_STRING);
    } else {
      if (!expression()) return 0;
      emit('.');                   // print number
    }
    emit('N');                     // newline
    EXPECT(TOK_RPAREN);
    EXPECT(TOK_SEMI);
  }

  // FOR var := low TO high DO statement
  // Jump encoding used by the embedded interpreter:
  //   Jf <delta>   – conditional forward jump (taken when acc > 0)
  //   Jb <delta>   – unconditional backward jump
  // These are two-byte opcodes so the relative distance is easy to compute.
  else if (curr_tok.type == TOK_FOR) {
    char ctrl_name[MAX_SYM_LEN + 1];
    int ctrl_addr = -1;
    int high_slot;
    int loop_start;
    int exit_patch;
    int i;

    EXPECT(TOK_FOR);

    if (curr_tok.type != TOK_IDENT) {
      printf("\n%s <---- ERROR: Expected control variable after FOR\n", curr_tok.lexeme);
      longjmp(fail_jmp, 1);
    }
    strcpy(ctrl_name, curr_tok.lexeme);
    for (i = nsym - 1; i >= 0; i--) {
      if (strcmp(syms[i].name, ctrl_name) == 0 && syms[i].type == SYM_VAR) {
        ctrl_addr = syms[i].address;
        break;
      }
    }
    if (ctrl_addr == -1) {
      printf("\n%s <---- ERROR: Undeclared FOR control variable\n", ctrl_name);
      longjmp(fail_jmp, 1);
    }
    EXPECT(TOK_IDENT);
    EXPECT(TOK_ASSIGN);

    // low → control var
    if (!expression()) return 0;
    emit_op_par('S', (unsigned char)ctrl_addr);

    EXPECT(TOK_TO);

    // high → private temp slot
    high_slot = alloc_slot();
    if (!expression()) return 0;
    emit_op_par('S', (unsigned char)high_slot);

    EXPECT(TOK_DO);

    loop_start = bin_ptr;

    // test: acc = ctrl - high ;  if (acc > 0) goto exit
    emit_op_par('L', (unsigned char)ctrl_addr);
    emit_op_par('-', (unsigned char)high_slot);

    // Jf <delta>  (patched after body)
    exit_patch = bin_ptr;
    emit('g'); emit('f'); emit(0);          // placeholder delta

    // body
    if (!statement()) return 0;

    // increment control variable
    emit_op_par('L', (unsigned char)ctrl_addr);
    emit('I');
    emit_op_par('S', (unsigned char)ctrl_addr);

    // Jb <delta>  back to loop_start
    {
      int delta = bin_ptr + 3 - loop_start;   // distance from after this instr
      if (delta > 255) {
        printf("\nFOR loop body too large for relative jump\n");
        longjmp(fail_jmp, 1);
      }
      emit('g'); emit('b'); emit((char)delta);
    }

    // patch the forward exit jump
    {
      int delta = bin_ptr - (exit_patch + 3);
      if (delta > 255) {
        printf("\nFOR exit jump too far\n");
        longjmp(fail_jmp, 1);
      }
      bin[exit_patch + 2] = (char)delta;
    }
  }

  else {
    printf("\n%s <---- ERROR: Expected statement (ident, WRITELN or FOR)\n",
           curr_tok.lexeme);
    longjmp(fail_jmp, 1);
  }

  return 1;
}

// ---------------------------------------------------------------------------
// Top-level compile
// ---------------------------------------------------------------------------
int compile_pipeline(void) {
  SymbolType stype;
  int symbol_lookup_address;
  int last_assigned_var_offset = -1;

  char func_name[MAX_SYM_LEN + 1];
  char param_name[MAX_SYM_LEN + 1];
  int func_param_slot;
  int func_ret_slot;

  src_pos = -1;
  NEXT;
  lnum = 1;
  last_lnum = 0;
  bin_ptr = 0;
  nsym = 0;
  next_slot = 16;
  memset(mem, 0, sizeof(mem));

  // Implicit "X" variable (kept for compatibility with tp.c)
  add_symbol("X", SYM_VAR, alloc_slot());

  // PROGRAM name ;
  next_token();
  EXPECT(TOK_PROGRAM);
  EXPECT(TOK_IDENT);
  EXPECT(TOK_SEMI);

  // VAR / FUNCTION declarations
  while (curr_tok.type == TOK_VAR || curr_tok.type == TOK_FUNCTION) {
    if (curr_tok.type == TOK_VAR) {
      next_token();
      while (curr_tok.type == TOK_IDENT) {
        int slot = alloc_slot();
        add_symbol(curr_tok.lexeme, SYM_VAR, slot);
        last_assigned_var_offset = slot;
        EXPECT(TOK_IDENT);
        EXPECT(TOK_COLON);
        EXPECT(TOK_INTEGER);
        EXPECT(TOK_SEMI);
      }
    }
    else if (curr_tok.type == TOK_FUNCTION) {
      // Minimal support – allocate slots, but call/return still TODO
      next_token();
      strcpy(func_name, curr_tok.lexeme);
      func_param_slot = alloc_slot();
      func_ret_slot   = alloc_slot();

      EXPECT(TOK_IDENT);
      EXPECT(TOK_LPAREN);
      strcpy(param_name, curr_tok.lexeme);
      add_symbol(param_name, SYM_VAR, func_param_slot);
      EXPECT(TOK_IDENT);
      EXPECT(TOK_COLON);
      EXPECT(TOK_INTEGER);
      EXPECT(TOK_RPAREN);
      EXPECT(TOK_COLON);
      EXPECT(TOK_INTEGER);
      EXPECT(TOK_SEMI);

      add_symbol(func_name, SYM_VAR, func_ret_slot); // return value slot
      // Real function body would be emitted here and registered as a vector.
      // For the alpha version we just parse the body and ignore it for now.
      EXPECT(TOK_BEGIN);
      while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
        if (!statement()) return 0;
      }
      EXPECT(TOK_END);
      EXPECT(TOK_SEMI);
      add_symbol(func_name, SYM_FUNC, 0); // placeholder
    }
  }

  // MAIN BEGIN ... END .
  EXPECT(TOK_BEGIN);

  // Optional: store the incoming X parameter (slot already allocated)
  if (lookup_symbol("X", &stype, &symbol_lookup_address) != -1) {
    // On a real IVM we would receive the parameter in acc.
    // For the host runner we initialise it ourselves.
  }

  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) return 0;
  }

  EXPECT(TOK_END);
  EXPECT(TOK_DOT);

  // Leave last assigned variable in acc (mirrors tp.c behaviour)
  if (last_assigned_var_offset != -1)
    emit_op_par('L', (unsigned char)last_assigned_var_offset);

  emit(0);                         // stop
  return 1;
}

// ---------------------------------------------------------------------------
// Minimal IVM interpreter (host side) – enough to run the generated code
// ---------------------------------------------------------------------------
void run_ivm(void) {
  char *ipc = bin;
  unsigned int acc = 0;
  char op;

  printf("--- RUN IVM (%d bytes) ---\n", bin_ptr);

  while (1) {
    op = *ipc++;
    if (op == 0) break;            // end of program

    // Parameter-taking ops
    if (op == '+' || op == '-' || op == '*' || op == '/' ||
        op == '&' || op == '|' || op == '^' ||
        op == 'L' || op == 'S') {
      unsigned char slot = (unsigned char)*ipc++;
      switch (op) {
        case '+': acc += mem[slot]; break;
        case '-': acc -= mem[slot]; break;
        case '*': acc *= mem[slot]; break;
        case '/': if (mem[slot]) acc /= mem[slot]; break;
        case '&': acc &= mem[slot]; break;
        case '|': acc |= mem[slot]; break;
        case '^': acc ^= mem[slot]; break;
        case 'L': acc  = mem[slot]; break;
        case 'S': mem[slot] = acc;  break;
      }
      continue;
    }

    // No-parameter ops
    switch (op) {
      case ' ': break;             // nop
      case '0': acc = 0; break;
      case '9': acc = (unsigned int)-1; break;
      case 'I': ++acc; break;
      case 'J': --acc; break;
      case '~': acc = ~acc; break;
      case '.': printf("%u", acc); break;
      case 'N': putchar('\n'); break;
      case 'E': putchar((char)acc); break;
      case '"':                    // inline string
        while (*ipc) putchar(*ipc++);
        ipc++;                     // skip the terminating 0
        break;

      case 'g': {                  // two-byte relative jumps  gf / gb
        char kind = *ipc++;
        unsigned char delta = (unsigned char)*ipc++;
        if (kind == 'f') {         // conditional forward (taken if acc > 0)
          if ((int)acc > 0)
            ipc += delta;
        } else if (kind == 'b') {  // unconditional backward
          ipc -= delta;
        } else {
          printf("\n[IVM] unknown jump kind '%c'\n", kind);
          return;
        }
        break;
      }
      default:
        printf("\n[IVM] unknown opcode 0x%02X ('%c')\n",
               (unsigned char)op, isprint(op) ? op : '?');
        return;
    }
  }

  printf("--- END ---\n");
  printf("acc = %u\n", acc);
}

// ---------------------------------------------------------------------------
// Debug dump of generated bytecode
// ---------------------------------------------------------------------------
void dump_bytecode(void) {
  int i;
  printf("\n--- GENERATED BYTECODE (%d bytes) ---\n", bin_ptr);
  for (i = 0; i < bin_ptr; i++) {
    char c = bin[i];
    if (c == 0) printf("\\0 ");
    else if (isprint(c) && c != ' ') printf("%c ", c);
    else printf("%02X ", (unsigned char)c);
    if ((i + 1) % 32 == 0) printf("\n");
  }
  printf("\n------------------------------------\n\n");
}

// ---------------------------------------------------------------------------
int main(void) {
  int ch;
  int buf_idx = 0;

  memset(src_buf, 0, MAX_SRC_LEN);
  while ((ch = getchar()) != EOF && ch != 0xff && buf_idx < MAX_SRC_LEN - 1)
    src_buf[buf_idx++] = (char)ch;
  src_buf[buf_idx] = 0;

  if (!buf_idx) return 0;

  if (setjmp(fail_jmp) || !compile_pipeline()) {
    unsigned int err_pos = src_pos;
    printf(">> FAIL on line:\n\n%d: ", lnum);
    src_pos = cur_line;
    do {
      if (src_pos == err_pos) printf(" <<<---ERROR--- ");
      putchar(NEXT);
    } while (C && C != '\n');
    return 1;
  }

  dump_bytecode();
  run_ivm();
  return 0;
}
