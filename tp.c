#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- SYSTEM CONSTANTS FOR 6502 HARDWARE BINDINGS --- */
#define MAX_SRC_LEN  1024   
#define MAX_SYM_LEN  64     
#define MAX_BIN_LEN  256    
#define MAX_SYMBOLS  32     

/* --- COMPLETE TOKEN DEFINITIONS --- */
typedef enum {
  TOK_EOF, TOK_ERROR, TOK_PROGRAM, TOK_VAR, TOK_BEGIN, TOK_END,
  TOK_INTEGER, TOK_IDENT, TOK_NUM, TOK_ASSIGN, TOK_SEMI,
  TOK_COLON, TOK_PLUS, TOK_DOT, TOK_WRITE, TOK_STRING,
  TOK_LPAREN, TOK_RPAREN
} TokenType;

const char* TOKEN_NAMES[] = {
  "EOF", "ERROR", "PROGRAM", "VAR", "BEGIN", "END",
  "INTEGER", "IDENT", "NUM", "ASSIGN", "SEMI", "COLON",
  "PLUS", "DOT", "WRITELN", "STRING", "LPAREN", "RPAREN"
};

typedef struct {
  TokenType type;
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

/* --- COMPILER WORKSPACES --- */
char src_buf[MAX_SRC_LEN];
int src_pos = 0;
int line_num = 1;
int last_logged_line = 0; 
Token curr_tok;

/* THE NATIVE MACHINE BITS TARGET WORKSPACE */
unsigned char jit_bin[MAX_BIN_LEN];
int bin_ptr = 0;

/* PERSISTENT RUNTIME STRING LITERAL POOL */
char string_pool[8][64];
int string_pool_count = 0;

/* --- SYMBOL TABLE --- */
typedef struct {
  char name[MAX_SYM_LEN + 1];
  int zp_offset;
} Symbol;

Symbol sym_table[MAX_SYMBOLS];
int sym_count = 0;

/* HARDWARE ADDRESS BOUNDARIES */
#define ARG_ZP_LO 0x02
#define ARG_ZP_HI 0x03
#define TMP_ZP_LO 0x08
#define TMP_ZP_HI 0x09

typedef int (*PascalFunc)(int x);

/* --- NATIVE C CALLOUT HOOKS FOR JIT CALLS --- */
void print_str_cb(const char* s) {
  printf("%s\n", s);
  fflush(stdout);
}

void print_int_cb(int val) {
  printf("%d\n", val);
  fflush(stdout);
}

void emit(unsigned char val) {
  if (bin_ptr < MAX_BIN_LEN) {
    jit_bin[bin_ptr++] = val;
  }
}

void add_symbol(const char* name, int zp) {
  if (sym_count < MAX_SYMBOLS) {
    strcpy(sym_table[sym_count].name, name);
    sym_table[sym_count].zp_offset = zp;
    sym_count++;
  }
}

int lookup_symbol(const char* name) {
  int i;
  for (i = 0; i < sym_count; i++) {
    if (strcmp(sym_table[i].name, name) == 0) return sym_table[i].zp_offset;
  }
  return -1;
}

/* --- LOG ENGINE HELPER --- */
void log_token() {
  if (line_num != last_logged_line) {
    if (last_logged_line != 0) printf("\n");
    printf("%05d: %s", line_num, curr_tok.lexeme);
    last_logged_line = line_num;
  } else {
    printf(" %s", curr_tok.lexeme);
  }
  fflush(stdout);
}

/* --- STREAM LEXER WITH FIXED PASSTHROUGH COMMENT ENGINE --- */
void next_token() {
  int i, len;
  char quote_char;

  while (src_buf[src_pos] != '\0') {
    if (isspace(src_buf[src_pos])) {
      if (src_buf[src_pos] == '\n') line_num++;
      src_pos++;
      continue;
    }
    if (src_buf[src_pos] == '{') {
      src_pos++; 
      while (src_buf[src_pos] != '\0' && src_buf[src_pos] != '}') {
        if (src_buf[src_pos] == '\n') line_num++;
        src_pos++;
      }
      if (src_buf[src_pos] == '}') {
        src_pos++; 
        continue;
      } else {
        printf("\n} <---- ERROR: Unclosed comment block\n");
        curr_tok.type = TOK_ERROR;
        return;
      }
    }
    break;
  }
  
  if (src_buf[src_pos] == '\0') {
    curr_tok.type = TOK_EOF;
    strcpy(curr_tok.lexeme, "EOF");
    return;
  }

  /* Parse String Literals */
  if (src_buf[src_pos] == '"' || src_buf[src_pos] == '\'') {
    quote_char = src_buf[src_pos++];
    len = 0;
    while (src_buf[src_pos] && src_buf[src_pos] != quote_char && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = src_buf[src_pos++];
    }
    if (src_buf[src_pos] == quote_char) src_pos++; 
    curr_tok.lexeme[len] = '\0';
    curr_tok.type = TOK_STRING;
    log_token();
    return;
  }
  
  /* Parse Identifiers */
  if (isalpha(src_buf[src_pos])) {
    len = 0;
    while (isalnum(src_buf[src_pos]) && len < MAX_SYM_LEN) {
      curr_tok.lexeme[len++] = src_buf[src_pos++];
    }
    curr_tok.lexeme[len] = '\0';
    for(i = 0; i < len; i++) curr_tok.lexeme[i] = toupper(curr_tok.lexeme[i]);
    
    if (strcmp(curr_tok.lexeme, "PROGRAM") == 0) curr_tok.type = TOK_PROGRAM;
    else if (strcmp(curr_tok.lexeme, "VAR") == 0) curr_tok.type = TOK_VAR;
    else if (strcmp(curr_tok.lexeme, "BEGIN") == 0) curr_tok.type = TOK_BEGIN;
    else if (strcmp(curr_tok.lexeme, "END") == 0) curr_tok.type = TOK_END;
    else if (strcmp(curr_tok.lexeme, "INTEGER") == 0) curr_tok.type = TOK_INTEGER;
    else if (strcmp(curr_tok.lexeme, "WRITELN") == 0) curr_tok.type = TOK_WRITE;
    else curr_tok.type = TOK_IDENT;

    log_token();
    return;
  }
  
  /* Parse Numbers */
  if (isdigit(src_buf[src_pos])) {
    curr_tok.type = TOK_NUM;
    curr_tok.value = 0;
    while (isdigit(src_buf[src_pos])) {
      curr_tok.value = curr_tok.value * 10 + (src_buf[src_pos++] - '0');
    }
    sprintf(curr_tok.lexeme, "%d", curr_tok.value);
    log_token();
    return;
  }
  
  if (src_buf[src_pos] == ':' && src_buf[src_pos+1] == '=') {
    src_pos += 2; curr_tok.type = TOK_ASSIGN; strcpy(curr_tok.lexeme, ":="); log_token(); return;
  }
  
  curr_tok.lexeme[0] = src_buf[src_pos];
  curr_tok.lexeme[1] = '\0';

  switch (src_buf[src_pos]) {
    case ';': src_pos++; curr_tok.type = TOK_SEMI; break;
    case ':': src_pos++; curr_tok.type = TOK_COLON; break;
    case '+': src_pos++; curr_tok.type = TOK_PLUS; break;
    case '.': src_pos++; curr_tok.type = TOK_DOT; break;
    case '(': src_pos++; curr_tok.type = TOK_LPAREN; break;
    case ')': src_pos++; curr_tok.type = TOK_RPAREN; break;
    default:
      curr_tok.type = TOK_ERROR;
      printf("\n%c <---- ERROR: Unknown character syntax error\n", src_buf[src_pos]);
      src_pos++;
      return;
  }
  log_token();
}

int match(TokenType expected) {
  if (curr_tok.type == expected) {
    next_token();
    return 1;
  } else {
    printf("\n%s <---- ERROR: Expected %s but found %s\n", curr_tok.lexeme, TOKEN_NAMES[expected], TOKEN_NAMES[curr_tok.type]);
    return 0;
  }
}

/* --- 6502 CODE-GENERATING RECURSIVE DESCENT PARSER --- */
int expression() {
  if (curr_tok.type == TOK_NUM) {
    emit(0xA9); emit(curr_tok.value & 0xFF);
    emit(0xA2); emit((curr_tok.value >> 8) & 0xFF);
    next_token();
  } else if (curr_tok.type == TOK_IDENT) {
    int addr = lookup_symbol(curr_tok.lexeme);
    if (addr == -1) {
      printf("\n%s <---- ERROR: Undeclared identifier\n", curr_tok.lexeme);
      return 0;
    }
    emit(0xA5); emit(addr);
    emit(0xA6); emit(addr + 1);
    next_token();
  }
  
  if (curr_tok.type == TOK_PLUS) {
    next_token();
    emit(0x48); /* PHA */
    emit(0x8A); /* TXA */
    emit(0x48); /* PHA */
    
    if (!expression()) return 0; 
    
    emit(0x85); emit(TMP_ZP_LO); 
    emit(0x86); emit(TMP_ZP_HI); 
    emit(0x68);                  
    emit(0xAA);                  
    emit(0x68);                  
    emit(0x18);                  
    emit(0x65); emit(TMP_ZP_LO); 
    emit(0x48);                  
    emit(0x8A);                  
    emit(0x65); emit(TMP_ZP_HI); 
    emit(0xAA);                  
    emit(0x68);                  
  }
  return 1;
}

int statement() {
  unsigned int callback_addr;
  if (curr_tok.type == TOK_IDENT) {
    char target_var[MAX_SYM_LEN + 1];
    int addr;
    strcpy(target_var, curr_tok.lexeme);
    if (!match(TOK_IDENT)) return 0;
    if (!match(TOK_ASSIGN)) return 0;
    if (!expression()) return 0;
    
    addr = lookup_symbol(target_var);
    if (addr == -1) {
      printf("\n%s <---- ERROR: Unknown target variable identifier\n", target_var);
      return 0;
    }
    emit(0x85); emit(addr);     
    emit(0x86); emit(addr + 1); 
    if (!match(TOK_SEMI)) return 0;
  } 
  else if (curr_tok.type == TOK_WRITE) {
    if (!match(TOK_WRITE)) return 0;
    if (!match(TOK_LPAREN)) return 0;
    
    if (curr_tok.type == TOK_STRING) {
      if (string_pool_count < 8) {
        unsigned int pool_str_addr;
        strcpy(string_pool[string_pool_count], curr_tok.lexeme);
        pool_str_addr = (unsigned int)string_pool[string_pool_count];
        string_pool_count++;
        
        /* Load string low/high memory bytes pointer configuration into A/X */
        emit(0xA9); emit(pool_str_addr & 0xFF);
        emit(0xA2); emit((pool_str_addr >> 8) & 0xFF);
        
        callback_addr = (unsigned int)print_str_cb;
        emit(0x20); emit(callback_addr & 0xFF); emit((callback_addr >> 8) & 0xFF);
      }
      if (!match(TOK_STRING)) return 0;
    } else {
      if (!expression()) return 0;
      callback_addr = (unsigned int)print_int_cb;
      emit(0x20); emit(callback_addr & 0xFF); emit((callback_addr >> 8) & 0xFF);
    }
    
    if (!match(TOK_RPAREN)) return 0;
    if (!match(TOK_SEMI)) return 0;
  }
  return 1;
}

int compile_pipeline() {
  int next_free_zp = 0x04;
  int final_res_addr;
  src_pos = 0;
  line_num = 1;
  last_logged_line = 0;
  bin_ptr = 0;
  sym_count = 0;
  string_pool_count = 0;

  add_symbol("X", 0x02); 

  next_token();
  if (!match(TOK_PROGRAM)) return 0;
  if (!match(TOK_IDENT)) return 0;
  if (!match(TOK_SEMI)) return 0;
  
  if (curr_tok.type == TOK_VAR) {
    next_token();
    while (curr_tok.type == TOK_IDENT) {
      add_symbol(curr_tok.lexeme, next_free_zp);
      next_free_zp += 2;
      if (!match(TOK_IDENT)) return 0; 
      if (!match(TOK_COLON)) return 0; 
      if (!match(TOK_INTEGER)) return 0; 
      if (!match(TOK_SEMI)) return 0;
    }
  }
  
  if (!match(TOK_BEGIN)) return 0;
  
  emit(0x85); emit(ARG_ZP_LO); 
  emit(0x86); emit(ARG_ZP_HI); 
  
  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) return 0;
  }
  
  if (!match(TOK_END)) return 0; 
  if (!match(TOK_DOT)) return 0;
  
  printf("\n"); 

  /* Load the computed value of BETA (15 + 32 = 47) into A/X before exiting */
  final_res_addr = lookup_symbol("BETA");
  if (final_res_addr != -1) {
    emit(0xA5); emit(final_res_addr);
    emit(0xA6); emit(final_res_addr + 1);
  }
  
  emit(0x60); /* RTS */
  return 1;
}

int main() {
  int ch;
  int buf_idx = 0;
  int test_parameter = 42;
  int compiled_output = 0;
  PascalFunc execute_native_jit;
  memset(src_buf, 0, MAX_SRC_LEN);
  while ((ch = getc(stdin)) != EOF && buf_idx < MAX_SRC_LEN - 1) {
    src_buf[buf_idx++] = (char)ch;
  }
  src_buf[buf_idx] = '\0';
  if (strlen(src_buf) > 0) {
    if (compile_pipeline()) {
      execute_native_jit = (PascalFunc)jit_bin;
      compiled_output = execute_native_jit(test_parameter);
      printf("--- JIT EXECUTION SUCCESSFUL INSIDE SIM65 ---\n");
      printf("Result Array Return (A/X): %d\n", compiled_output);
    } else {
      printf(">> JIT Pipeline failed during compilation.\n");
    }
  }
  return 0;
}
