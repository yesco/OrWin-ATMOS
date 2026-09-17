// OrWIN Shell pipeline execute

// TODO:
// - uniq
// - sort (-u)
// - freq or huniq
// - bc
// - datamash qsv vsv

// TODO: behaves differently,, like never ends for set/print?
#define SHELLTRACE

#define SHELLINFO
//#define SHELLTEST

#define MAX_TRAIN 16

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>

// TODO: make it a printable string?
#ifndef EVENTS

  // TODO: should come from shared orwin.c?
  #define WAITKEY ((char*)0x300)
  #define CLEANUP	((char*)0x4fe)
  #define EVENTS  ((char*)0x500)
  #define EOS     ((char*)0x500)

#endif


#ifdef __CC65__
  extern size_t _heapmemavail(void);
#endif

typedef void* (*cmdfun)(void* state, char* line);

char* dummyfun(void* state, char* line) {
  return NULL;
  (void)state; (void)line;
}
 
typedef cmdfun* cmdtrain;

unsigned int traincleanbits;

#define REQUEST_CLEANUP() (traincleanbits|=1)



typedef struct simplestate { cmdfun f; int i; } simplestate;

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

// TODO: wtf? (2 bytes fail!)

  char* r= malloc(bytes+2);
#ifdef __CC65__
//  printf("BYTES=%5d\t%p\tAVAIL=%u\n", bytes, r, _heapmemavail());
#endif
  assert(r != NULL);
  return memcpy(r, p, bytes);
}

// TODO: ugly, use my fillins.c?

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


////////////////////////////////////////////////////////////
// line reuse

extern char isliteral(void* p);

#define LFREE(x) xfree((void**)&(x))

#if 0

// 

// 14s iota ... 

void lfree(char* line) {
  if (line > EVENTS) free(line);
  return;
}

char* lstrdup(const char* s) {
  return strdup(s);
}

#else

// 31710 bytes using clever reuse
// 31556 bytes - just wrappers
// (- 31710 31556) = 154 bytes (/ 14 13.0) = 8% faster???

char relen=0, *reuse= 0;

// 13s iota ... ONLY SAVES 1s? barely worth the effort!!!
// 10 should be according to CHEAPEST: in app_shell.c

// save S if bigger than reuse ptr!
// Always return 0, to make easy return after disposal
char* lfree(char* line) {
  char len;

  if (isliteral(line)) return 0;
  if (line <= EVENTS) return 0;
  
  // TODO: get actual size of allocation!
  len= strlen(line);
  if (!reuse || len > relen) {
    // it's bigger
    free(reuse);
    reuse= line; relen= len;
  }
  return 0;
}

// copy s
// TODO: make it taken len!
char* lstrdup(const char* s) {
  char slen= strlen(s), *line;
  if (slen <= relen && relen) {
    // reuse
    line= reuse; reuse= NULL; relen= 0;
    return strcpy(line, s);
  } else {
    // need bigger
    free(reuse); reuse= NULL; relen= 0;
    //printf("[ALLOCATE]"); // no bug?
    return strdup(s);
  }
}

#endif

void xfree(void** pp) {
  if (!pp) return;
  lfree(*pp);
  *pp= NULL;
}
  

////////////////////////////////////////////////////////////
// printing

void shprint(char* line) {
  static char lastc= 0;
  
#if defined(SHELLTRACE) || defined(SHELLINFO)
  if (!line)         puts("*NULL*");    else
  if (line==EOS)     puts("*EOS*");     else
  if (line==CLEANUP) puts("*CLEANUP*"); else {
#else
  if (line <= EVENTS) return;
  else {
#endif
    while(*line) putchar(lastc= *line++);
    if (lastc != '\n') putchar('\n');
  }
}

///////////////////////////////////////////////////////////
// unix "commands"

void* pwd(simplestate* state, char* line) {
  if (!state) return SIMPLEALLOC(pwd);

  if (!line) return EOS;

  // generate a value on EOS (or any), lol
  lfree(line);
  return lstrdup("/home/orwin");
}


void* grep(pstate* state, char* line) {
  if (!state) return PSTALLOC(grep, line);

  // pass-through backtracking
  if (!line || line==EOS) return line;

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
    return NULL;
    state= STALLOC(filestate, cat);
    if (!state) return NULL;

    REQUEST_CLEANUP();
    if ((state->fil= fopen(line, "r"))) return state;

    // errors
    free(state);
    return NULL; // TODO: logic?
  } else if (line==CLEANUP) {
    if (state->fil) fclose(state->fil); state->fil= NULL;
    return NULL;
  }

  lfree(line);

  // EOF if eof or error?

#ifdef __CC65__



  // TODO: fix: thisis unsafe



  ln= calloc(81,1);
  if (NULL!=fgets(ln, sz, state->fil)) {
#else

  if (EOF==getline(&ln, &sz, state->fil)) {
#endif
    //printf("==eof==\n");
    lfree(ln);
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
  

#define LS
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


#if defined( __ATMOS__) || defined(__CC65__) || defined(OSCAR64)

// NO HAVE FILES ON ATMOS

#ifdef __ATMOS__
// (used by whom?)
int open(char* path, int flags, int mode) { return 0; (void)path; (void)flags; (void)mode; }
int close(int fd) { return 0; (void) fd; }
#endif // ATMOS

typedef struct lsstate { int x; } lsstate;

// Dummy
 
#define ls dummyfun

#else // !ATMOS && !CC65
 

#ifdef __CC65__
// TODO: no have on atmos... (maybe works on C64)
//   doesn't have on sim65 :-(
#include <dirent.h>

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

#else // UNIX
 
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
  struct dirent* de;
  char* p;
  
  if (!state) {
    state = STALLOC(lsstate, ls);
    if (!state) return NULL;

    if (line && *line) {
      // If it contains a wildcard or is an explicit filename, save it as a filter pattern
      if (strchr(line, '*')) {

        p= strrchr(line, '/');
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

#endif // CC65 ... UNIX

#endif // __ATMOS__
 

// TODO: move to supporting functions shared w app_
 
///////////////////////////////////////////////////
// line space delimited parameter choppers
// If none: returns the DeFauLT value!
//
// NOTE: they modify the incoming line
// NOTE: if you need to keep the string do strdup!

// 123 : nextStr, NATIVE_CODE:code
char* nextStr(char** line, const char* dflt) {
  char *r, *p= *line;
  if (!line || !*line) return (char*)dflt;
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
  return *r? r: (char*)dflt;
}

// 68 : nextInt, NATIVE_CODE:code
int nextInt(char** line, int dflt) {
  char *r= nextStr(line, NULL);
  return (r && (isdigit(*r) || *r=='-'))
    ? atoi(r): dflt;
}

///////////////////////////////////////////////////

#ifdef __CC65__
  typedef int intptr_t; // LOL
#endif

#ifdef OSCAR64
  typedef int intptr_t;
#endif

typedef struct countstate {
  cmdfun f;
  int n;
  int e;
  intptr_t d; // dual use
} countstate;

void* iota(countstate* state, char* line) {
  if (!state) {
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
    char s[10];
    // TODO: use returned length:
    sprintf(s, "%d", state->n);
    state->n+= state->d;
    return lstrdup(s);
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
  unsigned int start;
  char** ring;
  
  if (!state) {
    state = STALLOC(countstate, tail);
    if (!state) return NULL;

    // +3 means skip 3 lines, -3 means last 3
    state->n= 0;
    if (*line=='+') ++line;
    state->e= -nextInt(&line, 10);
    //    if (state->e < -2) state->e+= 2;
    if (state->e > 0) state->d= (intptr_t)calloc(state->e, sizeof(char*));
    return state;
  }

  #ifdef SHELLTRACE
  printf("\n\t  [TAIL %d %d %d %p]\n", state->n, state->e, (int)state->d, (void*)state->d);
  #endif
  
  // skip lines code
  if (!state->d) {
    if (state->e++ >= 0) return line;
    lfree(line);
    return NULL;
  }

  // tail code (keep ring buffer)
  ring= (char**)state->d;

  if (line && line != EOS) { 
    // insert
    if (++state->n >= state->e) state->n= 0;
    LFREE(ring[state->n]);
    return NULL;
  }

  // generate output
  start= state->n;
  do {
    if (++state->n >= state->e) state->n= 0;
    line= ring[state->n];
    ring[state->n]= NULL;
    if (line) return line;
  } while (state->n != start);

  return EOS;
}


///////////////////////////////////////////////////
// Variable manipulators
 
// cc65: (- 35460 33462) = 1998 bytes = frickin hell!
// osc : (- 23802 22061) = 1741 // was 2007 if not used doesn/t count!
//
// cc65: 4065 free only... 7370 bytes with NO ENV...
 
// LOC: 85 lines (/ 1733 85) ~ 20 bytes/line

 
// oscar64: 106 vnth,   50 vbind
//          126 vgeti, 202 vgets
//          204 vseti, 173 vsets
//           30 vevals
//
#define ENVVARS
 
#ifndef ENVVARS
 
// dummies when not included
#define let   dummyfun
#define print dummyfun 

#else
 
// TODO: make it part of each "train"

#define MAX_VARS 32
 
// first is emtpy
struct var {
  char* name; // %var points to int %str points to string!
  union {
    int *  iptr;
    char** sptr;
    char*  ostr; 
   } val; // oscar64 requires a named union!
} vars[MAX_VARS]; // = {0}; cl65 cannot

unsigned char nvar= 0;

 
// 106 : vnth, NATIVE_CODE:code
char vnth(char* name) {
  char i= nvar, *nm;
  if (!((intptr_t)name)>>8) return *name; // 0x80+i
  do {
    if ((nm= vars[i].name) && 0==strcmp(name, nm)) return i;
  } while(--i);

  // not found - add var
  if (nvar >= MAX_VARS-1) return 0;
  ++nvar;

  // we make a copy if it's not a program literal!
  vars[nvar].name= isliteral(name)? name: strdup(name);
  vars[nvar].val.sptr= NULL;
  return nvar;
}

void vcleanup() {
  char i= nvar, *nm;
  do {
    if ((nm= vars[i].name)) {
      if (*nm == '$') free(vars[i].val.ostr);
      if (!isliteral(nm)) free(nm);
    }
  } while(--i);

  nvar= 0;
  memset(vars, 0, sizeof(vars));
}

// you can bind %var and _var
// TODO: unbind using ptr=NULL - action: don't want change order...

// 59 : vbind, NATIVE_CODE:code
char vbind(char* name, void* ptr) {
  char n= vnth(name);

  //assert(n);
  //assert(*name != '$');

  vars[n].val.iptr= ptr;
  return n;
}

char* vgets(char* name);
 
// 126 : vgeti, NATIVE_CODE:code
int vgeti(char* name) {
  char n= vnth(name);
  return !n? 0:
    (*name == '%')? *vars[n].val.iptr:
    atoi(vgets(name));
}
 
// Depending on $var (can be modified) const _var
char tmp10char[10]= "(int)"; // TODO: share?
 
// Return string representation of ?VAR
// It even works for int %VAR but gives a very temporary
// string that has to be used IMMEDIATELY (strdup maybe).
//
// If not var, return NAME! This means it can be used as "expander"

// 202 : vgets, NATIVE_CODE:code
char* vgets(char* name) {
  char n= vnth(name), *s= 
    !n? "":
    (*name == '_')? *vars[n].val.sptr:
    (*name == '$')? vars[n].val.ostr:
    (*name == '%')? (sprintf(tmp10char, "%d", vgeti(name)),tmp10char):
    name; // lol
  return s? s: "";
}
 
char* vsets(char* name, char* val);
 
// 204 : vseti, NATIVE_CODE:code
int vseti(char* name, int val) {
  char n= vnth(name);
  if (*name == '$') {
    sprintf(tmp10char, "%d", val);
    vsets(name, strdup(tmp10char));
    return val;
  } else if (*name == '_') return 0;
  return (*(vars[n].val.iptr)= val);
}

// gives ownershipt to $VAR of VAL string

// 173 : vsets, NATIVE_CODE:code
char* vsets(char* name, char* val) {
  char n= vnth(name), **sp;
  if (*name == '%') { vseti(name, atoi(val)); return val; }
  if (*name != '$') return "";
  lfree(*(sp=&vars[n].val.ostr));
  return (*sp= val);
}

// set ?VAR from string VAL, copy if $VAR, otherwise convert

//  91 : vsetsfrom, NATIVE_CODE:code
char* vsetsfrom(char* name, char* val) {
  return vsets(name, *name=='$'? strdup(val): val);
}
 

////////////////////////////////////////////////////////////
// variable commands

/*
  
1. The Standard $ Special Parameters (The Basics)

In POSIX shells,these are read-only macros maintained natively by the
shell's state machine.

$* and $@: All positional parameters. (In your code, mapping $* to the remaining raw string line matches classic Bourne shell behavior).
$#: The number of positional parameters currently set (as a stringified integer).
$?: The exit status of the last executed foreground command (crucial for && and || chaining).
$$: The Process ID (PID) of the current shell instance. 
$!: The Process ID (PID) of the most recently executed background command.
$0: The name of the shell script or the shell invocation string itself.
$1 to $9 (and ${10}): The explicit positional arguments.

3. Esoteric Prefixes Found in Other Shells

Depending on how much flavor you want to add to your shell, these
exist in the wild:

! History Expansion): Used by Bash/Zsh interactively.
!! repeats the last command, and !$ grabs the last argument of the
previous command.

^ (Rc Shell / Es): The rc shell (Plan 9) uses ^ as an explicit string
concatenation operator rather than a variable prefix, turning lists
into flattened arrays

*/

// TDOO: nextStr quote '$foo' - what about 'foo$foo' - maybe split?
// TODO: nextStr to break on ^ and do concat
// TODO: varrevals (take an array and "concat" implicitly)
//   but if get ^ no space, otherwise space
    
char* vevals(char* x, char** pline) {
  // TODO: make more generic
       if (0==strcmp(x, "$*"))   return *pline; // TODO: $@ ???
  else if (0==strcmp(x, "$++"))  return nextStr(pline, ""); // shift
  else                           return vgets(x);
}       

// TODO: add formattting %.3foo $-7bar %05i - lol!


typedef struct varstate {
  cmdfun fun;
  char*  name;   // TODO: make it store (char*)(char)idx
  char   varidx; // TODO: use
  char*  expr;    // Owned if _VAR
} varstate;

// "LET - Lexial EnvironmenT binding"
//(397 : set, NATIVE_CODE:code)
// 474 : set, NATIVE_CODE:code
char* set(varstate* state, char* line) {
  if (!state) {
    char *name;
    state= STALLOC(varstate, set);

    // variable name to set to expr
    name= state->name= strdup(nextStr(&line, (char*)""));
    state->varidx= vnth(name);
    state->expr= strdup(nextStr(&line, ""));
    return (char*)state;

  } else if (line==CLEANUP) {
    LFREE(state->name);
    LFREE(state->expr);
    return line;
  }

  if (line<=EVENTS) return line;
  else {
    char* oline= line;
    char* endline= line+strlen(line);
    //printf("SET:"); shprint(line);
    //printf("xxx: %s %s\n", state->name, state->expr);

    vsetsfrom(state->name, vevals(state->expr, &line));

    if (line==oline) return line;
    else if (line <= endline) {
      // pass on what's left
      endline= strdup(line); lfree(line); return endline;
    } else return ""; // something
  }
}

// printer
typedef struct printstate {
  cmdfun fun;
  char** params;
} printstate;
 
// actually, PRINT is CONCAT w spaces, or foo ^bar no space!
// (SPRINTF, how?) "%03.4foo" lol?
//
// 457 : print, NATIVE_CODE:code
char* print(printstate* state, char* line) {
  if (!state) {
    char np= 0, *param[16]= {0}, *p, *endline= line+strlen(line);
    state= STALLOC(varstate, print);
    if (!state) return NULL;
    do {
      p= param[np++]= strdup(nextStr(&line, NULL));
      //printf("\tprint %u %s\n", np, p);

// TODO: give "error" at 16
// TODO: nextStr doesn't know how to terminate!

    //} while(p!=NULL && line < endline);
    } while(line < endline);

    state->params= memdup(param, (np+1)*sizeof(char*));
    if (!state->params) { free(state); return NULL; }
    return (char*)state;

  } else if (line==CLEANUP) {
    char** p= state->params;
    while(*p) LFREE(*p++);

    LFREE(state->params);
    return line;
  }
  

  if (line<=EVENTS) return line;
  
  //printf("PRINT: "); shprint(line);
  // For every data return, print a line fill in params
  {
    char tmp[128]= {0}; // TODO: use dstr!
    char** p= state->params;
    char* ln= line;
    char* x;

    // TODO: varrevals(p, *line)
    while(*p) {
      //printf("\t%p : %s => %s\n", p, *p, vgets(*p));
      x= *p;
      if (*x!='^') strcat(tmp, " "); else ++x;
      strcat(tmp, vevals(x, &line));
      ++p;
    }
    lfree(ln); // used up!

    // TODO: "current" LINE should be set by system?
    return strdup(tmp);
  }
}

#endif // ENVVARS


#define SHELL_STATS_COMMAND
#ifdef SHELL_STATS_COMMAND

// Inside your stats state handler structure
typedef struct {
  cmdfun fun;
  char done;
  // TODO: float? oscar64 can do it
  int n;
  int min;
  int max;
  int sum;
  int sqsum;
  int samples[16];
  // results
  int avg;        // TODO: use 100x to get 2 decimals?
  int median;
  int var;
  int stddev;
} StatsState;

char* stats(StatsState* state, char* line) {
  if (!state) {
    state= STALLOC(StatsState, stats);
    state->min= 0x7fff;
    state->max= 0x8000;

    vbind("%count",  &state->n);
    vbind("%min",    &state->min);
    vbind("%max",    &state->max);
    vbind("%sum",    &state->sum);
    vbind("%sqsum",  &state->sqsum);
    vbind("%avg",    &state->avg);
    vbind("%median", &state->median);
    vbind("%var",    &state->var);
    vbind("%stddev", &state->stddev);
    return (char*)state;

  }
  
  // End Of Stream => report

#ifdef SHELLINFO
  printf("  LINE:"); shprint(line);
#endif

  if (state->done) return (lfree(line),EOS);
  
  if (line==EOS) {

    // TODO: only runs during trace" ???

    char report[128]= {0};
    printf("HERE!\n");
    state->avg    = state->sum / state->n;
    state->var    = (state->sqsum-((state->sum*state->sum)/state->n))/state->n;

// TODO:
//    state->stddev = sqrt(var);

    // TODO: median, histogram?
    sprintf(report, "count:\t%u\nmin:\t%d\nmax:\t%d\nsum:\t%d\nsqsum:\t%d\n%avg:\t%d\n%median:\t%d\nvar:\t%d\nstddev:\t%d",
      state->n, state->min, state->max, state->sum, state->sqsum, state->avg, state->median, state->var, state->stddev);
    state->done= 1;
    return strdup(report);
  }
  
  if (line < EVENTS) return line;
  
  // Process one piece of data
  { 
    int v= atoi(line);
    ++state->n;
    state->sum+= v;
    state->sqsum+= v*v;
    if (v < state->min) state->min= v;
    if (v > state->max) state->max= v;
    
    //printf("STATS: %u %d\n", state->n, v);
    
    // backgtrack to suck up  more
    lfree(line);
    return NULL;
  }
}
 

// Sort sample_buffer and grab the middle index for the median approximation!


#endif // SHELL_STATS



///////////////////////////////////////////////////
// Control structure words:
//
//    MATCH $foo $bar 
//    ONCE
//    REPEAT 7
//    EMPTY cond
//    NONE cond
//    ALL cond
//    AGGREGATE

/*

I've been considering an "if" or using "&&" and "||", but then one
might need support () or at least { .. } hmmm. Lot's of hubris.

Potentially, if it's just oneliners the match/once/repeat 7/empty
operations may be enough.

*/
 
///////////////////////////////////////////////////

#ifdef INCLUDE_PS

/// ~/GIT/OrWin-ATMOS $ ps -aux
// USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
// u0_a230   5252  0.5  2.8 18907508 223924 ?     S<l   1970 172:30 com.termu
// u0_a230   5667  0.0  0.0 10826768 900 pts/0    Ss+   1970   0:01 /data/dat
// u0_a230   6599  0.0  0.0 10909212 4372 pts/1   Ss    1970   0:23 /data/dat
// u0_a230   6670  0.0  0.0 10843152 1684

char* wstate(char* ret) {
  if (ret==WAITKEY) return "KEY";
  //if (SLEEP(ret)) return "SLP"; // TODO:
  return "RUN";
}
 
#ifdef __CC65__
#include <cc65.h> // for udiv32by16r16
#endif
 
void* ps(simplestate* state, char* line) {
  char s, p, ln[60]; // ... shell args...
  long packed_result;
  Window *w;
  unsigned int m;
  
  if (!state) return STALLOC(simplestate, ps);

  if (state->i++ == 0)
    return strdup(
//----------------------------------------
 " PID %C #M  SZ  ST  TIME CMD");
//4203 27 33 437 KEY 27:30 foobar -a"

  w= NULL;
  p= state->i - 1;
  while(p < nwin+1) {
    w= wins + p;
    if (w->status) break;
    w= NULL; ++p;
  }

  state->i= p + 1;
  if (!w) return EOS;
  
#ifdef __CC65__  
  // A single assembly loop calculates both values simultaneously
  packed_result = udiv32by16r16(w->ticks/CLOCKS_PER_SEC, 60);

  // Extract the pieces from the 32-bit packed register
  m = (unsigned int)(packed_result & 0xFFFF);
  s = (unsigned int)(packed_result >> 16);
#else
  m = (unsigned int)(w->ticks/CLOCKS_PER_SEC/60);
  s = w->ticks/CLOCKS_PER_SEC - m*60;
#endif

#ifdef OSCAR64
  // Drops the safety size limit parameter completely for Oscar64
  #define snprintf(buf, size, ...) sprintf(buf, __VA_ARGS__)
#endif

  // WARNING! sizeof not used!
  snprintf(ln, sizeof(ln), "42%02d %2d %2d%4d %.3s%3d:%02d %s %s"
	   , p

	   , w->cpu, w->nalloc // == w->mem,
	   , -1 //w->abytes,
	   , wstate(w->ret)
	   , m, s
	   , wname(p), w->args
	   );

  // enable if disable shprint, lol
  //puts(ln); return 0;
    
  return lstrdup(ln);
  (void)line;
}

#else
 
// Dummy
#define ps dummyfun
   
#endif // INCLUDE_PS

///////////////////////////////////////////////////



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
  lfree(line);
  
  // force backtracking, why different?
  return line==EOS? EOS: NULL;
}

const char* cmdnames[]= {
  "pwd", "grep", "cat", "wc", "ls", "iota", "head", "tail",
  "ps",
  "set", "print",
  "stats",
  "teeterminal", "terminal",
  
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
  ps,
  set, print,
  stats,
  teeterminal, terminal,
  
};

////////////////////////////////////////////////////////////

char* wtrainstep(cmdtrain** train, char* line) {
  cmdfun *fp;
  
  if (!(fp=**train)) return EOS;
  line= (*fp)(fp, line);
  if (line) ++*train; else --*train;

  return line;
}

int wrunsystrain(cmdtrain* train) {
  cmdfun *fp;
  char* line= EOS;
  cmdtrain *origtrain= train;

  ++train; // skip initial 0

#ifndef SHELLTRACE
  // Beatifully simple!
  
#if 1
  do {
    line= wtrainstep(&train, line);
  } while(line!=EOS);
#else
  while((fp=*train)) {
    line= (*fp)(fp, line);
    if (line) ++train; else --train;
  }    
#endif

#else

  // SHELLTRACE
  while((fp=*train)) {
    printf("\t[%d \"%s\" %p]\n", (int)(long)(train-origtrain),
	   !line? "(NULL)": line==EOS? "*EOS*": line, fp);

    line= (*fp)(fp, line);

    printf("\t%p >>>", line); fflush(stdout); shprint(line);

    if (line) ++train; else --train;
  }    

  printf("\t[**SYSTRAIN**: DONE]\n");
#endif // SHELLRTRACE

  
  // TODO: address of last program
  return 0;
  (void)fp;
}


 void shellcleanup(cmdtrain* train, int n, unsigned int cleanbits) {
  // CLEANUP
  do {
    cmdfun *f= train[n];
    if (f) {
      //' TODO: simplify - just send leanup to all!
      if (cleanbits & 1) {
        #ifdef SHELLINFO
	printf("\t[**CLEANUP**: %d %p]\n", n, train[n]);
        #endif
        (*f)(f, CLEANUP);
      }
      // remove that state
      free(f);
    }

    cleanbits>>= 1;
  } while(--n);
  
  free(train);
}

 cmdtrain* wsysparse(char* cmd, char* pi, unsigned int *bitsout) {
  // TODO: check overflow this per command?
  char *line;

  char c, i, *p, **n;
  cmdfun* f;
  void* state;

  void* arr[MAX_TRAIN + 2 + 2]; // 2 head, 2 NULL
  unsigned int cleanbits;
  cmdtrain *train;
  
  if (!cmd) return NULL;

  // make copy so we can chop it up!
  line= strdup(cmd);
  
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

    // = find command fun
    // TODO: move to function
    n= (char**)cmdnames;
    f= (cmdfun*)commands;
    while(*n && *f) {
      //printf("  ?  %s %s\n", line, (char*)*n);
      if (0==strcmp(line, (char*)*n)) goto found;
      ++n; ++f;
    }

    // TODO: how to handle error stderr?
    printf("%%Error.system: not found >%s< (%s)\n  %p %p %d %d\n",
	   line, cmd, *n, *f, !*n, !*f);
    return NULL;

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
    arr[++i]= state= (*f)(NULL, line);

    if (!state) {
      // TODO: ABORT stderr?
      printf("%%Command.init \"%s\" gave NULL!\n", cmd);
      return NULL;
    }

    #ifdef SHELLINFO
    printf("\"%s\" %p %p]\n", line, f, state);
    #endif
  }
  
  putchar('\n');

  // make a copy
  // TODO: pack it in as {CLEANBITS, NULL, ...., NULL} !
  cleanbits= traincleanbits;

  //putchar('\n'); for(int i=0; i<16; ++i) printf("%2d: %p\n", i, arr[i]);

  train= memdup(arr, (i+2)*sizeof(arr[0]));

  //putchar('\n'); for(int i=0; i<16; ++i) printf("%2d: %p\n", i, train[i]);

  *pi= i; *bitsout= traincleanbits;
  return train;
}


int wsystem(char* command) {
  char* cmd= strdup(command); // for dstructive chopping
  char i;
  unsigned int cleanbits= 0;
  cmdtrain* train= wsysparse(cmd, &i, &cleanbits);
  int r;
  
  free(cmd);

  // TODO: what error is appropriate?
  if (!train) return -1;
    
  r= wrunsystrain(train);

  shellcleanup(train, i, cleanbits);
  return r;
}


#define system wsystem



#ifndef MAIN
 
void tsystem(char* cmd) {
  printf("\n\n----(%u) %s\n", _heapmemavail(), cmd);
  system(cmd);
}
  
void gts(char* name) {
  printf("%s: \"%s\"\n", name, vgets(name));
}

void gti(char* name) {
  printf("%s: %d\n", name, vgeti(name));
}

#include "qputs.c"

// 175 bytes cc65 (oscar removes if not called, lol)
void vdump() {
  char i= 0, *name;
  while(++i < MAX_VARS) {
    if (!(name= vars[i].name)) continue;
    printf(";%d:%s=", i, name);
    switch(*name) {
    case '%': printf("%d", vgeti(name)); break;
    case '_':
    case '$': printf("\"%s\"", vgets(name)); break;
    default:  printf("???"); break;
    }
  }
  putchar('\n');
}

// for test
#ifdef __CC65__

#define ISLITERAL
extern void* _heaporg;
//extern void* _heapptr;
//extern void* _heapend;

char isliteral(void* p) {
  return (p < &_heaporg);
}
#endif // cc65


//int main(int argc, char** argv) {
int main() {
  // Test string binding
  {
    char* bar= "fish";
    vbind("_bar", &bar);
    gts("_bar");
    gti("_bar");
    
    bar= "fourtytwo";
    gts("_bar");
    gti("_bar");
    
    bar= "666";
    gts("_bar");
    gti("_bar");

    putchar('\n'); vdump(); putchar('\n');
  }

  // Test int binding
  {
    int foo= 33;
    vbind("%foo", &foo);
    gts("%foo");
    gti("%foo");
    
    foo= 42;
    gts("%foo");
    gti("%foo");
    
    vsets("%foo", "666");
    gts("%foo");
    gti("%foo");

    putchar('\n'); vdump(); putchar('\n');
  }

  // Test int binding
  {
    vsets("$fie", "33");
    gts("$fie");
    gti("$fie");
    
    vseti("$fie", 42);
    gts("$fie");
    gti("$fie");
    
    putchar('\n'); vdump(); putchar('\n');
  }

  vcleanup();

  printf("---- wrunsystrain: MOCK: pwd | terminal\n");
  
  if (0)
  {
    cmdtrain mock[4]= {0};
    mock[1]= pwd(0, 0);
    mock[2]= terminal(0, 0);

    wrunsystrain(mock);
  }
  
  // Error codes? How & semantics

  printf("---- wsystem: pwd | terminal\n");
  tsystem("pwd | terminal");
  tsystem("cat numbers.txt | grep o | terminal");
  tsystem("ls | terminal");
  tsystem("ls *.c | terminal");
  tsystem("ls *.md | terminal");
  tsystem("ls ../OrWin-ATMOS/*~ | terminal");

  tsystem("iota 2 22 | terminal");
  tsystem("iota 10 -10 -2 | terminal");

  tsystem("iota 1 10 | tail +6 | terminal");

  // Crash
// TODO: nothing! fix!
  tsystem("iota 1 10 | tail -3 | terminal");

  //exit(0);

  tsystem("cat numbers.txt | head | terminal");
  tsystem("cat numbers.txt | head -3 | terminal");  

  // TODO: gives nothing! LOL
  tsystem("set $foo fish | print $foo FISH $foo | terminal");  

  // NOTE: FISH ^$foo canNOT write FISH^$foo !
  tsystem("iota 1 3 | set $foo fish | print $foo $* ^FISH ^$foo Iota: ^$++ None: ^$++| terminal");  

  tsystem("iota 1 100 | stats | print max= $max sum= $sum $@ | terminal");
  
  //wsystem("ls | head -3 | terminal");

  
  return 0;
}

#endif // MAIN
