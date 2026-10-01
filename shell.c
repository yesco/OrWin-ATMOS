// OrWIN Shell pipeline execute
// 
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

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
// For an interactive shell you'd wrap it something like this:
//
//   editline | sh -C $line | terminal
//
// This may be done by the terminal user app.
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


// 20459 Bytes before
// 20294 Bytes after ... not much diff

// orwin: 36978 byte before       36675 before zline
//        36675 byte after        36389 ....
//
// (- 36978 36675) == 308 bytes saved by zapp!
// (- 36675 36389) == 286 bytes saved by zline!
//
// these are probably equal to what oscar64 does, LOL

#ifdef __CC65__
#pragma bss-name (push, "ZEROPAGE")
char* zapp;
char* zline;
#pragma bss-name (pop)
#endif // __CC65__

#ifdef OSCAR
__zeropage extern char* zapp;
__zeropage extern char* zline;
#endif

#define voidapp zapp

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

typedef void* (*cmdfun)();

char* dummyfun() {
  return NULL;
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

#define PSTALLOC(fun, p) (app=STALLOC(pstate, fun), app->s=strdup(p), app)

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

// TODO: also implicit"

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
#define APP varstate
char* set() {
  if (!zapp) {
    char *name;
    zapp= STALLOC(varstate, set);

    // variable name to set to expr
    // TODO: shouldn't need this strdup and name?
    name= app->name= strdup(nextStr(&zline, (char*)""));

    // TODO: only works for $var if not exist, not %var!!!
    assert(*name == '$');

    app->expr= strdup(nextStr(&zline, ""));
// TODO: reconsider
    return (char*)app;

  } else if (zline==CLEANUP) {
    LFREE(app->name);
    LFREE(app->expr);
    return zline;
  }

  if (zline<=EVENTS) return zline;
  else {
    char* origline= zline;
    char* endline = zline+strlen(zline);
    //printf("SET:"); shprint(line);
    //printf("xxx: %s %s\n", app->name, app->expr);

    vsetsfrom(app->name, vevals(app->expr, &zline));

    if (zline==origline) return line;
    else if (zline <= endline) {
      // pass on what's left
      endline= strdup(zline); lfree(zline); return endline;
    } else return ""; // something, lol
  }
}
#undef APP

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
#define APP printstate
char* print() {
  if (!app) {
    char np= 0, *param[16]= {0}, *p, *endline= zline+strlen(zline);
    app= STALLOC(varstate, print);
    if (!app) return NULL;
    do {
      p= param[np++]= strdup(nextStr(&zline, NULL));
      //printf("\tprint %u %s\n", np, p);

// TODO: give "error" at 16
// TODO: nextStr doesn't know how to terminate!

    //} while(p!=NULL && zline < endline);
    } while(zline < endline);

    app->params= memdup(param, (np+1)*sizeof(char*));
    if (!app->params) { free(app); return NULL; }
    return (char*)app;

  } else if (zline==CLEANUP) {
    char** p= app->params;
    while(*p) LFREE(*p++);

    LFREE(app->params);
    return zline;
  }
  
  if (zline<=EVENTS) return zline;
  
  // For every data return, print a line fill in params
  // TODO: move  to sarrevals()
  {
    char tmp[128]= {0}; // TODO: use dstr!
    char** p= app->params;
    char* ln= zline;
    char* x;
    char n= 255;

    // TODO: vevalarrs(p, *zline)
    while(*p) {
      //printf("\t%p : %s => %s\n", p, *p, vgets(*p));
      x= *p;
      if (++n) { if (*x!='^') strcat(tmp, " "); else ++x; }
      strcat(tmp, vevals(x, &zline));
      ++p;
    }
    lfree(ln); // used up!

    // TODO: "current" LINE should be set by system?
    return strdup(tmp);
  }
}
#undef APP
 
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

#define APP varliststate
char* varlist() {
  if (!app) {
    app= STALLOC(varliststate, varlist);
    return (char*)app;
  }

// TODO: implement
  assert(!"not implmeneted");
  
  if (zline && zline<EVENTS) return zline;

  // if done: reset and request next
//  if (state->i >= nvar) { lfree(zline); state->i= 0; return NULL; }
  // if first: return the result
  if (!app->i++) return zline;

  // and then every variable for that line
//  state->name= vars[state->i - 1].name;
  // slow, lol
  app->vint= vgeti(app->name);
  app->vstr= vgets(app->name);

  lfree(zline);
  {
    // LOL
    char* ln= malloc(1+1+strlen(app->name)+2+5+2+strlen(app->vstr));
    sprintf(ln, "\t%s\t=%6d  \"%s\"", app->name, app->vint, app->vstr);
    return ln;
  }
}  
#undef APP

#endif // ENVVARS



///////////////////////////////////////////////////////////
// unix "commands"

#define PWDSTATE NULL
 
#define APP simplestate
void* pwd() {
  if (!app) return SIMPLEALLOC(pwd);
  if (!zline) return EOS;

  // generate a value on EOS (or any), lol
//  lfree(line);
//  return lstrdup("/home/orwin"); - hang
//  return "/home/orwin"; = hang
  return strdup("/home/orwin");
}
#undef APP

// TODO: maybe make it count line numbers? matches etc
#define GREPSTATE NULL

#define APP pstate
void* grep() {
  if (!app) return PSTALLOC(grep, zline);

  // pass-through backtracking
  if (!zline || zline==EOS) return zline;

//  printf("  GREP: %s\n", line);
  return strstr(zline, app->s)? zline: lfree(zline);
}
#undef APP
 
#ifdef FAKE
// fake file
char* fakefile[]= { "one", "two", "three", "four", "five", NULL };

#define CATSTATE NULL
 
typedef struct fakefilestate {
  cmdfun f;
  BOUNDSTART

  char** fil;
} fakefilestate;

#define APP fakefilestate
void* cat() {
  if (!app) {
    app= STALLOC(fakefilestate, cat);
    if (!app) return NULL;
    app->fil= fakefile;
    return app;
  }

  lfree(zline);
  return *app->fil? strdup(*appe->fil++): EOS;
}
#undef APP

#else

#define CATSTATE NULL

typedef struct filestate {
  cmdfun f;
  BOUNDSTART

  FILE* fil;
} filestate;

#define APP filestate
void* cat() {
  char* ln= NULL;
  size_t sz= 0;

  if (!app) {

// TODO: haha
    
    return NULL;
    app= STALLOC(filestate, cat);
    if (!app) return NULL;

    REQUEST_CLEANUP();
    if ((app->fil= fopen(zline, "r"))) return app;

    // errors
    free(app);
    return NULL; // TODO: logic?
  } else if (zline==CLEANUP) {
    if (app->fil) fclose(app->fil); app->fil= NULL;
    return NULL;
  }

  lfree(zline);

  // EOF if eof or error?

#ifdef __CC65__



  // TODO: fix: thisis unsafe



  ln= calloc(81,1);
  if (NULL!=fgets(ln, sz, app->fil)) {
#else

  if (EOF==getline(&ln, &sz, app->fil)) {
#endif
    //printf("==eof==\n");
    lfree(ln);
    fclose(app->fil); app->fil= NULL;
    return EOS;
  } else {
    //printf("==line==>%s< %p\n", ln, ln);
    // Reuse isdifficult as we haven't recorded sz?
    return ln;
  }
}
#undef APP
#endif
  

#define WCSTATE "%lines%words%bytes"
 
typedef struct wcstate {
  cmdfun f;
  BOUNDSTART

  unsigned int ln, wn, cn;
} wcstate;

#define APP wcstate
void* wc() {
  char c;
  unsigned int n= 0;
  
  if (!app) {
    app= STALLOC(wcstate, wc);
    return app;
  }

  // only generates one value
  if (!zline) return EOS;

  // no other events (except EOS)
  if (zline<EVENTS) return zline;
  
  // EOS: Output summary at end
  if (zline==EOS) {
    zline= malloc(25);
    sprintf(zline, "%u %u %u", app->ln, app->wn, app->cn);
    return zline;
    // TODO: do we need to put code to give EOF?
  }

  // process one line
  zptr= zline;
  app->ln++;
  app->cn+= strlen(zptr);
  while((c= *zptr)) {
    skipspc();
    app->wn++;
    --zptr;
    while((c= *++zptr) && !isspace(c));
  }

  // returns null (backtracks to get next line)
  lfree(zline);
  return NULL;
}
#undef APP  

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

#define APP lsstate
void* ls() {
  if (!app) {
    app = STALLOC(lsstate, ls);
    if (!app) return NULL;
    
    if (zline && *zline) {
      // If it contains a wildcard or is an explicit filename, save it as a filter pattern
      if (strchr(zline, '*')) {
        char* p= strrchr(zline, '/');
        if (p) { *p= 0; app->pat = strdup(p+1); }
        // TODO: simplify duplication
        else { app->pat = strdup(zline); zline = 0; }
      }	else { app->pat = strdup(zline); zline = 0; }
    }

    if (0 != open_dir(&app->dir, (zline && *zline)? zline: ".")) {
      app->dir_open = 1;
      REQUEST_CLEANUP();
      // all good
      // TODO: size? more attributes? timestamp"
      //vbind("_name", &state->name);
      return app;
    }
    // fail
    free(app);
    return NULL;
  }

  lfree(zline);
  if (!app->dir_open) return EOS;

  do {
    if (0==read_dir(&app->dir, &app->entry)
      || zline == CLEANUP) {
      if (app->dir_open) {
        close_dir(&app->dir);
        app->dir_open = 0;
        free(app->pat);
      }
      return EOS;
    }
  } while(!wildmatch(app->pat, app->entry.name));

  // found a matching one
  return lstrdup(app->name= app->entry.name);
}
#undef APP

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

#define APP lstate
void* ls() {
  struct dirent* de;
  char* p;
  
  if (!app) {
    app = STALLOC(lsstate, ls);
    if (!app) return NULL;

    if (zline && *zline) {
      // If it contains a wildcard or is an explicit filename, save it as a filter pattern
      if (strchr(zline, '*')) {

        p= strrchr(zline, '/');
        if (p) { *p= 0; app->pat = strdup(p+1); }
        // TODO: simplify duplication
        else { app->pat = strdup(zline); zline = 0; }
      }	else { app->pat = strdup(zline); zline = 0; }
    }

    app->dir = opendir((zline && *zline)? zline: ".");
    REQUEST_CLEANUP();
    // TODO: size? more attributes? timestamp"
    if (app->dir) return app;
    // fail
    free(app);
    return NULL;
  }

  lfree(zline);
  if (!app->dir) return EOS;

  do {
    if (!(de=readdir(app->dir)) || zline == CLEANUP) {
      if (app->dir) {
        closedir(app->dir);
        app->dir = NULL;
        free(app->pat);
      }
      return EOS;
    }

  } while(!wildmatch(app->pat, de->d_name));

  // found a matching one
  return lstrdup(app->name= de->d_name);
}
#undef APP

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

#define APP countstate
void* iota() {
  if (!app) {
    app = STALLOC(countstate, iota);
    if (!app) return NULL;

    app->n = nextInt(&zline, 1);
    app->e = nextInt(&zline, 10);
    app->d = nextInt(&zline, 1);
    // we need to compensate for first
    app->n-= app->d;

    return app;
  }

  lfree(zline);
  app->n+= app->d;
  if ((app->d > 0 && app->n <= app->e) ||
    (app->d < 0 && app->n >= app->e)) {
    char s[10];
    // TODO: use returned length:
    sprintf(s, "%d", app->n);
    return lstrdup(s);
  }

  return EOS;
}
#undef APP
        
#define HEADSTATE NULL
 
#define APP countstate
void* head() {
  if (!app) {
    app = STALLOC(countstate, head);
    if (!app) return NULL;

    app->n= 10; // default
    if (zline && *zline) {
      if (*zline=='-') ++zline;
      app->n= atoi(zline);
    }
    return app;
  }

  if (!zline || zline==EOS) return zline;

  if (app->n-- > 0) return zline;

  // This "cuts-off" the consumer
  lfree(zline);
  return EOS;
}
#undef APP
 
#define TAILSTATE NULL

#define APP countstate
void* tail() {
  unsigned int start;
  char** ring;
  
  if (!app) {
    app = STALLOC(countstate, tail);
    if (!app) return NULL;

    // +3 means skip 3 lines, -3 means last 3
    app->n= 0;
    if (*zline=='+') ++zline;
    app->e= -nextInt(&zline, 10);
    //    if (state->e < -2) state->e+= 2;
    if (app->e > 0) app->d= (intptr_t)calloc(app->e, sizeof(char*));
    return app;
  }

  #ifdef SHELLTRACE
  printf("\n\t  [TAIL %d %d %d %p]\n", app->n, app->e, (int)app->d, (void*)app->d);
  #endif
  
  // skip lines code
  if (!app->d) {
    if (app->e++ >= 0) return zline;
    lfree(zline);
    return NULL;
  }

  // tail code (keep ring buffer)
  ring= (char**)app->d;

  if (zline && zline != EOS) { 
    // insert
    if (++app->n >= app->e) app->n= 0;
    LFREE(ring[app->n]);
    return NULL;
  }

  // generate output
  start= app->n;
  do {
    if (++app->n >= app->e) app->n= 0;
    zline= ring[app->n];
    ring[app->n]= NULL;
    if (zline) return zline;
  } while (app->n != start);

  return EOS;
}
#undef APP

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

#define APP StatsState
char* stats() {
  if (!app) {
    app= STALLOC(StatsState, stats);
    app->min= 0x7fff;
    app->max= 0x8000;

    return (char*)app;

  }
  
  // End Of Stream => report
  if (app->done) return (lfree(zline),EOS);
  if (zline==EOS) {
    char report[128]= {0};
    app->avg    = app->sum / app->n;
    app->var    = (app->sqsum-((app->sum*app->sum)/app->n))/app->n;

    // TODO: (no have sqrtr!)
    //    app->stddev = sqrt(app->var);

    // median histogram
    qsort(app->samples, SAMPLES, sizeof(int), cmpint);

    #if 0
    // print samples for debugging
    {
      int i;
      printf("SAMPLES: ");
      for(i=0; i<SAMPLES; ++i) printf("%d ", app->samples[i]);
      putchar('\n');
    }
    #endif
        
    app->median= app->samples[SAMPLES/2-1]; // "middle'
    
    sprintf(report, "count:\t%u\nmin:\t%d\nmax:\t%d\nsum:\t%ld\nsqsum:\t%ld\navg:\t%d\nmedian:\t%d\nvar:\t%d\nstddev:\t%d",
      app->n, app->min, app->max, app->sum, app->sqsum, app->avg, app->median, app->var, app->stddev);
    app->done= 1;
    return strdup(report);
  }
  
  if (zline < EVENTS) return zline;
  
  // Process one piece of data
  { 
    // TODO: consider a iline var set by... how about args?
    int v= atoi(zline);
    lfree(zline);

    app->sum+= v;
    app->sqsum+= ((long)v) * v; // increase precision
    if (v < app->min) app->min= v;
    if (v > app->max) app->max= v;
    
    // with lower probability: insert at random position for median!
    // Scale down probability precisely: 1 / (n + 1)
    if ((app->mask|= app->n) < SAMPLES)
      app->samples[app->n]= v;
    else if ((rand() & (app->mask/2)) < SAMPLES) 
      //{ printf("REPLACE %d\n", v);
      app->samples[rand()&(SAMPLES-1)]= v;
      //}
    
    ++app->n;

    // backtrack to suck up more
    return NULL;
  }
}
#undef APP 

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
 
#define APP psstate
void* ps() {
  char s, p, ln[60]; // ... shell args...
  long packed_result;
  Window *w;
  unsigned int m;
  
  if (!app) {
    app= STALLOC(psstate, ps);
    return app;
  }

  // return header before data line
  if (app->i++ == 0)
    return strdup(
//----------------------------------------
// " PID %C #M  SZ  ST  TIME CMD");
" PID #M STT TIME CMD");
//4203 27 33 437 KEY 27:30 foobar -a"

  // return data lines

  // - walk ot next live window/process structure
  w= NULL;
  p= app->i - 1;
  while(p < nwin+1) {
    w= wins + p;
    if (w->status) break;
    w= NULL; ++p;
  }
  app->i= p + 1;

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

  app->pid   = 0x4200 | p;
  app->cpu   = w->cpu;
  app->mem   = w->nalloc;
  app->size  = -1; // w->abytes
  app->mins  = m;
  app->secs  = s;
  app->name  = wname(p);
  app->args  = w->args;
  
  // WARNING! sizeof not used!
//snprintf(ln, sizeof(ln), "42%02d %2d %2d%4d %.3s%2d:%02d %s %s"
  snprintf(ln, sizeof(ln), "42%02d %2d %.3s%2d:%02d %s %s"
    , p

//  , w->cpu
    , w->nalloc // == w->mem,
//  , -1 //w->abytes,
    , wstate(w->ret)
    , m, s
    , app->name, w->args
  );

  // enable if disable shprint, lol
  //puts(ln); return 0;
    
  lfree(zline);
  return lstrdup(ln);
}
#undef APP
 
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

#define APP editlinestate
void* editline() {
  if (!app) {
    return STALLOC(editlinestate, editline);
  } //else if (!KEYEVENT(zline)) return WAITKEY;
  else {
    // generealize... dstr?
    char c, *s= app->s, len= s? strlen(s): 0;
    app->s= s= realloc(app->s, (len | 15) + 17); // hmmm
    if (app->i >= MAX_EDIT) return WAITKEY;
    s[app->i]= 0;
    
    lfree(zline);
    
    // TODO: wraps if too long
    printf("\r> %s", s);
    //c= cursorgetc();
    c= getchar();

    // Key input
    if (c==27 || c&0x80 || c=='C'-'@') {
      // ESC RET FUNC- CTRL-C (BREAK)
      lfree(s);
      app->s= NULL;
      app->i= 0;
      return NULL;
    } else if (c==10 || c==13 || c=='D'-'@') {
      // RETURN CTRL-D
      char *r= s;
      app->s= NULL;
      app->i= 0;
      putchar('\n');
      return r;
      // TODO: ^P get previous line (save it!)
    } else if (c=='U'-'@') {
      // clear line CTRL-U
      printf("\\\n");
      s[app->i= 0]= 0;
    } else if (c==127 || c==8) {
      // backspace
      printf("\b \b");
    } else {
      // insert char
      s[app->i++]= c;
      s[app->i]= 0;

      putchar(c);
    }

    return WAITKEY;
  }
}
#undef APP    

#define TEETERMINALSTATE NULL
 
// more like "tee -"
#define APP simplestate
void* teeterminal() {
  if (!app) return STALLOC(wcstate, wc);

  shprint(zline);
  return zline;
}
#undef APP

#define TERMINALSTATE NULL

// can only be last in chain!
#define APP simplestate
void* terminal() {
  if (!app) return STALLOC(simplestate, terminal);

  shprint(zline);
  lfree(zline);
  
  // force backtracking, why different?
  return zline==EOS? EOS: NULL;
}
#undef APP

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
// TODO: maybe not need fp? just make speical app define
  zapp= (void*)fp;
  zline= line;
  zline= (*fp)();
  if (zline) ++*loco; else --*loco;

#ifdef SHELLINFO
  printf("> %s => ", taskname(*fp));
  shprint(zline); 
#endif  

  return zline;
}

int wrunsystrain(cmdtrain* train) {
  cmdfun *fp;
  cmdtrain *origtrain= train;

  // used to find variables!
  trainptr= train;
  ++train; // skip initial 0
  zline= EOS;

#ifndef SHELLTRACE
  // Beatifully simple!
  
#if 1

  do {
    // passing around zline, redundant LOL
    zline= wtrainstep(&train, zline);
  } while(*train);

#else
  // inline
  
  while((fp=*train)) {
    zapp= fp;
    zline= (*fp)();
    if (zline) ++train; else --train;
  }    

#endif

#else

  // SHELLTRACE
  while((fp=*train)) {
    printf("\t  [%d \"%s\" %p]\n", (int)(long)(train-origtrain),
	   !zline? "(NULL)": zline==EOS? "*EOS*": zline, fp);

    zapp= fp;
    zline= (*fp)();

#ifdef VARINFO
    printf("\t%s %p:", taskname(*fp), zline); fflush(stdout); shprint(zline);
#endif

    if (zline) ++train; else --train;
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
        zapp= (void*)f;
        zline= CLEANUP;
        (*f)();
      }
      // remove that state
      free(f);
    }

// TODO: vclearvars

// TODO: remove
    cleanbits>>= 1;

  } while(--n);
  
  free(train);
}

cmdtrain* wsysparse(char* cmd, char* pi, unsigned int *bitsout) {
  // TODO: check overflow this per command?
  char i, **n, *p;
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


    args= NULL;
    
    // TODO: same as getNext!? - make this better!
    // - extract command name
    skipspc();
    name= zptr;
    skipword();
    if (*zptr!='|')  {
      *zptr++= 0;

      // TODO: this may give empty string...
      skipspc();

      // - extract arguments
      args= zptr;
      skiptill('|');
      // trim args end
      p= zptr;
      if (!*zptr) zptr= 0; else *zptr++; //= 0;
      do { *p= 0; } while(*--p == ' ');
    } else *zptr++= 0;

    //printf("name>%s<\n", name);
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
      zapp= NULL;
      zline= args;
// TODO: capture zapp, maybe default init/mgr
      arr[++i]= state= (*f)();

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


// cleanup magic macros


#undef voidapp


#ifndef MAIN
 
void tsystem(char* cmd) {
  printf("\n\n----(%u) %s\n", _heapmemavail(), cmd);
  system(cmd);
}
  
//void gts(char* name) {
//  printf("%s: \"%s\"\n", name, vgets(name));
//}

//void gti(char* name) {
//  printf("%s: %d\n", name, vgeti(name));
//}

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
  system("iota 1 17 |grep 7 |terminal"); exit(0);

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
