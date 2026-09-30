// TDOO:
//
// magically, it gained 200 bytes from:
//   git checkout 427179348ead7ca5cb56f077fef03c1d0b1e14e7

// OrWIN Shell pipeline execute
// 
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)

// Implements unix style shell processing with "pipes".
// This implements it extemly efficiently but still keeping
// it superficially equivalent.
//
// This is achieved by these specifics:
// - no real processes, just "task" with a "run"-method
// - no buffers: only a single line "passed around"
// - run: input a line from previous process; output a new line to next
// - backtracking: grep: if no match return NULL
//
// Extentions:
// - generic backtracking, more like database generators
// - event-driven, can wait on KEYEVENTS, WAITMS (:TODO) etc..
// - EOS marker, other "out-of-band" events: CLEANUP
// - named typed variables (%int _conststr $str)
// - results can be structured/named

// Implementation:
// 
// A TRAIN of commands delimited by | each are initialized
// to each own state-structure. A train is a list of pointers
// wrapped in a leading NULL and ending NULL. If go to either 
// end, we're done!
//
// A result from like "grep" is passed to the next process in
// the chain. If no result (==NULL) the locomotive backtracks.
//
// This simply implements a SINGLE LINE buffered shell pipeline
// runner:
//
//   line= EOS;
//   while(*loco) {
//      line=(**loco)(*loco, line);
//      if (line) loco++; else loco--;
//   }
//
// For an interactive shell you'd wrap it like this:
//
//   editline | sh -C $line | terminal
//
// This may be done by there terminal user app.
//
// For more info see SHELL.md



// TODO:
// - uniq
// - sort (-u)
// - freq or huniq
// - bc
// - datamash qsv vsv from

// TODO: behaves differently,, like never ends for set/print?
//#define SHELLTRACE

// prints internal line tokens in plain text
//#define SHELLINFO
//#define VARINFO
//#define SHELLTEST

#define MAX_TRAIN 16

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>


// takes 4K makes OrWIN negative heap! lol

#ifndef MAIN
// We're testing
#define ENVVARS

#include "misc.c"

#endif



#ifdef __CC65__
  typedef int intptr_t; // LOL
#endif

#ifdef OSCAR64
  typedef int intptr_t;
#endif

//#include "malloc-trace.c"
#ifndef yreport
  #define yreport() (void)0
#endif

// TODO: make it a printable string?
#ifndef EVENTS

  // TODO: should come from shared orwin.c?
  #define WAITKEY ((char*)0x300)
  #define CLEANUP ((char*)0x4fe)
  #define EVENTS  ((char*)0x500)
  #define EOS     ((char*)0x500)

#endif


#ifndef HEAPMEM
   //extern size_t _heapmemavail(void);
#else
  size_t _heapmemavail() {
    return 4711;
  }

  char isliteral(void* line) {
    return 0; // lol not going to work well...
  }

#endif

typedef void* (*cmdfun)(void* state, char* line);

char* dummyfun(void* state, char* line) {
  return NULL;
  (void)state; (void)line;
}
 
typedef cmdfun* cmdtrain;

unsigned int traincleanbits;

#define REQUEST_CLEANUP() (traincleanbits|=1)


#ifdef ENVVAR
  #define BOUNDSTART char* bound;
#else
  #define BOUNDSTART 
#endif


typedef struct simplestate {
// TODO: make stateheader already!!!
  cmdfun f;
  BOUNDSTART

  int i;
} simplestate;

void* stalloc(unsigned int size, void* f) {
  simplestate* state= calloc(size, 1);
  state->f= f;
  return state;
}

#define STALLOC(strct, fun) stalloc(sizeof(strct), fun)

#define SIMPLEALLOC(fun) STALLOC(simplestate,fun)

typedef struct pstate {
  cmdfun f;
  BOUNDSTART

  char* s;
} pstate;

#define PSTALLOC(fun, p) (state=STALLOC(pstate, fun), state->s=strdup(p), state)

void* memdup(void* p, unsigned int bytes) {

// TODO: wtf? (2 bytes fail!)

//  char* r= malloc(bytes+2);
  char* r= malloc(bytes);
#ifdef __CC65__
//  printf("BYTES=%5d\t%p\tAVAIL=%u\n", bytes, r, _heapmemavail());
#endif
  // TODO: give better error message! and don't drop out!
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
#ifndef lfree
char* lfree(char* line) {
  char len;

  if (line <= EVENTS) return 0;
  if (line == "") return 0;
  if (isliteral(line)) return 0;

  //putchar('.');

// testing:
//  return 0;
//  free(line);  return 0;
  
  // TODO: get actual size of allocation!
  len= strlen(line);
  if (!reuse || len > relen) {
    // it's bigger
    free(reuse);
    reuse= line; relen= len;
  }
  return 0;
}
#endif

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
  

// TODO: move to supporting functions shared w app_
 
///////////////////////////////////////////////////
// line space delimited parameter choppers
// If none: returns the DeFauLT value!
//
// NOTE: they modify the incoming line
// NOTE: if you need to keep the string do strdup!

// 123 : nextStr, NATIVE_CODE:code
char* nextStr(char** line, const char* dflt) {
  char *r;
  if (!line || !*line) return (char*)dflt;
  zptr= *line;
  skipspc();
  // r points to first non whitespace (or at end)
  r= zptr;
  // skip till end of "word"
  while(*zptr && !isspace(*zptr)) ++zptr;
  // truncate string (we either on 0 or whitespace)
  if (*zptr) *zptr++= 0;
  // move input pointer to rest
  *line= zptr;
  return *r? r: (char*)dflt;
}

// 68 : nextInt, NATIVE_CODE:code
int nextInt(char** line, int dflt) {
  char *r= nextStr(line, NULL);
  return (r && (isdigit(*r) || *r=='-'))
    ? atoi(r): dflt;
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

///////////////////////////////////////////////////
// Variable manipulators
 
// cc65: (- 35460 33462) = 1998 bytes = frickin hell!
// osc : (- 23802 22061) = 1741 // was 2007 if not used doesn/t count!
//
// cc65: 4065 free only... 7370 bytes with NO ENV...
//
// cc65: WITH ENV and BINDINGS: 880B free!!! LOL 4KB stuff
//
// TODO: idea, manage vbind on higher level - slots!
  
// LOC: 85 lines (/ 1733 85) ~ 20 bytes/line

 
// oscar64: 106 vnth,   50 vbind
//          126 vgeti, 202 vgets
//          204 vseti, 173 vsets
//           30 vevals
//


// global train start pointer must be set before calling wtrainstep
 
cmdtrain* trainptr;
  
#ifndef ENVVARS
 
// dummies when not included
#define set     dummyfun
#define print   dummyfun 
#define varlist dummyfun

#define VARSTATE NULL
#define SETSTATE NULL
#define PRINTSTATE NULL
#define VARLISTSTATE NULL

#define vcleanup() (void)0
 
#else
 
// TODO: remove
#ifdef VARINFO
char* taskname(void* fun);
#endif
 
// TODO: remove
// Returns a pointer to 
//static char vptrtype;
 
char** vptr(char* name) {
  cmdtrain* loco= trainptr+1;
  char ** state, *p, c, n, *pn;
  
  int i= 1;

  while((state= (char**)*loco)) {
#ifdef VARINFO
    printf("%d: loco:%04x @%04x %-8s : %s\n",
             i,      loco,state,
      taskname(state[0]),  state[1]);
#endif
    p= state[1]; // TODO: use header.bound
    if (p) {
      n= 0;
      while((c= *p++)) {
        if (c > '@' || isdigit(c)) continue; // TODO: or just isalnum?

        // we have % | _ | $ | @ (or any non apha)
        //vptrtype= c;
        ++n;

        // check if string match
        printf("compare: %s '%c' %s\n", name, c, p);
        pn= name;
// TODO: ouit of sync because of %% ?
        while(c == *pn++ && isalnum(c= *p)) ++p;

        // match if at end of both
        printf("  HERE: *pn=%d '%c' c=%d '%c' \n", *pn, *pn, c, c);
        if (*pn || isalnum(c)) continue;
        // found!
        printf("FOUND: %d %d %04x == #%d\n", i, n, state+n, state[n]);
        return state+n;
      }
    }
    ++loco; ++i;
  }
  putchar('\n');

  return NULL;
}

void vcleanup() {
  //assert(!"foobar");
  // TODO: walk the train
}

char* vgets(char* name);
 
// 126 : vgeti, NATIVE_CODE:code
int vgeti(char* name) {
  char** p= vptr(name);
  return !p? 0:
    (*name == '%')? *(int*)p:
    // TODO: lookup twice... optimize
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
  char** p= vptr(name), *s= 
    !p? "":
    (*name == '_')? *p:
    (*name == '$')? *p:
    (*name == '%')? (sprintf(tmp10char, "%d", vgeti(name)),tmp10char):
    name; // lol
  return s? s: "";
}
 
char* vsets(char* name, char* val);
 
// 204 : vseti, NATIVE_CODE:code
int vseti(char* name, int val) {
  if (*name == '$') {
    sprintf(tmp10char, "%d", val);
    vsets(name, strdup(tmp10char));
    return val;
  } else if (*name == '_') return 0;
  else {
    int* p= *(int**)vptr(name);
    return (*p= val);
  }
}

// gives ownership to $VAR of VAL string

// 173 : vsets, NATIVE_CODE:code
char* vsets(char* name, char* val) {
  if (*name == '%') { vseti(name, atoi(val)); return val; }
  if (*name != '$') return "";
  else {
    char** p= vptr(name);
    lfree(*p);
    return (*p= val);
  }
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


#define VARSTATE "$name%expr"
#define SETSTATE "$name%expr"

typedef struct varstate {
  cmdfun fun;
  // Bound
  // BOUNDSTART:
  // (notice how name slots into the bound position!)
  char*  name;   // TODO: make it store (char*)(char)idx
  char*  expr;    // Owned if _VAR
} varstate;


// TODO: only works for $var if not exist, not %var!!!

// "SET - S(L)exial EnvironmenT binding"
//(397 : set, NATIVE_CODE:code)
// 474 : set, NATIVE_CODE:code
char* set(varstate* state, char* line) {
  if (!state) {
    char *name;
    state= STALLOC(varstate, set);

    // variable name to set to expr
    // TODO: shouldn't need this strdup and name?
    name= state->name= strdup(nextStr(&line, (char*)""));

    // TODO: only works for $var if not exist, not %var!!!
    assert(*name == '$');

    state->expr= strdup(nextStr(&line, ""));
    return (char*)state;

  } else if (line==CLEANUP) {
    LFREE(state->name);
    LFREE(state->expr);
    return line;
  }

  if (line<=EVENTS) return line;
  else {
    char* origline= line;
    char* endline = line+strlen(line);
    //printf("SET:"); shprint(line);
    //printf("xxx: %s %s\n", state->name, state->expr);

    vsetsfrom(state->name, vevals(state->expr, &line));

    if (line==origline) return line;
    else if (line <= endline) {
      // pass on what's left
      endline= strdup(line); lfree(line); return endline;
    } else return ""; // something, lol
  }
}

// printer
#define PRINTSTATE NULL
 
typedef struct printstate {
  cmdfun fun;
  BOUNDSTART

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
  
  // For every data return, print a line fill in params
  // TODO: move  to sarrevals()
  {
    char tmp[128]= {0}; // TODO: use dstr!
    char** p= state->params;
    char* ln= line;
    char* x;
    char n= 255;

    // TODO: vevalarrs(p, *line)
    while(*p) {
      //printf("\t%p : %s => %s\n", p, *p, vgets(*p));
      x= *p;
      if (++n) { if (*x!='^') strcat(tmp, " "); else ++x; }
      strcat(tmp, vevals(x, &line));
      ++p;
    }
    lfree(ln); // used up!

    // TODO: "current" LINE should be set by system?
    return strdup(tmp);
  }
}

#define VARLISTSTATE "%%vindex_name_vstr%vint"
 
typedef struct varliststate {
  cmdfun fun;
  // Bound:
  BOUNDSTART
  
  int i;
  char* name;
  char* vstr;
  int   vint;
} varliststate;

char* varlist(varliststate* state, char* line) {
  if (!state) {
    state= STALLOC(varliststate, varlist);
    return (char*)state;
  }

// TODO: implement
  assert(!"not implmeneted");
  
  if (line && line<EVENTS) return line;

  // if done: reset and request next
//  if (state->i >= nvar) { lfree(line); state->i= 0; return NULL; }
  // if first: return the result
  if (!state->i++) return line;

  // and then every variable for that line
//  state->name= vars[state->i - 1].name;
  // slow, lol
  state->vint= vgeti(state->name);
  state->vstr= vgets(state->name);

  lfree(line);
  {
    // LOL
    char* ln= malloc(1+1+strlen(state->name)+2+5+2+strlen(state->vstr));
    sprintf(ln, "\t%s\t=%6d  \"%s\"", state->name, state->vint, state->vstr);
    return ln;
  }
}  

#endif // ENVVARS



///////////////////////////////////////////////////////////
// unix "commands"

#define PWDSTATE NULL
 
void* pwd(simplestate* state, char* line) {
  if (!state) return SIMPLEALLOC(pwd);
  if (!line)  return EOS;

  // generate a value on EOS (or any), lol
//  lfree(line);
//  return lstrdup("/home/orwin"); - hang
//  return "/home/orwin"; = hang
  return strdup("/home/orwin");
}


// TODO: maybe make it count line numbers? matches etc
#define GREPSTATE NULL

void* grep(pstate* state, char* line) {
  if (!state) return PSTALLOC(grep, line);

  // pass-through backtracking
  if (!line || line==EOS) return line;

//  printf("  GREP: %s\n", line);
  return strstr(line, state->s)? line: lfree(line);
}

#ifdef FAKE
// fake file
char* fakefile[]= { "one", "two", "three", "four", "five", NULL };

#define CATSTATE NULL
 
typedef struct fakefilestate {
  cmdfun f;
  BOUNDSTART

  char** fil;
} fakefilestate;

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

#define CATSTATE NULL

typedef struct filestate {
  cmdfun f;
  BOUNDSTART

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
  

#define WCSTATE "%lines%words%bytes"
 
typedef struct wcstate {
  cmdfun f;
  BOUNDSTART

  unsigned int ln, wn, cn;
} wcstate;

void* wc(wcstate* state, char* line) {
  char c;
  unsigned int n= 0;
  
  if (!state) {
    state= STALLOC(wcstate, wc);
    return state;
  }

  // only generates one value
  if (!line) return EOS;

  // no other events (except EOS)
  if (line<EVENTS) return line;
  
  // EOS: Output summary at end
  if (line==EOS) {
    line= malloc(25);
    sprintf(line, "%u %u %u", state->ln, state->wn, state->cn);
    return line;
    // TODO: do we need to put code to give EOF?
  }

  // process one line
  zptr= line;
  state->ln++;
  state->cn+= strlen(zptr);
  while((c= *zptr)) {
    skipspc();
    state->wn++;
    --zptr;
    while((c= *++zptr) && !isspace(c));
  }

  // returns null (backtracks to get next line)
  lfree(line);
  return NULL;
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
#define LSSTATE NULL

#else // !ATMOS && !CC65
 

#ifdef __CC65__
// TODO: no have on atmos... (maybe works on C64)
//   doesn't have on sim65 :-(
#include <dirent.h>

#define LSSTATE "%_name%size"
 
typedef struct lsstate {
  cmdfun f;
  // BOUNDSTART:
  char* name; // this takes the place of bound!
  unsigned int size;
  
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
      // all good
      // TODO: size? more attributes? timestamp"
      //vbind("_name", &state->name);
      return state;
    }
    // fail
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
  return lstrdup(state->name= state->entry.name);
}

#else // UNIX
 
// ============================================================================
// POSIX / Linux / Termux Target Implementation
// ============================================================================
#include <sys/types.h>
#include <dirent.h>
 
#define LSSTATE NULL

typedef struct lsstate {
  cmdfun f;
  BOUNDSTART

  DIR* dir;
  char* pat;

  char* name;
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
    // TODO: size? more attributes? timestamp"
    if (state->dir) return state;
    // fail
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
  return lstrdup(state->name= de->d_name);
}

#endif // CC65 ... UNIX

#endif // __ATMOS__
 



///////////////////////////////////////////////////

#define IOTASTATE "%%n%e%d"
 
typedef struct countstate {
  cmdfun f;
  BOUNDSTART

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
    // we need to compensate for first
    state->n-= state->d;

    return state;
  }

  lfree(line);
  state->n+= state->d;
  if ((state->d > 0 && state->n <= state->e) ||
      (state->d < 0 && state->n >= state->e)) {
    char s[10];
    // TODO: use returned length:
    sprintf(s, "%d", state->n);
    return lstrdup(s);
  }

  return EOS;
}
        
#define HEADSTATE NULL
 
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

#define TAILSTATE NULL

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


#define SHELL_STATS_COMMAND
#ifdef SHELL_STATS_COMMAND

// Must be 2^n for bit logic to work
//#define SAMPLES 16
#define SAMPLES 32
 
// Inside your stats state handler structure
    // truncated
#ifdef LITTLE_ENDIAN
  #define STATSSTATE "%%n%min%max%sumlo%sumhi%sqsumlo%sqsumhi%avg%median%var%stddev"
#else
  #define STATSSTATE "%%n%min%max%sumhi%sumlo%sqsumhi%sqsumlo%avg%median%var%stddev"
#endif

typedef struct {
  cmdfun fun;
  BOUNDSTART

  // TODO: float? oscar64 can do it

  int n;
  int min;
  int max;

  // TODO: use isum as "truncated int"
  long sum, sqsum;

  // results
  int avg;        // TODO: use 100x to get 2 decimals?
  int median;
  int var;
  int stddev;

  int samples[SAMPLES];
  int mask;
  char done;
} StatsState;

int cmpint(const void *a, const void *b) {
  return *(const int*)a < *(const int *)b? -1:
    *(const int*)a == *(const int *)b? 0: +1;
}

#define LITTLE_ENDIAN (1 == *(unsigned char *)(&(const int){1}))

char* stats(StatsState* state, char* line) {
  if (!state) {
    state= STALLOC(StatsState, stats);
    state->min= 0x7fff;
    state->max= 0x8000;

    return (char*)state;

  }
  
  // End Of Stream => report
  if (state->done) return (lfree(line),EOS);
  if (line==EOS) {
    char report[128]= {0};
    state->avg    = state->sum / state->n;
    state->var    = (state->sqsum-((state->sum*state->sum)/state->n))/state->n;

    // TODO: (no have sqrtr!)
    //    state->stddev = sqrt(var);

    // median histogram
    qsort(state->samples, SAMPLES, sizeof(int), cmpint);

    #if 0
    // print samples for debugging
    {
      int i;
      printf("SAMPLES: ");
      for(i=0; i<SAMPLES; ++i) printf("%d ", state->samples[i]);
      putchar('\n');
    }
    #endif
        
    state->median= state->samples[SAMPLES/2-1]; // "middle'
    
    sprintf(report, "count:\t%u\nmin:\t%d\nmax:\t%d\nsum:\t%ld\nsqsum:\t%ld\navg:\t%d\nmedian:\t%d\nvar:\t%d\nstddev:\t%d",
      state->n, state->min, state->max, state->sum, state->sqsum, state->avg, state->median, state->var, state->stddev);
    state->done= 1;
    return strdup(report);
  }
  
  if (line < EVENTS) return line;
  
  // Process one piece of data
  { 
    int v= atoi(line);
    lfree(line);

    state->sum+= v;
    state->sqsum+= ((long)v) * v; // increase precision
    if (v < state->min) state->min= v;
    if (v > state->max) state->max= v;
    
    // with lower probability: insert at random position for median!
    // Scale down probability precisely: 1 / (n + 1)
    if ((state->mask|= state->n) < SAMPLES)
      state->samples[state->n]= v;
    else if ((rand() & (state->mask/2)) < SAMPLES) 
      //{ printf("REPLACE %d\n", v);
      state->samples[rand()&(SAMPLES-1)]= v;
      //}
    
    ++state->n;

    // backtrack to suck up more
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
 
#define PSSTATE "%%pid%cpu%mem%size%ticks%mins%secs_name_args"
 
typedef struct psstate {
  cmdfun f;
  BOUNDSTART
  
  int pid; // "window id"
  int cpu;
  int mem;
  int size;
  int ticks;
  int mins;
  int secs;
  char* name;
  char* args;

  // internal
  int i;
} psstate;
 
void* ps(psstate* state, char* line) {
  char s, p, ln[60]; // ... shell args...
  long packed_result;
  Window *w;
  unsigned int m;
  
  if (!state) {
    state= STALLOC(psstate, ps);
    return state;
  }

  // return header before data line
  if (state->i++ == 0)
    return strdup(
//----------------------------------------
// " PID %C #M  SZ  ST  TIME CMD");
" PID #M STT TIME CMD");
//4203 27 33 437 KEY 27:30 foobar -a"

  // return data lines

  // - walk ot next live window/process structure
  w= NULL;
  p= state->i - 1;
  while(p < nwin+1) {
    w= wins + p;
    if (w->status) break;
    w= NULL; ++p;
  }
  state->i= p + 1;

  // done?
  if (!w) return EOS;
  
#ifdef __CC65__
  // TODO: does this actually save time/code?
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

  state->pid   = 0x4200 | p;
  state->cpu   = w->cpu;
  state->mem   = w->nalloc;
  state->size  = -1; // w->abytes
  state->mins  = m;
  state->secs  = s;
  state->name  = wname(p);
  state->args  = w->args;
  
  // WARNING! sizeof not used!
//snprintf(ln, sizeof(ln), "42%02d %2d %2d%4d %.3s%2d:%02d %s %s"
  snprintf(ln, sizeof(ln), "42%02d %2d %.3s%2d:%02d %s %s"
    , p

//  , w->cpu
    , w->nalloc // == w->mem,
//  , -1 //w->abytes,
    , wstate(w->ret)
    , m, s
    , state->name, w->args
  );

  // enable if disable shprint, lol
  //puts(ln); return 0;
    
  lfree(line);
  return lstrdup(ln);
}

#else
 
// Dummy
#define PSSTATE NULL
#define ps dummyfun
   
#endif // INCLUDE_PS

///////////////////////////////////////////////////
// terminal IO editing

// TODO: make dynamic?
 
#define MAX_EDIT 80
 
#define EDITLINESTATE "%%line%pos"
 
typedef struct editlinestate {
  cmdfun fun;
  BOUNDSTART
  
  char* s;
  char i;
} editlinestate;
  
#ifndef WAITKEY
  #define WAITKEY 0
#endif
 
#ifndef KEYEVENT(e)
  // hack
  #define KEYEVENT(e) 1
#endif

void* editline(editlinestate* state, char* line) {
  if (!state) {
    return STALLOC(editlinestate, editline);
  } //else if (!KEYEVENT(line)) return WAITKEY;
  else {
    // generealize... dstr?
    char c, *s= state->s, len= s? strlen(s): 0;
    state->s= s= realloc(state->s, (len | 15) + 17); // hmmm
    if (state->i >= MAX_EDIT) return WAITKEY;
    s[state->i]= 0;
    
    lfree(line);
    
    // TODO: wraps if too long
    printf("\r> %s", s);
    //c= cursorgetc();
    c= getchar();

    // Key input
    if (c==27 || c&0x80 || c=='C'-'@') {
      // ESC RET FUNC- CTRL-C (BREAK)
      lfree(s);
      state->s= NULL;
      state->i= 0;
      return NULL;
    } else if (c==10 || c==13 || c=='D'-'@') {
      // RETURN CTRL-D
      char *r= s;
      state->s= NULL;
      state->i= 0;
      putchar('\n');
      return r;
      // TODO: ^P get previous line (save it!)
    } else if (c=='U'-'@') {
      // clear line CTRL-U
      printf("\\\n");
      s[state->i= 0]= 0;
    } else if (c==127 || c==8) {
      // backspace
      printf("\b \b");
    } else {
      // insert char
      s[state->i++]= c;
      s[state->i]= 0;

      putchar(c);
    }

    return WAITKEY;
  }
}
    

#define TEETERMINALSTATE NULL
 
// more like "tee -"
void* teeterminal(simplestate* state, char* line) {
  if (!state) return STALLOC(wcstate, wc);

  shprint(line);
  return line;
}

#define TERMINALSTATE NULL

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
  "set", "print", "varlist",
  "stats",
  "teeterminal", "terminal", "editline", // == readline?
  
  // "LS CAT find "
  // "GREP cut tr sed " 
  // "echo " - PRINT
  // "TAIL HEAD diff uniq comm "
  // "WC less sort gzip gunzip unzip "

  // "xargs " - SHELL redundant
  // "history man "

  // "tar paste "
  // "awk "  
  // "PWD date "
  // "clear basname dirname "
  // "PS df top htop kill free whoami uptime uname killall "
  // "cd rm cp mv mkdir chmod chown touch ln rmdir chgrp "
  // "curl wget rsync scp "
  // "ping ip ss netstat "
  // "git "

  NULL
};

void* commands[]= {
  pwd, grep, cat, wc, ls, iota, head, tail,
  ps,
  set, print, varlist,
  stats,
  teeterminal, terminal, editline,
};

#ifdef ENVVAR
char* varnames[]={
  PWDSTATE, GREPSTATE, CATSTATE, WCSTATE, LSSTATE, IOTASTATE, HEADSTATE, TAILSTATE,
  PSSTATE,
  SETSTATE, PRINTSTATE, VARLISTSTATE,
  STATSSTATE,
  TEETERMINALSTATE, TERMINALSTATE, EDITLINESTATE,
};
#endif
 
////////////////////////////////////////////////////////////

#ifdef SHELLINFO
char* taskname(void* fun) {
  char**  n= (char**)cmdnames;
  cmdfun* f= (cmdfun*)commands;
  while(*n && *f) {
    if (*f==fun) return *n;
    ++n; ++f;
  }
  return "-N/A-";
}  
#endif

// Moves a LOCOMOTIVE one step feeding it LINE
// Ends on start/end of train (NULL)
// Returns: next LINE or NULL for back
char* wtrainstep(cmdtrain** loco, char* line) {
  cmdfun *fp;
  
#ifdef SHELLINFO
  printf(">>> %s\n", taskname(*fp));
#endif

//  if (!(fp=**train)) return line; // not rigth? was EOS = not right!
  if (!(fp=**loco)) return EOS;
  line= (*fp)(fp, line);
  if (line) ++*loco; else --*loco;

#ifdef SHELLINFO
  printf("> %s => ", taskname(*fp));
  shprint(line); 
#endif  

  return line;
}

int wrunsystrain(cmdtrain* train) {
  cmdfun *fp;
  char* line= EOS;
  cmdtrain *origtrain= train;

  // used to find variables!
  trainptr= train;
  
  ++train; // skip initial 0

#ifndef SHELLTRACE
  // Beatifully simple!
  
#if 1

  do {
    line= wtrainstep(&train, line);
  } while(*train);

#else
  // inline
  
  while((fp=*train)) {
    line= (*fp)(fp, line);
    if (line) ++train; else --train;
  }    

#endif

#else

  // SHELLTRACE
  while((fp=*train)) {
    printf("\t  [%d \"%s\" %p]\n", (int)(long)(train-origtrain),
	   !line? "(NULL)": line==EOS? "*EOS*": line, fp);

    line= (*fp)(fp, line);

#ifdef VARINFO
    printf("\t%s %p:", taskname(*fp), line); fflush(stdout); shprint(line);
#endif

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
  char i, l, **n;
  cmdfun* f;
  void** state; // treat like slots!

  char *name, *args, *dofree;

  void* arr[MAX_TRAIN + 2 + 2]; // 2 head, 2 NULL
  unsigned int cleanbits;
  cmdtrain *train;
  
  if (!cmd) return NULL;

  // make copy so we can chop it up!
  zptr= dofree= strdup(cmd);
  
  #ifdef SHELLTEST
  printf("\n------ wsystem: \"%s\"\n", cmd);
  #endif
  
  // a train {NULL, ..., NUL} ! - simplifies logic!
  memset(arr, 0, sizeof(arr));

  traincleanbits= 0;
  i= 0;
  
  while(zptr && *zptr) {
    // extract command name
    skipspc();
    name= zptr;
    skipword();
    *zptr++= 0;
    //printf("name>%s<\n", name);

    // extract arguments
    skipspc();
    args= zptr;
    skiptill('|');
    // TODO: overwrite?
    if (!*zptr) zptr= 0; else *zptr++= 0;
    // trim args
    while((l= strlen(args)) && args[l-1]==' ') args[l-1]= 0;

//printf("PARSEtillBAR: >%s< ARGS=>%s<\n", zptr?zptr:"(nUll)", args);

    //printf("args>%s<\n\nn", args);

    // find command fun
    n= (char**) cmdnames;
    f= (cmdfun*)commands;
    while(*n && *f) {
      //printf("  ?  %s %s\n", line, (char*)*n);
      if (0==strcmp(name, (char*)*n)) goto found;
      ++n; ++f;
    }

    // NOT found!

    // TODO: how to handle error stderr?
    printf("%%Shell: NO >%s< (%s)\n  %p %p %d %d\n",
      name, cmd, *n, *f, !*n, !*f);
    free(dofree);
    return NULL;

  found:
    #ifdef SHELLINFO
    printf("\t[%s: ", name);
    #endif

    // TODO: trim func? only here
    // remove trailing spaces
    //while(isspace(zptr[-1]) && p>zptr) --p;
    //*p= 0;
    
    // TODO: all get's CLENAUP request?
    traincleanbits<<= 1;

    // This calls the INIT for the command!
    { // save zptr as it may be used in parsing inside init!
      char * zsave= zptr;

      // TODO: let's allocate too!
      arr[++i]= state= (*f)(NULL, args);
      zptr= zsave;
    }

    if (state) {
      // TODO: remove from "init"
      state[0]= *f;

      #ifdef ENVVAR
      state[1]= varnames[n-cmdnames];
      #endif      

    } else {

      // TODO: ABORT stderr?
      printf("%%command.init \"%s %s\" gave NULL!\n", *n, args);
      free(dofree);
      return NULL;
    }

    #ifdef SHELLINFO
    printf("\"%s\" %p %p]\n", cmd, f, state);
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

  free(dofree);
  return train;
}


int wsystem(char* command) { vcleanup(); {
  char* cmd= strdup(command); // for dstructive chopping
  char i;
  unsigned int cleanbits= 0;
  cmdtrain* train= wsysparse(cmd, &i, &cleanbits);
  int r;

  // already used by parser
  free(cmd);

  // TODO: what error is appropriate?
  if (!train) return -1;
    
  r= wrunsystrain(train);

  shellcleanup(train, i, cleanbits);
  return r;
} }


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
  assert(!"to implement");
#if 0
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
#endif
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
  //tsystem("editline | print foo $* bar | terminal"); exit(3);

  //system("iota 1 3 | print %n | terminal"); exit(0);
  //system("iota 1 1000|grep 7|terminal"); exit(0);
//  system("iota 1 17|grep 7|terminal"); exit(0);
  system("iota 1 17|grep 7 |terminal"); exit(0);

#if 0  
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
#endif

  printf("---- wrunsystrain: MOCK: pwd | terminal\n");
  
#if 0
  {
    cmdtrain mock[4]= {0};
    mock[1]= pwd(0, 0);
    mock[2]= terminal(0, 0);

    wrunsystrain(mock);
  }
#endif
  
  // test malloc leaks
#if 0
  tsystem("iota 1 1000 | terminal");
  tsystem("iota 1 1000 | terminal");
  tsystem("iota 1 1000 | terminal");
  tsystem("iota 1 1    | terminal");

  yreport();
exit(0);
#endif
  


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

  tsystem("iota 1 1000 | stats | print max= %max sum= %sum $* | varlist | print %vindex _vname %vint _vstr | terminal");
//  tsystem("iota 1 1000 | stats | print max= %max sum= %sum $* | varlist | terminal");

  tsystem("iota 1 1000 | stats | terminal");
  tsystem("iota 1 1000 | stats | varlist | terminal");
  
  tsystem("iota 1 3 | set $i $*| varlist | terminal");

  //tsystem("iota 1 1000 | wc | terminal");
  tsystem("iota 1 1000 | terminal");

  tsystem("iota 1 17 | terminal");

  
  // crash
  // - not work for %i at least not to define
  tsystem("iota 1 3 | set %i $*| varlist | terminal");

  //wsystem("ls | head -3 | terminal");

  
  return 0;
}

#endif // MAIN
