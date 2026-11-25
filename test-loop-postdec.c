// test-loop-postdec.c - for (i = N; i-- > 0; ): the test with a side effect
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  ... S i  <i> G 0 zj end  J i  <body>  B back
//
// No loop op: the test, J and B each round.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  printf("postdec:\t23800\n\t\t");
  for (i = N; i-- > 0; ) s = s + i;
  printf("%u\n", s & 32767);

  return 0;
}
