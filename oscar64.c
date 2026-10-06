// sim6502.c - abstracted ORIC ATMOS machine
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// This is for the OrWIN-ATMOS project,
// to allow it to compile and run under a simulator.
//
// All ORIC specific "pokes" have been abstracted
// this this file and API and can be implemented
// for other platforms
// 
// This file targets to run under oscar64

#define GOTOXY

// Hmmm?

#define WRITE
size_t write(int fd, char* s, size_t count) {
  while(count-- >= 0) putchar(*s++);
}

#include <stdint.h>

#define ISLITERAL
//extern char __heap_start[];
extern char __heap_start;
#define HEAP_START ((uint16_t)__heap_start)
char isliteral(void* p) {
  return ((uint16_t)p > HEAP_START);
}

#define HEAPMEM

// no difference
//#pragma heapsize(0)

size_t heapfreex() {
  //char* p= malloc(1);
  return heapfree();
}
  
#define _heapmemavail heapfreex
#define _heapmaxavail heapfreex

// ------------------------------------------------
// we have two variants, both require OSCAR64 symbol
// the default is to run it simulated using ANSI

#ifdef ORIC

// duplicated from atmos.c

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

#ifdef printf
   fish: TODO: capture
#endif

// cc65 missing \e
#define ESC "\x1b"

#endif // ORIC
