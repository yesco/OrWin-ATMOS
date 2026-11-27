// test-ternary.c - c ? a : b expressions, and &arr of a static array
//
// Plain C: gcc test-ternary.c && ./a.out gives the reference output.

#include <stdio.h>

char buf[10];

int pick(int a) { return a ? 10 : 20; }

int twice(int v) { return v + v; }

int main(void) {
  int c, y, z;
  char ch;
  char *p;

  // --- constants, a variable condition ------------------------------------
  putchar('A'); putchar(' ');
  c = 1;
  y = c ? 7 : 9;
  putn(y);
  c = 0;
  y = c ? 7 : 9;
  putn(y);
  putchar('\n');

  // --- a compare as the condition, expressions as values ------------------
  putchar('B'); putchar(' ');
  z = 3;
  y = z > 2 ? z + 100 : z - 1;
  putn(y);
  y = z < 2 ? z + 100 : z - 1;
  putn(y);
  putchar('\n');

  // --- nested (right to left): c ? a : d ? b : e ----------------------------
  putchar('C'); putchar(' ');
  y = z < 2 ? 1 : z == 3 ? 2 : 3;
  putn(y);
  z = 9;
  y = z < 2 ? 1 : z == 3 ? 2 : 3;
  putn(y);
  putchar('\n');

  // --- in a return, as an argument, in parens, in an if ---------------------
  putchar('D'); putchar(' ');
  putn(pick(0));
  putn(pick(5));
  putn(twice(z ? 4 : 5));
  y = (z ? 4 : 5) + 1;
  putn(y);
  if (z ? 0 : 1) putn(1); else putn(2);
  putchar('\n');

  // --- char values ------------------------------------------------------------
  putchar('E'); putchar(' ');
  ch = z > 5 ? 'y' : 'n';
  putchar(ch);
  ch = z > 50 ? 'y' : 'n';
  putchar(ch);
  putchar('\n');

  // --- &arr: a static array's address -------------------------------------
  putchar('F'); putchar(' ');
  p = (char*)&buf;                       // C: &buf is a char (*)[10]
  p[2] = 'x';
  putchar(buf[2]);
  putchar('\n');

  return 0;
}
