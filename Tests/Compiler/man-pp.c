// man-pp.c - MeteoriC-man.md 2: the preprocessor. MC65 only: it tests our
// predefined __MC65__ / __TARGET_6502__
//
// Each case: its name and the values we give, then (indented) what we got.

#include <stdio.h>

#define SIZE 8
#define MASK 0x0f
#define GREETING "hello"

// our compiler or another one
#ifdef __MC65__
  #define TARGET 6502
#else
  #define TARGET 0
#endif

#ifndef SIZE
  this is skipped: SIZE is defined
#endif

// #if: a number (0 false, else true); a name: see the NOTE in the manual
#if 1
  #define TRACE 0
#else
  #define TRACE 1
#endif

#if 0
  anything here is skipped, also #if / #endif inside:
  #if 1
  #endif
#endif

char buf[SIZE];

int main(void) {
  int x;

  printf("numbers:\t8 15 6502 0\n\t\t");
  x = sizeof(buf); printf("%d ", x);
  x = 255 & MASK; printf("%d ", x);
  printf("%d ", TARGET);
  printf("%d\n", TRACE);

  printf("strings:\thello\n\t\t");
  puts(GREETING);

  printf("predefined:\t1\n\t\t");
  printf("%d\n", __TARGET_6502__);
  return 0;
}
