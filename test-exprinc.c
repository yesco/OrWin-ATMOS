// test-exprinc.c - ++x x++ --x x-- and x = E inside expressions
//
// Plain C: gcc test-exprinc.c && ./a.out gives the reference output.
// Only the printf forms the grammar knows: "%d " and "%d\n".
// (a = 3 + b = 7 is not C: = binds loosest, (3 + b) = 7 has no variable)

#include <stdio.h>

int main(void) {
  int a, b, x, y;
  char c, d;

  // --- int: the new value / the old one ---------------------------------------
  putchar('A'); putchar(' ');
  a = 5;
  x = ++a + 3;  printf("%d ", x); printf("%d ", a);
  x = a++ + 3;  printf("%d ", x); printf("%d ", a);
  x = --a * 2;  printf("%d ", x); printf("%d ", a);
  x = a-- * 2;  printf("%d ", x); printf("%d ", a);
  putchar('\n');

  // --- char: wraps at 255 / 0 ---------------------------------------------------
  putchar('B'); putchar(' ');
  c = 255; d = c++; printf("%d ", d); printf("%d ", c);
  c = 0; d = c--; printf("%d ", d); printf("%d ", c);
  c = 9; d = ++c; printf("%d ", d);
  putchar('\n');

  // --- x = E as an expression: chained, in parens, in a condition -------------
  putchar('C'); putchar(' ');
  x = y = 7; printf("%d ", x); printf("%d ", y);
  x = (b = 7) + 3; printf("%d ", x); printf("%d ", b);
  a = 0;
  while ((b = a++) < 3) printf("%d ", b);
  if ((c = 300) == 44) printf("%d ", c);      // a char: 300 & 255
  putchar('\n');

  // --- statements stay statements ---------------------------------------------
  putchar('D'); putchar(' ');
  a = 1; a++; ++a; a--; printf("%d ", a);
  for (a = 0; a < 10; ) a = a + 3;
  printf("%d\n", a);

  return 0;
}
