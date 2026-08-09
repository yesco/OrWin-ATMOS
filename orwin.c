#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>

// (- 10719 8975) = 1744 bytes code for mowin :-(
#define MOWIN

extern int counter_main(int argc, char** argv);
extern int ascii_main(int argc, char** argv);
extern int atmos_main(int argc, char** argv);
extern int echo_main(int argc, char** argv);

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
#define SPAWN_REC 20

// SPAWN_STEP*SPAWN_REC is Data stack allocation
//#define SPAWN_STEP 15
#define SPAWN_STEP 30

typedef int (*app)();

//#define TRACE

#ifdef TRACE
  #define DEB(c) cputc(c)
  #define DKEY() cgetc()
#else
  #define DEB(c) 
  #define DKEY() 
#endif


#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F
#define SCREENROWS 28
#define SCREENCOLS 40
#define SCREENSIZE (SCREENROWS*SCREENCOLS)
#define SCREENLAST (TEXTSCREEN+SCREENSIZE-1)

// TODO: make my own interrupt timer!
#define HITIME (*(unsigned char*)0x305)

// hi byte of timer at yield
char wtime= 0;


#include "orwin.h"

//////////////----------------------------------------
#define HELP "FUNCT-3 spc Prev Next Toggle Run Kill"

jmp_buf orwinjmp;
jmp_buf orwinnext;

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

  char status;
  jmp_buf start, cont;
  char exit;
} Window;

char nwin= 0, wfocus= 0, wcur= 0;
Window win[WIN_MAX];
Window* winp= 0;


void updatewinptr() {
  winp->p= SCREENXY(winp->x + winp->c, winp->y + winp->r);
}

void wgotoxy(char x, char y) {
  winp->c= x;
  winp->r= y;
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

// minimal terminal codes
// Free codes: 11, 14; 24,25,26, 28,29,30,31
char wputc(char c) {
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
    winp->c= 0;
    // overflow rows?
    if (winp->r++ +1 >= winp->h) winp->r= 0;

    // yield at new line (minimize jitter/zig)
    yield();

    // set current (new) colors
    *SCREENXY(winp->x-2, winp->y + winp->r)= BG | winp->bg;
    *SCREENXY(winp->x-1, winp->y + winp->r)=      winp->fg;

    wclreol();
    updatewinptr();
  }

 done:
  // TODO: somehow here yield() crashes!

  // 4th bit => 0..4095ms
  //  if ((wtime^HITIME) & 0b1000) yield();
  return c;
}

void wputs(char* s) {
  while(*s) wputc(*s++);
  // good time to release, minimic terminal avoid jitter
  yield();
}

void wputi(int i) {
  char s[10]= {0};
  sprintf(s, "%d", i);
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
  fill(w->x-1, w->y, w->w+4, w->h+1, BG+BLACK);

  wclrscr();

  // set text color after
  fill(w->x + w->w +1, w->y, 1, w->h, WHITE);
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
  char c;
  
  if (wkey && wfocus==win) return wkey;
  if (!kbhit()) {
    if (win) yield();
    return 0;
  }
  
  c= cgetc();
  sprintf(TEXTSCREEN, "[%d]", c);

  if (c&128) {
    //cprintf("  #%d '%c' ", c, c&0x7f);
    wdecorate();
    // Capture FUNC keys Window Keys
    c^= 128;
    switch(toupper(c)) {
    case 'N': case ' ': setfocus(wfocus+1); break;
    case 'P':           setfocus(wfocus-1); break;
    case 27 : case 'I': setfocus(wprev);    break; // toggle 
    case 127: case 'Q': case 'K': winkill(); setfocus(wfocus+1); break; // Kill

    case 13 :
    case 'R': newwin("foo", ascii_main); break; // List

    case 'L': info(); break;
    case 'H': help(); break;

#ifdef MOWIN
    case   8: mowin(-1,0,0,0); break;
    case   9: mowin(+1,0,0,0); break;
    case  10: mowin(0,+1,0,0); break;
    case  11: mowin(0,-1,0,0); break;
      // TODO: conflicting, conflat with SHIFT/CTRL
    case 'W': mowin(0,0,+1,0); break;
    case 'S': mowin(0,0,-1,0); break;
    case 'T': mowin(0,0,0,+1); break;
    case 'Z': mowin(0,0,0,-1); break;
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

char yield() {
  DEB('?');

  // Handle keyboard, if there are no keyboard apps
  if (kbhit()) wkbhit(0);
  
  // snapshot hi byte
  wtime= HITIME;
  
  // it returns non-zero when we come back to app!
  if (wfocus==wcur) togglecursor();
  if (setjmp(winp->cont)) {
    // and we're back
    if (wfocus==wcur) togglecursor();
    return 1;
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
  char dummy[SPAWN_STEP]= {0};
  unsigned int safe= 0x54FE; // check for overflow
  app main;

  DEB('.');

  if (n) spawn_alloc(n-1 + dummy[0]);
  else {
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

      // run & exit
      winp->exit= main(-1, NULL);
      // TODO: reuse allocation winp->cont
  
      DEB('*');
      DKEY();

      // go back to scheduler
      longjmp(orwinjmp, 1);
    }
  } 
}

// TODO: parameters
void spawn(app main) {
  if (!setjmp(orwinjmp))
    longjmp(orwinnext, (int)main);
}

#define IS_BAD_CONTRAST(fg, bg) ((0xB1 >> ((fg) ^ (bg))) & 1)

char overlap(char x, char y, char w, char h) {
  int i, j;

  if (x<2 || x+w >= 38) return 1;
  if (y<1 || y+h >= 28) return 1;
  
  for(j= y-1; j<y+h+2; ++j)
    for(i= x-2; i<x+w+3; ++i)
      if (*SCREENXY(i, j)!=126) return 1;
  return 0;
}

void newwin(char* title, app main) {
  char x, y, w, h, bg, fg;

 again:
  do {

    do {
      x= (rand() % (40-3-4-2)) + 2;
      y= (rand() % (28-4-3-1)) + 1;
      w= (rand() % (30-x/2-3-3)) + 3;
      h= (rand() % (20-y/2-3-4)) + 4;
    } while(overlap(x, y, w, h));

    do {
      bg= rand() & 7;
      fg= rand() & 7;
    } while(IS_BAD_CONTRAST(fg, bg));

  } while(0);
  
  window(x, y, w, h, bg, fg);
  wstatus(-1, title);
  //  wputc('('); wputi(x); wputc(','); wputi(y); wputc(')');
  //  wputi(w); wputc('x'); wputi(h);
  //  spawn(main);
  cprintf("(%d,%d) %dx%d  ", x,y,w,h);
  cgetc();
  goto again;
}

// TODO: title not moved...
// TODO: colors messed up...
//   (because rewrite doesnpt restore fb bg only r c)
void mowin(signed char dx, signed char dy, signed char dw, signed char dh) {
#ifdef MOWIN
  Window* wf= win+wfocus;
  char x= wf->x, y= wf->y, w= wf->w, h= wf->h, b= wf->bg, f= wf->fg;
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
#endif // MOWIN
}

// TODO: apps

int main() {
  int i= 0, j= 0, z= 0;

  int napp;
  
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

#define DEMO
#define FISH
  
#ifdef DEMO
  // OK
  // window( 3,  2, 23,  7, GREEN, BLACK);
  // CRASH: smaller than 23 crash+++
  // window( 3,  2, 22,  7, GREEN, BLACK);
  // ok
  window( 2,  2, 23,  7, GREEN, BLACK);
  wstatus(-1, "Counter");
  spawn(counter_main);

#ifdef FISH
  window( 4, 12, 11,  7, BLUE,  WHITE);
  wstatus(-1, "ECHO");
  spawn(echo_main);

  window(22, 12, 14, 14, BLACK, YELLOW);
  wstatus(-1, "File Edit Options Tools");
  spawn(atmos_main);
#endif 

  window( 2, 22, 14,  5, CYAN,  RED);
  wstatus(-1, "ASCII");
  spawn(ascii_main);

  window(31,  5,  6,  5, WHITE, BLUE);
  wstatus(-1, "ASCII");
  spawn(ascii_main);

#endif // DEMO
  

  // done setup
  wdecorate();


  ///////////////////////////////////
  // SCHEDULER!
  napp= nwin;

  // make us always come back here!
  while(setjmp(orwinjmp)!=42) {
    DEB('^');

    // move next app
  next:
    if (!--napp) napp= nwin;
    setwin(napp);
    // TODO: if no active windows will go on 
    if (!winp->status) goto next;
    
    DEB('0'+napp);
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

void printbuf(char* j) {
  cputc(' ');
  //  cputd(j[2]);
  // cputd(*(unsigned*)j);
  // cputd((((unsigned int)j[3])<<8) | j[4]);
}

void info() {
  char i, *save= malloc(SCREENSIZE);
  Window* w= win;

  if (!save) return;
  memcpy(save, TEXTSCREEN, SCREENSIZE);

  clrscr();

#undef clrscr
  for(i= 0; i<WIN_MAX; ++i,++w) {
    cputc('\r'); cputc('\n');
    cput2h(i); cputc(' '); cput2h(w->status);
    cput2h(w->exit);
    printbuf((char*)&w->start);
    printbuf((char*)&w->cont);
  }
  cgetc();

  memcpy(TEXTSCREEN, save, SCREENSIZE);
  free(save);
}
