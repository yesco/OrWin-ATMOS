// test-loop-for-lt.c - for (i = 0; i < N; i++): the counting loop
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  u i N end  <body>  n back
//
// The fused loop ops: u tests i < N once, n does i++, the test and
// the jump back in one op. The fastest loop in the interpreters.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("for-lt:\t23800\n\t\t");
  for (i = 0; i < N; i++) s = s + i;
  printf("%u\n", s & 32767);

  return 0;
}
