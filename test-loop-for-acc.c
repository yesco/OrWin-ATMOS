// test-loop-for-acc.c - for with a body that starts with i (ACC holds it)
//
// One of the test-loop-*.c family: the same work (N rounds, the
// same body) in a different loop form, to track what each form
// costs on every backend: ./ptest 'loop-*'
//
// Cycles per round: (its cycles - test-empty's) / N.
//
// The bytecode (-O):  Z i  u i N end  U s i  n back
//
// s = i + s is U s i (c.bnf), as s = s + i. Before that it was
// <i> + s S s: the body starts with i.
//
// After u and n the VM's ACC is i, the JIT's A/X too. A body that
// does start with i still loads it in the interpreters: the ACC
// contract has t u v n as UNDEF (ivm-walk.c).
//
// The sum is printed & 32767: its low bits are the same in gcc's
// 32 bit int and our 16 bit one.

#include <stdio.h>

#define N 10000

int main() {
  int i, s, n;

  s = 0; n = 0;

  printf("for-acc:\t23800\n\t\t");
  for (i = 0; i < N; i++) s = i + s;
  printf("%u\n", s & 32767);

  return 0;
}
