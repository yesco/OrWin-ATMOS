// test-loop-for-le.c - for (i = 0; i <= 9999; i++): <= instead of <
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  t i 9999 end  <body>  n back
//
// t is u for <=. N - 1 as the limit: test-loop-for-le-expr.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("for-le:\t23800\n\t\t");
  for (i = 0; i <= 9999; i++) s = s + i;
  printf("%u\n", s & 32767);

  return 0;
}
