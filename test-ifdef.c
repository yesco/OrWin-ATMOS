// test-ifdef.c - #if 0 / #if N / #ifdef / #ifndef / #else / #endif
//
// Each case: its name and the values C gives, then (indented) what we got:
//   if 1 0:         1 2
//                   1 2
// The IVM VM and the JIT must print the same; gcc too, except the last
// case (__MC65__ and __TARGET_6502__ are ours: gcc prints 0 0).
// A false branch is skipped line by line (\% in rule Q, c.bnf) up to its
// #else / #endif; nested #if.. #endif in it are skipped whole. An #else
// reached is the end of a true branch: skipped to its #endif.
// - at the top level: a function only in one branch
// - in a block: statements in one branch
// - #ifdef: a #define'd name (ours: any declared name counts)
// - predefined: __TARGET_6502__ 1, __MC65__ the .ivm version (the mark's
//   last 2 bytes, hi lo: 'M' 0 = 19712)

#include <stdio.h>

#define FOO 1

#ifdef FOO
int which() {
  return 1;
}
#else
int which() {
  return 2;
}
#endif

#ifndef FOO
int gone() { return junk + ; }
#endif

int main(void) {
  int r;

  // --- #if 1 / #if 0, with and without #else ---------------------------------
  putz("if 1 0:\t\t1 2\n\t\t");
#if 1
  r = 1;
#endif
  putn(r);
#if 0
  r = 9;
#else
  r = 2;
#endif
  putn(r);
  putz("\n");

  // --- #ifdef / #ifndef: defined FOO, undefined BAR --------------------------
  putz("ifdef:\t\t3 4 5 6\n\t\t");
#ifdef FOO
  r = 3;
#else
  r = 9;
#endif
  putn(r);
#ifdef BAR
  r = 9;
#else
  r = 4;
#endif
  putn(r);
#ifndef BAR
  r = 5;
#endif
  putn(r);
#ifndef FOO
  r = 9;
#else
  r = 6;
#endif
  putn(r);
  putz("\n");

  // --- nested: in a false branch all skipped, in a true one parsed ----------
  putz("nested:\t\t7 8\n\t\t");
#if 0
  r = 9;
#ifdef FOO
  r = 9;
#else
  r = 9;
#endif
  r = 9;
#else
  r = 7;
#endif
  putn(r);
#ifdef FOO
#ifdef BAR
  r = 9;
#else
  r = 8;
#endif
#endif
  putn(r);
  putz("\n");

  // --- top level: the function from the true branch ---------------------------
  putz("top level:\t1\n\t\t");
  putn(which());
  putz("\n");

  // --- predefined (gcc: 0 0) ---------------------------------------------------
  putz("predefined:\t1 19712\n\t\t");
#ifdef __TARGET_6502__
  putn(__TARGET_6502__);
#else
  putz("0 ");
#endif
#ifdef __MC65__
  putn(__MC65__);
#else
  putz("0 ");
#endif
  putz("\n");

  return 0;
}
