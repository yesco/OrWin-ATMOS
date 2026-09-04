// fillins.c - fill in generic functions missing
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// This is for the OrWIN-ATMOS project,
// to allow it to compile and run under a simulator.
//
// This file provides generic implementations of
// "missing" functions: strdup getline ...

//////////////////////////////
#ifndef SCREENROWS

// We're not dealing with REAL hardware but
// simulated environment, like running in 
// a terminal.

// text screen direct addresses macros

// TODO: not unless we simulate it!

char* wcurscr= NULL;

#define TEXTSCREEN (wcurscr)

// TODO: handle resize if vt100? see unix/sim
#define SCREENROWS 28
#define SCREENCOLS 40

#define SCREENSIZE (SCREENROWS*SCREENCOLS)

#define SCREENXY(x, y) ((char*)(TEXTSCREEN+(5*(y))*8+(x)))

// make it be checkboard gray 50%
//#define SCRFILLCHAR 127
#define SCRFILLCHAR '#'

#ifndef SHADESTR
  //#define SHADESTR  "░" // 25% (U+2591 — Light shade)
  #define SHADESTR    "▒" //  50% (U+2592) — Medium shade)
  //#define SHADESTR  "▓" //  75% (U+2593) — Dark shade)
  #define FULLSTR     "█" // 100% (U+2588) — 100% Filled)
#endif

#ifdef PUTCHAR
  #undef PUTCHAR
#endif

#ifdef putchar
  #undef putchar
#endif

#ifdef printf
  #undef printf
#endif

// vt100
void vt_clear() { printf("\x1b[2J\x1b[H"); }
void vt_clearend() { printf("\x1b[K"); }
void vt_cleareos() { printf("\x1b[J"); }
void vt_resetcolors() { printf("\x1b[0m"); } // bgcol= 0; fgcol= 7; }

void vt_gotorc(int r, int c) {
  // negative values breaks the ESC seq giving garbage on the screen!
  assert(r>=0 && c>=0);
  printf("\x1b[%d;%dH", r+1, c+1);
}

char* woldscr= NULL;

// TODO: clever updatedatescreen();

void redrawscreen() {
  char x= 0, y, c, *p= TEXTSCREEN-1;

  for(y=0; y<SCREENROWS; ++y) {
    vt_gotorc(y, x); vt_resetcolors();

    for(x=0; x<SCREENCOLS; ++x) {
      // TODO: handle colors
      switch((c= *++p)) {
      case 127: fputs(FULLSTR, stdout); break;
      default:
        // TODO: hibit inversion
        if (c <= 7) {
          // ink
          printf("\e[%dm", 7-(c)+30); break;
        } else if (c >= 0x10 && c <= 0x17) {
          // bg
          printf("\e[%dm", 7-(c-0x10)+40); break;
        } else 
          putchar(c);
      }
    }
  }
  
  // save current state
  memcpy(woldscr, wcurscr, SCREENSIZE);

  vt_gotorc(winp->y + winp->r, winp->x + winp->c);
}

void initscreen() {
  wcurscr= malloc(SCREENSIZE);
  woldscr= malloc(SCREENSIZE);
  
  memset(wcurscr, SCRFILLCHAR, SCREENSIZE);
  memset(woldscr, 0,           SCREENSIZE);

  redrawscreen();
}

#else

// dummy
void initscreen() { }

#endif




//////////////////////////////
#ifndef STRDUP

#define STRDUP
char* strdup(const char* s) {
  char* r;
  if (!s) return 0;
  if (!(r= calloc(strlen(s)+1, 1))) return 0;
  return strcpy(r, s);
}

#endif // STRDUP

//////////////////////////////
#ifndef GETLINE

#define GETLINE

#define PR(...) (void)0

#if 1

// TODO: this one is working, cleanup
int getline(char **s, size_t *z, FILE* f) {
  size_t pos = 0;
  int c;

  PR("GET: 11111\n");
    
  // Enforce explicit initialization guard
  if (!s || !z || !f) return -1;

  PR("GET: 22222\n");
  if (!*s || *z == 0) {
    PR("GET: 3333\n");
    *z = 80;
    *s = (char*)realloc(*s, *z);
    PR("GET: 444\n");
    if (!*s) return -1;
    PR("GET: 555\n");
  }

  while ((c = fgetc(f)) != EOF) {
    PR("GET: 666\n");
    // Grow buffer if we are running out of space (leaving room for \n and \0)
    if (pos >= *z - 2) {
      char* new_s;
      PR("GET: 777\n");
      *z += 40;
      new_s = (char*)realloc(*s, *z);
      PR("GET: 888\n");
      if (!new_s) return -1;
      PR("GET: 999\n");
      *s = new_s;
    }

    PR("GET: aaa\n");
    (*s)[pos++] = (char)c;

    // Break on newline
    if (c == '\n') {
      PR("GET: bbb\n");
      break;
    }
    PR("GET: ccc\n");
  }

  // Handle End of File conditions cleanly
  PR("GET: ddd\n");
  if (pos == 0 && c == EOF) {
    PR("GET: eee\n");
    return -1;
  }

  (*s)[pos] = '\0'; // Null-terminate
  PR("GET: ffff\n");
  return (int)pos;
}

#else

// TODO: this oen maybe not working, at least not
//   on oscar64
int getline(char **s, size_t *z, FILE* f) {
  char* r;
  size_t len;
  char* rr;

  // Initialize buffer if it's empty
  if (!*s || !*z) {
    printf("get000\n");
    if (!(*s= realloc(*s, *z= 80))) return -1;
    printf("get111\n");
  }

  r= *s;

  static char xyz[128]= "blueberry";
  printf("get11212\n");
//  while ((rr= fgets(r, *z - (r - *s), f))) {
  while ((rr= fgets(xyz, sizeof(xyz), f))) {
    printf("get2222\n");
    len= strlen(r);
    // continues? (no nl)
    printf("get333\n");
    if (len > 0 && r[len - 1] != '\n') {
      printf("get444\n");
      *z+= 40;
      if (!(*s= realloc(*s, *z))) return -1;
      printf("get555\n");
      r= *s + strlen(*s);
    } else {
      printf("get666\n");
      break;
    }
  }
  printf("get777: rr= >%s<\n", rr);

      
  if (r == *s && feof(f) && strlen(*s) == 0) return -1;

  printf("get888\n");
  return strlen(*s);
}
#endif

#endif // GETLINE


//////////////////////////////
#ifndef BZERO

#define BZERO
#define bzero(p, z) memset((p), 0, (z))

#endif //BZERO

//////////////////////////////
#ifndef ZERO

// #self
#define ZERO(p) memset((p), 0, sizeof(*(p)))

#endif // ZERO

//////////////////////////////
#ifndef FILL

#define FILL
void fill(char x, char y, char w, char h, char c) {
  assert(0);
  // TODO: loop over terminal gotoxy...
}

#endif // FILL


//////////////////////////////
#ifndef CLOCK

  #define CLOCK
  // time.h included - I think!

  // if we don't know time, at least make
  // clock monotonically increasing!
  clock_t clock() {
    static clock_t counter= 0;
    return ++counter;
  }
    
#endif // CLOCK


//////////////////////////////
#ifndef HELP

  #define HELP
  void help() {
    // TODO: save part of screen, display HELPTEXT, restore
    assert(0);
  }

#endif // HELP

//////////////////////////////
#ifndef HEAPMEM

  #define HEAPMEM
  size_t _heapmemavail(void) { return 4711; }
  size_t _heapmaxavail(void) { return   42; }

#endif // HEAPMEME

//////////////////////////////
//#ifndef

void init() {
  initscreen();
}

//////////////////////////////
#ifndef NL_IMPL

#define NL_IMPL

void nl()     { putchar('\n'); }
void nlpure() { putchar('\n'); }
void clnl()   { putchar('\n'); }

char putcraw(char c) { putchar(c); return c; }

#endif // NL_IMPL

//////////////////////////////
#ifndef MYGETC

#define MYGETC
char mygetc() { return getc(); }

#endif // MYGET


//////////////////////////////
#ifndef SAVEWIN

#define SAVEWIN
char* savewin() { return NULL; }
// TODO: lol

#endif // SAVEWIN


//////////////////////////////
#ifndef LOADEWIN

#define LOADWIN
void loadwin(char* p) { free(p); }
// TODO: lol


#endif // SAVEWIN

//////////////////////////////
#ifndef CURSORGETC

#define CURSORGETC
char cursorgetc() {
  char c;
  // TODO: use kbhit
  cputc('*');  // "cursor"
  c= mygetc();
  cputc(8);
  cputc(c);
  return c;
}
// TODO: lol

#endif // CURSORGETC

//////////////////////////////
#ifndef CPUTC

#define CPUTC
void cputc(char c) { putchar(c); }

#endif // CPUTC
