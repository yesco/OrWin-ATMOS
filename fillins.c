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

// cc65 missing \e
#define ESC "\x1b"

// vt100
char vt_bg= 0, vt_fg= 7;

void vt_clear()       { printf("\x1b[2J\x1b[H"); }
void vt_clearend()    { printf("\x1b[K"); }
void vt_cleareos()    { printf("\x1b[J"); }
void vt_resetcolors() { printf("\x1b[0m"); vt_bg= 0; vt_fg= 7; }
void vt_cursoroff()   { printf("\x1b[?25l"); }
void vt_cursoron()    { printf("\x1b[?25h"); }
void vt_ink(char c)   { vt_fg= c & 7; printf(ESC "[%dm", vt_fg+30); }
void vt_paper(char c) { vt_bg= c & 7; printf(ESC "[%dm", vt_bg+40); }

void vt_gotorc(int r, int c) {
  // negative values breaks the ESC seq giving garbage on the screen!
  assert(r>=0 && c>=0);
  printf("\x1b[%d;%dH", r+1, c+1);
}

char* woldscr= NULL;

// TODO: clever updatedatescreen();

// doesn't handle inverse of color attributes, lol
//#define VT_INVERSE

// TODO: this one is only used to draw char in sequnce
//   not be used interactively, it translates memory
//   oric screen char and prints it out
void vt_putc(char c) {
  // inverse
  if (c & 0x80) {

//    c&= 0x80;
    // ink / paper
//    if (c < 32) c&= 7;

    // inverse
    #ifdef VT_INVERSE
    printf(ESC "[7m");
    #else
    vt_ink  (7 - vt_fg);
    vt_paper(7 - vt_bg);
    #endif

    vt_putc(c & 0x7f);
    
    //if (c < 32) {   c&= 7;
    
    // restore
    #ifdef VT_INVERSE
    printf(ESC "[27m");
    #else
    vt_ink  (7 - vt_fg);
    vt_paper(7 - vt_bg);
    #endif

    return;
  }
  
  // normal
  switch(c) {
  case 126: fputs(SHADESTR, stdout); break; // ~ lol
  case 127: fputs(FULLSTR, stdout); break;
  default:
    // We're "simulating" ORIC where color change uses one position!
    // and colors reset at every line
    if (c <= 7) {
      vt_ink(c & 7); putchar(' ');
    } else if (c >= 0x10 && c <= 0x17) {
      vt_paper(c & 7); putchar(' ');
    } else 
      putchar(c);
  }
}

extern void redrawscreen() {
  char x= 0, y, c, *p= TEXTSCREEN-1;

  vt_cursoroff();

  for(y=0; y<SCREENROWS; ++y) {
    vt_gotorc(y, x); putchar(13);
    vt_resetcolors();
    //printf(">%02d:", y);

    for(x=0; x<SCREENCOLS; ++x) vt_putc(*++p);
  }

  
  // save current state
  memcpy(woldscr, wcurscr, SCREENSIZE);

  // move cursor to actual position, maybe?
  
  //vt_gotorc(winp->y + winp->r, winp->x + winp->c);
  
  // lower right corner, no clobber when exit!
  vt_gotorc(255,255);

  // TODO: turn on when exiting...
  //vt_cursoron();
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
  if (0)
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
//void initscreen() { }

#define initscreen()     (void)0

#define redrawscreen()   (void)0

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

void nl()     { wputc('\n'); }
void nlpure() { wputc('\n'); }
void clnl()   { wputc('\n'); }

// TODO: ...
char putcraw(char c) { wputc(c); return c; }

#endif // NL_IMPL

//////////////////////////////
#ifndef MYGETC

#define MYGETC
char mygetc() { return getc(); }

#endif // MYGET


//////////////////////////////
#ifndef GOTOXY

#define GOTOXY

#undef gotoxy // lol
void gotoxy(char x, char y) {
  assert(0);
  (void)x; (void)y;
}
    
#endif // GOTOXY

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

