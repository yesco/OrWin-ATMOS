// hexdump style, ascci "source" style
#ifndef QPUTS

#include <ctype.h>

#define QPUTS

// from ADDRESS, using OFFSET, print LEN bytes using STYLE
// every 2 bytes print a space (abcde)
//
// Returns: next address
char* prhexline(char* a, unsigned int offset, char n, char style) {
  char i= n, j= 0, *p= a+offset, c, d;

  printf("%04X:", offset);
  do {
    printf("%02x", *p++);
    if ((++j & 1)==0) putchar(' ');
  } while(--i);

  if (style<32 || toupper(style)=='C') {
    i= n; j= 0; p= a+offset;
    do {
      if ((++j & 3)==1) putchar(' ');
      c= (d=*p++) & 0x7f;
      // printing with highbit only inverse on "oric"
      #ifdef HIBITINVERSE
        putchar( (d&0x80) | (c<32 && c>125 ? '.' : d) );
      #else
        putchar( (c<32 || c>125 ? '.' : c>=128? '\'': d) );
      #endif
    } while(--i);
  }

  putchar('\n');
  return p;
}
  
//          1         2         3         4
// 12345678901234567890123456789012345678901234567890

// ADDR 1122 3344 5566 7788 abcd efgh

void prhexdump(char* a, unsigned int n, char style) {
  int i= n;
  unsigned int offset= 0;
  char d= 16;

  putchar('\n');
  while(i>0) {
    //prhexline(a, offset, i>=8? 8: i, style);
    prhexline(a, offset, d, style);
    offset+= d;
    i-= 8;
  }
}

#define fputqsnw(s, len, f, width) unsigned_fputqsnw((unsigned char*)(s), (len), (f), (width))
  
int unsigned_fputqsnw(unsigned char* s, int len, FILE* f, int width) {
  int n= 0; unsigned char c;

  //printf("fputqsnw: %04X %d %04X %d\n", s,  len, f, width);
  
  if (!s)  return fputs("(NULL)", f);
  if (!*s) return fputs("\"\"", f);
  
  n += fputs("\"", f);
 next:
  --len;
  if (width > 0 && width-n <= 3) { n+= fprintf(f, "..."); goto spaces; }
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
    if (c==0 && len < 0) goto done;
//    if (c<32 || c>126 || c&0x80) // nah, doesn't catch it
    if (c<32 || c&0x80) {
      n+= fprintf(f, "\\x%02x", c);
    } else {
//    if (c<128)  // doesn't do anything!
//      n+= fprintf(f, "%c", c&0x7f);// also nothing
//    n+= fprintf(f, "%c", 42); // crashes too!
//    ++n; fputc(42, f);
      ++n; fputc(c, f);
    }
    if (len>0 && len) goto next;
  }
 done:
  n+= fputs("\"", f);

 spaces:
  while (n++ < width) putchar(' ');

  return n;
}

void fputqsn(char* s, int len, FILE* f) {
  fputqsnw((unsigned char*)s, len, f, -1);
}

// convenience, assumes ends with 0
void qputs(char* s) {
  fputqsnw(s, strlen(s), stdout, -1);
}

//////////////////////////////
#ifndef NL

  #define nl() { putchar('\n'); }

  #define NL

#endif // NL




#ifdef OLD
// TODO: get rid of, but first see if we have
//   any fixes to forward port!?


// TODO: replace and use the oafs: fputqsnw function instead
//   or maybe here just a byte/hex print %x lol w &
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

#endif


#endif // QPUTS
