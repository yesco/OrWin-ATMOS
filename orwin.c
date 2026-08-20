#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>

// (- 13701 12410) = 1291 bytes code for mowin :-(
// TODO: RESIZE crashes... 
#define MOWIN 
// (- 14332 12410) = 1922, (- 1922 1291) = 631 bytes more
#define OPTMOV

// (- 11136 10883) = 256 bytes
#define INFO

// Enable to get some stats (#putc)
#define STATS

// optimized version
// TODO: make putz default and putc call it?
#define OPTPUTZ
#define MAXPUTZ 128

// TODO: see apprun.c !

extern int counter_main(int argc, char** argv);
extern int timer_main(int argc, char** argv);
extern int ascii_main(int argc, char** argv);
extern int atmos_main(int argc, char** argv);
extern int flipflop_main(int argc, char** argv);
extern int echo_main(int argc, char** argv);
extern int calc_main(int argc, char** argv);

extern int app_clock(void* state, char* line);

#define WIN_MAX 16

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


#include "orwin.h"


// TODO: make my own interrupt timer!
#define HITIME (*(unsigned char*)0x305)

// hi byte of timer at yield
char wtime= 0;

clock_t clock() {
  // ORIC TIMER 100 interrupts/s,
  // TODO: make clock_t bigger and handle wraparound
  return ~*(unsigned int*)0x276;
}



//////////////----------------------------------------
#define HELP "FUNCT-3 spc Prev Next List Run Kill"

jmp_buf orwinjmp;
jmp_buf orwinnext;

#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F
#define SCREENROWS 28

#define SCREENCOLS 40
#define SCREENSIZE (SCREENROWS*SCREENCOLS)
#define SCREENLAST (TEXTSCREEN+SCREENSIZE-1)

#define curscr TEXTSCREEN
#define SCREENXY(x, y) ((char*)(curscr+(5*(y))*8+(x)))

void fill(char x, char y, char w, char h, char c) {
  char* p= SCREENXY(x, y);
  for(; h; --h) {
    // TODO: is memset faster?
    for(x= w; x; --x) *p=c,++p;
    p+= 40-w;
  }
}


typedef struct Window {
  char x, y, w, h;
  char r, c;
  char *p;
  char bg, fg;

#ifdef STATS
  unsigned int nputc;
#endif

  // TASK
  // (TODO: separate out tasks? only 12 B overhead)
  // (only if can have several in one window...)
  // (or windowless backgound task)

  char status;
  char exit;

  // light-weight process task
  void* fun;
  void* state;

  // StackTask
  jmp_buf start, cont;

} Window;

char nwin= 0, wfocus= 0, wcur= 0;
Window win[WIN_MAX];
Window* winp= 0;


char* updatewinptr() {
  return winp->p= SCREENXY(winp->x + winp->c, winp->y + winp->r);
}

void wgotoxy(char x, char y) {
  winp->c= x<winp->w? x: winp->w;
  winp->r= y<winp->h? y: winp->h;
  updatewinptr();
}

void wclreol() {
  fill(winp->x + winp->c, winp->y + winp->r, winp->w - winp->c + 1, 1, winp->bg);
}

void wclrscr() {
  fill(winp->x, winp->y, winp->w, winp->h, 32);
  // reset cursor position
  winp->r= 0;
  winp->c= 0;
  updatewinptr();
  
  // background color
  fill(winp->x-2, winp->y, winp->w+4, winp->h, BG | winp->bg);

  // set text color
  fill(winp->x-1, winp->y, 1, winp->h, winp->fg);
}

char* winptr() {
  return winp->p;
}

// minimal terminal codes
// Free codes: 11, 14; 24,25,26, 28,29,30,31
char wputc(char c) {
#ifdef STATS
  ++winp->nputc;
#endif
  
  switch(c) {
  case 8: if (winp->c) winp->c--;  // CTRL-H = \b - BS - ^h
    else  if (winp->r) winp->r--,winp->c= winp->w;
    updatewinptr();
    goto done;
  // Tab 8 forward
  case '\t': if ((winp->c= ((winp->c + 8) & 0xf8)) > winp->w) {
      c=10; break; } else updatewinptr(); goto done;
  case 10: break;             // CTRL-J = \n - see below
  case 11: goto done;         // CTRL-K
  case 12: wclrscr(); break;  // CTRL-L
  case 13: winp->c= 0; break; // CTRL-M = \r = CR
  case 14: wclreol(); break;  // CTRL-N
  case 15: goto done;         // CTRL-O

  // Graphical/Text-Mode switches
  case 24: case 25: case 26:
  case 28: case 29: case 30: case 31:

  // All other codes are oric attributes (color/blink)
  case 27: break; // ESC, TODO: understood by puts maybe
    //case 0...7:  winf->fg= c; break;   // 0-7   : inc
    //case 16...23: winp->bg= c; break; // 16-23 : paper

  default:
    if (c<24) if (c<8) winp->fg= c; else winp->bg= c;

    *winp->p++= c;
  }

  // newline / line wrap?
  if (c==10 || winp->c++ >= winp->w) {
    char* p;
    
    winp->c= 0;
    // overflow rows?
    if (winp->r++ +1 >= winp->h) winp->r= 0;

    // yield at new line (minimize jitter/zig)
    yield();

    p= updatewinptr();

    // set current (new) colors
    p[-2]= BG | winp->bg;
    p[-1]=      winp->fg;

    wclreol();
  }

 done:
  // TODO: somehow here yield() crashes!

  // not clear why higher value crawsh more easy?
  //if ((wtime^HITIME) & 0b1000000) yield();
  if ((wtime^HITIME) & 0b1000000) yield();
  //if ((wtime^HITIME) & 0b10000) yield();
  // 4th bit => 0..4095us
  //if ((wtime^HITIME) & 0b1000) yield();
  //if ((wtime^HITIME) & 0b100) yield(); // every 3 chars
  //if ((wtime^HITIME) & 0b10) yield(); // every 1.5 char
  //if ((wtime^HITIME) & 0b1) yield(); // every 1.5 char
  return c;
}

void nl() { putchar('\n'); }

#ifndef OPTPUTZ
void wputz(char* s) {
  while(*s) wputc(*s++);
  // good time to release, minimic terminal avoid jitter
  yield();
}
#else
void wputz(char* s) {
  char n, c, r, *p, k, w= winp->w, h= winp->h;
#ifdef STATS
  unsigned int nputc= winp->nputc;
#endif

 restart:
  n= MAXPUTZ; c= winp->c, r= winp->r, p= winp->p - 1;
  --s;

  while((k= *++s)) {
    // handle special chars
    if ((k & 0x7f) < 32) break;

    //    if ((wtime^HITIME) & 0b1000000) yield(); // feels chunky
    if (!--n) yield();

    // just put it there
    *++p= k;
#ifdef STATS
    ++nputc;
#endif
    if (c++ >= w) {
      winp->c= c= 0;
      winp->r= r= (++r >= h)? 0: r;
      p= updatewinptr()-1;
    }
  }
  
  winp->c= c; winp->p= p+1;
#ifdef STATS
  winp->nputc= nputc;
#endif
	       
  if (k) { wputc(k); ++s; goto restart; }

  // good time to release, minimic terminal avoid jitter
  yield();
}
#endif

void wputs(char* s) {
  wputz(s); nl();
}

void wputi(int i) {
  char s[10]= {0};
  sprintf(s, "%d", i);
  // TODO: doesn't seem to wrap perfectly, sometimes in
  //   last change back text color column
  wputs(s);
}

void wstatus(signed char c, char* s) {
  char* p= winp->y * 40 + winp->x + c + TEXTSCREEN - 40;
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
  char* p= w->y * 40 + w->x + TEXTSCREEN - 40 -2;
  char i;
  for(i= w->w+4; i--; ) p[i]^= 128;
}

char wprev= 1;

void setfocus(signed char new) {
  //cputc('#');
  //cputc('0'+new);
 next:
  wprev= wfocus;
  wfocus= new;
  if (new > nwin) wfocus= 1;
  if (new <= 0)   wfocus= nwin;
  // TODO: eteral loop/block if no actives
  if (!win[wfocus].status) { new= wfocus+1; goto next; }
}

void winerase(Window* w) {
  fill(w->x-2, w->y-1, w->w+5, w->h+3, 126);
}

void winkill() {
  Window* w= win+wfocus;
  winerase(w);
  w->status= 0;
}

// returns old state
char togglecursor() {
  return !(*winp->p^= 128);
}

void setwin(char w) {
  // TODO: set curwin?
  winp= win+w;
  wcur= w;
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

// TODO: title+= bar?
char window(char x, char y, char w, char h, char bg, char fg) {
  if (nwin==WIN_MAX) return 0;
  
  // TODO: so 0 is full screen...
  winp= win+ ++nwin;
  winp->x= x;
  winp->y= y;
  winp->w= w;
  winp->h= h;
  winp->bg= BG | bg;
  winp->fg= fg;
  winp->exit= 0;
  winp->status= 1;
    
  // TODO: verify space/clash/overflow?
  windraw(winp);
  
  return nwin;
}


void info();

void help() {
  char tmp[40];
  memcpy(tmp, TEXTSCREEN, sizeof(tmp));
  memcpy(TEXTSCREEN, HELP, sizeof(HELP));
  cgetc();
  memcpy(TEXTSCREEN, tmp, sizeof(tmp));
}

void newwin(char* title, app main);
void mowin(signed char dx, signed char dy, signed char dw, signed char dh);

char wkey= 0;

#undef kbhit

// non-blocking
// (win=0 to not yield here as its called fom yield)
char wkbhit(char win) {
  char c, k;
  
  if (wkey && wfocus==win) return wkey;
  if (!kbhit()) {
    if (win) yield();
    return 0;
  }
  
  c= cgetc(); k= *(char*)0x209;

  // debug print key
  // ORIC ATMOS: ROM magical shift key $209
  // $38 - no key
  // $a2 - CTRL key
  // $a4 - left shift
  // $a5 - FUNCT key
  // $a7 - right shift
  sprintf(SCREENXY(30,27), "[%d %x] ", c, k);

  // FUNCT || CTRL & ARROWKEY
  if (c&128 || (k==0xa2 && c>=8 && c<=11)) {
    //cprintf("  #%d '%c' ", c, c&0x7f);
    wdecorate();
    // Capture FUNC keys Window Keys
    c^= 128;
    switch(toupper(c)) {
    case ' ':
    case 'N': setfocus(wfocus+1); break;
    case 'P': setfocus(wfocus-1); break;
    case 27 : setfocus(wprev);    break; // toggle 

    // TODO: too easy kill, ask question!
    case 127:
    case 'K': winkill(); setfocus(wfocus+1); break; // Kill

    case 13 :
    case 'R': newwin("foo", app_clock); break; // List

    case 'L': info(); break;
    case 'H': help(); break;

#ifdef MOWIN
    // moving using FUNCT & ARROWKEYS
    case 0x08: mowin(-1,0,0,0); break;
    case 0x09: mowin(+1,0,0,0); break;
    case 0x0a: mowin(0,+1,0,0); break;
    case 0x0b: mowin(0,-1,0,0); break;
    // resizing using CTRL & ARROWKYS
    case 0x88: mowin(0,0,+1,0); break;
    case 0x89: mowin(0,0,-1,0); break;
    case 0x8a: mowin(0,0,0,+1); break;
    case 0x8b: mowin(0,0,0,-1); break;
#endif // MOWIN

    default:  if (isdigit(c))     setfocus(c-'0');
    }
    wdecorate();
    return 0;
  }

  // save it
  wkey= c;
  return wkey && wfocus==win;
}

// blocking per app, but will yield
char wgetc(char win) {
  char c;
  
  while(1) {
    // TODO: suspend and wake up
    while(!wkbhit(win)) yield();
    if (wkey && wfocus==win) {
      c= wkey;
      wkey= 0;
      return c;
    }
  }
}

void yield() {

  // cheap tasks running inside process 0
  if (winp->fun) return;
  

  // Enable to "see" yields!
  //wputc('|');
  
  DEB('?');

  // Handle keyboard, if there are no keyboard apps
  if (kbhit()) wkbhit(0);
  
  // snapshot hi byte
  wtime= HITIME;
  
  // it returns non-zero when we come back to app!
  // TODO: it seems to expect cursor to always
  //   be overwritten, what if gotoxy?
  if (wfocus==wcur) togglecursor();
  if (setjmp(winp->cont)) {
    // and we're back
    if (wfocus==wcur) togglecursor();
    return;
  }

  // if 0 then we're in OrWin scheduler!
  DEB('/');
  DKEY();

  longjmp(orwinjmp, 1);
}

// basically recurses doing stack allocations
// at end marks it in orwinnext, longjmp there
// with app main to call! It'll push ahead.
void spawn_alloc(char n) {
  //  unsigned int safe= 0x54FE; // check for overflow
  char dummy[SPAWN_STEP]= {0};

  DEB('.');

  //  memset(dummy, n, SPAWN_STEP);
  //  dummy[0]= 0;
  
  if (--n) spawn_alloc(n + dummy[0]);
  else {
    app main;
    // arrived at end of allocated stack
    DEB('!');
    //cprintf("@%u\r\n", &dummy);
    DEB('\n');
    DKEY();

    // mark current stack pointer as next allocation
    main= (void*)setjmp(orwinnext);
    if (!main) {

      // we've allocated the next spawn stack chunk
      return;

    } else {

      // make the stack reusable
      memcpy(winp->start, orwinnext, sizeof(orwinnext));
      
      // we got a function address to call (an app main)

      // alloate space for this process by moving orwinnext forward (for next allocation)
      spawn_alloc(SPAWN_REC);

      // clear rest of unused stack
      // clean the stack so can measure usage
      {
	char ss= winp->start[2];
	char so=    orwinjmp[2];
	char d= ss-so;
      
	memset((char*)0x100, 0, ss-1);
      }

      // run & exit
      winp->exit= main(-1, NULL);
      // TODO: reuse allocation winp->cont
  
      DEB('*');
      DKEY();

      // go back to scheduler
      longjmp(orwinjmp, 1);
    }
  } 

  n= 0; // make memory "clean"
}

// TODO: parameters
void spawn(app main) {
  if (!setjmp(orwinjmp))
    longjmp(orwinnext, (int)main);
}

#define IS_BAD_CONTRAST(fg, bg) ((0xB1 >> ((fg) ^ (bg))) & 1)

char overlap(char x, char y, char w, char h) {
  int i, j;

  if (x<2 || x+w >= 37) return 1;
  if (y<1 || y+h >= 28) return 1;
  
  for(j= y-2; j<y+h+4; ++j)
    for(i= x-3; i<x+w+5; ++i)
      if (*SCREENXY(i, j)!=126) return 1;
  return 0;
}

void newwin(char* title, app main) {
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

  winp->status= 1;
  winp->fun= (void*)main;
  // TODO: arg
  winp->state= main(0,0);
  
  //  wputc('('); wputi(x); wputc(','); wputi(y); wputc(')');
  //  wputi(w); wputc('x'); wputi(h);
  //  spawn(main);
  //cprintf("(%d,%d) %dx%d  ", x,y,w,h);
}

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
    // Idea, cut a +1 on all sides cutout from screen and just MOVE that
    // if not gray when expanding or moving there then, then abort
    char i, *t, *p, W= w+4+2+1, H= h+2+2;
    char *tmp, *s= SCREENXY(x-3, y-2);

    // make sure to have 1 line of gray around!
    if (x+dx < 3) return;
    if (x+w+dx >= 40-3) return;

    if (y+dy < 2) return;
    if (y+h+dy >= 28-1) return;

    // non-overlap, or directly adjacent
    // (make sure have gray line at edge)
    // TODO: too much code ! ?
    if (dy) {
      tmp= s + (dy<0? -40: 40*H);
      for(i= W; i--;)
	if (tmp[i] != 126) return;
    } else {
      tmp= s + (dx<0? -1: W);
      for(i= H; i--;) {
	if (*tmp != 126) return;
	tmp+= 40;
      }
    }
      
    // let's copy part to mvoe
    tmp= malloc(W*H);
    if (!tmp) return;

    // copy from screen to tmp (strided)
    p= s; t= tmp;
    for(i= H; i--;) {
      memcpy(t, p, W);
      p+= 40; t+= W;
    }

    // calculate new pos to read from
    t= tmp - dx;
    if (dy<0) t+= W;

    // copy back from tmp to screen (strided)
    p= s;
    if (dy>0) p+= 40;

    for(i= H; --i;) {
      memcpy(p, t, W+dx);
      p+= 40; t+= W;
    }

    // update it
    wf->x+= dx; wf->y+= dy;
    
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

// TODO: apps

int main() {
  int i= 0, j= 0, z= 0;

  // KBRPT - keyboard repeat rate
  *(char*)0x24f= 2;
  // KBDLY - keyboard delay before repeat
  *(char*)0x24e= 4;
  
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

  // Print logo in Right-Hand Corner
  strncpy(SCREENXY(34, 0), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 1), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 2),      " ATMOS", 6);

  memset(win, 0, sizeof(win));

  // initlize multitasker!
  spawn_alloc(SPAWN_REC);
  // make it "pseudo task"
  win[0].status= -1;

#define DEMO
#define TIMER
//#define ATMOS
//#define FISH
#define ECHO
#define ASCII2
//#define CALC
  
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

#ifdef ASCII2
  window( 2, 22, 14,  5, cyan,  red);
  wstatus(-1, "ASCII");
  spawn(ascii_main);
#endif
  
#ifdef ATMOS
  // Atmos
  window(22, 12, 14, 14, black, yellow);
  wstatus(-1, "File Edit Options Tools");
  //spawn(atmos_main);
  spawn(flipflop_main);
#endif ATMOS
  
#else

  // TODO: need at least one because allocations
  window(30,  5,  6,  5, blue, white);
  wstatus(-1, "ASCII");
  spawn(ascii_main);

#endif // DEMO
  


  //////////////////////////////////////////////////////////
  // SCHEDULER!
  wcur= nwin;
  wfocus= nwin;

  wdecorate();

  // make us always come back here!
  while(setjmp(orwinjmp)!=42) {
    DEB('^');

    // move next app
  next:
    if (!--wcur) wcur= nwin;
    setwin(wcur);

    // TODO: if no active windows/task will LOOP
    if (!winp->status) goto next;

    // Handle cheap tasks
    if (winp->fun) {
      // TODO: arg, keyevent? resize
      (*(app)winp->fun)((int*)winp->state, 0);
      goto next;
    }
    
    DEB('0'+wcur);
    DKEY();
    
    // TODO: how can I use it as a test value, LOL
    if (*winp->cont) longjmp(winp->cont, 1);
    else DEB('\\');
  }



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

void info() {
#ifdef INFO
  char i, s, *save= malloc(SCREENSIZE), *r, *p;
  unsigned int j;
  Window* w= win;

  if (!save) return;
  memcpy(save, TEXTSCREEN, SCREENSIZE);

#undef clrscr
  clrscr();

  for(i= 0; i<=nwin; ++i,++w) {
    cputc('\r'); cputc('\n');
    cput2h(i);
    cputc(i==wfocus? '!': i==wcur? '=': ' ');
    cput2h(w->status);
    cspc(); cput2h(w->exit);
    s= w->start[2];

#ifdef DEBUG
    p= r= printbuf((char*)&w->start);
    printbuf((char*)&w->cont);
#else
    cspc(); cput2h(s);
    cputc('-'); cput2h(w->cont[2]);
    p= r= (char*)*(unsigned*)&w->start;
#endif

    if (w->status) {
      // data stack
      char slnz= s, *lastnonzero= p;
#ifdef DEBUG
      cputc('\r'); cputc('\n'); cputc('=');
#endif
      for(j=0; j<BYTES; ++j)
	if (*--p) {
	  lastnonzero= p;
	  //cput2h(*p);
	}
	else
	  // cputc('.')
	  ;
      cspc(); cputc('S'); cputc(':'); cputd(r-lastnonzero);
      cputc('/'); cputd(BYTES);

      // R 6502 HardWare stack (page 1)
#if 0
      // print whole stack
      s= 255;
      p= (char*)0x200;
      for(j=256; --j; )
	if (*--p) {
	  slnz= j;
	  cput2h(*p);
	}
	else
	  cputc('.')
	  ;
      cgetc();
#else
      p= (char*)(0x100+s);
      for(j=s; j>s-SPAWN_REC*2+1; --j)
	if (*--p) {
	  slnz= j;
	  //cput2h(*p);
	}
	else
	  //cputc('.')
	  ;
#endif
      cspc(); cputc('R'); cputc(':'); cputd(s-slnz);
      cputc('/'); cputd(2*SPAWN_REC);
    }

#ifdef STATS
    cspc(); cputc('#'); cputd(w->nputc);
#endif     

  }
  cgetc();

  memcpy(TEXTSCREEN, save, SCREENSIZE);
  free(save);
#endif
}
