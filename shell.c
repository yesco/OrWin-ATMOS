// OrWIN Shell pipeline execute

//#define SHELLTRACE
#define SHELLINFO
#define SHELLTEST

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>

// TODO: make it a printable string?
#define EOS     ((char*)1)
#define CLEANUP	((char*)2)



typedef void* (*cmdfun)(void* state, char* line);

typedef cmdfun* cmdtrain;

unsigned int traincleanbits;

#define REQUEST_CLEANUP() (traincleanbits|=1)



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
  if (line && line!=EOS && line!=CLEANUP) free(line);
  return NULL;
}

char* lstrcpy(char* line, char* s) {
  if (line && s && strlen(s) <= strlen(line))
    return strcpy(line, s);
  lfree(line);
  return strdup(s);
}

#define lstrdup strdup

///////////////////////////////////////////////////////////


// generate one value
void* pwd(simplestate* state, char* line) {
  if (!state) return SIMPLEALLOC(pwd);

  if (!line) return EOS;

  // generate a value on EOS (or any), lol
  lfree(line);
  return strdup("/home/orwin");
}


void* grep(pstate* state, char* line) {
  if (!state) return PSTALLOC(grep, line);

  // pass-through backtracking
  if (!line || line==EOS) return line;

  // match one line
  return strstr(line, state->s)? line: lfree(line);
}


#ifdef FAKE
// fake file
char* fakefile[]= { "one", "two", "three", "four", "five", NULL };

typedef struct fakefilestate { cmdfun f; char** fil; } fakefilestate;

void* cat(fakefilestate* state, char* line) {
  if (!state) {
    state= STALLOC(fakefilestate, cat);
    if (!state) return NULL;
    state->fil= fakefile;
    return state;
  }

  lfree(line);
  return *state->fil? strdup(*state->fil++): EOS;
}

#else

typedef struct filestate {
  cmdfun f;
  FILE* fil;
} filestate;

void* cat(filestate* state, char* line) {
  char* ln= NULL;
  size_t sz= 0;
  if (!state) {
    state= STALLOC(filestate, cat);
    if (!state) return NULL;

    REQUEST_CLEANUP();
    if ((state->fil= fopen(line, "r"))) return state;

    // errors
    free(state);
    return NULL; // TODO: logic?
  } else if (line==CLEANUP) {
    if (state->fil) fclose(state->fil);
    return NULL;
  }
    

  lfree(line);

  // EOF if eof or error?
  if (EOF==getline(&ln, &sz, state->fil)) {
    //printf("==eof==\n");
    free(ln);
    fclose(state->fil); state->fil= NULL;
    return EOS;
  } else {
    //printf("==line==>%s< %p\n", ln, ln);
    // Reuse isdifficult as we haven't recorded sz?
    return ln;
  }
}
#endif
  

typedef struct wcstate { cmdfun f; unsigned int ln, wn, cn; } wcstate;

void* wc(wcstate* state, char* line) {
  char c, *s= line;
  unsigned int n= 0;
  
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
  

// ============================================================================
// ls


#ifndef NOSTACK

// Returns 1 if string matches glob pattern, 0 otherwise
int wildmatch(char* pat, char* s) {
  //printf("  -- >%s<\t>%s<\n", pat, s);
	 
  if (!pat) return 1;
  while(*pat) {
    if (*pat == '*') {
      // Multiple consecutive stars are treated as a single star
      while (*pat == '*') ++pat;
      if (!*pat) return 1; // Trailing star matches everything remaining
      
      while (*s) {
        if (wildmatch(pat, s)) return 1;
        ++s;
      }
      return 0;
    } else if (*pat == *s) {
      ++pat;
      ++s;
    } else {
      return 0;
    }
  }
  return !*s;
}

#else

int wildmatch(char* pat, char* s) {
  char* p_track = NULL;
  char* s_track = NULL;
  if (!pat) return 1;

  while (*s) {
    if (*pat == '*') {
      while (*pat == '*') ++pat;
      if (!*pat) return 1;
      p_track = pat;
      s_track = s;
    } else if (*pat == *s) {
      ++pat;
      ++s;
    } else if (p_track) {
      pat = p_track;
      s = ++s_track;
    } else {
      return 0;
    }
  }
  while (*pat == '*') ++pat;
  return !*pat;
}

#endif


// ============================================================================
// CC65 Implementation
// ============================================================================
#include <errno.h>

#ifdef __CC65__
#include <directory.h>

typedef struct lsstate {
  cmdfun f;
  unsigned char dir_open;
  struct directory dir;
  struct direntry entry;
  char* pat;
} lsstate;

void* ls(lsstate* state, char* line) {
  if (!state) {
    state = STALLOC(lsstate, ls);
    if (!state) return NULL;
    
    if (line && *line) {
      // If it contains a wildcard or is an explicit filename, save it as a filter pattern
      if (strchr(line, '*')) {
	char* p= strrchr(line, '/');
	if (p) { *p= 0; state->pat = strdup(p+1); }
	// TODO: simplify duplication
	else { state->pat = strdup(line); line = 0; }
      }	else { state->pat = strdup(line); line = 0; }
    }

    if (0 != open_dir(&state->dir, (line && *line)? line: ".")) {
      state->dir_open = 1;
      REQUEST_CLEANUP();
      return state;
    }
    free(state);
    return NULL;
  }

  lfree(line);
  if (!state->dir_open) return EOS;

  do {
    if (0==read_dir(&state->dir, &state->entry)
	|| line == CLEANUP) {
      if (state->dir_open) {
	close_dir(&state->dir);
	state->dir_open = 0;
	free(state->pat);
      }
      return EOS;
    }
  } while(!wildmatch(state->pat, state->entry.name));

  // found a matching one
  return lstrdup(state->entry.name);
}

#else

// ============================================================================
// POSIX / Linux / Termux Target Implementation
// ============================================================================
#include <sys/types.h>
#include <dirent.h>

typedef struct lsstate {
  cmdfun f;
  DIR* dir;
  char* pat;
} lsstate;

void* ls(lsstate* state, char* line) {
  if (!state) {
    state = STALLOC(lsstate, ls);
    if (!state) return NULL;

    if (line && *line) {
      // If it contains a wildcard or is an explicit filename, save it as a filter pattern
      if (strchr(line, '*')) {
	char* p= strrchr(line, '/');
	if (p) { *p= 0; state->pat = strdup(p+1); }
	// TODO: simplify duplication
	else { state->pat = strdup(line); line = 0; }
      }	else { state->pat = strdup(line); line = 0; }
    }

    state->dir = opendir((line && *line)? line: ".");
    REQUEST_CLEANUP();
    if (state->dir) return state;
    free(state);
    return NULL;
  }

  lfree(line);
  if (!state->dir) return EOS;

  struct dirent* de;
  do {
    if (!(de=readdir(state->dir)) || line == CLEANUP) {
      if (state->dir) {
	closedir(state->dir);
	state->dir = NULL;
	free(state->pat);
      }
      return EOS;
    }

  } while(!wildmatch(state->pat, de->d_name));

  // found a matching one
  return lstrdup(de->d_name);
}

#endif


///////////////////////////////////////////////////

char* nextStr(char** line, char* dflt) {
  char *r, *p= *line;
  if (!line || !*line) return dflt;
  // skip spaces
  while(isspace(*p)) ++p;
  // r points to first non whitespace (or at end)
  r= p;
  // skip till end of "word"
  while(*p && !isspace(*p)) ++p;
  // truncate string (we either on 0 or whitespace)
  if (*p) *p++= 0;
  // move input pointer to rest
  *line= p;
  return *r? r: dflt;
}

int nextInt(char** line, int dflt) {
  char *r= nextStr(line, NULL);
  return (r && (isdigit(*r) || *r=='-'))
    ? atoi(r): dflt;
}

///////////////////////////////////////////////////

typedef struct countstate {
  cmdfun f;
  int n;
  int d;
  int e;
} countstate;


void* iota(countstate* state, char* line) {
  if (!state) {
    int x;
    state = STALLOC(countstate, iota);
    if (!state) return NULL;

    state->n = nextInt(&line, 1);
    state->e = nextInt(&line, 10);
    state->d = nextInt(&line, 1);
    return state;
  }

  lfree(line);
  if ((state->d > 0 && state->n <= state->e) ||
      (state->d < 0 && state->n >= state->e)) {
    line= calloc(16,1);
    sprintf(line, "%d", state->n);
    state->n+= state->d;
    return line;
  }

  return EOS;
}
        

void* head(countstate* state, char* line) {
  if (!state) {
    state = STALLOC(countstate, head);
    if (!state) return NULL;

    state->n= 10; // default
    if (line && *line) {
      if (*line=='-') ++line;
      state->n= atoi(line);
    }
    return state;
  }

  if (!line || line==EOS) return line;

  if (state->n-- > 0) return line;

  // This "cuts-off" the consumer
  lfree(line);
  return EOS;
}


void* tail(countstate* state, char* line) {
  if (!state) {
    state = STALLOC(countstate, tail);
    if (!state) return NULL;

    // +3 means skip 3 lines, -3 means last 3
    state->n= 0;
    if (*line=='+') ++line;
    state->e= -nextInt(&line, 10);
    //    if (state->e < -2) state->e+= 2;
    if (state->e > 0) state->d= (int)calloc(state->e, sizeof(char*));
    return state;
  }

  printf("\t[TAIL %d %d %d %p]\n", state->n, state->e, state->d, state->d);

  // skip lines code
  if (state->e < 0) {
    state->e++;
    lfree(line);
    return NULL;
  }
  if (state->e == 0) return line;

  // tail code (keep ring buffer)
  char** ring= (char**)state->d;
  // step
  if (++state->n >= state->e) state->n= 0;
  
  if (line==EOS || !line) {
    // generate output
    if (state->e-- <= 0) return EOS; // TODO: cleanup!

    line= ring[state->n];
    ring[state->n]= NULL;
    return line;
  }
  
  // insert
  lfree(ring[state->n]);
  ring[state->n]= line;
  return NULL;
}

///////////////////////////////////////////////////



void shprint(char* line) {
  char lastc;
  
  if (line==EOS)  puts("*EOS*");
  else if (!line) puts("*NULL*");
  else {
    while(*line) putchar(lastc= *line++);
    if (lastc != '\n') putchar('\n');
  }
}

// more like "tee -"
void* teeterminal(simplestate* state, char* line) {
  if (!state) return STALLOC(wcstate, wc);

  shprint(line);
  return line;
}

// can only be last in chain!
void* terminal(simplestate* state, char* line) {
  if (!state) return STALLOC(simplestate, terminal);

  shprint(line);
  // force backtracking, why different?
  return line==EOS? EOS: NULL;
}

char* cmdnames[]= {
  "pwd",
  "grep",
  "cat",
  "wc",
  "ls",
  "iota",
  "head",
  "tail",

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
  pwd, grep, cat, wc, ls, iota, head, tail,
  teeterminal, terminal,
  
};

////////////////////////////////////////////////////////////


int wsystrain(cmdtrain *train) {
  cmdfun *fp;
  char* line= EOS;
  cmdtrain *origtrain= train;

  ++train; // skip initial 0

  while((fp=*train)) {
    #ifdef SHELLTRACE
    printf("\t[%d \"%s\" =>]\n", (char)(train-origtrain),
	   !line? "(NULL)": line==EOS? "*EOS*": line);
    #endif
    line= (*fp)(fp, line);
    if (line) ++train; else --train;
  }    

  // TODO: address of last program
  return 0;
}


int wsystem(char* cmd) {
  // TODO: check overflow this per command?
  static char line[80];
  char c, i, *p, **n;
  cmdfun* f;
  void* state;
  static void* arr[16];
  
  #ifdef SHELLTEST
  printf("\n------ wsystem: \"%s\"\n", cmd);
  #endif
  
  // a train {NULL, ..., NUL} ! - simplifies logic!
  memset(arr, 0, sizeof(arr));
  traincleanbits= 0;
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

    #ifdef SHELLINFO
    printf("\t[%s: ", line);
    #endif

    // = Extract arguments (how about intial)

    p= line;
    // skip spaces
    while(isspace(*cmd)) ++cmd;
    // copy rest of arguments
    while((c=*cmd) && c != '|') *p++= c,++cmd;

    // remove trailing spaces
    while(isspace(p[-1]) && p>line) --p;
    *p= 0;
    
    traincleanbits<<= 1;

    // This calls the INIT for the command!
    // (state==0)
    arr[++i]= state= (*f)(0, line);

    if (!state) {
      // ABORT!
      printf("Command %s init error gave NULL!\n", cmd);
      return -1;
    }

    #ifdef SHELLINFO
    printf("\"%s\" %p %p]\n", line, f, state);
    #endif
  }
  
  putchar('\n');

  // make a copy
  // TODO: pack it in as {CLEANBITS, NULL, ...., NULL} !
  unsigned int cleanbits= traincleanbits;
  cmdtrain *train= memdup(arr, (i+1)*sizeof(arr[0]));

  int r= wsystrain(train);

  // CLEANUP

 cleanup:

  do {
    cmdfun *f= train[i];
    if (f) {
      if (cleanbits & 1) {
        #ifdef SHELLINFO
	printf("\t[**CLEANUP**: %d %p]\n", i, train[i]);
        #endif
        (*f)(f, CLEANUP);
      }
      // remove that state
      free(f);
    }

    cleanbits>>= 1;
  } while(--i);
  
  free(train);

  return r;
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
  
  // Error codes? How & semantics

  printf("------------ wsystem: pwd | terminal\n");
  wsystem("pwd | terminal");
  wsystem("cat numbers.txt | grep o | terminal");
  wsystem("ls | terminal");
  wsystem("ls *.c | terminal");
  wsystem("ls *.md | terminal");
  wsystem("ls ../OrWin-ATMOS/*~ | terminal");

  wsystem("iota 2 22 | terminal");
  wsystem("iota 10 -10 -2 | terminal");

  wsystem("iota 1 100 | tail +96 | terminal");

  wsystem("iota 1 100 | tail -4 | terminal");
	  

  wsystem("cat numbers.txt | head | terminal");
  wsystem("cat numbers.txt | head -3 | terminal");  

  //wsystem("ls | head -3 | terminal");
  
  return 0;
}
