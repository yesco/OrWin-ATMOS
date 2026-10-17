#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

// Dumps hex etc...
#define TPDEBUG

// --- SYSTEM CONSTANTS FOR 6502 HARDWARE BINDINGS ---
#define MAX_SRC_LEN  1024   
#define MAX_SYM_LEN  64     
#define MAX_BIN_LEN  512    
#define MAX_SYMBOLS  32     

// --- COMPLETE TOKEN DEFINITIONS ---
typedef enum {
  TOK_EOF, TOK_ERROR, TOK_PROGRAM, TOK_VAR, TOK_BEGIN, TOK_END,
  TOK_INTEGER, TOK_IDENT, TOK_NUM, TOK_ASSIGN, TOK_SEMI,
  TOK_COLON, TOK_PLUS, TOK_DOT, TOK_WRITE, TOK_STRING,
  TOK_LPAREN, TOK_RPAREN, TOK_FUNCTION
} TokenType;

// TODO: WRITE()
// TODO: IF
// TODO: FOR
// TODO: WHILE
// TODO: REPEAT
const char* TOKEN_NAMES[] = {
  "EOF", "ERROR", "PROGRAM", "VAR", "BEGIN", "END",
  "INTEGER", "IDENT", "NUM", "ASSIGN", "SEMI", "COLON",
  "PLUS", "DOT", "WRITELN", "STRING", "LPAREN", "RPAREN", "FUNCTION"
};

typedef struct {
  TokenType type;
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

jmp_buf fail_jmp;

// --- COMPILER WORKSPACES ---
// TODO: seems stupid, do we really need backtracking? Hmmm
//   or maybe it's good for a tp w editor integrated, lol

// TODO: double indirection... use p++ instead?
char src_buf[MAX_SRC_LEN];
int src_pos = 0; 

int lnum = 1;
int last_lnum = 0; 
Token curr_tok;

// THE NATIVE MACHINE BITS TARGET WORKSPACE
unsigned char bin[MAX_BIN_LEN];
int bin_ptr = 0;

char string_pool[MAX_SYM_LEN + 1];
int string_pool_count = 0;

// --- SYMBOL TABLE ---
typedef enum { SYM_VAR, SYM_FUNC } SymbolType;
typedef struct {
  char name[MAX_SYM_LEN + 1];
  SymbolType type;
  int address; 
} Symbol;

Symbol syms[MAX_SYMBOLS];
int nsym = 0;

typedef int (*PascalFunc)(int x);

void next_token(void);
void expect(TokenType expected);
int expression(void);
int statement(void);

#define EXPECT(expected) expect(expected)

// --- NATIVE C CALLOUT HOOKS FOR CALLS ---
void print_str_cb(const char* s) {
  printf("%s\n", s);
}

void print_int_cb(int val) {
  printf("%d\n", val);
}

// DONE: 122 emit() calls = (* 5 122) = 610 bytes
// DONE: emit(0x00)x2 == 9 times = 90 bytes, could be 9*3=27
// (- 10094 9596) = 498 bytes saved w W WZERO LDAX STAX
// (- 5174870 5164021) 10,849= 11 ms slower... still 5.2s...

void B(unsigned char val) {
  if (bin_ptr < MAX_BIN_LEN) {
    bin[bin_ptr++] = val;
  }
}

void W(unsigned int addr) {
  B(addr & 0xff); B(addr >> 8);
}

void WZERO() { B(0); B(0); }

void STAX(unsigned int addr) {
  // STA w ; STX w+1
  B(0x8D); W(addr);
  B(0x8E); W(addr+1);
}

void LDAX(unsigned int addr) {
  // LDA w ; LDX w+1
  B(0xAD); W(addr);
  B(0xAE); W(addr+1);
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
  // Loop walks backwards from the top to prioritize function entry points over variable slots
  for (i = nsym - 1; i >= 0; i--) {
    if (strcmp(syms[i].name, name) == 0) {
      *type = syms[i].type;
      *address = syms[i].address;
      return i;
    }
  }
  return -1;
}

void log_token() {
  if (lnum != last_lnum) {
    if (last_lnum != 0) printf("\n");
    printf("%05d: %s", lnum, curr_tok.lexeme);
    last_lnum = lnum;
  } else {
    printf(" %s", curr_tok.lexeme);
  }
  //fflush(stdout); // no have on OSCAR
}


char cur_c;

// saved 661 bytes code, slightly faster!
#define C cur_c
#define NEXT next()
char next() { return (cur_c= src_buf[++src_pos]); }


void next_token() {
  int i, len;
  char quote_char;

  while (C) {

    // SKIP SPACE
    if (isspace(C)) {
      if (C == '\n') lnum++;
      NEXT;
      continue;
    }

    // { COMMENT }
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
  
  // EOF = DONE
  if (!C) {
    curr_tok.type = TOK_EOF;
    strcpy(curr_tok.lexeme, "EOF"); // TODO: needed?
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
    
    // compiler makes tail-call!
    log_token();
    return;
  }
  
  // IDENT: token/symbol
  if (isalpha(C)) {
    len = 0;
    while (isalnum(C) && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = C;
      NEXT;
    }
    curr_tok.lexeme[len] = 0;
    for(i = 0; i < len; i++) curr_tok.lexeme[i] = toupper(curr_tok.lexeme[i]);
    
// TODO: use TOKEN_NAMES to match...
    if (strcmp(curr_tok.lexeme, "PROGRAM") == 0) curr_tok.type = TOK_PROGRAM;
    else if (strcmp(curr_tok.lexeme, "VAR") == 0) curr_tok.type = TOK_VAR;
    else if (strcmp(curr_tok.lexeme, "BEGIN") == 0) curr_tok.type = TOK_BEGIN;
    else if (strcmp(curr_tok.lexeme, "END") == 0) curr_tok.type = TOK_END;
    else if (strcmp(curr_tok.lexeme, "INTEGER") == 0) curr_tok.type = TOK_INTEGER;
    else if (strcmp(curr_tok.lexeme, "WRITELN") == 0) curr_tok.type = TOK_WRITE;
    else if (strcmp(curr_tok.lexeme, "FUNCTION") == 0) curr_tok.type = TOK_FUNCTION;
    else curr_tok.type = TOK_IDENT;

    log_token();
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

    log_token();
    return;
  }
  
  // ASSIGN
  if (C == ':' && src_buf[src_pos+1] == '=') { // PEEK
    NEXT; NEXT; curr_tok.type = TOK_ASSIGN;
    strcpy(curr_tok.lexeme, ":=");
    log_token();
    return;
  }
  
  // OPERATORS - single char "tokens"
  curr_tok.lexeme[0] = C;
  curr_tok.lexeme[1] = 0;

  // TODO: strchr and order string according to enum?
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
      return;
  }
  NEXT;
  log_token();
}

// TODO: change to EXPECT and longjmp FAIL
void expect(TokenType expected) {
  if (curr_tok.type == expected) { next_token(); return; }
  // ERROR
  printf("\n%s <---- ERROR: Expected %s but found %s\n", curr_tok.lexeme, TOKEN_NAMES[expected], TOKEN_NAMES[curr_tok.type]);
  longjmp(fail_jmp, 1);
}

int expression(void) {
  SymbolType stype;
  int saddr;
  char name[MAX_SYM_LEN + 1];
  unsigned int base;
  // TODO: too long names..
  unsigned int target_func_pc;
  unsigned int param_storage_addr;
  unsigned int return_storage_addr;

  base = (unsigned int)bin;

  if (curr_tok.type == TOK_NUM) {
    // NUMBER
    B(0xA9); B(curr_tok.value & 0xFF);        // LDA #immediate_lo
    B(0xA2); B((curr_tok.value >> 8) & 0xFF); // LDX #immediate_hi
    next_token();

  } else if (curr_tok.type == TOK_IDENT) {
    // VARIBLE || FUN(...
    strcpy(name, curr_tok.lexeme);
    
    if (lookup_symbol(name, &stype, &saddr) == -1) {
      printf("\n%s <---- ERROR: Undeclared identifier\n", curr_tok.lexeme);
      return 0;
    }
    
    // FUNC / VAR?
    next_token();
    if (stype == SYM_FUNC) {

      // FUNCTION: ( expr )
      EXPECT(TOK_LPAREN);
      if (!expression()) return 0; // Evaluates incoming parameter argument into AX
      EXPECT(TOK_RPAREN);
      
      // Calculate function targets using the newly corrected absolute entry address offsets template
      // - Parameter sits 4 bytes behind entry
      param_storage_addr = base + (unsigned int)saddr - 4; 
      // - Return sits 2 bytes behind entry
      return_storage_addr = base + (unsigned int)saddr - 2;
      // - Target points directly to saddr
      target_func_pc = base + (unsigned int)saddr;         
      
      STAX(param_storage_addr);
      B(0x20); W(target_func_pc);
      LDAX(return_storage_addr);

    } else {

      // VARIABLE
      LDAX(base + (unsigned int)saddr);

    }
  }
  
  // OPERATOR: A + B
  //
  //   TODO: can it be generlized for all operators?
  //   TODO: this is massive...
  if (curr_tok.type == TOK_PLUS) {
    int jmp_patch_idx;
    unsigned int skip_target_ram;
    unsigned int local_math_tmp;
    unsigned int local_math_tmp2;
    
    next_token();
    
    // Fortran style inline storage allocation: completely bypass ZeroPage
    // TODO: Cannot do recursion, unless added as local var... lol
    B(0x4C); // JMP absolute to skip past variable byte storage allocation
    jmp_patch_idx = bin_ptr;
    WZERO();
    
    // HAHA! TODO: shouldn't 2 be enough, at least if COMMUNINATIVE OP
    // Reserve 4 static bytes right here in the code array stream for this node's addition layers
    local_math_tmp = base + bin_ptr;  WZERO();
    local_math_tmp2 = base + bin_ptr; WZERO();
    
    // PATCH(jmp_patch_idx, skip_target_ram)
    skip_target_ram = base + bin_ptr;
    bin[jmp_patch_idx]     = skip_target_ram & 0xFF;
    bin[jmp_patch_idx + 1] = (skip_target_ram >> 8) & 0xFF;
    
    // Store Left-Hand Side value into the private local block
    STAX(local_math_tmp);

    // Evaluate RIGHT-hand side into AX
    if (!expression()) return 0; // FAIL
    
    // Store Right-Hand Side value into the private local block
    STAX(local_math_tmp2);
    
    // Perform pure absolute 16-bit Addition completely isolated from other function branches
    // CLC
    // lobyte: ADC local_math_tmp2
    //         LDA local_math_tmp
    B(0x18);
    B(0xAD); B(local_math_tmp & 0xFF); B((local_math_tmp >> 8) & 0xFF); 
    B(0x6D); B(local_math_tmp2 & 0xFF); B((local_math_tmp2 >> 8) & 0xFF);

    // TODO: use Y?
    // PHA (Save Low Byte sum to hardware stack)
    B(0x48);             
    
    // hibyte: LDA local_math_tmp+1
    //         ADC local_math_tmp2+1
    B(0xAD); B((local_math_tmp + 1) & 0xFF); B(((local_math_tmp + 1) >> 8) & 0xFF); 
    B(0x6D); B((local_math_tmp2 + 1) & 0xFF); B(((local_math_tmp2 + 1) >> 8) & 0xFF);

    // TAX (Final High Byte result into X)
    B(0xAA);

    // PLA (lobyte) TODO: use Y
    B(0x68);
  }
  return 1;
}

int statement(void) {
  // VAR :=  or FUN()
  if (curr_tok.type == TOK_IDENT) {
    char target_var[MAX_SYM_LEN + 1];
    int addr = -1;
    int i;
    
    strcpy(target_var, curr_tok.lexeme);
    
    // Explicit variable lookup loop to prevent function name shadowing on assignments

    //   TODO: why only here? only on assignemnt?
    //   TODO: generalize
    for (i = nsym - 1; i >= 0; i--) {
      if (strcmp(syms[i].name, target_var) == 0 && syms[i].type == SYM_VAR) {
        addr = syms[i].address;
        break;
      }
    }
    if (addr == -1) {
      printf("\n%s <---- ERROR: Unknown variable identifier\n", target_var);
      return 0;
    }
    
    EXPECT(TOK_IDENT);
    EXPECT(TOK_ASSIGN);
    
    if (!expression()) return 0;
    
    STAX(((unsigned int)bin) + (unsigned int)addr);

    EXPECT(TOK_SEMI);
  } 

  // WRITELN ( expr )
  else if (curr_tok.type == TOK_WRITE) {
    EXPECT(TOK_WRITE);
    EXPECT(TOK_LPAREN);
    
    if (curr_tok.type == TOK_STRING) {
      int jmp_patch_idx;
      unsigned int str_start_ram;
      unsigned int str_end_ram;
      int idx = 0;
      
      // JMP: Skip over inline string!
      B(0x4C);
      jmp_patch_idx = bin_ptr;
      WZERO();
      
      str_start_ram = ((unsigned int)bin) + (unsigned int)bin_ptr;
      
      while (curr_tok.lexeme[idx] != '\0') {
        B(curr_tok.lexeme[idx]);
        idx++;
      }
      B(0x00);
      
      str_end_ram = ((unsigned int)bin) + (unsigned int)bin_ptr;
      // PATCH(loc, addr)
      bin[jmp_patch_idx]     = str_end_ram & 0xFF;
      bin[jmp_patch_idx + 1] = (str_end_ram >> 8) & 0xFF;
      
      // LDAX(str_start_ram)
      B(0xA9); B(str_start_ram & 0xFF);        
      B(0xA2); B((str_start_ram >> 8) & 0xFF); 
      
      // JSR print_str_ch
      B(0x20); W((unsigned int)print_str_cb);
      
      // TODO: if going to give error just longjmp(FAIL) inside
      // TODO: why test a second time???
      EXPECT(TOK_STRING);

    } else {

      // JSR print_int_cb
      if (!expression()) return 0;
      B(0x20); W((unsigned int)print_int_cb);

    }
    
    // ) ;
    EXPECT(TOK_RPAREN);
    EXPECT(TOK_SEMI);

  } else {
    printf("\n%s <---- ERROR: Expected statement identifier\n", curr_tok.lexeme);
    return 0;
  }
  return 1;
}

int compile_pipeline(void) {
  int main_patch_idx;
  unsigned int base;
  SymbolType stype;
  int symbol_lookup_address; 
  int last_assigned_var_offset = -1; 
  
  char func_name[MAX_SYM_LEN + 1];
  char param_name[MAX_SYM_LEN + 1];
  int func_base_offset;
  int func_ret_addr;
  int func_param_addr;

  unsigned int main_addr;

  src_pos= -1;
  NEXT;
  lnum = 1;
  last_lnum = 0;
  bin_ptr = 0;
  nsym = 0;
  string_pool_count = 0;
  
  base = (unsigned int)bin;
  printf("\n[Debug] bin starts at RAM address: $%04X\n", base);
  
  // Emit an absolute jump vector to bypass function definition blocks
  B(0x4C); // JMP absolute
  main_patch_idx = bin_ptr; 
  WZERO();
  
  // Magical "X" implicit injected variable!
  // TODO: remove? ARGV? pascal standard?
  add_symbol("X", SYM_VAR, bin_ptr);
  WZERO();
  
  // PROGRAM foo;
  next_token();
  EXPECT(TOK_PROGRAM);
  EXPECT(TOK_IDENT);
  EXPECT(TOK_SEMI);
  
  // VAR x: integer;
  while (curr_tok.type == TOK_VAR || curr_tok.type == TOK_FUNCTION) {
    if (curr_tok.type == TOK_VAR) {
      next_token();
      while (curr_tok.type == TOK_IDENT) {
        add_symbol(curr_tok.lexeme, SYM_VAR, bin_ptr);
        last_assigned_var_offset = bin_ptr; 
        WZERO();
        
        EXPECT(TOK_IDENT);
        EXPECT(TOK_COLON);
        // TODO: string
        EXPECT(TOK_INTEGER);
        // TODO: it's not comma?
        EXPECT(TOK_SEMI);
      }
    }

    // FUNCTION name(name : integer): integer
    //   TODO: 0 arg
    //   TODO: many arg; must have next:
    //   > TODO: recursive/overlap - at least detect
    else if (curr_tok.type == TOK_FUNCTION) {
      next_token();
      strcpy(func_name, curr_tok.lexeme);
      
      // Fortran-Style Inlined Function Layout Allocation:
      func_param_addr = bin_ptr; WZERO();
      func_ret_addr   = bin_ptr; WZERO();
      
      EXPECT(TOK_IDENT);
      EXPECT(TOK_LPAREN);
      
      // param : INTEGER
      strcpy(param_name, curr_tok.lexeme);
      add_symbol(param_name, SYM_VAR, func_param_addr);
      EXPECT(TOK_IDENT);
      EXPECT(TOK_COLON);
      EXPECT(TOK_INTEGER);

      // ) : INTEGER ;
      EXPECT(TOK_RPAREN);
      EXPECT(TOK_COLON);
      EXPECT(TOK_INTEGER);
      EXPECT(TOK_SEMI);
      
      add_symbol(func_name, SYM_VAR, func_ret_addr);
      
      // BEGIN
      EXPECT(TOK_BEGIN);
      
      // Register the function symbol mapped to the actual executable instruction entry point address
      func_base_offset = bin_ptr; 
      add_symbol(func_name, SYM_FUNC, func_base_offset);
      
      // Store AX in fixed parameter location
      STAX(base + (unsigned int)func_param_addr);

      // (BEGIN) ... END;
      while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
        if (!statement()) return 0;
      }
      EXPECT(TOK_END);
      EXPECT(TOK_SEMI);
      
      // Return: load AX with stored return value
      LDAX(base + (unsigned int)func_ret_addr);
      B(0x60); // RTS
    }
  }
  
  // (MAIN) BEGIN ... 
  EXPECT(TOK_BEGIN);
  
  // Back-patch JMP using pure integer addition to prevent pointer corruption
  main_addr = base + (unsigned int)bin_ptr;
  bin[main_patch_idx] = main_addr & 0xFF;
  bin[main_patch_idx + 1] = (main_addr >> 8) & 0xFF;
  
  //printf("[Debug] Entry JMP patched at offset %d to point to Main Code: $%04X\n", main_patch_idx, main_addr);

  if (lookup_symbol("X", &stype, &symbol_lookup_address) != -1)
    STAX(base + (unsigned int)symbol_lookup_address);

  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) return 0;
  }
  
  // END .
  EXPECT(TOK_END);
  EXPECT(TOK_DOT);

  printf("\n"); 

  // implicit return of "last_var" TODO: ???
  if (last_assigned_var_offset != -1)
    LDAX(base + (unsigned int)last_assigned_var_offset);
  
  B(0x60); // RTS
  return 1;
}

// --- EXPLORATORY BYTECODE PRINTER SUBSYSTEM ---
#ifdef TPDEBUG

void dumphex(void) {
  int i;

  printf("\n--- EMITTED BYTECODE INSPECTOR ---\n");
  for (i = 0; i < bin_ptr; i++) {
    printf("%02X ", bin[i]);
    if ((i + 1) % 16 == 0) printf("\n");
  }
  printf("\n----------------------------------\n\n");
}

#else

#define dumphex() (void)0

#endif // TPDEBUG

int main(void) {
  int ch;
  int buf_idx = 0;
  int X_parameter = 42;
  int res = 0;
  PascalFunc program;

  // Read program into buffer (Why)
  memset(src_buf, 0, MAX_SRC_LEN);
  // TODO: OSCAR64 getc give char so EOF ... ?
  //  while ((ch = getc(stdin)) != EOF && buf_idx < MAX_SRC_LEN - 1) {
  while ((ch = getchar()) != EOF && ch != 0xff && buf_idx < MAX_SRC_LEN - 1) {
    src_buf[buf_idx++] = (char)ch;
  }
  src_buf[buf_idx] = 0;

  if (!buf_idx) return 0;
  
  // COMPILE
  if (setjmp(fail_jmp) || !compile_pipeline()) {
    printf(">> FAIL!\n");
    return 0;
  }
    
  // SUCESSS

  dumphex();

  // RUN
  printf("--- RUN %d bytes---\n", bin_ptr);
  program= (PascalFunc)bin;
  res = program(X_parameter);
  printf("--- END ---\n");
  printf("Last var/val: AX=%d\n\n", res);
  
  return 0;
}
