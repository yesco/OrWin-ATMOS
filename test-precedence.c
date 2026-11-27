// test-precedence.c - binary operators: left to right with C's precedence
//
// Plain C: gcc test-precedence.c && ./a.out is the reference; the IVM VM
// and the JIT must print exactly the same.
// Each case: its name and the values C gives, then (indented) what we got:
//   mul add:        63 57 5
//                   63 57 5
// The grammar evaluates left to right without temps: an operator may only
// be followed by one of the same or a looser level (rule O, \> \<), so C's
// precedence holds for everything that parses. Levels, tightest first:
//   * / %    + -    << >>    < <= > >=    == !=    &    ^    |
// - same level: left to right (a - b - c, a / b / c, a << b >> c)
// - a looser one after a tighter one: (a * b) + c
// - a tighter one after a looser one does not parse (te/test-precedence-
//   err.c): it needs parentheses, tested here
// - an R operand (a call, (E)): the left side is spilled; - / % << >>
//   load it back (the R shift forms were wrong before)
// - !: binds tight, !a + b is (!a) + b
// a = 12, b = 5, c = 3: all results below 32768 (gcc int is 32 bit)

#include <stdio.h>

int id(int x) {
  return x;
}

int main(void) {
  int a, b, c, r;

  a = 12; b = 5; c = 3;

  // --- * / % then + - ------------------------------------------------------
  putz("mul add:\t63 57 5\n\t\t");
  r = a * b + c; printf("%d ", r);
  r = a * b - c; printf("%d ", r);
  r = a / b + c; printf("%d ", r);
  putz("\n");

  putz("same level:\t6 4 10 0 20\n\t\t");
  r = a % b * c; printf("%d ", r);
  r = a - b - c; printf("%d ", r);
  r = a - b + c; printf("%d ", r);
  r = a / b / c; printf("%d ", r);
  r = a * b / c; printf("%d ", r);
  putz("\n");

  // --- shifts: below + -, left to right among themselves -------------------
  putz("shift:\t\t34 15 48\n\t\t");
  r = a + b << 1; printf("%d ", r);
  r = a * b >> 2; printf("%d ", r);
  r = a << b >> c; printf("%d ", r);
  putz("\n");

  // --- compares, then == != ------------------------------------------------
  putz("compare:\t1 0 0 1\n\t\t");
  r = a + b > c; printf("%d ", r);
  r = a - b < c; printf("%d ", r);
  r = a << 1 <= b; printf("%d ", r);
  r = a > b == 1; printf("%d ", r);
  putz("\n");

  putz("equal:\t\t1 0 1\n\t\t");
  r = a - c == 9; printf("%d ", r);
  r = a * b != 60; printf("%d ", r);
  r = a + 1 != b; printf("%d ", r);
  putz("\n");

  // --- & ^ |, below == ------------------------------------------------------
  putz("bitwise:\t7 11 7 1 1 13\n\t\t");
  r = a & b | c; printf("%d ", r);
  r = a ^ b | c; printf("%d ", r);
  r = a & b ^ c; printf("%d ", r);
  r = a + b & c; printf("%d ", r);
  r = a == 12 & c; printf("%d ", r);
  r = a | b; printf("%d ", r);
  putz("\n");

  // --- R operands: calls on the right ---------------------------------------
  putz("R mul add:\t63 63 4 6 5\n\t\t");
  r = id(a) * id(b) + id(c); printf("%d ", r);
  r = a * id(b) + c; printf("%d ", r);
  r = a - id(b) - c; printf("%d ", r);
  r = a % id(b) * c; printf("%d ", r);
  r = a / id(b) + c; printf("%d ", r);
  putz("\n");

  putz("R shift cmp:\t48 3 1 0 7\n\t\t");
  r = a << id(2); printf("%d ", r);
  r = a >> id(2); printf("%d ", r);
  r = a > id(b); printf("%d ", r);
  r = a <= id(b); printf("%d ", r);
  r = a & id(b) | id(c); printf("%d ", r);
  putz("\n");

  // --- parentheses: a tighter level after a looser one ---------------------
  putz("parens:\t\t27 13 96 51 1\n\t\t");
  r = a + (b * c); printf("%d ", r);
  r = a | (b & c); printf("%d ", r);
  r = a * (b + c); printf("%d ", r);
  r = (a + b) * c; printf("%d ", r);
  r = a == (b + 7); printf("%d ", r);
  putz("\n");

  // --- !: binds tight --------------------------------------------------------
  putz("not:\t\t5 6 0\n\t\t");
  r = !a + b; printf("%d ", r);
  r = !(a - 12) + b; printf("%d ", r);
  r = !a; printf("%d ", r);
  putz("\n");

  return 0;
}
