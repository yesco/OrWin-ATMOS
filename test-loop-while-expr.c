// test-loop-while-expr.c - while (i + 1 <= N): a test the loop ops don't know
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  <i> + 1 > N G end  <body>  I i  B back
//
// No fused op at all: what the loop ops save, on every backend.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  printf("while-expr:\t23800\n\t\t");
  i = 0;
  while (i + 1 <= N) { s = s + i; i++; }
  printf("%u\n", s & 32767);

  return 0;
}
