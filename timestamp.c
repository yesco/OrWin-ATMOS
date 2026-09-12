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

#if 1

struct TimeStamp {
  uint16_t Y;
  char M, D, h, m, s;
} TS;

// this +16 Y have high "second" resolution
#define BASEYEAR 2026

// TODO: should work - bug!
//#define BASEYEAR 2007

#if 0
uint32_t PACKI;

// oscar -DNOFLOAT -DNOLONG 1746 bytes (- 1746 1349) = 397

//
// cc65 -DPROGSIZE: 3977

//#pragma optimize(push)
//#pragma optimize(0)
void packint(char n, uint16_t x) {
  PACKI= (PACKI<<n) | x;
}

uint32_t encodeTS() {
  uint32_t r; char* p= (char*)&r;
  int16_t my;
  char ny= 0;
  my= TS.Y - BASEYEAR;
  while(my>=16) { ++ny; my-= 16; }
  printf("---NY= %d\n", ny);
  // 10YY YYMM  MMDD DDDh  hhhh mmmm  mmss ssss
//  return packint(packint(packint(packint(packint(packint(
//              0b01, 4,my), 4,TS.M), 5,TS.D), 5,TS.h), 6,TS.m), 6,TS.s);
  PACKI= 0b10;
  packint(4, my);
  packint(4, TS.M);
  packint(5, TS.D);
  packint(5, TS.h);
  packint(6, TS.m);
  packint(6, TS.s);
  return PACKI;
}
//#pragma optimize(pop)

#else
// -Os 4102
// oscar64 
uint32_t encodeTS() {
  uint32_t r; char* p= (char*)&r;
  int16_t my;
  char ny= 0;
  my= TS.Y - BASEYEAR;
  while(my>=16) { ++ny; my-= 16; }
  printf("---NY= %d\n", ny);
  // Compiles more compact on both cc65 and oscar64
#if 1
  #if 0
    // oscar -DNO 1596
    // cc65 4066
  // 10YY YYMM  MMDD DDDh  hhhh mmmm  mmss ssss
      // TODO: actually correct byte order!
    // just doesan't compare as INT! LOL
  p[0]= (0b10<<6) | (my<<2) | (TS.M>>2);
  p[1]= (TS.M<<6) | (TS.D<<1) | (TS.h>>4);
  p[2]= (TS.h<<4) | (TS.m>>2);
  p[3]= (TS.m<<6) | TS.s;
  #else
// SMALLTEST!???
// oscar -DNOFLOAT -DNOLONG 1430 bytes (- 1430 1349) = 81
  r= (((((((((((0b10L<<4) | my)<<4) | TS.M)<<5) | TS.D)<<5) | TS.h)<<6) | TS.m)<<6) | TS.s;
  #endif
#else
  // oscar -DNO 1430 - SAME!
  // cc65 3967
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
#endif


uint32_t timestamp(
  uint16_t Y, char M, char D, char h, char m, char s) {
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
  decodeTS(ts);
  printf("%04d-%02d-%02d %02d:%02d:%02d",
    TS.Y, TS.M, TS.D, TS.h, TS.m, TS.s);
}

#else
// -Os 4173
// oscar 4904 bytes (- 5496 4904) 592 bytes
// oscar -DNOFLOAT -DNOLONG 1349

// cc65 2803 bytes

struct TimeStamp {
  char Yh, Yl;
  char M, D, h, m, s6ms2, ms8;
} TS;

uint16_t TSY;
char TSs;
uint16_t TSms;

// bytes: Y,Y,M,D, h,m,s,x == 8 long long

//uint32_t encodeTS() {
//}

char* timestamp(
  uint16_t Y, char M, char D, char h, char m, char s, uint16_t ms) {
  TS.Yh= Y>>8; TS.Yl= Y; TS.M= M; TS.D= D; TS.h= h; TS.m= m;
  TS.s6ms2= (s<<2) | (ms>>8); TS.ms8= ms;
  return (char*)&TS;
}

void decodeTS() {
  TSY= (TS.Yh<<8) | TS.Yl;
  TSs= TS.s6ms2>>2; TSms= (TS.s6ms2 & 3)<<8 | TS.ms8;
}

void printTS() {
  // YYYY-MM-DD hh:mm:ss.xxx
  decodeTS();
  printf("%04d-%02d-%02d %02d:%02d:%02d.%03d",
    TSY, TS.M, TS.D, TS.h, TS.m, TSs, TSms);
}

#endif

int main() {
#if 1
  uint32_t a= timestamp(2026,9,11, 4,55,54);
  printf("a= %08lx\n", a);
  printTS(a); putchar('\n');

//  a= timestamp(1984,12,17, 16,44,03);
//  printf("a= %08lx\n", a);
//  printTS(a); putchar('\n');
#else
  timestamp(2026,9,11, 4,55,54, 666);
  //printf("a= %08lx\n", a);
  printTS(); putchar('\n');
#endif
  return 0;
}
