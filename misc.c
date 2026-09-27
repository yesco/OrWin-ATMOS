// Misc
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

#ifndef MISC_C

#define MISC_C

#include <ctype.h>

// Parsing primitives using a zero page zptr, so
// no need passing it back and forth. Saves
// 148 bytes at least!
//
// 36826 (- 36974 36781) = 193! bytes saved


#pragma bss-name (push, "ZEROPAGE")

char* zptr;

#pragma bss-name (pop)


void skiptill(char c) {
  while(*zptr && *zptr!=c) ++zptr;
}

void skipspc() {
  while(*zptr && isspace(*zptr)) ++zptr;
}

void skipword() {
  while(*zptr && (isalnum(*zptr) || *zptr=='_' || *zptr=='-')) ++zptr;
}

#endif // MISC_C
