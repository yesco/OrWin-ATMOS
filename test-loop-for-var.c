// test-loop-for-var.c - for (i = 0; i < n; i++): a variable limit
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  C n N  Z i  u i n end  <body>  n back
//
// The limit is a slot either way: a variable costs the same.
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("for-var:\t23800\n\t\t");
  n = N;
  for (i = 0; i < n; i++) s = s + i;
  printf("%u\n", s & 32767);

  return 0;
}
