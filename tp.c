#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

// --- COMPILER WORKSPACES ---
char src_buf[MAX_SRC_LEN];
int src_pos = 0;
int line_num = 1;
int last_logged_line = 0; 
Token curr_tok;

// THE NATIVE MACHINE BITS TARGET WORKSPACE
unsigned char jit_bin[MAX_BIN_LEN];
int bin_ptr = 0;

// FIXED: String pool configured as a proper 2D array
char string_pool[8][MAX_SYM_LEN + 1]; // FIXED: Proper 2D tracking matrix
int string_pool_count = 0;

// --- SYMBOL TABLE ---
typedef enum { SYM_VAR, SYM_FUNC } SymbolType;
typedef struct {
  char name[MAX_SYM_LEN + 1];
  SymbolType type;
  int address; 
} Symbol;

Symbol sym_table[MAX_SYMBOLS];
int sym_count = 0;

// HARDWARE ADDRESS BOUNDARIES
#define ARG_ZP_LO 0x02
#define ARG_ZP_HI 0x03
#define TMP_ZP_LO 0x08
#define TMP_ZP_HI 0x09

typedef int (*PascalFunc)(int x);

// --- C89 FORWARD DECLARATIONS FOR CL65 ---
void next_token(void);
int match(TokenType expected);
int expression(void);
int statement(void);

// --- NATIVE C CALLOUT HOOKS FOR JIT CALLS ---
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

void add_symbol(const char* name, SymbolType type, int address) {
  if (sym_count < MAX_SYMBOLS) {
    strcpy(sym_table[sym_count].name, name);
    sym_table[sym_count].type = type;
    sym_table[sym_count].address = address;
    sym_count++;
  }
}

int lookup_symbol(const char* name, SymbolType* type, int* address) {
  int i;
  for (i = 0; i < sym_count; i++) {
    if (strcmp(sym_table[i].name, name) == 0) {
      *type = sym_table[i].type;
      *address = sym_table[i].address;
      return i;
    }
  }
  return -1;
}

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
    else if (strcmp(curr_tok.lexeme, "FUNCTION") == 0) curr_tok.type = TOK_FUNCTION;
    else curr_tok.type = TOK_IDENT;

    log_token();
    return;
  }
  
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
  
  // FIXED: Safe array byte index modification to support C89 lvalues
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

int expression(void) {
  SymbolType stype;
  int saddr;
  char name[MAX_SYM_LEN + 1];
  unsigned int jit_base_addr;
  unsigned int target_func_pc;
  unsigned int param_storage_addr;
  unsigned int return_storage_addr;
  unsigned int var_ram_addr;
  int math_tmp_addr;
  int math_tmp2_addr;
  unsigned int math_tmp_ram;
  unsigned int math_tmp2_ram;

  jit_base_addr = (unsigned int)jit_bin;

  if (curr_tok.type == TOK_NUM) {
    emit(0xA9); emit(curr_tok.value & 0xFF);        
    emit(0xA2); emit((curr_tok.value >> 8) & 0xFF); 
    next_token();
  } else if (curr_tok.type == TOK_IDENT) {
    strcpy(name, curr_tok.lexeme);
    
    if (lookup_symbol(name, &stype, &saddr) == -1) {
      printf("\n%s <---- ERROR: Undeclared identifier\n", curr_tok.lexeme);
      return 0;
    }
    
    next_token();
    if (stype == SYM_FUNC) {
      if (!match(TOK_LPAREN)) return 0;
      if (!expression()) return 0; 
      if (!match(TOK_RPAREN)) return 0;
      
      param_storage_addr = jit_base_addr + (unsigned int)saddr + 0;
      return_storage_addr = jit_base_addr + (unsigned int)saddr + 2;
      target_func_pc = jit_base_addr + (unsigned int)saddr + 4;
      
      emit(0x8D); emit(param_storage_addr & 0xFF); emit((param_storage_addr >> 8) & 0xFF); 
      emit(0x8E); emit((param_storage_addr + 1) & 0xFF); emit(((param_storage_addr + 1) >> 8) & 0xFF); 
      
      emit(0x20); emit(target_func_pc & 0xFF); emit((target_func_pc >> 8) & 0xFF); 
      
      emit(0xAD); emit(return_storage_addr & 0xFF); emit((return_storage_addr >> 8) & 0xFF); 
      emit(0xAE); emit((return_storage_addr + 1) & 0xFF); emit(((return_storage_addr + 1) >> 8) & 0xFF); 
    } else {
      var_ram_addr = jit_base_addr + (unsigned int)saddr;
      emit(0xAD); emit(var_ram_addr & 0xFF); emit((var_ram_addr >> 8) & 0xFF); 
      emit(0xAE); emit((var_ram_addr + 1) & 0xFF); emit(((var_ram_addr + 1) >> 8) & 0xFF); 
    }
  }
  
  if (curr_tok.type == TOK_PLUS) {
    next_token();
    
    lookup_symbol("__MATH_TMP", &stype, &math_tmp_addr);
    math_tmp_ram = jit_base_addr + (unsigned int)math_tmp_addr;
    
    emit(0x8D); emit(math_tmp_ram & 0xFF); emit((math_tmp_ram >> 8) & 0xFF);         
    emit(0x8E); emit((math_tmp_ram + 1) & 0xFF); emit(((math_tmp_ram + 1) >> 8) & 0xFF); 
    
    if (!expression()) return 0; 
    
    lookup_symbol("__MATH_TMP2", &stype, &math_tmp2_addr);
    math_tmp2_ram = jit_base_addr + (unsigned int)math_tmp2_addr;
    
    emit(0x8D); emit(math_tmp2_ram & 0xFF); emit((math_tmp2_ram >> 8) & 0xFF);
    emit(0x8E); emit((math_tmp2_ram + 1) & 0xFF); emit(((math_tmp2_ram + 1) >> 8) & 0xFF);
    
    emit(0x18);             
    emit(0xAD); emit(math_tmp_ram & 0xFF); emit((math_tmp_ram >> 8) & 0xFF);         
    emit(0x6D); emit(math_tmp2_ram & 0xFF); emit((math_tmp2_ram >> 8) & 0xFF); 
    emit(0x48);             
    
    emit(0xAD); emit((math_tmp_ram + 1) & 0xFF); emit(((math_tmp_ram + 1) >> 8) & 0xFF); 
    emit(0x6D); emit((math_tmp2_ram + 1) & 0xFF); emit(((math_tmp2_ram + 1) >> 8) & 0xFF); 
    emit(0xAA);             
    emit(0x68);             
  }
  return 1;
}

int statement(void) {
  unsigned int callback_addr;
  if (curr_tok.type == TOK_IDENT) {
    char target_var[MAX_SYM_LEN + 1];
    SymbolType stype;
    int addr;
    unsigned int var_ram_addr;
    
    strcpy(target_var, curr_tok.lexeme);
    
    if (lookup_symbol(target_var, &stype, &addr) == -1) {
      printf("\n%s <---- ERROR: Unknown identifier\n", target_var);
      return 0;
    }
    
    if (!match(TOK_IDENT)) return 0;
    if (!match(TOK_ASSIGN)) return 0;
    if (!expression()) return 0;
    
    var_ram_addr = ((unsigned int)jit_bin) + (unsigned int)addr;
    emit(0x8D); emit(var_ram_addr & 0xFF); emit((var_ram_addr >> 8) & 0xFF);     
    emit(0x8E); emit((var_ram_addr + 1) & 0xFF); emit(((var_ram_addr + 1) >> 8) & 0xFF); 
    if (!match(TOK_SEMI)) return 0;
  } 
  else if (curr_tok.type == TOK_WRITE) {
    if (!match(TOK_WRITE)) return 0;
    if (!match(TOK_LPAREN)) return 0;
    
    if (curr_tok.type == TOK_STRING) {
      int jmp_patch_idx;
      unsigned int str_start_ram;
      unsigned int str_end_ram;
      int idx = 0;
      
      emit(0x4C);
      jmp_patch_idx = bin_ptr;
      emit(0x00); emit(0x00); 
      
      str_start_ram = ((unsigned int)jit_bin) + (unsigned int)bin_ptr;
      
      while (curr_tok.lexeme[idx] != '\0') {
        emit(curr_tok.lexeme[idx]);
        idx++;
      }
      emit(0x00); 
      
      str_end_ram = ((unsigned int)jit_bin) + (unsigned int)bin_ptr;
      jit_bin[jmp_patch_idx] = str_end_ram & 0xFF;
      jit_bin[jmp_patch_idx + 1] = (str_end_ram >> 8) & 0xFF;
      
      emit(0xA9); emit(str_start_ram & 0xFF);        
      emit(0xA2); emit((str_start_ram >> 8) & 0xFF); 
      
      callback_addr = (unsigned int)print_str_cb;
      emit(0x20); emit(callback_addr & 0xFF); emit((callback_addr >> 8) & 0xFF); 
      
      if (!match(TOK_STRING)) return 0;
    } else {
      if (!expression()) return 0;
      callback_addr = (unsigned int)print_int_cb;
      emit(0x20); emit(callback_addr & 0xFF); emit((callback_addr >> 8) & 0xFF); 
    }
    
    if (!match(TOK_RPAREN)) return 0;
    if (!match(TOK_SEMI)) return 0;
  } else {
    printf("\n%s <---- ERROR: Expected statement identifier\n", curr_tok.lexeme);
    return 0;
  }
  return 1;
}

int compile_pipeline(void) {
  int next_free_zp = 0x04;
  int main_prog_jump_patch_idx;
  unsigned int jit_base_addr;
  SymbolType stype;
  int final_res_addr;
  
  char func_name[MAX_SYM_LEN + 1];
  char param_name[MAX_SYM_LEN + 1];
  int func_base_offset; 
  int func_ret_addr;
  int func_param_addr;
  unsigned int ret_ram;
  unsigned int x_ram;
  unsigned int beta_ram;
  unsigned int main_prog_start_ram_addr;

  src_pos = 0;
  line_num = 1;
  last_logged_line = 0;
  bin_ptr = 0;
  sym_count = 0;
  string_pool_count = 0;
  
  jit_base_addr = (unsigned int)jit_bin;
  printf("\n[JIT Debug] jit_bin starts at RAM address: $%04X\n", jit_base_addr);
  
  // FIXED: The very first bytes at index 0 MUST be the absolute entry bypass JMP
  emit(0x4C); // JMP absolute
  main_prog_jump_patch_idx = bin_ptr; // Offset 1
  emit(0x00); emit(0x00);             // Offsets 1 and 2 placeholder targets
  
  // Allocate static scratch RAM variables immediately AFTER the entry JMP instruction
  add_symbol("__MATH_TMP", SYM_VAR, bin_ptr);
  emit(0x00); emit(0x00);
  
  add_symbol("__MATH_TMP2", SYM_VAR, bin_ptr);
  emit(0x00); emit(0x00);
  
  add_symbol("X", SYM_VAR, bin_ptr);
  emit(0x00); emit(0x00);
  
  next_token();
  if (!match(TOK_PROGRAM)) return 0;
  if (!match(TOK_IDENT)) return 0;
  if (!match(TOK_SEMI)) return 0;
  
  // Global Variable / Function Declaration Processing Loop
  while (curr_tok.type == TOK_VAR || curr_tok.type == TOK_FUNCTION) {
    if (curr_tok.type == TOK_VAR) {
      next_token();
      while (curr_tok.type == TOK_IDENT) {
        add_symbol(curr_tok.lexeme, SYM_VAR, bin_ptr);
        emit(0x00); emit(0x00); 
        
        if (!match(TOK_IDENT)) return 0; 
        if (!match(TOK_COLON)) return 0; 
        if (!match(TOK_INTEGER)) return 0; 
        if (!match(TOK_SEMI)) return 0;
      }
    }
    else if (curr_tok.type == TOK_FUNCTION) {
      func_base_offset = bin_ptr; 
      next_token();
      strcpy(func_name, curr_tok.lexeme);
      
      add_symbol(func_name, SYM_FUNC, func_base_offset);
      
      func_param_addr = bin_ptr;  emit(0x00); emit(0x00);
      func_ret_addr = bin_ptr;    emit(0x00); emit(0x00);
      
      if (!match(TOK_IDENT)) return 0;
      if (!match(TOK_LPAREN)) return 0;
      
      strcpy(param_name, curr_tok.lexeme);
      add_symbol(param_name, SYM_VAR, func_param_addr);
      
      if (!match(TOK_IDENT)) return 0;
      if (!match(TOK_COLON)) return 0;
      if (!match(TOK_INTEGER)) return 0;
      if (!match(TOK_RPAREN)) return 0;
      if (!match(TOK_COLON)) return 0;
      if (!match(TOK_INTEGER)) return 0;
      if (!match(TOK_SEMI)) return 0;
      
      add_symbol(func_name, SYM_VAR, func_ret_addr);
      
      if (!match(TOK_BEGIN)) return 0;
      while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
        if (!statement()) return 0;
      }
      if (!match(TOK_END)) return 0;
      if (!match(TOK_SEMI)) return 0;
      
      ret_ram = jit_base_addr + (unsigned int)func_ret_addr;
      emit(0xAD); emit(ret_ram & 0xFF); emit((ret_ram >> 8) & 0xFF); 
      emit(0xAE); emit((ret_ram + 1) & 0xFF); emit(((ret_ram + 1) >> 8) & 0xFF); 
      emit(0x60); 
    }
  }
  
  if (!match(TOK_BEGIN)) return 0;
  
  // Back-patch JMP using pure integer addition to prevent pointer corruption
  main_prog_start_ram_addr = jit_base_addr + (unsigned int)bin_ptr;
  jit_bin[main_prog_jump_patch_idx] = main_prog_start_ram_addr & 0xFF;
  jit_bin[main_prog_jump_patch_idx + 1] = (main_prog_start_ram_addr >> 8) & 0xFF;
  
  printf("[JIT Debug] Entry JMP patched at offset %d to point to Main Code: $%04X\n", main_prog_jump_patch_idx, main_prog_start_ram_addr);

  final_res_addr = lookup_symbol("X", &stype, &final_res_addr) ? final_res_addr : -1;
  if (final_res_addr != -1) {
    x_ram = jit_base_addr + (unsigned int)final_res_addr;
    emit(0x8D); emit(x_ram & 0xFF); emit((x_ram >> 8) & 0xFF);                 
    emit(0x8E); emit((x_ram + 1) & 0xFF); emit(((x_ram + 1) >> 8) & 0xFF);     
  }
  
  while (curr_tok.type != TOK_END && curr_tok.type != TOK_EOF) {
    if (!statement()) return 0;
  }
  
  if (!match(TOK_END)) return 0; 
  if (!match(TOK_DOT)) return 0;
  
  printf("\n"); 

  final_res_addr = lookup_symbol("BETA", &stype, &final_res_addr) ? final_res_addr : -1;
  if (final_res_addr != -1) {
    beta_ram = jit_base_addr + (unsigned int)final_res_addr;
    emit(0xAD); emit(beta_ram & 0xFF); emit((beta_ram >> 8) & 0xFF);             
    emit(0xAE); emit((beta_ram + 1) & 0xFF); emit(((beta_ram + 1) >> 8) & 0xFF); 
  }
  
  emit(0x60); 
  return 1;
}

int main(void) {
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
      printf("--- RUN ---\n");
      compiled_output = execute_native_jit(test_parameter);
      printf("--- END ---\n");
      printf("AX=%d\n", compiled_output);
    } else {
      printf(">> JIT Pipeline failed during compilation.\n");
    }
  }
  return 0;
}
