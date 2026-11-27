// test-loop-for-pluseq.c - for with s += i: one U op
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  u i N end  U s i  n back
//
// U s i: mem[s] += mem[i], ACC = mem[s]: one op instead of three.
// s = s + i (test-loop-for-lt) is the same U now (c.bnf).
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  putz("for-pluseq:\t23800\n\t\t");
  for (i = 0; i < N; i++) s += i;
  printf("%u\n", s & 32767);

  return 0;
}
