// oric.c - abstractable ORIC ATMOS machine
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// This is for the OrWIN-ATMOS project,
// to allow it to compile and run under a simulator.
//
// All ORIC specific "pokes" have been abstracted
// this this file and API and can be implemented
// for other platforms


/// ORIC ------------------------------------
// oric charset addresses

#define CHARSET    ((char*)0xB400) // $B400-B7FF
#define CHARDEF(C) ((char*)(CHARSET+(C)*8))
#define ALTSET     ((char*)0xB800) // $B800-BB7F

// text screen direct addresses macros

#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F
#define SCREENROWS 28
#define SCREENCOLS 40

#define SCREENSIZE (SCREENROWS*SCREENCOLS)
#define SCREENLAST (TEXTSCREEN+SCREENSIZE-1)

// LOL
#define curscr TEXTSCREEN
#define SCREENXY(x, y) ((char*)(curscr+(5*(y))*8+(x)))





// Various generic "specialized" implentations:

#define CGETC
#define CPUTC
#define CPRINTF
#define GOTOXY

#define PUTCHAR

// have
#define BZERO
#define CPUTC
#define QSORT


// TODO: local implementations
// TODO: already define on ORIC, what does it do?

#include <stdint.h>

#define CLOCK
clock_t clock() {
  // ORIC TIMER 100 interrupts/s,
  // TODO: make clock_t bigger and handle wraparound
  return ~*(volatile unsigned int*)0x276;
}

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

// Init the platform

#define INIT
void init() {
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
}

// TODO: why these? just use nl nlpure clnl from fillins


// doesn't scroll, just wraps around, no wclreol()
#define NLPURE_IMPL
void nlpure() {
  winp->c= 0;
  if (++winp->r >= winp->h) winp->r= 0;
  updatewinptr();

  // set current (new) colors
  winp->p[-2]= BG | winp->bg;
  winp->p[-1]=      winp->fg;
}

#define CLNL_IMPL
void clnl() {
  wclreol();
  nlpure();
}

// doesn't scroll, just wraps around, DOES wclreol()
#define NL_IMPL
void nl() {
  nlpure();
  wclreol();
}


#define PUTCRAW
char putcraw(char c) {
  *winp->p= c;
  wputc(KEYRIGHT);
  return c;
}


// on oric: patch all printf etc!
#define PUTCHAR

#undef putchar
int putchar(int c) { return wputc(c); }
#define putchar wputc


// For efficiency implemented just like this!

#define HELP
void help() {
  char tmp[40];
  memcpy(tmp, TEXTSCREEN, sizeof(tmp));
  memcpy(TEXTSCREEN, HELPTEXT, sizeof(HELPTEXT));
  // wait - no echo
  cgetc();
  memcpy(TEXTSCREEN, tmp, sizeof(tmp));
}


#define CURSORGETC
char cursorgetc() {
  char c;
  togglecursor();
  c= cgetc();
  togglecursor();
  return c;
}

#define SAVEWIN
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

#define LOADWIN
void loadwin(char* p) {
  char h= winp->h, w= winp->w + 3, *r= p;
  char *s= SCREENXY(winp->x - 2, winp->y);
  while(h--) {
    memmove(s, p, w);
    p+= w; s+= SCREENCOLS;
  }
  free(r);
}


#define ISLITERAL
extern void* _heaporg;
//extern void* _heapptr;
//extern void* _heapend;

char isliteral(void* p) {
  return (p < &_heaporg);
}


#define HEAPMEM
size_t _heapmemavail(void);
size_t _heapmaxavail(void);


// Remapping arrow keys w FUNCT and CTRL
// 
#define MYGETC
char mygetc() {
  // 209: Keyshifts, s= ScanCodes
  char orig= cgetc(), k= *(char*)0x209, s= *(char*)0x208, c= orig;

  // ORIC ATMOS: ROM magical shift key $209
  #define NOKEY     0x38
  #define CTRLKEY   0xa2
  #define LSHIFTKEY 0xa4
  #define FUNCTKEY  0xa5
  #define RSHIFTKEY 0xa7

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

  #ifdef DEBUGKEY
  sprintf(SCREENXY(SCREENCOLS-3*4+1-2,SCREENROWS-1),
	  "[%02x %02x %02x %02x]", c, orig, k, s);
  #endif // DEBUGKEY

  return c;
}

#ifdef TODOREAD

#ifndef READSECTOR
#define FDC_STATUS   ((volatile uint8_t*)0x0310)
#define FDC_CMD      ((volatile uint8_t*)0x0310)
#define FDC_TRACK    ((volatile uint8_t*)0x0311)
#define FDC_SECTOR   ((volatile uint8_t*)0x0312)
#define FDC_DATA     ((volatile uint8_t*)0x0313)
#define FDC_CTRL     ((volatile uint8_t*)0x0314)

/* 
 * Reads a 512-byte physical sector from the Oric Microdisc
 * track:  0 - 79
 * sector: 1 - 17
 * side:   0 or 1
 * drive:  0 to 3 (usually 0 for Drive A:)
 */
uint8_t oric_read_sector(uint8_t track, uint8_t sector, uint8_t side, uint8_t drive, uint8_t *buffer) {
    // 1. Select the target Drive and Side via the Microdisc Control Register
    // Bit 0-1: Drive Select, Bit 4: Side Select, Bit 7: EPROM overlay disable (enable RAM mapping)
    *FDC_CTRL = (drive & 0x03) | ((side & 0x01) << 4) | 0x80;
    
    // 2. Set the target Track and Sector indices on the controller chip
    *FDC_TRACK  = track;
    *FDC_SECTOR = sector;
    
    // 3. Issue the FDC Read Command ($80 = Read Sector, Type II command)
    __asm("sei"); // CRITICAL: Disable interrupts so IRQs don't steal cycles mid-sector!
    *FDC_CMD = 0x80; 
    
    // 4. Tight high-speed assembly block loop to harvest 512 bytes
    // Using standard pointer registers passed from Oscar64 variables
    __asm(
        "ldx #0\n"               // Loop register for Page 1 (256 bytes)
        "ldy #0\n"               // Loop register for Page 2 (256 bytes)
        "lda %0\n"               // Load the buffer pointer low byte
        "sta $00\n"
        "lda %1\n"               // Load the buffer pointer high byte
        "sta $01\n"
        
".read_loop_p1:\n"
        "lda $0310\n"            // Read FDC status register
        "bmi .read_loop_p1\n"    // Wait while Bit 7 (Busy) is high
        "lda $0313\n"            // Pull raw data byte from FDC Data Register
        "sta ($00),y\n"          // Store into destination memory address
        "iny\n"
        "bne .read_loop_p1\n"    // Loop naturally rolls over after 256 bytes
        
        "inc $01\n"              // Increment memory pointer to the next 256-byte page block
".read_loop_p2:\n"
        "lda $0310\n"
        "bmi .read_loop_p2\n"
        "lda $0313\n"
        "sta ($00),y\n"
        "iny\n"
        "bne .read_loop_p2\n"
        : 
        : "r"((uint16_t)buffer & 0xFF), "r"((uint16_t)buffer >> 8)
        : "x", "y", "a"
    );
    __asm("cli"); // Re-enable scheduling interrupts safely
    
    // Verify execution status from FDC (0 = Success, non-zero = Read Error/CRC Fail)
    return *FDC_STATUS;
}
#endif // READSECTOR

#endif // TODO:...
