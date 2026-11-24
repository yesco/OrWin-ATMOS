// test-ternary.c - c ? a : b expressions, and &arr of a static array
//
// Plain C: gcc test-ternary.c && ./a.out gives the reference output.
// Only the printf forms the grammar knows: "%d " and "%d\n".

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
  printf("%d ", y);
  c = 0;
  y = c ? 7 : 9;
  printf("%d ", y);
  putchar('\n');

  // --- a compare as the condition, expressions as values ------------------
  putchar('B'); putchar(' ');
  z = 3;
  y = z > 2 ? z + 100 : z - 1;
  printf("%d ", y);
  y = z < 2 ? z + 100 : z - 1;
  printf("%d ", y);
  putchar('\n');

  // --- nested (right to left): c ? a : d ? b : e ----------------------------
  putchar('C'); putchar(' ');
  y = z < 2 ? 1 : z == 3 ? 2 : 3;
  printf("%d ", y);
  z = 9;
  y = z < 2 ? 1 : z == 3 ? 2 : 3;
  printf("%d ", y);
  putchar('\n');

  // --- in a return, as an argument, in parens, in an if ---------------------
  putchar('D'); putchar(' ');
  printf("%d ", pick(0));
  printf("%d ", pick(5));
  printf("%d ", twice(z ? 4 : 5));
  y = (z ? 4 : 5) + 1;
  printf("%d ", y);
  if (z ? 0 : 1) printf("%d ", 1); else printf("%d ", 2);
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
