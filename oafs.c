// OAFS - OrWIN ATMOS File System
//
// (C) 2026 Jonas S Karlsson jsk@yesco.org
//
// A "SmallTable" implementation
//
// Features:
// - "simple"
// - ordered by string keys <= 80 chars
// - page oriented index
// - inline small data (<= 80 chars)
// - file prefix recursive meta forwarding index entries! (=> log n!)
// - delete thombstone allows "versioning"
// - design for idempotency
// - "safe" (maybe optionally log-based)
// - optinally transactional for several entries "appends" (log file at end/beginning)

// It's designed to execute as part of OrWIN ATMOS windowing
// multi-concurrent actor system. OrWIN Actor, lol

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

FILE* aof= 0;

int qputsn(char* s, int len, FILE* f) {
  int n= 0; char c;

  if (!s) return fputs("(NULL)", f);
  n += fputc('"', f);

 next:
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
    if (c<32 || c>126)
      n+= fprintf(f, "\\x%02x", c);
    else
      n+= fputc(c, f);
    if (len>0 && --len) goto next;
  }

  n+= fputc('"', f);
  return n;
}

void nl() { putchar('\n'); }


// TODO: read "any size"? (smaller bigger)
char* readsector(char* buff, unsigned int n) {
  // we allocate one byte more, lol, to terminate any "strings"
  char* b= buff? buff: calloc(257, 1);
  size_t rd= b? fread(b, 256, 1, aof): 0;
  printf("RD %zu: ", rd); qputsn(b, 256, stdout); nl();
  if (!rd && !buff) { free(b); b= 0; }
  return b;
}

char* writesector(char* buff, unsigned int n) {
  size_t wr= buff? fwrite(buff, 256, 1, aof): 0;
  printf("WR %zu: " , wr); qputsn(buff, 256, stdout); nl();
  return wr? buff: 0;
}


// ENTRY:
//   <keycollen> <prefixlen+1> <coloffset> keycolrest...
//   <timestamp>
//   <datalen> <data> 0
//
// TODO: <keycollen> ???

// keycollen: is total expanded key column length expanded, incl 0 0
// prefixlen: how many bytes to take from previous keycol
// coloffset: which bytes in expanded is columnoffset
// keycolrest: and add these bytes to it to form the current
// 0: added to make it "printable" (if only text data)
// NOTE: keycol reconstituted is: KEY 0 COL
// NOTE2: COL is always ending with 0:
// COL: 0       : no column name, 0 length
//      -128    : explicit column name string of length n including 0
//      128-255 : column number

// timestamp: monotonically increasing number (32 bits, or runlen encoded)
//
// col(len):
//   - len < 128: is length of column name string (max 64?)
//   - 'len (>128): is column number
// column: len bytes
// 0: terminated with a redundant zero
//
// datalen:
// 

char* LEB128(char* s, long v) {
  while(v >= 128) {
    *s++= (v & 127) | 128;
    v>>= 7;
  }
  *s++= v;
  return s;
}

char* unLEB128(char* s, long *v) {
  char c;
  if ((c= *s) < 128) {
    *v|= c;
    return s+1;
  } else {
    *v|= c & 127;
    *v<<= 7;
    return unLEB128(s+1, v);
  }
}


// reverse LEB128 encoding (big endian)
char* BEL128(char* s, unsigned long v) {
  if (v >= 128) {
    *s= 128;
    s= BEL128(s, v >> 7);
  }

  *s^= v;
  return s+1;
}

char* unBEL128(char* s, long *v) {
  static char c;
  *v= 0;
  while((c= *s++) >= 128) {
    *v|= c & 127;
    *v<<= 7;
  }
  *v|= c;
  return s;
}

// Assumption SMALL_ENDIAN 6502/ARM
// TODO: make it independent
union oaq_type {
  // TODO: rename for bits
  unsigned char c;
  uint16_t      w;
  int16_t       i;
  uint32_t      u;
  int32_t       l;

  char          arr[4];
  struct b {
    char first;
    char second;
    char third;
    char fourth;
  } b;
} oaq_val;

char* OAQ(char* s, uint16_t w) {
  if (w < 0x80) { *s= w; return s+1; }
  oaq_val.w= w;
  // TODO: or is it < 0xf80 ???
  if (oaq_val.b.second+1 < 0b01111001) {
    s[0]= oaq_val.b.second | 0x80;
    s[1]= oaq_val.b.first;
    return s+2;
  } else {
    s[0]= 0b1111000 | 0x80;
    // LOL, reverse, direct extract
    s[1]= oaq_val.b.first;
    s[2]= oaq_val.b.second;
    return s+3;
  }
}

char* LOAQ(char* s, uint32_t l) {
  if (l < 0xffff) return OAQ(s, l);
  { char i, n= 1;
    oaq_val.l= l;
    // start encoding first non-zero byte
    for(i=3; i--; )
      if ((s[n]= oaq_val.arr[i]) || n > 1) ++n;

    s[0]= 0b11111000 + n - 3; // -3 I think... lol
    return s + n - 1;
  }
}

char* QAO(char* s, uint16_t *w) {
  char c= *s++;
  if (c < 0x80) { *w= c; return s; }
  // hi-bit set
  if (c+1 < 0b11111001) {
    *w= ((~c? c ^ 0x80: c) << 8) | *s++;
    return s;
  } else {
    *w= *(unsigned int*)s; // LOL
    return s+2;
  }
}

char* QAOL(char* s, uint32_t *l) {
  char c= *s++;
  //  if (c < 0x80) { *w= c; return s; }
  // hi-bit set
  l= 0;
  if (c+1 < 0b11111001) return QAO(s, (uint32_t*)l);
  // generic loop
  { char n= 0;
    oaq_val.l= 0;
    c-= 0b1111000 - 2;
    while(c--)
      oaq_val.arr[n++]= *s++;
    return s;
  }
}

// RUNLENGTH encoding of time:
//   0-127: itself
//   hival (preval << 7) | (hival & 0x7f)
// (terminates before next byte < 128)
//
// OR USES LEB129
// minimal bytes keylen prefixlen 0 collen 0 datalen 0

// extracts: a long from a pointer to char* pointer by moving it forward:
//   char* s= ...
//   long val= JSK128(&s);
//   // s is advanced to the next byte to process
//
long unJSK128(char* *s) {
  static char c; static long v;
  if ((c= *(*s)++) < 128) return c;
  v= c ^ 128;
  while((c= *++*s) >= 128) {
    v= (v<<7) + c ^ 128;
  }
  return v;
}

/*
  // AX= ptr to next byte
  unJSK128:
    sta ptr
    stx ptr+1
    ldy #0
    ldX #0

    lda (ptr),y
    inc ptr
    bne :+
    inc ptr+1
  :
    bpl ret

    and #127

  loop:
    


  ret:
    rts
 */


#define MAX_KEYS (256 / 9) // 28


// simplest hack
// TODO: make dynamic/growing?
typedef struct OAFSpage {
  char n;

  // ordered keys
  char* keys[MAX_KEYS];
  char* data[MAX_KEYS];
} OAFSpage;

int main(int argc, char** argv) {
  // test encoding
  #if 0
  // specific number 3,4,5 in 128 lol
  char s[10];
  memset(s, 0x00, sizeof(s));
  long z= 0;
  z<<=7; z+= 3;
  z<<=7; z+= 4;
  z<<=7; z+= 5;
  char* pe= BEL128(s, z);
  long w;
  char* pd= unBEL128(s, &w);
  printf("%8ld: %ld %ld %ld === %s\n", i, pe-s, pd-s, (long)w, w==i? "OK": "---FAIL---");
  printf("\tEN: "); qputsn(s, pe-s, stdout); nl();
  return 0;
  #endif
    
  // 32 BIT
  // for(long i=-512; i<666666; i+= i<1024 || (i>65200 && i<65536+100)? 1: 1024) {

  // 16 BIT
  for(long i=-512; i<32768; i+= (i<1024 || i>31700)? 1: 1024) {
    char s[10];
    memset(s, 0xff, sizeof(s));
    //char* pe= LEB128(s, i);
    //char* pe= BEL128(s, i);
    char* pe= OAQ(s, i);
    uint32_t l;
    int16_t w;
    //w= 0; // TODO: remove
    //char* pd= unLEB128(s, &w);
    //char* pd= unBEL128(s, &w);
    char* pd= QAO(s, &w);
    printf("%9ld: %ld %ld %9ld === %s", i, pe-s, pd-s, (long)w, w==i? "   OK     ": "---FAIL---");
    printf("\tEN: "); qputsn(s, pe-s, stdout); nl();
  }
  return 0;
  
  assert(argc);
  aof= fopen(argv[1], "rw+");
  assert(aof);

  char* xs= readsector(0, 0);

  fclose(aof);
  return 0;
}
