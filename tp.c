#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- SYSTEM CONSTANTS FOR SMALL MEMORY (6502/cc65) --- */
#define MAX_SRC_LEN  8192   
#define MAX_SYM_LEN  64     
#define MAX_SYMBOLS  32     

/* --- EDITOR & LEXER STATES --- */
char src_buf[MAX_SRC_LEN];   
int src_pos = 0;
int line_num = 1;

typedef enum {
  TOK_EOF, TOK_ERROR, TOK_PROGRAM, 
  TOK_VAR, TOK_BEGIN, TOK_END,     
  TOK_INTEGER, TOK_IDENT, TOK_NUM, 
  TOK_ASSIGN, TOK_SEMI, TOK_COLON, 
  TOK_PLUS, TOK_MINUS, TOK_WRITE,  
  TOK_LPAREN, TOK_RPAREN,          
  TOK_STRING,                      
  TOK_DOT                          
} TokenType;

const char* TOKEN_NAMES[] = {
  "EOF", "ERROR", "PROGRAM", "VAR", "BEGIN", "END",
  "INTEGER", "IDENT", "NUM", "ASSIGN", "SEMI", "COLON",
  "PLUS", "MINUS", "WRITELN", "LPAREN", "RPAREN", "STRING", "DOT"
};

typedef struct {
  TokenType type;
  char lexeme[MAX_SYM_LEN + 1];
  int value;
} Token;

Token curr_tok;

/* --- SYMBOL TABLE --- */
typedef struct {
  char name[MAX_SYM_LEN + 1];
  int zp_offset; 
} Symbol;

Symbol sym_table[MAX_SYMBOLS];
int sym_count = 0;
int next_zp_addr = 0x60; 

/* --- LEXICAL ANALYZER (WITH FIXED PASSTHROUGH COMMENT ENGINE) --- */
void next_token() {
  int i, len;
  char quote_char;

  /* Continuous check loop for Whitespace and Pascal Blocks */
  while (src_buf[src_pos] != '\0') {
    if (isspace(src_buf[src_pos])) {
      if (src_buf[src_pos] == '\n') line_num++;
      src_pos++;
      continue;
    }
    
    /* Detect and flush out bracketed comment contents */
    if (src_buf[src_pos] == '{') {
      src_pos++; /* Advance past '{' */
      while (src_buf[src_pos] != '\0' && src_buf[src_pos] != '}') {
        if (src_buf[src_pos] == '\n') line_num++;
        src_pos++;
      }
      if (src_buf[src_pos] == '}') {
        src_pos++; /* Advance past closing '}' */
        continue;
      } else {
        printf("} <---- ERROR: Unclosed comment block detected at end of file\n");
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
    printf("%05d: STRING(\"%s\")\n", line_num, curr_tok.lexeme);
    return;
  }

  /* Parse Identifiers and Keywords */
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
    
    if (curr_tok.type == TOK_IDENT) {
      printf("%05d: IDENT(\"%s\")\n", line_num, curr_tok.lexeme);
    } else {
      printf("%05d: %s\n", line_num, TOKEN_NAMES[curr_tok.type]);
    }
    return;
  }

  /* Parse Constants */
  if (isdigit(src_buf[src_pos])) {
    curr_tok.type = TOK_NUM;
    curr_tok.value = 0;
    while (isdigit(src_buf[src_pos])) {
      curr_tok.value = curr_tok.value * 10 + (src_buf[src_pos++] - '0');
    }
    sprintf(curr_tok.lexeme, "%d", curr_tok.value);
    printf("%05d: NUM(%d)\n", line_num, curr_tok.value);
    return;
  }

  /* Parse Operators / Special Punctuations */
  if (src_buf[src_pos] == ':' && src_buf[src_pos+1] == '=') {
    src_pos += 2;
    curr_tok.type = TOK_ASSIGN;
    strcpy(curr_tok.lexeme, ":=");
    printf("%05d: ASSIGN\n", line_num);
    return;
  }

  /* Properly format string assignments for single character bounds */
  curr_tok.lexeme[0] = src_buf[src_pos];
  curr_tok.lexeme[1] = '\0';

  switch (src_buf[src_pos]) {
    case ';': src_pos++; curr_tok.type = TOK_SEMI; break;
    case ':': src_pos++; curr_tok.type = TOK_COLON; break;
    case '+': src_pos++; curr_tok.type = TOK_PLUS; break;
    case '-': src_pos++; curr_tok.type = TOK_MINUS; break;
    case '(': src_pos++; curr_tok.type = TOK_LPAREN; break;
    case ')': src_pos++; curr_tok.type = TOK_RPAREN; break;
    case '.': src_pos++; curr_tok.type = TOK_DOT; break;
    default:
      curr_tok.type = TOK_ERROR;
      printf("%c <---- ERROR: Unknown character syntax error\n", src_buf[src_pos]);
      src_pos++;
      return;
  }
  
  printf("%05d: SYM(\"%s\")\n", line_num, curr_tok.lexeme);
}

/* --- COMPILER BACKEND --- */
void add_symbol(const char* name) {
  if (sym_count >= MAX_SYMBOLS) {
    printf("Symbol table overflow error\n");
    return;
  }
  strcpy(sym_table[sym_count].name, name);
  sym_table[sym_count].zp_offset = next_zp_addr;
  next_zp_addr += 2; 
  sym_count++;
}

int lookup_symbol(const char* name) {
  int i;
  for (i = 0; i < sym_count; i++) {
    if (strcmp(sym_table[i].name, name) == 0) return sym_table[i].zp_offset;
  }
  return -1;
}

int match(TokenType expected) {
  if (curr_tok.type == expected) {
    next_token();
    return 1;
  } else {
    printf("%s <---- ERROR: Expected %s but found %s\n", curr_tok.lexeme, TOKEN_NAMES[expected], TOKEN_NAMES[curr_tok.type]);
    return 0;
  }
}

void expression() {
  int addr;
  if (curr_tok.type == TOK_NUM) {
    printf("\tLDA #$%02X\n\tLDX #$%02X\n", curr_tok.value & 0xFF, (curr_tok.value >> 8) & 0xFF);
    next_token();
  } else if (curr_tok.type == TOK_IDENT) {
    addr = lookup_symbol(curr_tok.lexeme);
    if (addr == -1) {
      printf("%s <---- ERROR: Undeclared identifier\n", curr_tok.lexeme);
      return;
    }
    printf("\tLDA $%02X\n\tLDX $%02X\n", addr, addr + 1);
    next_token();
  }

  if (curr_tok.type == TOK_PLUS) {
    next_token();
    printf("\tPHA\n\tTXA\n\tPHA\n"); 
    expression();
    printf("\tPLX\n\tPLA\n\tCLC\n\tADC $%02X\n\tSTA $%02X\n", next_zp_addr, next_zp_addr); 
  }
}

int statement() {
  char target_var[MAX_SYM_LEN + 1];
  int addr;
  static int string_id = 0;

  if (curr_tok.type == TOK_IDENT) {
    strcpy(target_var, curr_tok.lexeme);
    if (!match(TOK_IDENT)) return 0;
    if (!match(TOK_ASSIGN)) return 0;
    
    expression();
    
    addr = lookup_symbol(target_var);
    if (addr == -1) {
      printf("%s <---- ERROR: Unknown target variable identifier\n", target_var);
      return 0;
    }
    printf("\tSTA $%02X\n\tSTX $%02X\n", addr, addr + 1);
    if (!match(TOK_SEMI)) return 0;
  } 
  else if (curr_tok.type == TOK_WRITE) {
    if (!match(TOK_WRITE)) return 0;
    if (!match(TOK_LPAREN)) return 0;
    
    if (curr_tok.type == TOK_STRING) {
      printf("\tLDA #<STR_%d\n\tLDX #>STR_%d\n\tJSR PRINT_STR_SYS\n", string_id, string_id);
      printf(".rodata\nSTR_%d:\t.asciiz \"%s\"\n.code\n", string_id, curr_tok.lexeme);
      string_id++;
      next_token();
    } else {
      expression();
      printf("\tJSR PRINT_INT_SYS\n");
    }
    
    if (!match(TOK_RPAREN)) return 0;
    if (!match(TOK_SEMI)) return 0;
  }
  return 1;
}

int compile() {
  src_pos = 0;
  line_num = 1;
  sym_count = 0;
  next_zp_addr = 0x60;
  
  next_token();
  
  if (!match(TOK_PROGRAM)) return 0;
  if (!match(TOK_IDENT)) return 0;
  if (!match(TOK_SEMI)) return 0;
  
  if (curr_tok.type == TOK_VAR) {
    next_token();
    while (curr_tok.type == TOK_IDENT) {
      add_symbol(curr_tok.lexeme);
      next_token();
      if (!match(TOK_COLON)) return 0;
      if (!match(TOK_INTEGER)) return 0;
      if (!match(TOK_SEMI)) return 0;
    }
  }
  
  if (!match(TOK_BEGIN)) return 0;
  printf("; --- BEGIN 6502 CODE GENERATION ---\n");
  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) return 0;
  }
  if (!match(TOK_END)) return 0;
  if (!match(TOK_DOT)) return 0; 
  printf("; --- COMPILATION SUCCESSFUL. RTS EMITTED ---\n\tRTS\n");
  return 1;
}

void mini_editor() {
  int ch;
  int buf_idx = 0;
  memset(src_buf, 0, MAX_SRC_LEN);

  while ((ch = getc(stdin)) != EOF) {
    if (buf_idx < MAX_SRC_LEN - 1) {
      src_buf[buf_idx++] = (char)ch;

      if (buf_idx >= 4 &&
          (src_buf[buf_idx - 4] == 'Q' || src_buf[buf_idx - 4] == 'q') &&
          (src_buf[buf_idx - 3] == 'U' || src_buf[buf_idx - 3] == 'u') &&
          (src_buf[buf_idx - 2] == 'I' || src_buf[buf_idx - 2] == 'i') &&
          (src_buf[buf_idx - 1] == 'T' || src_buf[buf_idx - 1] == 't')) {
        buf_idx -= 4;
        break;
      }
    } else {
      break;
    }
  }
  src_buf[buf_idx] = '\0';
}

#ifdef TPTEST
void run_file_test(const char* filepath) {
  size_t bytes_read;
  int success;
  FILE* file = fopen(filepath, "r");
  if (!file) {
    printf("Diagnostic Error: Could not load testing program '%s'\n", filepath);
    return;
  }
  
  memset(src_buf, 0, MAX_SRC_LEN);
  bytes_read = fread(src_buf, 1, MAX_SRC_LEN - 1, file);
  fclose(file);
  
  success = compile();
  printf(">> DIAGNOSTIC RESULT: %s\n", success ? "[PASS]" : "[FAIL]");
}
#endif

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

#ifdef TPTEST
  run_file_test("tp-test.p");
  return 0;
#else
  mini_editor();
  if (strlen(src_buf) > 0) {
    compile();
  }
  return 0;
#endif
}
