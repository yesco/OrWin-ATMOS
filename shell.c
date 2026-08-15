// OrWIN Shell pipeline execute

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include <stdio.h>

#define EOS ((char*)-1)

char* cmdnames=
  "iota "
  "ls cat find "
  "grep cut tr sed " 
  "echo "
  "tail head diff uniq comm "
  "wc less sort gzip gunzip unzip "

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

  ;

typedef void* (*cmdfun)(void* state, char* line);

cmdfun commands[]= {
};

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

#define PSTALLOC(fun, p) (state=STALLOC(pstate, fun), state->s=p, state)

#if !defined(_POSIX_C_SOURCE) && !defined(__ANDROID__) && !defined(_STRDUP_DEFINED)
  #define strdup(s) safe_fallback_strdup(s)
  
  static char* safe_fallback_strdup(const char* s) {
      if (!s) return NULL;
      size_t len = strlen(s) + 1;
      char* d = malloc(len);
      return d ? memcpy(d, s, len) : NULL;
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
    state->fil= fakefile;
    return state;
  }

  // pass on
  if (!line || line==EOS) return line;

  return lstrcpy(line, *state->fil++);
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
    while(c && !isspace(c)) c=*++s,++n;
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
  // force backtracking
  return NULL;
}


void wsystrain(cmdtrain *train) {
  cmdfun *fp;
  char* line= EOS;
  cmdtrain *origtrain= train;

  ++train; // skip initial 0
  while((fp=*train)) {
    printf("\t[%d \"%s\" =>]\n", (char)(train-origtrain), line&&line!=EOS? line: "(NULL)");
    line= (*fp)(fp, line);
    if (line) ++train; else --train;
  }    
}


int wsystem(char* cmd) {
  cmdtrain mock[]= {
    0,
    pwd(0, 0),
    terminal(0, 0),
    0,
  };
  
  // Error codes? How & semantics
  wsystrain(mock);

  return 0;
}

int main(int argc, char** argv) {
  wsystem("pwd | terminal");
  return 0;
}
