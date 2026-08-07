#include <stdio.h>
#include <string.h>
#include <conio.h>

#define WIN_MAX 16

#define SPAWN_REC 16
#define SPAWN_STEP 32

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

#include "orwin.h"

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

  jmp_buf cont;
  char exit;
} Window;

char nwin= 0;
Window win[WIN_MAX], *winp;



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
    else  if (winp->r) winp->r--,winp->c= winp->w-1;
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

    // set current (new) colors
    *SCREENXY(winp->x-2, winp->y + winp->r)= BG | winp->bg;
    *SCREENXY(winp->x-1, winp->y + winp->r)=      winp->fg;

    wclreol();
    updatewinptr();
  }
 done:
  return c;
}

void wputs(char* s) {
  while(*s) wputc(*s++);
}

void wputi(int i) {
  char s[10]= {0};
  sprintf(s, "%d", i);
  wputs(s);
}

void wstatus(signed char c, char* s) {
  char* p= winp->y * 40 + winp->x + c + TEXTSCREEN - 40;
  char w= winp->w + 2 + 1;
  while(*s && w--) *p++= *s++ ^ 128;
}
	     
void setwin(char w) {
  // TODO: set curwin?
  winp= win+w;
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
  // TODO: verify space/clash/overflow?
  
  // header
  // TODO: only when active
  fill(x-2, y-1, w+4, 1, 127); // white block

  // shadow (resets BG to BLACK)
  fill(x-1, y, w+4, h+1, BG+BLACK);

  // text area background
  wclrscr();

  // set text color after! (TODO: require "spacing" between frames
  fill(x+w+1, y, 1, h, WHITE);

  return nwin;
}

char yield() {
  DEB('?');

  // it returns non-zero when we come back to app!
  if (setjmp(winp->cont)) return 1;

  // if 0 then we're in OrWin scheduler!
  DEB('/');
  DKEY();

  longjmp(orwinjmp, 1);
}

typedef int (*app)();

void spawn_alloc(char n) {
  char dummy[SPAWN_STEP]= {0};
  char var= n;
  app main;

  DEB('.');

  if (n) spawn_alloc(n-1 + dummy[0]);
  else {
    // arrived at end of allocated stack
    DEB('!');
    cprintf("@%u ", &dummy);
    DEB('\n');
    DKEY();

    // mark current stack pointer as next allocation
    main= (void*)setjmp(orwinnext);
    if (!main) {

      // we've allocated a chunk
      return;

    } else {

      // we got a function address to call (an app main)

      // alloate space for this process by moving orwinnext forward (for next allocation)
      spawn_alloc(SPAWN_REC);

      // run & exit
      winp->exit= main();
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

extern void counter_loop();
extern void atmos_loop();
extern void ascii_loop();

extern int counter_main();
extern int ascii_main();
extern int atmos_main();


// TODO: apps

int main() {
  int i= 0, j= 0, z= 0;

  int napp;
  
  // clear background to "gray" checkerboard
  fill(0, 0, SCREENCOLS, SCREENROWS, 126);

  // Print logo in Right-Hand Corner
  strncpy(SCREENXY(34, 0), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 1), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 2),      " ATMOS", 6);

  // initlize multitasker!
  spawn_alloc(SPAWN_REC);

  window( 5,  2, 23,  7, GREEN, BLACK);
  wstatus(-1, "Counter");
  spawn(counter_main);

  window( 5, 13,  7,  7, BLUE,  WHITE);
  wstatus(-1, "ASCII");
  spawn(ascii_main);

  window(20, 12, 14, 14, BLACK, YELLOW);
  wstatus(-1, "File Edit Options Tools");
  spawn(atmos_main);
 
  napp= nwin;

  // make us always come back here!
  while(setjmp(orwinjmp)!=42) {
    DEB('^');

    // move next app
    if (!--napp) napp= nwin;
    setwin(napp);
    DEB('0'+napp);
    DKEY();
    
    if (*winp->cont) longjmp(winp->cont, 1);
    else DEB('\\');
  }

  return 0;
}
