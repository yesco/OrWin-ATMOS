// timestamp.c - Multi-format timestamps for OAFS
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

// For the Ordered Attribute File System (OAFS).
//
// Goals:
// - (reverse) orderable for "minitable" LogMergeTree ala bigtable
// - timestamps are optional
// - OR indicated by a single byte non-timestamp indicator (0)
// - OR simple comparable historical YYYY-MM-DD HH:MM:SS encoding
// - OR capable of 64-bit unix-timestamps
// - OR monotonically increasing stamps

// A human timestamp is logically:
//   
// = a 32-bit value =
// 10YY YYMM  MMDD DDDh  hhhh mmmm  mmss ssss
//
// 10 = indicator
// Y4 = 0-15 years from BASEYEAR
// M4 = 0-11 month JAN-DEC
// D5 = 0-31 day of month
// h4 = 0-23 hour
// m6 = 0-59 minute
// s6 = "monotonic seconds"

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

struct TimeStamp {
  uint16_t Y;
  char M, D, h, m, s;
} TS;

// this +16 Y have high "second" resolution
#define BASEYEAR 2026

// TODO: should work - bug!
//#define BASEYEAR 2007

// oscar64 #x112 = 274 bytes - exactly same 1 or 0 !
uint32_t encodeTS() {
  uint32_t r;
  int16_t my;
  char ny= 0;
  my= TS.Y - BASEYEAR;
  while(my>=16) { ++ny; my-= 16; }
  printf("---NY= %d\n", ny);
  // Compiles more compact on both cc65 and oscar64
#if 0
  r= (((((((((((0b10L<<4) | my)<<4) | TS.M)<<5) | TS.D)<<5) | TS.h)<<6) | TS.m)<<6) | TS.s;
#else
  r= 0b10; 
  r<<= 4; r|= my;
  r<<= 4; r|= TS.M;
  r<<= 5; r|= TS.D;
  r<<= 5; r|= TS.h;
  r<<= 6; r|= TS.m;
  r<<= 6; r|= TS.s;
#endif
  // We set lowest bit to 1 to generate overflow!
  while(ny--) { r>>= 1; ++r; }

  return r;
}  

uint32_t timestamp(uint16_t Y, char M, char D, char h, char m, char s) {
  TS.Y= Y; TS.M= M; TS.D= D; TS.h= h; TS.m= m; TS.s= s;
  return encodeTS();
}

// oscar64: #xda bytes ?? (- #x1803 #x172a) = 217 
void decodeTS(uint32_t ts) {
  char ny= 0;
  uint16_t my= BASEYEAR;
  while(ts < 0x80000000) { ts<<= 1; my+= 16; ++ny; }
  printf("---NY= %d\n", ny);

  TS.s= ts & 63; ts>>= 6;
  TS.m= ts & 63; ts>>= 6;
  TS.h= ts & 31; ts>>= 5;
  TS.D= ts & 31; ts>>= 5;
  TS.M= ts & 15; ts>>= 4;
  TS.Y+=ts & 15; ts>>= 4;
  assert(ts==0b10);
}  

void printTS(uint32_t ts) {
  // YYYY-MM-DD hh:mm:ss
  char str[22];
  decodeTS(ts);
  printf("%04d-%02d-%02d %02d:%02d:%02d",
    TS.Y, TS.M, TS.D, TS.h, TS.m, TS.s);
}
  
uint32_t encodeTSc() { return 44; }

int main() {
  uint32_t a= timestamp(2026,9,11, 4,55,54);
  printf("a= %08lx\n", a);
  printTS(a); putchar('\n');
  return 0;
}
