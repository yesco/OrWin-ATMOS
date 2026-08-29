#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>

#include <assert.h>
// LOL, doesn/t suppress  thew warning, lol
//#define STATIC_ASSERT(COND, MSG) typedef char static_assertion_##MSG[(COND) ? 1 : -1]

/* Sizes

ORWIN:  9508
APPS :  8412
DATA :  1797
MISC :  3888 other apps older app-( stackers
CC65 :  6208
CCDAT:   428
------------
TAP  : 30241 (- 30241 9508 8412 1797 6208 428) 3888

(- (+ 21942 1672) 9508 8412 1797 6208) -2311


*/


// Preferred Window Layout

#define NHORIZWIN  3
#define NVERTWIN 3

#define WMAX ((40-NHORIZWIN*5) / NHORIZWIN -1)
#define HMAX ((28-NVERTWIN*4) / NVERTWIN)

// TODO: magenta on blue - not sood good
#define IS_BAD_CONTRAST(fg, bg) ((0xB1 >> ((fg) ^ (bg))) & 1)

#include "orwin.h"

#define HSPARKS "\x20\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xdf"

// Maybe can be used to draw graphs
// TODO: or arrows (remember needs ALTMODE prefix)
#define GRAPHX          "\xe7"
#define GRAPHHORIZTICKS "\xe8"
#define GRAPHCHROSS     "\xe9"
#define GRAPHVERTICKS   "\xea"

#define VSPARKS "\x20\xeb\xec\xed\xee\xef\xdf"

//  "\0\0\0\0\0\x3f\x3f\x3f" \ // TODO: already 32+3*16

//  "\0\0\0\x3f\x3f\x3f\x3f\x3f" \ // TODO: 96-3


#define SPARKDEFS \
  "\0\0\0\0\0\0\0\x3f" \
  "\0\0\0\0\0\0\x3f\x3f" \
  "\0\0\0\0\0\x3f\x3f\x3f" \
  "\0\0\0\0\x3f\x3f\x3f\x3f" \
  "\0\0\0\x3f\x3f\x3f\x3f\x3f" \
  "\0\0\x3f\x3f\x3f\x3f\x3f\x3f" \
  "\0\x3f\x3f\x3f\x3f\x3f\x3f\x3f" \
\
  "\x00\x21\x12\x0c\x0c\x12\x21\x00" \
  "\x38\x20\x20\x20\x38\x20\x20\x20" \
  "\x20\x20\x20\x20\x20\x20\x20\x3f" \
  "\x00\x00\x00\x00\x00\x20\x20\x3f" \
\
  "\x20\x20\x20\x20\x20\x20\x20\x20" \
  "\x30\x30\x30\x30\x30\x30\x30\x30" \
  "\x38\x38\x38\x38\x38\x38\x38\x38" \
  "\x3c\x3c\x3c\x3c\x3c\x3c\x3c\x3c" \
  "\x3e\x3e\x3e\x3e\x3e\x3e\x3e\x3e"


  
// (- 13701 12410) = 1291 bytes code for mowin :-(
// TODO: RESIZE crashes... 
#define MOWIN 
// (- 14332 12410) = 1922, (- 1922 1291) = 631 bytes more
#define OPTMOV

// optimized version
// TODO: make putz default and putc call it?

// TODO: wclreol doesn't know position
#define OPTPUTZ

#define MAXPUTZ 128

// TODO: see apprun.c !

#include "apps.ext"

typedef unsigned int uint;

// Dummies, these are never called, satiesfy orwin.reg

#define shell 0
#define orwin 0
#define SUMMARY 0
#define CC65 0

struct apps {
  char* name;
  void* fun;
  uint size;
} apps[] = {

  //#include "apps.reg"
  #include "orwin.reg"

  {0, 0}
};

// Process Stack allocation sizes
// ==============================

// The cc65 stack has two parts:
// - Hardware stack 256 bytes ~ 128 calls deeep
// - Data stack (hi memory) ~ 2048 bytes

//                       (/ 128 R)   (* R S)
// SPAWN_REC  SPAWN_STEP  #procs   #stacksize
// ---------  ----------  ------   ----------
//    20          15        6         300      works for 5 + 1
//    15

// (/ 128 20)

// SPAWN_REC is a measure of Hardware stack allocation
//#define SPAWN_REC 20
//#define SPAWN_REC 16 // works a while
//#define SPAWN_REC 19 // like 20

//#define SPAWN_REC 20
// FUNCT-List needs 24 rets! (48 bytes)
// to run everywhere, seems we are "deepP
// cprintf was taking too much, cputd is 6 recursive
// TODO: nested yields?

//#define SPAWN_REC 24 // works with CTRL-L
#define SPAWN_REC 25 
//#define SPAWN_REC 40 // not even enough for move+

// SPAWN_STEP*SPAWN_REC is Data stack allocation
//#define SPAWN_STEP 15
//#define SPAWN_STEP 18 // lines up
#define SPAWN_STEP 8 // lines up
//#define SPAWN_STEP 30

#define BYTES (SPAWN_REC*(SPAWN_STEP+1))


typedef int (*app)();

//#define TRACE

#ifdef TRACE
  #define DEB(c) cputc(c)
  #define DKEY() cgetc()
#else
  #define DEB(c) 
  #define DKEY() 
#endif


// TODO: make my own interrupt timer!
#define HITIME (*(volatile unsigned char*)0x305)

// hi byte of timer at yield
char wtime= 0;

clock_t clock() {
  // ORIC TIMER 100 interrupts/s,
  // TODO: make clock_t bigger and handle wraparound
  return ~*(volatile unsigned int*)0x276;
}



//////////////----------------------------------------
#define HELP "FUNCT-3 Prev spc Next List Run Kill Shll"

// TODO: make an "oric.h"

#define CHARSET    ((char*)0xB400) // $B400-B7FF
#define CHARDEF(C) ((char*)(CHARSET+(C)*8))
#define ALTSET     ((char*)0xB800) // $B800-BB7F

#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F
#define SCREENROWS 28
#define SCREENCOLS 40

#define SCREENSIZE (SCREENROWS*SCREENCOLS)
#define SCREENLAST (TEXTSCREEN+SCREENSIZE-1)

#define curscr TEXTSCREEN
#define SCREENXY(x, y) ((char*)(curscr+(5*(y))*8+(x)))

void fill(char x, char y, char w, char h, char c) {
  char* p= SCREENXY(x, y);
  // strided
  while(h--) {
    memset(p, c, w);
    p+= SCREENCOLS;
  }
}


char nwin= 0, wfocus= 0, wcur= 0, wnext, *wret;

Window win[WIN_MAX]= {
  { SCREENCOLS-15, 3, 14, SCREENROWS-3,
    0, 0,
    NULL,
    black, white,
    0,
    0, 0,
    0, 0}
};

Window* winp= 0;


char* wname(char winid) {
  void* fun= win[winid].fun;
  struct apps* p= apps;
  
  while(p && (p->fun != fun)) ++p;

  return p? p->name: 0;
}



////////////////////////////////////////////////////////////
// Heap stuff

void* walloc(size_t z) {
  ++winp->nalloc;
  return malloc(z);
}

void* wcalloc(size_t z, size_t n) {
  ++winp->nalloc;
  return calloc(z, n);
}

void* wrealloc(void* p, size_t z) {
  if (z && !p) winp->abytes++;
  return realloc(p, z);
}

void wfree(void* p) {
  --winp->nalloc;
  free(p);
}


char* updatewinptr() {
  return winp->p= SCREENXY(winp->x + winp->c, winp->y + winp->r);
}

#undef putchar
// inefficient, but should do the job
// TODO: rename wputc?
int putchar(int c) { return wputc(c); }

// 2x-10x faster not calling putchar for every char!

// TODO: I think buggy!
int write(int fd, char* buf, size_t count) {
#ifndef WRITE

  char n= count;
  while(n--) putchar(*buf++);
  return count;
  (void)fd;
}

#else

  static uint n;
  static char c, *b, *p;
  static signed char left;

  n= count;
  b= buf; p= winp->p - 1; left= winp->w - winp->c;

  --b;
  while(n--) {
    if (left-- < 0 || ((c= *++b) & 0x7f) < 32) {
      winp->p= p; ++n; winp->c= winp->w - left;
      putchar(c);
      p= winp->p - 1; left= winp->w - winp->c;
    } else {
      *++p= c;
    }
  }
  winp->p= p+1;
  
  return count;
}

#endif



void wgotoxy(char x, char y) {
  winp->c= x<winp->w? x: winp->w;
  winp->r= y<winp->h? y: winp->h;
  updatewinptr();
}

void wscreensize(char* w, char* h) {
  *w= winp->w; *h= winp->h;
}

void wclreol() {
  // TODO: +1 clears last pos in ASCII, why?
  memset(winp->p, ' ', winp->w - winp->c + 1);
}

void wclrscr() {
  char b= winp->bg | BG, f= winp->fg;
  char h;

  // off by one
  fill(winp->x, winp->y, winp->w, h= winp->h, 'x');
  // reset cursor position
  winp->c= winp->r= 0;
  updatewinptr();

// TODO: not good because SHADOW requires FILL
// Also, some off by two or 1 error on right
// side, as it clears rows it expands background
#if 0
  { char* p;
  // set paper and ink
  p= winp->p - 2;
  ++h;
  while(--h) {
    *p= b; p[1]= f;
    // no effect?
    //p[winp->w]= white
    p+= SCREENCOLS;
  }
  }
#else
  // TODO: just make simple loop!
  
  // background color
  fill(winp->x-2, winp->y, winp->w+4, winp->h, BG | winp->bg);

  // set text color
  fill(winp->x-1, winp->y, 1, winp->h, winp->fg);
#endif
}

char* winptr() {
  return winp->p;
}


// doesn't scroll, just wraps around, no wclreol()
void nlpure() {
  winp->c= 0;
  if (++winp->r >= winp->h) winp->r= 0;
  updatewinptr();

  // set current (new) colors
  winp->p[-2]= BG | winp->bg;
  winp->p[-1]=      winp->fg;
}

void clnl() {
  wclreol();
  nlpure();
}

// doesn't scroll, just wraps around, DOES wclreol()
void nl() {
  nlpure();
  wclreol();
}

char putcraw(char c) {
  *winp->p= c;
  wputc(KEYRIGHT);
  return c;
}

// minimal terminal codes
// Free codes: 11, 14; 24,25,26, 28,29,30,31
char wputc(char c) {
  // HACK! (might "bleed across windows if not conseq)
  static char lastcolor;
      
  ++winp->nputc;
  
  switch(c) {

  case 127: wputc(8); wputc(' '); // fall-through
  case KEYLEFT: 
  case 8: if (winp->c) winp->c--;  // CTRL-H = \b - BS - ^h
    else  if (winp->r) winp->r--,winp->c= winp->w;
    updatewinptr();
    goto done;
  // (9) Tab 8 forward
  case '\t': if ((winp->c= ((winp->c + 8) & 0xf8)) > winp->w) {
      c=10; break; } else updatewinptr(); goto done;
  case 10: nl(); goto done;      // CTRL-J = \n & wclreol()
  case 11: nlpure(); goto done;   // CTRL-K = \n but NO wclreol!

  case 12: wclrscr(); goto done;     // CTRL-L = CLRSCR
  case 13: winp->c= 0; updatewinptr(); goto done; // CTRL-M = \r = CR

  // ASCII: ShiftOut (SO) - ALT font set - ORIC?
  case 14: wclreol(); goto done;  // CTRL-N CLREOL

  // ASCII: SHiftIn (SI) - Normal char set - ORIC?
  // (key unix: FLUSH0 togggle ignore output) - ORIC has it!
  case 15: goto done;         // CTRL-O

  // HIBIT SET
  case 128+12: gotoxy(0,0); goto done; // HOME
  case 128+13: clnl();      goto done; // CLNL


  // Graphical/Text-Mode switches
  case 24: break; // CTRL-X  CANcel (*ix: cancel input, emacs: ...)
  case 25: break; // CTRL-Y (EM end medium)  (*ix: delay suspend?)
  case 26: break; // CTRL-Z (SUB substitute) (cp/m: EOF, linux: suspend)

  // All other codes are oric attributes (color/blink)
  // https://notes.burke.libbey.me/ansi-escape-codes/

  case 27: break; // ESC, TODO: understood by puts maybe
    //case 0...7:  winf->fg= c; break;   // 0-7   : inc
    //case 16...23: winp->bg= c; break; // 16-23 : paper

  // Hazeltine:   CTRL-Y rawX rawY
  // vt100:       ESC [ 2 J - clear screen
  // emacs,shell:  CTRL-K ill till end of line
    
    // TODO: doesNOT do right! ... LOL

  case KEYRIGHT: winp->p++; break; // will reach column++
  case KEYDOWN:  if (++winp->r >= winp->h) winp->r= 0;          break;
  case KEYUP:    if (winp->r-- >= winp->h) winp->r= win->h-1;   break;

  // TODO: repeat char
  // vt100:     char ESC [ 70 b                   printf "=\e[79b\n"
  // tektronic: ESC ~ [count] [char]
  // heathkit:  CTRL-R [count+$1F] [char]
  // AppleII:   0x01 [Count Byte] [Character Byte]    

  // (Ultra-Compact):VT52 uses fixed-length,
  // binary-byte coordinate sequences, good for8-bit machine
  // ESC Y [Row+32] [Col+32]
																																																																									  
  // HIBIT - TODO: to allow for easy (no need use putcraw()!
  //
  // Oric Serial Attribute Codes (8 to 15)
  // Values from 8 to 15 control text modifiers like character sets,
  // height, and blinking on the Oric screen
  // -  8: Standard character set (single height,steady
  // -  9: Alternate character set (teletext/semi-graphics)
  // - 10: Double size standard character set
  // - 11: Double size alternate character set
  // - 12–15: Blinking variants and combinations of double size/charset options
  
  default:
    if (c==*BLACK) c= black; // it's really 0 but...
    // change INK or BG color for future, like ANSI!
    if (c<24) {
      if (c<8) {
	// inkk - use line attribute
	winp->fg= c & 0x7f;
	if (!winp->c) { lastcolor= winp->p[-1]= c; goto done; }
      } else if (c>=16) {
	// paper - use line attribute
	winp->bg= c & 0x7f;
	if (!winp->c && lastcolor > 24)
	  { lastcolor= winp->p[-2]= c & 0x7f; goto done; }
      }
      // or fall-through use a position on screen
    } else lastcolor= 255;
    
    *winp->p++= c;
  }

  // newline / line wrap?
  if (c==10 || winp->c++ >= winp->w) {
    char* p;
    
    winp->c= 0;
    // overflow rows?
    if (winp->r++ +1 >= winp->h) winp->r= 0;

    p= updatewinptr();

    // set current (new) colors
    p[-2]= BG | winp->bg;
    p[-1]=      winp->fg;

    wclreol();
  }

 done:
  return c;
}

// TODO: is it bettter than write optimized?

// TODO: remove all!

#ifndef OPTPUTZ

void wputz(char* s) {
  write(1, s, strlen(s));
}

#else

// Clever but old, should be using write!

void wputz(char* s) {
  char n, c, r, *p, k, w= winp->w, h= winp->h;
  unsigned int nputc= winp->nputc;

 restart:
  n= MAXPUTZ; c= winp->c, r= winp->r, p= winp->p - 1;
  --s;

  while((k= *++s)) {
    // handle special chars
    if ((k & 0x7f) < 32) break;

    //    if ((wtime^HITIME) & 0b1000000) yield(); // feels chunky
    //if (!--n) yield();

    // just put it there
    *++p= k;
    ++nputc;
    if (c++ >= w) {
      winp->c= c= 0;
      winp->r= r= (++r >= h)? 0: r;
      p= updatewinptr()-1;
    }
  }
  
  winp->c= c; winp->p= p+1;
  winp->nputc= nputc;
	       
  if (k) { wputc(k); ++s; goto restart; }

  // good time to release, minimic terminal avoid jitter
  //yield();
}
#endif

void wputs(char* s) {
  write(1, s, strlen(s));
  nl();
}

void wputi(int i) {
  char s[10]= {0};
  sprintf(s, "%d", i);
  // TODO: doesn't seem to wrap perfectly, sometimes in
  //   last change back text color column
  wputs(s);
}

void wstatus(signed char c, char* s) {
  char* p= winp->y * SCREENCOLS+ winp->x + c + TEXTSCREEN - SCREENCOLS;
  char w= winp->w + 2 + 1;
  //char xor= winp->bg&7==7?0 :128; // if white back
  char xor= 128;
  while(*s && w--) *p++= *s++ ^ xor;

  p[w-1]= ('0' | 128) + nwin;
}
	     
// (un)decorate wfocus
void wdecorate() {
  Window* w= win+wfocus;
  // Invert header (make it black if focused)
  char* p= w->y * SCREENCOLS + w->x + TEXTSCREEN - SCREENCOLS -2;
  char i;

  if (!w->status) return;

  for(i= w->w+4; i--; ) p[i]^= 128;
}

char wprev= 1;

void setwin(char);

void setfocus(signed char new) {
  char n= 0;
  //cputc('#');
  //cputc('0'+new);
  if (wfocus) wdecorate(); // show not active

 next:

  // TODO: confusing?
  
  wprev= wfocus;
  wfocus= new;
  if (new > nwin) wfocus= 1;
  if (new <= 0)   wfocus= nwin;
  if (n++ > WIN_MAX) return; // make sure not hang!
  if (!win[wfocus].status) { new= wfocus+1; goto next; }

  if (wfocus) wdecorate(); // show active
}

void winerase(Window* w) {
  fill(w->x-2, w->y-1, w->w+5, w->h+3, 126);
}

void winkill() {
  Window* w= win+wfocus;
  w->status= 0;
  free(w->state);
  //TODO: lfree(w->ret);
  winerase(w);
  setfocus(wfocus);
}

// returns old state
char togglecursor() {
  return !(*winp->p^= 128);
}

void windraw(Window* w) {
  // header (white block)
  fill(w->x-2, w->y-1, w->w+4, 1, 127);

  // shadow (resets BG to BLACK)
  fill(w->x-1, w->y, w->w+4, w->h+1, BG+black);

  wclrscr();

  // set text color after
  fill(w->x + w->w +1, w->y, 1, w->h, white);
}

void setwin(char w) {
  // TODO: set curwin?
  winp= win+w;
  wcur= w;
}

char newwin() {
  // TODO: reuse empty entries
  if (nwin==WIN_MAX) return 0;
  setwin(++nwin);
  setfocus(nwin);
}

char startline(app fun, char* line) {
  winp->status= 1;

  // TODO: glue it together? (like the train)
  winp->fun=   (void*)fun;
  winp->state= (void*)fun(0, line); // TODO: arg (template)

  return wcur;
}

char start(app fun) { return startline(fun, NULL); }


// This used to be conservative requring gray in bigger area around
// now, just make sure it's all gray.
char overlap(char x, char y, char w, char h) {
  int i, j;

#ifdef MOWIN
  if (x<3 || x+w >= SCREENCOLS-5) return 1;
  //  if (y<3 || y+h >= SCREENROWS-1) return 1;
  if (y<3) return 1;
  
  for(j= y-2; j<y+h; ++j)
    for(i= x-3; i<x+w+4; ++i)
      if (*SCREENXY(i, j) != 126) return 1;
#else
  // wrong?
  if (x<2 || x+w >= 41) return 1;
  if (y<1 || y+h >= 28) return 1;
  
  for(j= y-1; j<y+h+1; ++j)
    for(i= x-2; i<x+w+2; ++i)
      if (*SCREENXY(i, j) != 126) return 1;
#endif
  return 0;
}

// Open a window already created
//
// X  Y  == -1 (255): automatic placement
// FG BG == -1 (255): automatic "good contrast" colors
// W  == -1: TODO: automatic size
//
// Note: (can only be called once)

// TODO: if given exact coordinates, doesn't check...

char window(char x, char y, char w, char h, char bg, char fg) {
  char o;

  // already drawn/configure
  if (winp->w) return wcur;
  
  // placer
  if (x == 255 || y == 255) {
    x= 2; y= 1;
    while((o=overlap(x, y, w, h)) && y<29-h) {
      // doesn't work
      //gotoxy(28, 27); printf("(%2d %2d)", x, y);
      if (++x + w + 4 > 39) { ++y; x= 2; }
    }
    if (o) return -1;
  }
  winp->x= x;
  winp->y= y;

  winp->w= w;
  winp->h= h;

  if (bg==255 | fg==255) {
    // pick colors w "good contrast"
    do {
      bg= rand() & 7;
      fg= rand() & 7;
    } while(IS_BAD_CONTRAST(fg, bg));
  }
  
  winp->bg= BG | bg;
  winp->fg= fg;
  winp->exit= 0;
  winp->status= 1;
    
  // TODO: verify space/clash/overflow?
  windraw(winp);
  
  return nwin;
}


void help() {
  char tmp[40];
  memcpy(tmp, TEXTSCREEN, sizeof(tmp));
  memcpy(TEXTSCREEN, HELP, sizeof(HELP));
  cgetc();
  memcpy(TEXTSCREEN, tmp, sizeof(tmp));
}

void apprun();
void randnewwin(char* title, app main);

void mowin(signed char dx, signed char dy, signed char dw, signed char dh);
void task(app fun);

char wkey= 0;

#undef kbhit

char mygetc() {
  char o= cgetc(), k= *(char*)0x209, s= *(char*)0x208, c= o;

  // ORIC ATMOS: ROM magical shift key $209
  // $38 - no key
  // $a2 - CTRL key
  // $a4 - SHIFT left
  // $a5 - FUNCT key
  // $a7 - SHIFT right

  // remap ARROW KEYS: 8-11 to 28-31
  if (s==0xac || s==0xbc || s==0xb4 || s==0x9c) { // arrow key codes
    // CTRL -> 88..8b (original range + 128)
    if (k!=0xa2)  c+= 28-8; else c|= 128;
  }
  
  // CTRL keys "stunts" some bits put in range 0-31
  if (k==0xa2) c&= 0b10011111;

  // TODO: CTRL-M and RETURN are ambigous,
  // but RETURN probably should stay at 13, lol

  if (c==13 && k==0xa2) c== 11; // nlpure()

  // Set hi-bit if alt key
  if (k==0xa5) c|= 128;
  sprintf(SCREENXY(SCREENCOLS-3*4+1-2,SCREENROWS-1), "[%02x %02x %02x %02x]", c, o, k, s);

  return c;
}

// non-blocking
// (win=0 to not yield here as its called fom yield)
char wkbhit(char win) {
  char c;
  
 again:
  if (wkey && wfocus==win) return wkey;
  if (!kbhit()) return 0;
  
  // TODO: hmm, hsould be using getc?
  c= mygetc();

  // FUNCT || CTRL & ARROWKEY
  if (c&128) {
    //cprintf("  #%d '%c' ", c, c&0x7f);

    // Capture FUNC keys Window Keys
    c^= 128;
    switch((c= toupper(c))) {
    case ' ':
    case 'N': setfocus(wfocus+1); break;
    case 'P': setfocus(wfocus-1); break;
    case 27 : setfocus(wprev);    break; // toggle 

    // TODO: too easy kill, ask question!
    case 127:
    case 'K': winkill(); setfocus(wfocus+1); break; // Kill

    case 13 :
    case 'R': apprun(); break; // List
    case 'I': start(app_heap); break; 
    case 'S':
    case 'T': { // Shell/Terminal
      newwin();
      window(1, -1, 20-7, 10, green, black);
      start(app_shell);
      break; }

    // TODO: case 'M': maximize & minimize
  
    case 'L': { // ps
      newwin();
      // TODO: terminal overrides, hardcodes black...
      window(3, 17, 40-7, 10, yellow, black);
      startline(app_shell, "ps|terminal");
      break; }
    case 'H': help(); break;

#ifdef MOWIN
    // moving using FUNCT & ARROWKEYS
    case KEYLEFT : mowin(-1,0,0,0); goto again;
    case KEYRIGHT: mowin(+1,0,0,0); goto again;
    case KEYDOWN : mowin(0,+1,0,0); goto again;
    case KEYUP   : mowin(0,-1,0,0); goto again;
    // resizing using CTRL & ARROWKYS
    // (TODO : numeric combo?)
    case 0x98: mowin(0,0,+1,0); goto again;
    case 0x89: mowin(0,0,-1,0); goto again;
    case 0x8a: mowin(0,0,0,+1); goto again;
    case 0x8b: mowin(0,0,0,-1); goto again;
#endif // MOWIN

    default: if (isdigit(c) || isalpha(c) && c<='F')
	setfocus(c - (isdigit(c)? '0': 'A'+10));
    }

    return 0;
  }

  // save it
  wkey= c;
  return wkey && wfocus==win;
}

// Actually non-blocking
// will return 0 if no key
char wgetc(char win) {
  char c;
  
  if (wkbhit(win) && wkey && wfocus==win) {
    c= wkey;
    wkey= 0;
    return c;
  }
  // Nah, not yours and/or not in focus
  return wkey= 0;
}


char cursorgetc() {
  char c;
  togglecursor();
  c= cgetc();
  togglecursor();
  return c;
}

char* savewin() {
  char h= winp->h, w= winp->w + 3;
  char *p= malloc(w * h), *r= p;
  char *s= SCREENXY(winp->x - 2, winp->y);
  while(h--) {
    memmove(p, s, w);
    p+= w; s+= SCREENCOLS;
  }
  return r;
}

void loadwin(char* p) {
  char h= winp->h, w= winp->w + 3, *r= p;
  char *s= SCREENXY(winp->x - 2, winp->y);
  while(h--) {
    memmove(s, p, w);
    p+= w; s+= SCREENCOLS;
  }
  free(r);
}

// Pick app to run
//
// TODO: generalize the picker and move out
void apprun() {
  // TODO: common buff somewhere
  char line[40]= {0};
  char i= 0, w= wcur;
  struct apps *p, *found;
  char c, k, *spc, *tmp;

  // flush
  // TODO: remove, FUNC-R getting an r!
  while(kbhit()) getc();

  setwin(0);
  tmp= savewin();

  do {
    putchar(white);
    gotoxy(0, 0);
    putcraw(8);
    puts("\nStart APP\n");

    // -- list matches
    spc= strchr(line, ' ');
    if (spc) *spc= 0;
    p= apps; found= NULL;
    while(p->name) {

      if (strstr(p->name, line)) {
	putchar(green);
	if (!found) found= p;
      }
      else putchar(white);

      putcraw(8);
      
      wputz(p->name); // 7 cs !!! (printf 55 cs!)
      //wputc('\t'); wputi(p->size); // +19cs

      putchar(white);
      //' TODO: nl makes it flickr because of wclreol
      clnl();

      ++p;
    }
    clnl();
    if (spc) *spc= ' ';

    // -- get user input
    putcraw(8);
    putchar('>');
    putchar(found? green: white);
    putz(line); wclreol();

    c= cursorgetc(); k= *(char*)0x209; // TODO: abstract

    if (c==13 || c==27 || k==0xa5) break; // RET ESC FUNC

    // process input
    if (c==127 || c==8) {
      if (!i) continue;
      line[--i]= 0; putchar(127); continue;
    } 

    if (i>= 20) continue;
    // TODO: some bug where print one char outside of window!
    line[i++]= c;
    putchar(c);

  } while(1);

  loadwin(tmp);
  setwin(w);

  if (c == 13 && found) {
    //  printf("\nCHOOSEN: >%s<\n", line);

    // launch!
    newwin();
    startline(found->fun, spc? spc+1: 0);

    //cprintf("\n\n\n\n\n[WIN.%d: %p %p]", wcur, winp->state, winp->fun);

    // if it didn't open a window
    if (!winp->w) {
      char bg, fg;

      // pick colors w good contrast
      do {
	bg= rand() & 7;
	fg= rand() & 7;
      } while(IS_BAD_CONTRAST(fg, bg));
    
      // default tileable window size
      window(-1, -1, WMAX, HMAX, bg, fg);
      wstatus(-1, found->name);
      wdecorate();
    }
  }
}

#if 0
void randwin(char* title, app main) {
  char x, y, w, h, bg, fg;

  // randomize find space for window
  // (predictable random gen, lol)
  do {

    // pos,size not overlap existing
    do {
      x= (rand() % (40-3-4-2)) + 3;
      y= (rand() % (28-4-3-1)) + 2;
      w= (rand() % (30-x/2-3-3)) + 3;
      h= (rand() % (20-y/2-3-4)) + 4;
    } while(overlap(x, y, w, h));

    // colors w good contrast
    do {
      bg= rand() & 7;
      fg= rand() & 7;
    } while(IS_BAD_CONTRAST(fg, bg));

  } while(0);
  
  window(x, y, w, h, bg, fg);
  wstatus(-1, title);

  task(main);
  //  spawn(main);
}
#endif


// TODO: title not moved...
// TODO: colors messed up...
//   (because rewrite doesnpt restore fb bg only r c)
#ifdef MOWIN
void mowin(signed char dx, signed char dy, signed char dw, signed char dh) {
  Window* wf= win+wfocus;
  char x= wf->x, y= wf->y, w= wf->w, h= wf->h, b= wf->bg, f= wf->fg;

#ifdef OPTMOV
  // MOVING only smoothly
  if (!dw && !dh) {
    char i, *t, *p, W = w + (4 + 2 + 1)+1, H = h + 2 + 2;
    char *tmp, *s = SCREENXY(x - 3, y - 2);

    // Bounds checking
    if (x + dx < 3) return;
    if (x + w + dx >= SCREENCOLS - 3) return;
    if (y + dy < 2) return;
    if (y + h + dy >= SCREENROWS - 1) return;

    // Collision/Edge detection
    // TODO: tmp should be adjustested up and left?
    if (dy) {
      tmp = s + (dy < 0 ? -SCREENCOLS : SCREENCOLS * H);
      for (i = W; i--;) if (tmp[i] != 126) return;
    } else {
      tmp = s + (dx < 0 ? -1 : W);
      for (i = H; i--;) {
        if (*tmp != 126) return;
        tmp += SCREENCOLS;
      }
    }
      
    // Allocate temporary buffer
    tmp = malloc(W * H);
    if (!tmp) return;

    // 1. Copy original state from screen to tmp
    p = s; t = tmp;
    for (i = H; i--;) {
      memcpy(t, p, W);
      p += SCREENCOLS; 
      t += W;
    }

    // 2. Update window coordinates
    wf->x += dx;
    wf->y += dy;

    // 3. Copy back from tmp to the NEW screen position
    p = SCREENXY(wf->x - 3, wf->y - 2);
    t = tmp;
    for (i = H; i--;) {
      memcpy(p, t, W);
      p += SCREENCOLS; 
      t += W;
    }

    // 4. Clean up only the exposed trailing edge left behind
    if (dy < 0) {
      // Clear bottom row that was left exposed
      fill(x - 3, y - 2 + H - 1, W, 1, 126);
    } else if (dy > 0) {
      // Clear top row that was left exposed
      fill(x - 3, y - 2, W, 1, 126);
    } else if (dx < 0) {
      // Clear right column that was left exposed
      fill(x - 3 + W - 1, y - 2, 1, H, 126);
    } else if (dx > 0) {
      // Clear left column that was left exposed
      fill(x - 3, y - 2, 1, H, 126);
    }

    free(tmp);
  } else
#endif

  // TODO: RESIZE crashes!
  // Resize by capture text, undraw, and redraw
  {
    char i, j;
    int z= (w+3)*y+1; // 3= bg+fg+\n
    char *tmp= malloc(z), *t= tmp-1;

    // save text w newlines
    for(j= y; j<y+h; ++j) {
      // TODO: -2 to include bg,fg of each line!
      // but printing it may take space, lol
      // Adjust printing after NL
      for(i= x; i<x+w; ++i) 
	*++t= *SCREENXY(i, j);
      // TODO: edgecases...
      while(*t==' ') t--,cputc(8);
      *++t= '\n';
    }
    *++t= 0;

    winerase(wf);

    // only move if not overlap
    if (overlap(wf->x+dx, wf->y+dy, wf->w+dw, wf->h+dh)) {
      windraw(wf); return;
    } else {
      char iw= winp-win;
    
      wf->x+= dx; wf->y+= dy; wf->w+= dw; wf->h+= dh;
      i= wf->c; j= wf->r;

      // Pretend to be in wfocus
      setwin(wf-win);
      updatewinptr();

      // Draw new pusition + text
      windraw(wf);
      t= tmp;
      // not using wputs as it will yield()
      while(*t) wputc(*t++);

      // restore cursor position & color
      wf->c= i > wf->w? wf->w: i;
      wf->r= j > wf->h? wf->h: j;
      wf->bg= b; wf->fg= f;
      updatewinptr();

      setwin(iw);
    }

    free(tmp);
  }

}
#endif // MOWIN



//////////////////////////////////////////////////////////
// SCHEDULER!

void scheduler() {
  static clock_t latency, lastlatency;
  static clock_t run, runsum, runprocs, timesum;
  static clock_t rounds, lastupdate;

  runprocs= rounds= timesum= runsum= 0;
  
  wdecorate();
  setfocus(nwin);

  // TODO: error catchingg...
  //  while(setjmp(orwinjmp)!=42) {
  while(1) {
    DEB('^');

    // move to next app
  next:

    // every time we check if there is a KEY
    // if so: we route it directly to the wfocus window!
    // and this is prioritized as highly interactive app!
    //
    // TODO: winp->ret == WAITKEY
    while(wkbhit(0) || wkey) {
      // restore same window to be FAIR and NO starvation!
      wnext= wcur;

      // TODO: this is duplicated code - merge!
      
      // app can actually call kbhit() and getc()!
      setwin(wfocus);

      //cputc('!');
      run= clock();
      winp->ret= wret= (char*)(*(app)winp->fun)((int)winp->state, (char*)(wkey + 0x100));
      win->ticks+= run= clock()-run;
      win->cpu= run*rounds;
      runsum+= (run<<3) + 1;
      ++runprocs;
      
      wkey= 0;
      setwin(wnext);
    }

    // TODO: move out, or make it an APP!
    if (!--wcur) {
      clock_t now= clock();
      latency= now-lastlatency;
      lastlatency= now;
      ++rounds;
      timesum+= (latency<<3) +1;
      if (now-lastupdate > 100) {
	#undef gotoxy
	gotoxy(11, 0);
	cprintf("%3u#%3u%4d/s %2u%%%12c"
		, latency, rounds
		, (int)(runprocs*100L/(now-lastupdate))
		, (int)(runsum*100L/timesum)
		, ' '
		);
		
	lastupdate= now;
	rounds= runprocs= 0;
	// Rolling average (?)
	if (timesum > 4096) runsum/=4,timesum/=4;
      }
      wcur= nwin;
    }
    setwin(wcur); // inlinee ?

    // TODO: if no active windows/task will LOOP
    // TODO: use winp->ret in clever wait when SLEEPING
    if (!winp->status) goto next;

    // - Handle cheap tasks
    // (if WAITing for key has to have focus)
    if (winp->ret != WAITKEY) {
      ///cputc('.');
      //cputc('0'+wcur);
      *SCREENXY(39-wcur,0)= '0'+wcur;
      run= clock();
      winp->ret= wret= (char*)(*(app)winp->fun)((int)winp->state, 0);
      win->ticks+= run= clock()-run;
      win->cpu= run*rounds; // TODO: bad estimate
      runsum+= (run<<3) + 1;
      ++runprocs;
    }

    goto next;
    
    DEB('0'+wcur);
    DKEY();
  }
}

int main() {
  int i= 0, j= 0, z= 0;

  // needed for OrWIN on cc65, assumption and used extensivly
  //assert(sizeof(void*)==sizeof(int));

  memcpy(CHARDEF('_'), SPARKDEFS, 8);
  memcpy(ALTSET+(32+64)*8, SPARKDEFS, sizeof(SPARKDEFS));
	 
  // KBRPT - keyboard repeat rate
  *(char*)0x24f= 2;
  // KBDLY - keyboard delay before repeat
  *(char*)0x24e= 6;
  
  // cursor(0); // doesn't work
  // status location is at #26A.
  //  1 – cursor ON when set.
  //  2 – screen ON when set.
  //  4 – not used.
  //  8 – keyboard click OFF when set.
  // 16 – ESC has been pressed.
  // 32 – columns 0 and 1 protected when set.
  #define SCREENSTATE *((char*)0x26a)
  SCREENSTATE= 0+2+0+8+0;
  
  // clear background to "gray" checkerboard
  fill(0, 0, SCREENCOLS, SCREENROWS, 126);

  // Print logo in Upper Status Line
  strncpy(SCREENXY(0, 0), "0rWIN/ATMOS                                     ", 40);

  #if 0
  // Print logo in Right-Hand Corner
  strncpy(SCREENXY(34, 24), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 25), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 26),      " ATMOS", 6);

  // Print logo sideways right side
  strncpy(SCREENXY(38, 0), "\x0a""0", 2);
  strncpy(SCREENXY(38, 1), "\x0a""0", 2);
  strncpy(SCREENXY(38, 2), "\x0a""r", 2);
  strncpy(SCREENXY(38, 3), "\x0a""r", 2);
  strncpy(SCREENXY(38, 4), "\x0a""W", 2);
  strncpy(SCREENXY(38, 5), "\x0a""W", 2);
  strncpy(SCREENXY(38, 6), "\x0a""I", 2);
  strncpy(SCREENXY(38, 7), "\x0a""I", 2);
  strncpy(SCREENXY(38, 8), "\x0a""N", 2);
  strncpy(SCREENXY(38, 9), "\x0a""N", 2);
  strncpy(SCREENXY(38,10), "\x0a""/", 2);
  strncpy(SCREENXY(38,11), "\x0a""/", 2);

  strncpy(SCREENXY(38,12), " A", 2);
  strncpy(SCREENXY(38,13), " t", 2);
  strncpy(SCREENXY(38,14), " m", 2);
  strncpy(SCREENXY(38,15), " o", 2);
  strncpy(SCREENXY(38,16), " s", 2);
  #endif

  //memset(win, 0, sizeof(win));

  updatewinptr();
  
  win[0].status= -1;

// TODO: RWRITE TO BE APPS
  
//#define TIMER
//#define ATMOS
//#define FISH
//#define ECHO
//#define ASCII2
//#define CALC
//#define SNOW
  
#ifdef DEMO
  // OK
  // window( 3,  2, 23,  7, GREEN, BLACK);
  // CRASH: smaller than 23 crash+++
  // window( 3,  2, 22,  7, GREEN, BLACK);
  // ok
#ifdef TIMER
  window( 2,  2, 12,  7, green, black);
  wstatus(-1, "Timer");
  spawn(timer_main);
#else
  window( 2,  2, 23,  7, green, black);
  wstatus(-1, "Counter");
  spawn(counter_main);
#endif
  
#ifdef ECHO
  window( 4, 12, 11,  7, blue,  white);
  wstatus(-1, "ECHO");
  spawn(echo_main);
#endif

#ifdef FISH
  window(31,  5,  6,  5, white, blue);
  wstatus(-1, "ASCII");
  spawn(ascii_main);
#endif 

#ifdef CALC
  // Need odd line! for double!
  window( 4, 13, 17, 13, white,  black);
  wstatus(-1, "RPN-CALC");
  spawn(calc_main);
#endif

#ifdef ASCII
  window( 2, 22, 14,  5, cyan,  red);
  wstatus(-1, "ASCII");
  start(ascii_main);
#endif
  
#ifdef ATMOS
  // Atmos
  window(22, 12, 14, 14, black, yellow);
  wstatus(-1, "File Edit Options Tools");
  //spawn(atmos_main);
  spawn(flipflop_main);
#endif ATMOS
  
#else

#ifdef ASCII
  // TODO: need at least one because allocations
  window(30,  5,  6,  5, blue, white);
  wstatus(-1, "ASCII");
  spawn(ascii_main);
#endif
  
#endif // DEMO
  
#ifdef SNOW
  start(app_snow);
  window(39-16-2, 4, 16, 10, red, yellow);
  wstatus(-1, "Snow");
#endif

  newwin();
  window(-1, -1, 5, 3, blue, white);
  start(app_ascii);
  wstatus(-1, "ASCII");
  
  newwin();
  start(app_charset);


  scheduler();

  return 0;
}


void cput1h(char x) { x&= 0xf; cputc(x + (x<10? '0': 'A'-10)); }
void cput2h(char x) { cput1h(x>>4); cput1h(x); }
void cput4h(unsigned int x) { cput2h(x>>8); cput2h(x&0xff); }

void cputd(unsigned int d) { if (d>=10) cputd(d/10); cputc('0'+(d % 10)); }

void cspc() { cputc(' '); }


// returns
char* printbuf(char* j) {
  char *r= (char*)*(unsigned int*)j;;
  cspc();
  cputd(j[2]);
  cputc('-');
  cputd((unsigned int)r);
  cputc(':');
  cputd((((unsigned int)j[3])<<8) | j[4]);
  return r;
}
