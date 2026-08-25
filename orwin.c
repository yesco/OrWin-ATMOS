#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <assert.h>

// Preferred Window Layout

#define NHORIZWIN  3
#define NVERTWIN 3

#define WMAX ((40-NHORIZWIN*5) / NHORIZWIN -1)
#define HMAX ((28-NVERTWIN*4) / NVERTWIN)

// TODO: magenta on blue - not sood good
#define IS_BAD_CONTRAST(fg, bg) ((0xB1 >> ((fg) ^ (bg))) & 1)

//char wputc(char);

//int putchar(int c) { wputc(c); return c; }

#include "orwin.h"


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

#include "apps.ext"

typedef unsigned int uint;

void start(){}

// Dummies

void* shell() {} // TODO: make it an app!
void* orwin() {}
void* SUMMARY() {}
void* CC65() {}

struct apps {
  char* name;
  void* fun;
  uint size;
} apps[] = {

  //#include "apps.reg"
  #include "orwin.reg"

  {0, 0}
};


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
  // strided
  while(h--) {
    memset(p, c, w);
    p+= 40;
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
Window win[WIN_MAX]= {
  { 40-15, 3, 15, 28-3,
    0, 0,
    NULL,
    black, white,
#ifdef STATS
    0,
#endif
    0, 0,
    0, 0}
};

Window* winp= 0;


char* updatewinptr() {
  return winp->p= SCREENXY(winp->x + winp->c, winp->y + winp->r);
}

#undef putchar
// inefficient, but should do the job
// TODO: rename wputc?
int putchar(int c) { return wputc(c); }

// 2x-10x faster not calling putchar for every char!
int write(int fd, char* buf, size_t count) {
  static uint n;
  static char c, left, *b, *p;

  n= count;
  b= buf; p= winp->p - 1; left= winp->w - winp->c;

  --b;
  while(n--) {
    if ((c= *++b) < 32 || !--left) {
      winp->p= p+1; putchar(c); p= win->p - 1; left= winp->w - winp->c;
    } else {
      *++p= c;
    }
  }
  winp->p= p+1;
  
  return count;
}


void wgotoxy(char x, char y) {
  winp->c= x<winp->w? x: winp->w;
  winp->r= y<winp->h? y: winp->h;
  updatewinptr();
}

void wscreensize(char* w, char* h) {
  *w= winp->w; *h= winp->h;
}

void wclreol() {
  memset(winp->p, ' ', winp->w - winp->c + 1);
}

void wclrscr() {
  char b= winp->bg | BG, f= winp->fg;
  char h, *p;

  fill(winp->x, winp->y, winp->w, h= winp->h, 32);
  // reset cursor position
  winp->c= winp->r= 0;
  updatewinptr();

// TODO: not good because SHADOW requires FILL
#if 0
  // set paper and ink
  p= winp->p - 2;
  ++h;
  while(--h) {
    *p= b; p[1]= f;
    p+= 40;
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

// minimal terminal codes
// Free codes: 11, 14; 24,25,26, 28,29,30,31
char wputc(char c) {
#ifdef STATS
  ++winp->nputc;
#endif
  
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
    
  case KEYRIGHT: winp->p++; break; // will reach column++
  case KEYDOWN:  if (winp->r++ +1 >= winp->h) winp->r= 0;          break;
  case KEYUP:    if (winp->r--    >= winp->h) winp->r= win->h-1;   break;

  // TODO: repeat char
  // vt100:     char ESC [ 70 b                   printf "=\e[79b\n"
  // tektronic: ESC ~ [count] [char]
  // heathkit:  CTRL-R [count+$1F] [char]
  // AppleII:   0x01 [Count Byte] [Character Byte]    

  // (Ultra-Compact):VT52 uses fixed-length,
  // binary-byte coordinate sequences, good for8-bit machine
  // ESC Y [Row+32] [Col+32]
																																																																									  
  default:
    if (c==*BLACK) c= black; // it's really 0 but...
    
    *winp->p++= c;

    // change INK or BG color for future
    c&= 0x7f;
    if (c<24) if (c<8) winp->fg= c; else winp->bg= c;

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
  //if (wcur)
  // not clear why higher value crawsh more easy?
  //if ((wtime^HITIME) & 0b1000000) yield();

  //  if ((wtime^HITIME) & 0b1000000) yield();

  //if ((wtime^HITIME) & 0b10000) yield();
  // 4th bit => 0..4095us
  //if ((wtime^HITIME) & 0b1000) yield();
  //if ((wtime^HITIME) & 0b100) yield(); // every 3 chars
  //if ((wtime^HITIME) & 0b10) yield(); // every 1.5 char
  //if ((wtime^HITIME) & 0b1) yield(); // every 1.5 char
  return c;
}

// TODO: is it bettter than write optimized?

// TODO: remove all!

#ifndef OPTPUTZ
void wputz(char* s) {
  write(1, s, strlen(s));
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
    //if (!--n) yield();

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

char newwin() {
  // TODO: reuse empty entries
  if (nwin==WIN_MAX) return 0;
  winp= win+ ++nwin;
}

char overlap(char x, char y, char w, char h) {
  int i, j;

  if (x<2 || x+w >= 41) return 1;
  if (y<1 || y+h >= 28) return 1;
  
  for(j= y-1; j<y+h+1; ++j)
    for(i= x-2; i<x+w+2; ++i)
      if (*SCREENXY(i, j) != 126) return 1;

  return 0;
}

// TODO: title+= bar?
char window(char x, char y, char w, char h, char bg, char fg) {
  char o;
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

void apprun();
void randnewwin(char* title, app main);

void mowin(signed char dx, signed char dy, signed char dw, signed char dh);
void task(app fun);

char wkey= 0;

#undef kbhit

// non-blocking
// (win=0 to not yield here as its called fom yield)
char wkbhit(char win) {
  char c, k, s;
  
  if (wkey && wfocus==win) return wkey;
  if (!kbhit()) return 0;
  
  c= cgetc(); k= *(char*)0x209; s= *(char*)0x208;

  // debug print key
  // ORIC ATMOS: ROM magical shift key $209
  // $38 - no key
  // $a2 - CTRL key
  // $a4 - SHIFT left
  // $a5 - FUNCT key
  // $a7 - SHIFT right
  sprintf(SCREENXY(40-3*3+1-2,27), "[%02x %02x %02x]", c, k, s);

  // remap ARROW KEYS: 8-11 to 28-31
  if (((c & 0b01111100) == 0) &&
      (s==0xac || s==0xbc || s==0xb4 || s==0x9c))
    c+= 28-8;

  // NOTE: CTRL ARROW not reflected in code

  // TODO: CTRL-M and RETURN are ambigous,
  // but RETURN probably should stay at 13, lol
  //if (c==13 && k==0xa2) c== ???

  // FUNCT || CTRL & ARROWKEY
  if (c&128 || (k==0xa2 && c>=28 && c<=31)) {
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
    case 'R': apprun(); break; // List
    case 'S': { char bg, fg;
      // pick colors w good contrast
      do {
	bg= rand() & 7;
	fg= rand() & 7;
      } while(IS_BAD_CONTRAST(fg, bg));
    
      task(app_ascii);
      window(-1, -1, WMAX, HMAX, bg, fg);
      wstatus(-1, "foo");
      break;
    }

    // TODO: case 'M': maximize & minimize
  
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

    default:  if (isdigit(c)) setfocus(c-'0');
    }
    wdecorate();
    return 0;
  }

  // save it
  wkey= c;
  return wkey && wfocus==win;
}

// blocking per app, but will yield

// TODO: don't allow to be called!

char wgetc(char win) {
  char c;
  
  while(1) {
    while(!wkbhit(win));
    if (wkey && wfocus==win) {
      c= wkey;
      wkey= 0;
      return c;
    }
  }
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

void task(app fun) {
  newwin();

  winp->status= 1;
  winp->fun= (void*)fun;
  winp->state= (void*)fun(0, 0); // TODO: arg (template)
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
    p+= 40; s+= 40;
  }
  return r;
}

void loadwin(char* p) {
  char h= winp->h, w= winp->w + 3, *r= p;
  char *s= SCREENXY(winp->x - 2, winp->y);
  while(h--) {
    memmove(s, p, w);
    p+= 40; s+= 40;
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
    //nl();
    puts("\nStart APP\n");
    //clnl();

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
    task(found->fun);
    //cprintf("\n\n\n\n\n[WIN.%d: %p %p]", wcur, winp->state, winp->fun);

    // if it didn't open a window
    if (wcur == w) {
      char bg, fg;

      // pick colors w good contrast
      do {
	bg= rand() & 7;
	fg= rand() & 7;
      } while(IS_BAD_CONTRAST(fg, bg));
    
      window(-1, -1, WMAX, HMAX, bg, fg);
      wstatus(-1, found->name);
    }

  }  else {
    setwin(w);
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

  assert(sizeof(void*)==sizeof(int));

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

  // Print logo in Right-Hand Corner
  strncpy(SCREENXY(34, 24), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 25), "\x0a""0rWin", 6);
  strncpy(SCREENXY(34, 26),      " ATMOS", 6);

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

  //memset(win, 0, sizeof(win));

  updatewinptr();
  
  // initlize multitasker!
  spawn_alloc(SPAWN_REC);
  // make it "pseudo task"
  win[0].status= -1;

  //#define DEMO
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
  task(ascii_main);
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
  window(39-16-2, 4, 16, 10, red, yellow);
  wstatus(-1, "Snow");
  task(app_snow);
#endif

  // Need one to make it run!
  task(app_ascii);
  window(2, 1, 5, 5, blue, white);
  wstatus(-1, "ASCII");
  
  //  task(app_ascii);
  //  window(5, 18, 8, 5, green, black);
  //  wstatus(-1, "ASCII");
  
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
    wkbhit(0);

    if (!--wcur) wcur= nwin;
    
    setwin(wcur);

    // TODO: if no active windows/task will LOOP
    if (!winp->status) goto next;

    // Handle cheap tasks
    if (winp->fun) {
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

