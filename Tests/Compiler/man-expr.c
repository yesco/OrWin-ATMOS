// man-expr.c - MeteoriC-man.md 5: expressions, the precedence rule and how
// to rewrite: left to right, after an operator only one of the same or a
// looser level (see the table there). Parentheses cost nothing
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>

int f(int a) {
  return a * 2;
}

int main(void) {
  int a, b, c, x;

  a = 2; b = 3; c = 4;

  // tighter first, then looser: C's precedence holds
  printf("tight first:\t10 1 18 1\n\t\t");
  x = a * b + c; printf("%d ", x);
  x = a + 1 < c; printf("%d ", x);
  x = c * c + a; printf("%d ", x);
  x = c - b - a + 2; printf("%d\n", x);

  // the rewrites of "35 + 7 * 4" (an error): all the same code
  printf("35+(7*4):\t63 63 63\n\t\t");
  x = 35 + (7 * 4); printf("%d ", x);
  x = 7 * 4 + 35; printf("%d ", x);
  x = (7 * 4) + 35; printf("%d\n", x);

  printf("parens:\t\t22 14 1 13\n\t\t");
  x = (c * c) + (a * 3); printf("%d ", x);
  x = a + (b * c); printf("%d ", x);
  x = (a & b) == a; printf("%d ", x);
  x = (b << 2) + 1; printf("%d\n", x);

  // compares are values 0 / 1; ! binds tight
  printf("compares:\t1 0 1 0 1 2\n\t\t");
  x = a < b; printf("%d ", x);
  x = a == b; printf("%d ", x);
  x = a != b; printf("%d ", x);
  x = !a; printf("%d ", x);
  x = !x; printf("%d ", x);
  x = !0 + 1; printf("%d\n", x);

  printf("operators:\t1 1 2 7 5 12 0\n\t\t");
  x = 17 % 4; printf("%d ", x);
  x = b / 2; printf("%d ", x);
  x = b & 2; printf("%d ", x);
  x = b | 4; printf("%d ", x);
  x = 6 ^ b; printf("%d ", x);
  x = b << 2; printf("%d ", x);
  x = b >> 2; printf("%d\n", x);

  // calls anywhere in an expression, casts
  printf("calls casts:\t10 44 45\n\t\t");
  x = f(a) + f(b); printf("%d ", x);
  x = 300; x = (char)x; printf("%d ", x);
  x = 300; x = (char)x + 1; printf("%d\n", x);

  // negative literals, && || ?:, op=, assignments in expressions
  printf("neg logic:\t5 1 0 7\n\t\t");
  a = 2; b = 0;
  x = -5 + 10; printf("%d ", x);
  x = 0; if (a && a < 9) x = 1; printf("%d ", x);
  x = 0; if (b || a > 9) x = 1; printf("%d ", x);
  x = a ? 7 : 8; printf("%d\n", x);

  printf("op=:\t\t14 2 63 165 10 48 75\n\t\t");
  x = 100; x /= 7; printf("%d ", x);
  x %= 4; printf("%d ", x);
  x = 9; x *= 7; printf("%d ", x);
  x = 0x5a; x ^= 0xff; printf("%d ", x);
  x = 0x5a; x &= 0x0f; printf("%d ", x);
  x = 3; x <<= 4; printf("%d ", x);
  x = 300; x >>= 2; printf("%d\n", x);

  printf("assign in:\t33 43 56\n\t\t");
  a = b = 3; x = a * 10 + b; printf("%d ", x);
  x = (a = 3) + 1; x = x * 10 + a; printf("%d ", x);
  a = 5; x = a++; x = x * 10 + a; printf("%d\n", x);

  return 0;
}
