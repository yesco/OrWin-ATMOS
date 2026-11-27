// test-loop-while-count.c - while (i < N) { n++; i++; }: the same, written out
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  u i N end  I n  I i  B back
//
// Compare with test-loop-count: n against I B, when the body
// doesn't read i.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("while-count:\t10000\n\t\t");
  i = 0;
  while (i < N) { n++; i++; }
  putn(n);

  return 0;
}
