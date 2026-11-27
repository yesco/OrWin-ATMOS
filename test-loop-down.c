// test-loop-down.c - for (i = N; i; i--): counting down
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  ... S i  <i> zj end  <body>  J i  B back
//
// No loop op for this one: a zero test (zj), J and B.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("down:\t1032\n\t\t");
  for (i = N; i; i--) s = s + i;
  printf("%u\n", s & 32767);

  return 0;
}
