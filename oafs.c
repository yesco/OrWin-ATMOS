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
  assert(argc);
  aof= fopen(argv[1], "rw+");
  assert(aof);

  char* xs= readsector(0, 0);

  fclose(aof);
  return 0;
}
