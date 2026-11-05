// OrWIN ATMOS - Window System "unix" for ORIC ATMOS
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// An experimental C-based retro text-based windowing system and
// multitasking environment for the ORIC ATMOS.
//
// see README.md for documentation!
//
// cc65 size-tuned version. Suggested flags:
//   cl65 -t atmos -Oirs --codesize 200 ... ???
// (do NOT use -Cl: wputc() is recursive)

// (- 35910 34696) 1241 bytes "saved" but mess w comments



// TODO: this should really go to an oric-oscar-crt.c file!
#ifdef OSCAR64

#ifdef ORIC

int main(void);

// "magical" address holders for oscar64, define only once
// Note: they aren't really variables, defined twice loose value!
void StackStart, StackEnd, BSSStart, BSSEnd, CodeStart, CodeEnd, ZeroStart, ZeroEnd;
void HeapStart, HeapEnd;

#pragma section(code,     0x0000, CodeStart,  CodeEnd)
#pragma section(stack,    0x0000, StackStart, StackEnd)
#pragma section(bss,      0x0000, BSSStart,   BSSEnd)
#pragma section(zeropage, 0x0000, ZeroStart,  ZeroEnd)
#pragma section(heap,     0x0000, HeapStart,  HeapEnd)

#pragma section(startup, 0)
#pragma region(startup, 0x0500, 0x0520, , , {startup})
#pragma region(zeropage, 0x00, 0x80, , , {zeropage})
#pragma region(main, 0x0520, 0xB200, , , {code, data, bss, heap, stack})
#pragma heapsize(0x1000)
#pragma stacksize(0x1000)

// TODO: oscar64 - replace w real from crt
#ifdef SETUPMALLOC
__asm crt_malloc {
  lda #0 
  sta accu
  sta accu+1
  rts
}
__asm crt_free   {
  rts
}

#pragma runtime(malloc, crt_malloc)
#pragma runtime(free, crt_free)
#endif // SETUPMALLOC

#ifdef STARTUP

__asm startup {
ldx     #$ff
        txs
        sei

        lda     #<StackEnd-2
        sta     __sp
        lda     #>StackEnd-2
        sta     __sp+1

        /// TODO: (?) optional BSS clear using BSSStart..BSSEnd
        jsr     main
hang:   jmp     hang
}

#pragma startup(startup)

#endif // STARTUP

//#define OVERRIDEMAIN

#ifdef OVERRIDEMAIN
int main(void)
{
    *(char *)0xbb80 = '!';
    for (;;) ;
    return 0;
}
#endif // OVERRIDEMAIN

#endif // ORIC


#endif // OSCAR64


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>

// Hmmm; ATMOS override write
#if !defined(__ATMOS__) && !defined(OSCAR64)
#include <unistd.h> // write
#endif

#include <assert.h>

char wputc(char c);

#define HELPTEXT "FUNCT-3 Prev spc Next List Run Kill Shll"



#include "orwin.h"

// Each file sets FOOBAR if they provide foobar()
// Otherwise, "fillins.c" will fill them in!

#include "misc.c"




#ifdef OSCAR64

  void* my_malloc(unsigned int size);
  void  my_free(void* p);

  // enable for simple see where it crashed Trace
  #if 1
    #define T(c) (*(volatile char*)SCREENXY(39,27)= (c))
  #else
    #define T(c) 0
  #endif

  // This one have two targets depending on 
  // if ORIC is set! If not ORIC it's drawing
  // in memory and refreshing an ANSI screen!
  // two targets depending on ORIC variable
  #include "oscar64.c"

#else

  #define T(c) (void)0

  #ifdef __CC65__
    #ifdef __ATMOS__
      #include "oric.c"
    #else
      #include "sim6502.c"
    #endif
  #else
    #include "unix.c"
  #endif

#endif // !OSCAR64

// Any missing routines will be filled in
#include "fillins.c"



// Preferred Window Layout

#define NHORIZWIN  3
#define NVERTWIN 3

#define WMAX ((SCREENCOLS-NHORIZWIN*5) / NHORIZWIN -1)
#define HMAX ((SCREENROWS-NVERTWIN*4) / NVERTWIN)

#define MOWIN 
#define OPTMOV

// optimized version
// TODO: make putz default and putc call it?

// TODO: wclreol doesn't know position
#define OPTPUTZ

// TODO: see apprun.c !

#include "apps.ext"

typedef unsigned int uint;

// Dummies, these are never called, satisfy orwin.reg

#define shell 0
#define orwin 0
#define SUMMARY 0
#define CC65 0

struct App {
  const char* name;
  void* fun;
  uint size;
} apps[] = {
  #include "orwin.reg"
  {0, 0}
};

// Process Stack allocation sizes
// ==============================
// SPAWN_REC is a measure of Hardware stack allocation
// FUNCT-List needs 24 rets! (48 bytes)
// SPAWN_STEP*SPAWN_REC is Data stack allocation

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


#define SPAWN_REC 25 
#define SPAWN_STEP 8

// to run everywhere, seems we are "deepP
// cprintf was taking too much, cputd is 6 recursive
// TODO: nested yields?
#define BYTES (SPAWN_REC*(SPAWN_STEP+1))

//#define TRACE

#ifdef TRACE
  #define DEB(c) cputc(c)
  #define DKEY() cgetc()
#else
  #define DEB(c) 
  #define DKEY() 
#endif // TRACE

// hi byte of timer at yield
char wtime= 0;


//////////////----------------------------------------


char nwin= 0, wfocus= 0, wcur= 0, wnext, *wret;

Window wins[WIN_MAX]= {
  { SCREENCOLS-15, 3, 14, SCREENROWS-3,
    0, 0,
    NULL,
    black, white,
    0,
    0, 0,
    0, 0}
};

Window* winp= wins;


char* wname(char winid) {
  void* fun= wins[winid].fun;
  struct App* p= apps;
  
  while(p && (p->fun != fun)) ++p;

  return p? (char*)p->name: NULL;
}


////////////////////////////////////////////////////////////
// Heap stuff

#undef malloc
#undef calloc
#undef realloc
#undef free

// TODO: if defined(ORICLOCI) 
#if defined(OSCAR64)

static char *brk = 0;

// Simple bump-allocator to get started
void* my_malloc(unsigned size) {
  char *r;

  if (!brk) brk= &HeapStart;
  //size = (size + 1) & ~1u; align to even - 6502 no care
  if (brk + size > &HeapEnd) {
    T('E');
    done: goto done;
  }

  r = brk;
  brk+= size;
  return r;
}

void my_free(void* p) {
  (void)p;
}

void* walloc(size_t z) {
  if (z) ++winp->nalloc;
  return my_malloc(z);
}

void* wcalloc(size_t z, size_t n) {
  size_t bytes= z*n;
  char* r= my_malloc(bytes);
  if (bytes) ++winp->nalloc;
  memset(r, 0, bytes);
  return r;
}

void* wrealloc(void* p, size_t z) {
  char* r= my_malloc(z);
  if (z && !p) ++winp->nalloc;
  memcpy(r, p, z);
  my_free(p);
  return r;
}

void wfree(void* p) {
  if (p) --winp->nalloc;
  my_free(p);
}

#else
// CC65 use the normal malloc

void* walloc(size_t z) {
  if (z) ++winp->nalloc;
  return malloc(z);
}

void* wcalloc(size_t z, size_t n) {
  if (z && n) ++winp->nalloc;
  return calloc(z, n);
}

void* wrealloc(void* p, size_t z) {
  if (z && !p) ++winp->nalloc;
  return realloc(p, z);
}

void wfree(void* p) {
  if (p) --winp->nalloc;
  free(p);
}

#endif // OSCAR64

#define wmalloc
#define wcalloc
#define wrealloc
#define wfree

char* updatewinptr() {
  // oscar64 error ; expected
  // term starts w illegal token = 
  // funcoin expected for call
  return (winp->p= SCREENXY(winp->x + winp->c, winp->y + winp->r));
}

// Replace the default; makes printf etc work transparently!
// (cannot override for target: sim6502... putchar enough?)
#ifdef __ATMOS__

// Replace the default write; makes printf etc work transparently!
int write(int fd, char* buf, size_t count) {
  // 2x-10x faster not calling putchar for every char!

  // TODO: I think buggy!
  // Replace the default write; makes printf etc work transparently!
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

#endif // !WRITE

#endif // __ATMOS__


void wgotoxy(char x, char y) {
  winp->c= x<winp->w? x: winp->w;
  winp->r= y<winp->h? y: winp->h;
  updatewinptr();
}

void wscreensize(char* w, char* h) {
  *w= winp->w; *h= winp->h;
}

void wclreol() {
  // +1 clears last pos in ASCII
  memset(winp->p, ' ', winp->w - winp->c + 1);
}

// (the old initial fill(..,'x') was fully overwritten by the
//  attribute fill below, so it's removed)
void wclrscr() {
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


// minimal terminal codes
// Free codes: 11, 14; 24,25,26, 28,29,30,31
//
// Caching winp in a register var (zeropage) saves ~5 bytes/use
#pragma register-vars(push, on)
char wputc(char c) {
  register Window* w= winp;
  static char lastcolor;
      
  ++w->nputc;
  
  switch(c) {

  case 127: wputc(8); wputc(' '); // fall-through
  case KEYLEFT: 
  case 8: if (w->c) w->c--;  // CTRL-H = \b - BS - ^h
    else  if (w->r) w->r--,w->c= w->w;
    updatewinptr();
    goto done;
  // (9) Tab 8 forward
  case '\t': if ((w->c= ((w->c + 8) & 0xf8)) > w->w) {
      c=10; break; } else updatewinptr(); goto done;

  case 24: // CTRL-X  FLSHLINE: NL if "needed" (not "first" pos)
    if (w->c) { updatewinptr(); goto done; }
    // NL: fall-through
  case 10: nl(); goto done;     // CTRL-J = \n & CLREOL
  case 11: nlpure(); goto done; // CTRL-K = \n NO clreol

  case 12: wclrscr(); goto done;     // CTRL-L = CLRSCR
  case 13: w->c= 0; updatewinptr(); goto done; // CTRL-M = \r = CR

  // ASCII: ShiftOut (SO) - ALT font set - ORIC?
  // (key unix: FLUSH0 togggle ignore output) - ORIC has it!
  case 14: wclreol(); goto done;  // CTRL-N CLREOL
  case 15: goto done;             // CTRL-O

  // HIBIT SET
  case 128+12: gotoxy(0,0); goto done; // HOME
  case 128+13: clnl();      goto done; // CLNL

  case 25: break; // CTRL-Y (EM end medium)  - TODO: indent--
  case 26: break; // CTRL-Z (SUB substitute) - TODO: indent++
  case 27: break; // ESC, TODO: understood by puts maybe
    //case 0...7:  winf->fg= c; break;   // 0-7   : inc
    //case 16...23: winp->bg= c; break; // 16-23 : paper

  // All other codes are oric attributes (color/blink)
  // https://notes.burke.libbey.me/ansi-escape-codes/


  case KEYRIGHT: w->p++; break; // will reach column++
  case KEYDOWN:  if (++w->r >= w->h) w->r= 0;  updatewinptr(); goto done;
  case KEYUP:    if (w->r-- >= w->h) w->r= w->h-1;  updatewinptr(); goto done;

  // Hazeltine:   CTRL-Y rawX rawY
  // vt100:       ESC [ 2 J - clear screen
  // emacs,shell:  CTRL-K ill till end of line
    
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

  // All other codes are oric attributes (color/blink)
  default:
    if (c==*BLACK) c= black; // it's really 0 but...
    // change INK or BG color for future, like ANSI!
    if (c<24) {
      if (c<8) {
        // ink - use line attribute
        w->fg= c & 0x7f;
        if (!w->c) { lastcolor= w->p[-1]= c; goto done; }
      } else if (c>=16) {
        // paper - use line attribute
        w->bg= c & 0x7f;
        if (!w->c && lastcolor > 24)
          { lastcolor= w->p[-2]= c & 0x7f; goto done; }
      }
      // or fall-through use a position on screen
    } else lastcolor= 255;
    
    *w->p++= c;
  }

  // newline / line wrap?
  if (c==10 || w->c++ >= w->w) {
    char* p;
    
    w->c= 0;
    // overflow rows?
    if (w->r++ +1 >= w->h) w->r= 0;

    p= updatewinptr();

    // set current (new) colors
    p[-2]= BG | w->bg;
    p[-1]=      w->fg;

    wclreol();
  }

 done:
  return c;
}
#pragma register-vars(pop)


#ifndef OPTPUTZ

void wputz(char* s) {
  write(1, s, strlen(s));
}

#else

// Fast path for strings (the non-OPTPUTZ variant was removed)
void wputz(const char* s) {
  char c, r, *p, k, w= winp->w, h= winp->h;
  unsigned int nputc= winp->nputc;

 restart:
  c= winp->c, r= winp->r, p= winp->p - 1;
  --s;

  while((k= *++s)) {
    // handle special chars
    if ((k & 0x7f) < 32) break;

    *++p= k;
    ++nputc;
    // wrap line
    if (c++ >= w) {
      winp->c= c= 0;
      if (++r >= h) r= 0;
      winp->r= r;
      p= updatewinptr()-1;
    }
  }
  
  winp->c= c; winp->p= p+1;
  winp->nputc= nputc;
               
  if (k) { wputc(k); ++s; goto restart; }
}

#endif // OPTPUTZ

// wputz instead of strlen+write
void wputs(const char* s) {
  // ALT: write(1, (char*)s, strlen(s));
  wputz(s);
  nl();
}

// itoa is tiny compared to sprintf (which drags in all of printf)
void wputi(int i) {
  char s[8];
  itoa(i, s, 10);
  wputs(s);
}

//TODO: use instead of printf?
//
//void cput1h(char x) { x&= 0xf; cputc(x + (x<10? '0': 'A'-10)); }
//void cput2h(char x) { cput1h(x>>4); cput1h(x); }
//void cput4h(unsigned int x) { cput2h(x>>8); cput2h(x&0xff); }
//void cputd(unsigned int d) { if (d>=10) cputd(d/10); cputc('0'+(d % 10)); }
//
//void cspc() { cputc(' '); }

void wstatus(signed char c, const char* s) {
  char* p= SCREENXY(winp->x + c, winp->y - 1);
  char w= winp->w + 2 + 1;

  // once 0x80 was a var xor for some reason...
  while(*s && --w) *p++= *s++ ^ 0x80;

  p[w-1]= ('0' | 128) + nwin;
}
             
// (un)decorate wfocus (toggle hi-bit)
void wdecorate() {
  Window* w= wins + wfocus;
  // Invert header (make it black if focused)
  char* p= SCREENXY(w->x - 2, w->y - 1);
  char i;

  if (!w->status) return;

  for(i= w->w+4; i--; ) p[i]^= 128;
}

void setwin(char w) {
  winp= wins + w;
  wcur= w;
}

char wprev= 1;

void setfocus(signed char new) {
  char n= 0;
  if (wfocus) wdecorate(); // show not active

 next:
  wprev= wfocus;
  wfocus= new;
  if (new > nwin) wfocus= 1;
  if (new <= 0)   wfocus= nwin;
  if (n++ > WIN_MAX) return; // make sure not hang!
  if (!wins[wfocus].status) { new= wfocus+1; goto next; }

  if (wfocus) wdecorate(); // show active
}

char newwin() {
  // TODO: reuse empty entries
  if (nwin==WIN_MAX) return 0;
  setwin(++nwin);
  setfocus(nwin);
  return nwin;
}


void winerase(Window* w) {
  fill(w->x-2, w->y-1, w->w+5, w->h+3, 126);
}

void winkill() {
  Window* w= wins + wfocus;

  // TODO: make all safe
  // dorun(CLEANUP);
  
  // merge state w ret+

  free(w->state);
  free(w->args);
  lfree(w->ret);

  winerase(w);
  bzero(w, sizeof(*w));
  
  setfocus(wfocus); // goes next
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


// Statistics for IDLE %CPU %MEM
//
// 16-bit tick counters: differences stay correct modulo 65536,
// and avoid 32-bit (clock_t) arithmetic helpers.
// runsum/timesum stay 32-bit as they are multiplied by 100.
// NOTE: if other files `extern` these, update their types!

uint latency, lastlatency;
uint trun, rounds, runprocs, lastupdate;
clock_t runsum, timesum;

// Run and accumulate time/ticks/statistics
void dorun(char* line) {
  uint t0= (uint)clock();
  wtime= HITIME;
  winp->ret= wret= (char*)((*winp->fun)(winp->state, line));
  winp->ticks+= trun= (uint)clock()-t0;
  winp->cpu= trun*rounds;
  runsum+= (trun<<3) + 1;
  ++runprocs;
}

// start/exec process w parameters (line) in current window
// TODO: rename to exec?
void startline(runptr fun, const char* line) {
  winp->status= 1;
  winp->args= strdup(line);
  
  winp->fun= (void*)fun;
  dorun((char*)line);

  winp->state= winp->ret;
  winp->ret= NULL;
}

void start(runptr fun) { startline(fun, 0); }


// requires all gray in the area around (MOWIN layout only,
// the old #else variant was removed)
char overlap(char x, char y, char w, char h) {
  char i, j;

  if (x<3 || x+w >= SCREENCOLS-5) return 1;
  if (y<3) return 1;
  
  for(j= y-2; j<y+h; ++j)
    for(i= x-3; i<x+w+4; ++i)
      if (*SCREENXY(i, j) != 126) return 1;
  return 0;
}


// TODO: magenta on blue - not sood good
#define IS_BAD_CONTRAST(fg, bg) ((0xB1 >> ((fg) ^ (bg))) & 1)

// Draw a window in current process/window slot
//
// If draw before start()/startline() then it
// overrides the default that the app may use
// by it's own call to window()!
//
// X  Y  == -1 (255): automatic placement
// FG BG == -1 (255): automatic "good contrast" colors
// W     == -1: TODO: automatic size
//
// Note: (can only be called once)
//
// TODO: currently, if given exact coordinates, doesn't check...
//   possibly, it could save what's under and not allow switching
//   until have free area == becomes modal, or can be minimized
//
// TODO: 6 args = 5 pusha per call-site. If call-sites matter more
// than speed, consider passing a const struct/array pointer.

// TODO: actually it's 6 bytes, "constant"
char window(
  signed char x,  signed char y,
  signed char w,  signed char h,
  signed char bg, signed char fg)
{
  char o;

  // already drawn/configure
  if (winp->w) return wcur;
  
  // placer
  if (x == -1 || y == -1) {
    x= 2; y= 1;
    while((o=overlap(x, y, w, h)) && y<29-h) {
      if (++x + w + 4 > 39) { ++y; x= 2; }
    }
    if (o) return 0;
  }
  winp->x= x;
  winp->y= y;

  winp->w= w;
  winp->h= h;

  if (bg == -1 || fg == -1) {
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
    
  // TODO: verify space/clash/overflow
  windraw(winp);
  
  return nwin;
}

void apprun();
void randnewwin(char* title, runptr main);

void mowin(signed char dx, signed char dy, signed char dw, signed char dh);
void task(runptr fun);

char wkey= 0;

#undef kbhit

// non-blocking test if key hit for the given winnumber
char wkbhit(char win) {
  char c;
  
 again:
  if (wkey && wfocus==win) return wkey;
  if (!kbhit()) return 0;
  
  c= mygetc();

  // == FUNCT || CTRL & ARROWKEYS!
  if (c&128) {
    // Capture FUNCT keys Window Keys
    c^= 128;
    switch((c= toupper(c))) {
    case ' ':
    case 'N': setfocus(wfocus+1); break;
    case 'P': setfocus(wfocus-1); break;
    case 27 : setfocus(wprev);    break; // toggle 

    case 127:
    case 'K': winkill(); setfocus(wfocus+1); break; // Kill

    case 13 :
    case 'R': apprun(); break; // List
    case 'I': newwin(); start(app_heap); break; 
    case 'S':
    case 'T': { // Shell/Terminal
      newwin();
      window(1, -1, 20-7, 10, green, black);
      start(app_sh);
      break; }

    // TODO: case 'M': maximize & minimize
  
    case 'L': { // ps
      newwin();
      window(3, 17, 40-7, 10, yellow, black);
      startline(app_sh, "ps");
      break; }
    case 'H': help(); break;

    // moving using FUNCT & ARROWKEYS
    case KEYLEFT : mowin(-1,0,0,0); goto again;
    case KEYRIGHT: mowin(+1,0,0,0); goto again;
    case KEYDOWN : mowin(0,+1,0,0); goto again;
    case KEYUP   : mowin(0,-1,0,0); goto again;

#ifdef MOWIN
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
  return (wkey= 0);
}


// Pick app to run
//
// TODO: generalize the picker and move out
void apprun() {
  // TODO: common buff somewhere
  char line[40]= {0};
  char i= 0, w= wcur;
  struct App *p, *found;
  char c, k, *spc, *tmp;

  // TODO: remove? test! flush (FUNC-R was getting an r)
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
      
      //wputc('\t'); wputi(p->size); // +19cs
      wputz(p->name);  // 7 cs !!! (printf 55 cs!)

      putchar(white);
      //' TODO: nl makes it flickr because of wclreol
      clnl();

      ++p;
    }
    clnl();
    if (spc) *spc= ' ';

    // TODO: make and use common funtion for "shell.c:editline"
    
    // -- get user input
    putcraw(8);
    putchar('>');
    putchar(found? green: white);
    putz(line); wclreol();

    // TODO: ORIC specific k, only needed as we're modal here
    c= cursorgetc(); k= *(char*)0x209;

    if (c==13 || c==27 || k==0xa5) break; // RET ESC FUNC

    // process input
    if (c==127 || c==8) {
      if (!i) continue;
      line[--i]= 0; putchar(127); continue;
    } 

    // TODO: 20 what? length of input field, sizeof?
    if (i>= 20) continue;
    line[i++]= c;
    putchar(c);

  } while(1);

  loadwin(tmp);
  setwin(w);

  if (c == 13) {
    // launch!
    newwin();

    if (found)
      startline(found->fun, spc? spc+1: NULL);
    else
      startline(app_sh, line);

    // if it didn't open a window
    if (!winp->w) {
      // default tileable window size
      // TODO: WMAX HMAX bad names... lol
      window(-1, -1, WMAX, HMAX, -1, -1);
      wstatus(-1, winp->args);
      wdecorate();
    }
  }
}

// TODO: consider better placement function
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
#endif // 0


// Move focused window by (dx,dy), only one of them non-zero.
// Moves directly on screen with row-wise memmove: no malloc and no
// temporary buffer (the old version copied twice through malloc).
// Resize (dw,dh) was removed: it was marked "crashes" and cost a lot
// of code (restore from old version if needed).
void mowin(signed char dx, signed char dy, signed char dw, signed char dh) {
  Window* wf= wins + wfocus;
  char x= wf->x, y= wf->y, w= wf->w, h= wf->h;
  char i, *tmp, *d;
  char W= w + 8, H= h + 4;
  char *s= SCREENXY(x - 3, y - 2);
  int step= SCREENCOLS;

  if (dw || dh) return;

  // Bounds checking
  if (x + dx < 3) return;
  if (x + w + dx >= SCREENCOLS - 3) return;
  if (y + dy < 2) return;
  if (y + h + dy >= SCREENROWS - 1) return;

  // Collision/Edge detection
  if (dy) {
    tmp = s + (dy < 0 ? -SCREENCOLS : (int)SCREENCOLS * H);
    for (i = W; i--;) if (tmp[i] != 126) return;
  } else {
    tmp = s + (dx < 0 ? -1 : W);
    for (i = H; i--;) {
      if (*tmp != 126) return;
      tmp += SCREENCOLS;
    }
  }

  // Copy rows in an order that is safe for overlap
  if (dy > 0) { s+= (int)SCREENCOLS * (H - 1); step= -SCREENCOLS; }
  d= s + (int)SCREENCOLS * dy + dx;
  for (i = H; i--;) {
    memmove(d, s, W);
    s+= step; d+= step;
  }

  wf->x += dx;
  wf->y += dy;

  // Clean up only the exposed trailing edge left behind
  if (dy < 0)      fill(x - 3, y - 2 + H - 1, W, 1, 126);
  else if (dy > 0) fill(x - 3, y - 2, W, 1, 126);
  else if (dx < 0) fill(x - 3 + W - 1, y - 2, 1, H, 126);
  else if (dx > 0) fill(x - 3, y - 2, 1, H, 126);
}



//////////////////////////////////////////////////////////
// SCHEDULER!

unsigned int heapstart;

// Write number right-aligned in field of width w at p (no printf!)
static char* pnum(char* p, uint v, signed char w) {
  char s[6], n;
  utoa(v, s, 10);
  n= strlen(s);
  while (w > n) { *p++= ' '; --w; }
  memcpy(p, s, n);
  return p + n;
}

void scheduler() {
  runprocs= rounds= timesum= runsum= 0;
  
  wdecorate();
  setfocus(nwin);

  T('h');
  while(1) {
    DEB('^');

    // move to next app
  next:

    // every time we check if there is a KEY
    // if so: we route it directly to the wfocus window!
    // and this is prioritized as highly interactive app!
    while(wkbhit(0) || wkey) {
      // restore same window to be FAIR and NO starvation!
      wnext= wcur;

      // app can actually call kbhit() and getc()!
      setwin(wfocus);

      dorun((char*)(wkey + 0x100));
      
      wkey= 0;
      setwin(wnext);
    }

    if (!--wcur) {
      uint now= (uint)clock();
      latency= now-lastlatency;
      lastlatency= now;
      ++rounds;
      timesum+= (latency<<3) +1;
      if (now-lastupdate > 100) {
        // No display if was in menu
        if (latency < 100) {
          char* p= SCREENXY(11,0);
          uint hm= _heapmemavail();

          // On simulated ORIC we redraw the screen
          // every 100th ish clock pass, on simulated
          // this is so superfast we hardly blink redrawing
          // the screen "millions" of times per second!
          // (on ORIC this does nothing, not even fun-call)

          memset(SCREENXY(34,0), ' ', 6);
          // was: sprintf "%2u#%3u%4d/s%2u%%%2d%%%4d"
          p= pnum(p, latency, 2);   *p++= '#';
          p= pnum(p, rounds, 3);
          p= pnum(p, (int)(runprocs*100L/(uint)(now-lastupdate)), 4);
          *p++= '/'; *p++= 's';
          p= pnum(p, (int)(runsum*100L/timesum), 2); *p++= '%';
          p= pnum(p, (hm>>8)*100/(heapstart>>8), 2); *p++= '%';
          p= pnum(p, hm, 4);
          *p= ' '; // erase \0==BLACK TEXT

          // simulated ORIC: redraw screen (no-op on real ORIC)
          redrawscreen();
        }
        
        lastupdate= now;
        rounds= runprocs= 0;

        // Rolling average (?)
        if (timesum > 4096) runsum/=4,timesum/=4;
      }
      wcur= nwin;
    }
    setwin(wcur);

    // TODO: if no active windows/task will LOOP
    if (!winp->status) goto next;

    // - Handle cheap tasks
    // (if WAITing for key has to have focus)
    if (winp->ret != WAITKEY) {
      *SCREENXY(40-wcur,0)= '0'+wcur;
      dorun(0);
    }

    goto next;
  }
}

extern char* readsector(char*, unsigned int);
extern char* writesector(char*, unsigned int);

extern unsigned int insertlines(char* name);

extern void printPage();


#ifndef OVERRIDEMAIN

#define MAIN

#ifdef OSCAR64

// Poor man's string/memory writers
// for early boot stage without relying on much code

char* put1h(char* p, char x) {
  *p= (x<10? x: 'a'-'0'-10+x) + '0';
  return p+1;
}

char* put2h(char* p, char xx) {
  return put1h(put1h(p, xx>>4), xx & 0xf);
}

char* puth(char* p, unsigned int xxxx) {
  return put2h(put2h(p, xxxx >> 8), xxxx & 0xff);
}

char* xputc(char* p, char c) {
  *p= c;
  return p+1;
}

// 1. Reference the real linker symbols calculated by Oscar64
// TODO: remove as it can't be done twice...
//extern void HeapStart;
//extern void HeapEnd;

// This is here as I only was able to build and activate disk
// for oscar64 (because of size)

// TODO: somehow not working; try again... LOL

void enable_overlay_ram(void) {
  // Standard Oric VIA banking example:
  // Writing to the VIA Port B register to disconnect BASIC ROM and connect Overlay RAM
  volatile char* via_port_b = (char*)0x0300; 
    
  // Exact bits vary depending on whether you are doing a pure overlay write 
  // or full mapping, but the hardware latch must be flipped:
  *via_port_b &= ~0x10; // Example bit flip to swap out the BASIC ROM mapping
}

volatile char* microdisc_cfg = (char*)0x0314;

extern void ZeroStart;
extern void ZeroEnd;

extern struct Heap {
  void* next;
  void* end;
} HeapNode;

//#include "oafs.c"

// oscar64 doesn't provide standard main signature...
int main() {
  int argc;
  const char* argv[]= {"orwin", NULL};
  int i= 0, j= 0;
    
#else 
    
int main(int argc, char** argv) {

#endif // OSCAR64
  
  T('I');
  init();
  T('K');
  
#ifdef OSCAR64
  // First byte of pages $0500, $0600, … $1400 (16 sectors)
  {
    int n;
    char *line = TEXTSCREEN+40;
    for (n = 0; n < 16; n++) {
      unsigned char v = *(unsigned char *)(0x0500 + n * 256);
      line[n * 3]     = "0123456789ABCDEF"[v >> 4];
      line[n * 3 + 1] = "0123456789ABCDEF"[v & 15];
      line[n * 3 + 2] = ' ';
    }
  }

  j= 0;
  for(i=0x2529; i<=0x2529+16; ++i) {
    put2h(
      xputc(
        puth(TEXTSCREEN+(j+3)*40, i),
        ':'),
      *(char*)i);
    ++j;
  }
#endif 
  
// REMOVE THIS OSCAR DISK and then
// it loops to gray screen, reboots and reloads all the code!
#if 0    
  {
    char hex[]= "0123456789abcdef";
    unsigned int s= (int)&HeapStart;
    unsigned int e= (int)&HeapEnd;
      
    // 5d70-b200
    sprintf(SCREENXY(2, 10), "Heap: %c%c%c%c-%c%c%c%c"
      ,hex[(char)(s>>4*3)&0xf], hex[(char)(s>>4*2)&0xf], hex[(char)(s>>4*1)&0xf], hex[(char)(s>>4*0)&0xf]
      ,hex[(char)(e>>4*3)&0xf], hex[(char)(e>>4*2)&0xf], hex[(char)(e>>4*1)&0xf], hex[(char)(e>>4*0)&0xf]
    );
  }
#endif 

// DEBUG malloc for oscar64
//    T('?');
//    p= malloc(100);
//    p= my_malloc(100);
//    p= my_malloc(100);
//    p= my_malloc(100);
//    T(p!=NULL? '+': '-');

#ifdef OAFS
  T('O');
  {
    unsigned int n;
    n= insertlines((char*)"-");
    printf("Pages: %d\n", n);
  }
  readsector(0, 0);
  printPage();
  insertlines(argc>2? argv[2]: "-");
  T('A');
#endif // OAFS

  T('H');
  
  // clear background to "gray" checkerboard
  fill(0, 0, SCREENCOLS, SCREENROWS, 126);

  // Print logo in Upper Status Line
  strncpy(SCREENXY(0, 0), "0rWIN/ATMOS                                     ", 40);

  #if 0
   // Print logo in Right-Hand Corner
    strncpy(SCREENXY(34, 24), "\x0a""0rWin", 6);
    strncpy(SCREENXY(34, 25), "\x0a""0rWin", 6);
    strncpy(SCREENXY(34, 26),      " ATMOS", 6);
  #endif

  updatewinptr();
  
  wins[0].status= 255;

#define DEMO
#ifdef DEMO  

  newwin();
  window(-1, -1, 5, 3, blue, white);
  start(app_ascii);
  wstatus(-1, "ASCII");

#define DOUBLE
#ifdef DOUBLE
  newwin();
  window(3, 10, 5, 3, green, black);
  start(app_ascii);
  wstatus(-1, "ascii");

  setwin(2); setfocus(2);
  dorun(0);dorun(0);dorun(0);dorun(0);
#endif // DOUBLE

  newwin();
  start(app_charset);

#ifdef CRASH
  newwin();
  window(1, -1, 20-7, 10, green, black);
  start(app_sh);
#endif
  
#endif // DEMO

  T('S');
  scheduler();

  return 0;
  (void)argc; (void)argv;
}

#endif // OVERRIDEMAIN
