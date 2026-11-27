// test-loop-while.c - while (i < N) { ...; i++; }: the for loop written out
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  u i N end  <body>  I i  B back
//
// The test is still the fused u, the step is I and B (two ops).
//
// - Interpreters: two more ops each round, slower than for.
// - JIT: as fast as for (2026-11). It used to be faster: only
//   while's test kept i in A/X, for's n now does too.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("while:\t23800\n\t\t");
  i = 0;
  while (i < N) { s = s + i; i++; }
  putn(s & 32767);

  return 0;
}
