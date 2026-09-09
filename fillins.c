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

#ifdef clrscr
  #undef clrscr
#endif

// vt100
void vt_clear()       { printf("\x1b[2J\x1b[H"); }
void vt_clearend()    { printf("\x1b[K"); }
void vt_cleareos()    { printf("\x1b[J"); }
void vt_resetcolors() { printf("\x1b[0m"); } // bgcol= 0; fgcol= 7; }
void vt_cursoroff()   { printf("\x1b[?25l"); }
void vt_cursoron()    { printf("\x1b[?25h"); }

void vt_gotorc(int r, int c) {
  // negative values breaks the ESC seq giving garbage on the screen!
  assert(r>=0 && c>=0);
  printf("\x1b[%d;%dH", r+1, c+1);
}

char* woldscr= NULL;

// TODO: clever updatedatescreen();

#define ESC "\x1b"

void redrawscreen() {
  char x= 0, y, c, *p= TEXTSCREEN-1;

  vt_cursoroff();

  for(y=0; y<SCREENROWS; ++y) {
    vt_gotorc(y, x); putchar(13);
    //vt_resetcolors();
    printf(">%02d:", y);

    for(x=0; x<SCREENCOLS; ++x) {
      // TODO: handle colors
      switch((c= *++p)) {
      case 127: fputs(FULLSTR, stdout); break;
      default:
        // TODO: hibit inversion
        if (c <= 7) {
          // ink
          printf(ESC "[%dm", 7-(c)+30); break;
        } else if (c >= 0x10 && c <= 0x17) {
          // bg
          printf(ESC "[%dm", 7-(c-0x10)+40); break;
        } else 
          putchar(c);
      }
    }
  }

  
  // save current state
  memcpy(woldscr, wcurscr, SCREENSIZE);

  // move cursor to actual posotion"
  
  //vt_gotorc(winp->y + winp->r, winp->x + winp->c);
  
  // lower right corner, no clobeer when exit!
  vt_gotorc(255,255);

  vt_cursoron();
}

// Delays execution for a specific number of hardware "jiffies" 
// (1 jiffy ≈ 16.6ms on NTSC / 20ms on PAL)
void usleep(unsigned int count) {
// TODO:
#ifdef OSCAR64  
  while(count) {
    __asm {
      lda $a2         // Load the low byte of the system jiffy clock
   wait_loop:
      cmp $a2         // Compare it against itself until it ticks
      beq wait_loop
    };
  count--;
  }
#else
  {
    long n= count*1000;
    while(n--);
  }
#endif // !OSCAR64
}

void initscreen() {
  char a; // TODO: remove
  
  wcurscr= malloc(SCREENSIZE);
  woldscr= malloc(SCREENSIZE);
  
  vt_clear();

  // test speed
  if (1)
    for(a= ' '; a<128; ++a) {
      usleep(1000);
      memset(wcurscr, a, SCREENSIZE);
      redrawscreen();
    }
        
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

#include "getline.c"

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
  char* p= SCREENXY(x, y);
  // strided
  while(h--) {
    memset(p, c, w);
    p+= SCREENCOLS;
  }
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
#ifndef INIT

  #define INIT
  void init() {
    initscreen();
  }

#endif // INIT

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
#ifndef LOADWIN

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

//////////////////////////////
#ifndef CGETC

#define CGETC
char cgetc() { return mygetc(); }

#endif // CGETC

