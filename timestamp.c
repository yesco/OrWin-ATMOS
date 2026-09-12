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

// Testing

#define MAIN

#define OAQ_U32
#include "oaq.c"

#undef MAIN

struct TimeStamp {
  uint16_t Y;
  char M, D, h, m, s;
} TS;


#define SMALLTIME

// this +16 Y have high "second" resolution

// Now, is the best time, lol
#define BASEYEAR (2026-8)

#define YEARSTEP 16

#if 0
  #define YEARSHIFTS 4
  #define PREFIXBITS 0xf0000000
  #define TIMEROUND  15
#endif

#if 1
  #define YEARSHIFTS 8
  #define PREFIXBITS 0xff000000
  #define TIMEROUND  255
#endif

#if 0
  #define YEARSHIFTS 7
  #define PREFIXBITS 0xfe000000
  #define TIMEROUND  127
#endif

#if 0
  #define YEARSHIFTS 6
  #define PREFIXBITS 0xfc000000
  #define TIMEROUND  63
#endif

#if 0
  #define YEARSHIFTS 1
  #define PREFIXBITS 0x80000000
  #define TIMEROUND  1
#endif


// rounding up will eventually give >59 >23 >31 >12 == illegal!


//#define PREFIXBITS 0x80000000

//#define TIMEROUND 7
//#define TIMEROUND 1 
//#define TIMEROUND 0

uint32_t encodeTS() {
  int32_t r; char* p= (char*)&r;
  int16_t my;
  char ny= 0;

  //printf("Y=%d ", TS.Y);
  my= TS.Y - BASEYEAR;
  // TODO: how to handle "future"? ->  upgrade container!
  while(my < 0) { ++ny; my+= YEARSTEP; }
  printf("(%d) ", ny);

  // ???? oscar -DNO 1430 - SAME!
  // cc65 3967

  // -- full 1s resolution encoding
  // 10YY YYMM  MMDD DDDh  hhhh mmmm  mmss ssss | 1111 1111  1111 1111

  // -- archival encoding - date!
  //                       10YY YYMM  MMDD DDDh | hhhh mmmm  mmss ssss
  
  //                       YYYY YYMM  MMDD DDDD = 5bitY 4bitM 5bitD
  //                       (- 1970 32) = 1938 ... ?
  r= 0b10;
  r<<= 4; r|= my;
  r<<= 4; r|= TS.M;
  r<<= 5; r|= TS.D;
  r<<= 5; r|= TS.h;
  r<<= 6; r|= TS.m;
  r<<= 6; r|= TS.s;
  //printf("[%d]", my);

  while(ny--) 
    #ifdef SMALLTIME 
    if (((uint32_t)r)>>16==0xffff) r>>= 1;
    else
    #endif
    r>>= YEARSHIFTS;

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

#if 1
  // leading ones
  #ifdef SMALLTIME
  if ((ts>>16) == 0xffffL) {
    while((ts & 0xc000) != 0x8000) {
      ts<<= 1; ts|=1; TS.Y-= YEARSTEP; ++ny;
    }
  }
  #endif

  while((ts & PREFIXBITS)==PREFIXBITS) { ts<<= YEARSHIFTS; ts|=TIMEROUND; TS.Y-= YEARSTEP; ++ny; }

  // get back a leading 1, lol
  #ifdef xSMALLTIME
  if (ny) {
    ts>>= 1; ts|=0x80000000; --ny; TS.Y+= YEARSTEP;
  }
  #endif
  
#else
  // leading zeroes
  #ifdef SMALLTIME
  while(ts < (0x8000L<<(YEARSHIFTS-1))) { ts<<= YEARSHIFTS; ts|=1; TS.Y-= YEARSTEP; ++ny; }
  #endif
  while(ts < 0x80000000) { ts<<= YEARSHIFTS; ts|=TIMEROUND; TS.Y-= YEARSTEP; ++ny; }
#endif
  
  printf(" (%d) ", ny);

  TS.s = ts & 63; ts>>= 6;
  TS.m = ts & 63; ts>>= 6;
  TS.h = ts & 31; ts>>= 5;
  TS.D = ts & 31; ts>>= 5;
  TS.M = ts & 15; ts>>= 4;
  TS.Y+= ts & 15; //ts>>= 4; assert(ts==0b10);
}  

void printTS(uint32_t ts) {
  // YYYY-MM-DD hh:mm:ss
  decodeTS(ts);
  printf("%04d-%02d-%02d %02d:%02d:%02d",
    TS.Y, TS.M, TS.D, TS.h, TS.m, TS.s);
}

#ifndef MAIN
int main() {
  uint32_t y, i, last;

  //  for(y=BASEYEAR+YEARSTEP-1; y>=1900; y-= (y<1973)? 1: 3) {

  // SMALLTIME  YEARSHIFTS=8
  // -----------------------
  // 1842-07-xx xx:xx:xx ~1 byte
  // 1970-07-12 xx:xx:xx ~2 bytes (occasionally 3)
  // 2002-07-12 21:xx:xx ~4 bytes (21 might be "fake?")
  // 2018-07-12 21:42:17 ~5 bytes
  // 2033-07-12 21:42:17 ~5 bytes
  // (max, without readjusting "container"/page)
  // PARAMETERS/CONTAINER: BASEYEAR= (2026-8) GENERATION= $ff
  for(y=BASEYEAR+YEARSTEP-1; y>=1800; --y) {
    char buff[8]= {0}, *p;
    uint32_t a= timestamp(y,7,12, 21,42,17);
    uint32_t x= a;

    printf("%04x%04x ", (uint16_t)(a>>16), (uint16_t)(a&0xffff));

    for(i=32;i--;) {
      if (i%4==3) putchar(' ');
      putchar((x & 0x80000000)? '1': '0');
      x<<= 1;
    }
    //printf(" %4d", y);
    printTS(a);
    
    // show OAQ encoding bytes length
    printf(" #%d ", (int)(LOAQ(buff, a)-(char*)buff));
    printf(" ~%d ", (int)(LOAQ(buff, ~a)-(char*)buff));
    //for(i=0; i<sizeof(buff); ++i) printf("%02x", buff[i]);

    putchar('\n');
  }
  
  last= 0;
  for(y=0; last <= y; ++y) {
    char buff[8]= {0}, *p;
    uint32_t a= ~y;

    last= y;

    if (y > 65736L) { y= y + y/10; }
    if (y > 1000 && y < 0xffff-256) { y+= 256; continue; }
    
    printf("%7lu %04x%04x ", y, (uint16_t)(a>>16), (uint16_t)(a&0xffff));
    
    // show OAQ encoding bytes length
    printf(" #%d ", (int)(LOAQ(buff, a)-(char*)buff));
//    printf(" #%d ", (int)(LOAQ(buff, ~a)-(char*)buff));
    for(i=0; i<sizeof(buff); ++i)
      printf("%02x", buff[i]);

    putchar('\n');

  }
  
  return 0;
}
#endif // MAIN
