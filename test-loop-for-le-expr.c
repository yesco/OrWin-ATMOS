// test-loop-for-le-expr.c - for (i = 0; i <= N - 1; i++): <= and an expression
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  ... S t  t i t end  <body>  n back
//
// N - 1 is computed at run time, once, into a temp: the limit is a
// variable then. The plain <=: test-loop-for-le.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("for-le-expr:\t23800\n\t\t");
  for (i = 0; i <= N - 1; i++) s = s + i;
  putn(s & 32767);

  return 0;
}
