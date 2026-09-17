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
char isliteral(const void* p) {
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
