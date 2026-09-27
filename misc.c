// Misc
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

#include <ctype.h>

// TODO: make "parsing primitives" that work on a
//   pointer in ZP, so no need pass pointer!

#if 1
// 36944 (- 36974 36944) = 30 bytes saved
#pragma bss-name (push, "ZEROPAGE")


char* zptr;

#pragma bss-name (pop)


void skipTill(char c) {
  while(*zptr && *zptr!=c) ++zptr;
}

void skipspc() {
  while(*zptr && isspace(*zptr)) ++zptr;
}

#else
// 36974
// looked at skipTill(char** s...) but not save any bytes?
char* skipTill(char* s, char c) {
  while(*s && *s != c) ++s;
  return s;
}

char* skipspc(char* s) {
  while(*s && isspace(*s)) ++s;
  return s;
}
#endif
