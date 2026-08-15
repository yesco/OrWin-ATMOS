// OrWIN Shell pipeline execute

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>

// TODO: make it a printable string?
#define EOS ((char*)-1)


typedef void* (*cmdfun)(void* state, char* line);

typedef cmdfun* cmdtrain;



typedef struct simplestate { cmdfun f; } simplestate;

void* stalloc(unsigned int size, void* f) {
  simplestate* state= calloc(size, 1);
  state->f= f;
  return state;
}

#define STALLOC(strct, fun) stalloc(sizeof(strct), fun)

#define SIMPLEALLOC(fun) STALLOC(simplestate,fun)

typedef struct pstate { cmdfun f; char* s; } pstate;

#define PSTALLOC(fun, p) (state=STALLOC(pstate, fun), state->s=strdup(p), state)

void* memdup(void* p, unsigned int bytes) {
  char* r= malloc(bytes);
  assert(r);
  return memcpy(r, p, bytes);
}

#if !defined(_POSIX_C_SOURCE) && !defined(__ANDROID__) && !defined(_STRDUP_DEFINED)
  #define strdup(s) safe_fallback_strdup(s)
  
  static char* safe_fallback_strdup(const char* s) {
#if 1
    return memdup(s, strlen(s)+1);
#else
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* d = malloc(len);
    return d ? memcpy(d, s, len) : NULL;
#endif      
  }
#endif


void* lfree(char* line) {
  // TODO: keep a pool? reuse!
  free(line);
  return NULL;
}

char* lstrcpy(char* line, char* s) {
  if (line && s && strlen(s) <= strlen(line))
    return strcpy(line, s);
  lfree(line);
  return strdup(s);
}


///////////////////////////////////////////////////////////


// generate one value
void* pwd(simplestate* state, char* line) {
  if (!state) return SIMPLEALLOC(pwd);

  // pass-through backtracking
  if (!line) return line;

  // generate a value on EOF (or any), lol
  return strdup("/home/orwin");
}


void* grep(pstate* state, char* line) {
  if (!state) return PSTALLOC(grep, line);

  // pass-through backtracking
  if (!line || line==EOS) return line;

  // match one line
  return strstr(line, state->s)? line: lfree(line);
}


// fake file
char* fakefile[]= { "one", "two", "three", "four", "five", NULL };

typedef struct fakefilestate { cmdfun f; char** fil; } fakefilestate;

void* cat(fakefilestate* state, char* line) {
  if (!state) {
    state= STALLOC(fakefilestate, cat);
    // strdup(line)
    state->fil= fakefile;
    return state;
  }

  // pass on
  //if (!line || line==EOS) return line;

  //return *state->fil? lstrcpy(line, *state->fil++): EOS;
  if (line!=EOS) lfree(line);
  return *state->fil? strdup(*state->fil++): EOS;
}
  

typedef struct wcstate { cmdfun f; unsigned int ln, wn, cn; } wcstate;

void* wc(wcstate* state, char* line) {
  char c, *s= line;
  unsigned int n;
  
  if (!state) return STALLOC(wcstate, wc);

  // Output summary at end of file
  if (line==EOS) {
    line= malloc(25);
    sprintf(line, "%u %u %u", state->ln, state->wn, state->cn);
    return line;
    // TODO: do we need to put code to give EOF?
  }

  // process one line
  state->ln++;
  while((c=*s)) {
    while(isspace(c)) c=*++s,++n;
    if (c) state->wn++;
    while(!isspace(c) && c) c=*++s,++n;
  }
  state->cn+= n;
  
  // returns null (backtracks to get prev line)
  return lfree(line);
}
  
// more like "tee -"
void* teeterminal(simplestate* state, char* line) {
  if (!state) return STALLOC(wcstate, wc);
  if (line==EOS) puts("*EOS*");
  else if (line) puts(line);
  else puts("*NULL*");
  return line;
}

// more like "tee -"
void* terminal(simplestate* state, char* line) {
  if (!state) return STALLOC(simplestate, terminal);
  if (line==EOS) puts("*EOS*");
  else if (line) puts(line);
  else puts("*NULL*");
  // force backtracking, why different?
  return line==EOS? EOS: NULL;
}

char* cmdnames[]= {
  "pwd",
  "grep",
  "cat",
  "wc",
  "teeterminal",
  "terminal",
  
  //  "ls cat find "
  //"grep cut tr sed " 
  //"echo "
  //"tail head diff uniq comm "
  //"wc less sort gzip gunzip unzip "

  // "xargs "
  // "history man "

  // "tar paste "
  // "awk "  
  // "pwd date "
  // "clear basname dirname "
  // "ps df top htop kill free whoami uptime uname killall "
  // "cd rm cp mv mkdir chmod chown touch ln rmdir chgrp "
  // "curl wget rsync scp "
  // "ping ip ss netstat "
  // "git "

  NULL
};

void* commands[]= {
  pwd, grep, cat, wc, teeterminal, terminal,
  
};

////////////////////////////////////////////////////////////


int wsystrain(cmdtrain *train) {
  cmdfun *fp;
  char* line= EOS;
  cmdtrain *origtrain= train;

  ++train; // skip initial 0

  while((fp=*train)) {
    printf("\t[%d \"%s\" =>]\n", (char)(train-origtrain),
	   !line? "(NULL)": line==EOS? "*EOS*": line);
    line= (*fp)(fp, line);
    if (line) ++train; else --train;
  }    

  // TODO: address of last program
  return 0;
}


int wsystem(char* cmd) {
  // TODO: use shared area?
  static char line[80];
  char c, i, *p, **n;
  cmdfun* f;
  void* state;
  static void* arr[16];
  
  memset(arr, 0, sizeof(arr));
  i= 0;
  
  while(*cmd) {
  
    // === extract one separated command

    // skip spaces
    while(isspace((c=*cmd))) ++cmd;
    // skip |
    while((c=*cmd) == '|' && c) ++cmd;
    
    //printf("...>%s<\n", cmd);

    // = extract program name
    p= line;
    // skip spaces
    while(isspace((c=*cmd))) ++cmd;
    // copy name
    while((c=*cmd) && !isspace(c) && c!='|') *p++= c,++cmd;
    *p= 0;

    // done?
    //    if (!*line) return 0;

    // find
    n= cmdnames;
    f= (cmdfun*)commands;
    while(*n && *f) {
      //printf("  ?  %s %s\n", line, (char*)*n);
      if (0==strcmp(line, (char*)*n)) goto found;
      ++n; ++f;
    }

    printf("%%Error.system: not found >%s< (%s)\n  %p %p %d %d\n",
	   line, cmd, *n, *f, !*n, !*f);
    return -1;

  found:

    printf("\t[%s: ", line);

    // = Extract arguments (how about intial)

    p= line;
    // skip spaces
    while(isspace(*cmd)) ++cmd;
    // copy rest of arguments
    while((c=*cmd) && c != '|') *p++= c,++cmd;

    // remove trailing spaces
    while(isspace(p[-1]) && p>line) --p;
    *p= 0;
    
    // TODO: crash
    arr[++i]= state= (*f)(0, line);

    printf("\"%s\" %p %p]\n", line, f, state);
  }
  
  putchar('\n');
  
  cmdtrain *train= memdup(arr, ++i*sizeof(arr[0]));

  return wsystrain(train);
}

int main(int argc, char** argv) {
  printf("------------ wsystrain: pwd | terminal\n");
  cmdtrain mock[]= {
    0,
    pwd(0, 0),
    terminal(0, 0),
    0,
  };
  
  wsystrain(mock);
  
  printf("------------ wsystem: pwd | terminal\n");
  // Error codes? How & semantics
  wsystem("pwd | terminal");

  printf("------------ wsystem: cat fil | grep o | terminal\n");
  // Error codes? How & semantics
  wsystem("cat fil | grep o | terminal");

  return 0;
}
