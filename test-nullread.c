// test-nullread.c - a NULL pointer reads 0
//
// The image's startup writes 00 00 to $00-$01 (ivm-lib.asm
// _ivmlib_start, sim65 and Atmos): *NULL is 0, harmless, as lisp's
// (cdr nil) is nil. sim65 loads with $FF everywhere: 65535 without it.
//
// gcc: a NULL read crashes there, so the .out is written by hand.

#include <stdio.h>

int main() {
  int *p;

  putz("null:\t0\n\t\t");
  p = 0;
  putn(*p);

  return 0;
}
