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
#include <string.h>
#include <assert.h>

struct TimeStamp {
  uint16_t Y;
  char M, D, h, m, s;
} TS;

// this +16 Y have high "second" resolution
//#define BASEYEAR 2026
#define BASEYEAR 2032
#define YEARSTEP 16
// rounding up will eventually give >59 >23 >31 >12 == illegal!
#define TIMEROUND 1 
//#define TIMEROUND 0

uint32_t encodeTS() {
  uint32_t r; char* p= (char*)&r;
  int16_t my;
  char ny= 0;

  //printf("Y=%d ", TS.Y);
  my= TS.Y - BASEYEAR;
  // TODO: how to handle "future"? ->  upgrade container!
  while(my < 0) { ++ny; my+= YEARSTEP; }
  printf("(%d) ", ny);

  // ???? oscar -DNO 1430 - SAME!
  // cc65 3967
  // 10YY YYMM  MMDD DDDh  hhhh mmmm  mmss ssss
  r= 0b10;
  r<<= 4; r|= my;
  r<<= 4; r|= TS.M;
  r<<= 5; r|= TS.D;
  r<<= 5; r|= TS.h;
  r<<= 6; r|= TS.m;
  r<<= 6; r|= TS.s;
  //printf("[%d]", my);
  while(ny--) r>>= 1;

  return r;
}  

uint32_t timestamp(
  uint16_t Y, char M, char D, char h, char m, char s) {
  memset(&TS, 0, sizeof(TS));
  TS.Y= Y; TS.M= M; TS.D= D; TS.h= h; TS.m= m; TS.s= s;
  return encodeTS();
}

void decodeTS(uint32_t ts) {
  char ny= 0;

  memset(&TS, 0, sizeof(TS));
  TS.Y= BASEYEAR;
  
  // fill with ones to make "illegal dates" (rounding up)
  while(ts < 0x80000000) { ts<<= 1; ts|=TIMEROUND; TS.Y-= YEARSTEP; ++ny; }
  printf(" (%d) ", ny);

  TS.s = ts & 63; ts>>= 6;
  TS.m = ts & 63; ts>>= 6;
  TS.h = ts & 31; ts>>= 5;
  TS.D = ts & 31; ts>>= 5;
  TS.M = ts & 15; ts>>= 4;
  TS.Y+= ts & 15; ts>>= 4; assert(ts==0b10);
}  

void printTS(uint32_t ts) {
  // YYYY-MM-DD hh:mm:ss
  decodeTS(ts);
  printf("%04d-%02d-%02d %02d:%02d:%02d",
    TS.Y, TS.M, TS.D, TS.h, TS.m, TS.s);
}

int main() {
  int y, i;
  for(y=BASEYEAR+YEARSTEP-1; y>=1900; y-= (y<1973)? 1: 3) {
    uint32_t a= timestamp(y,7,12, 21,42,17);
    uint32_t x= a;
    printf("%04x%04x ", (uint16_t)(a>>16), (uint16_t)(a&0xffff));
    for(i=32;i--;) {
      putchar((x & 0x80000000)? '1': '0');
      x<<= 1;
    }
    printTS(a); putchar('\n');
  }
  
  return 0;
}
