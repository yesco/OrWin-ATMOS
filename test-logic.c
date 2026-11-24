// test-logic.c - && || (short-circuit, 0 / 1) and ^^ (ours: logical xor)
//
// Plain C: gcc test-logic.c && ./a.out gives the reference output. ^^ is
// not C: under __MC65__ only (the others compute the same another way).
// Only the printf forms the grammar knows: "%d " and "%d\n".

#include <stdio.h>

int calls;

// v, and count the calls: short-circuit skips them
int t(int v) {
  calls = calls + 1;
  return v;
}

int main(void) {
  int a, b, c, x;

  a = 1; b = 0; c = 5;

  // --- 0 / 1 results ---------------------------------------------------------
  putchar('A'); putchar(' ');
  printf("%d ", a && b);
  printf("%d ", a && c);
  printf("%d ", b || c);
  printf("%d ", b || b);
  printf("%d ", c && c);
  putchar('\n');

  // --- precedence: && over ||, compares over both, left to right ----------
  putchar('B'); putchar(' ');
  printf("%d ", a || b && b);
  printf("%d ", b && b || a);
  printf("%d ", c > 2 && c < 9);
  printf("%d ", c > 7 || c == 5);
  printf("%d ", a && b || c && a);
  printf("%d ", !b && (a || b));
  putchar('\n');

  // --- short-circuit: the right side only when needed ----------------------
  putchar('C'); putchar(' ');
  calls = 0; x = b && t(1); printf("%d ", x); printf("%d ", calls);
  calls = 0; x = a || t(1); printf("%d ", x); printf("%d ", calls);
  calls = 0; x = a && t(7); printf("%d ", x); printf("%d ", calls);
  calls = 0; x = b || t(0); printf("%d ", x); printf("%d ", calls);
  putchar('\n');

  // --- in if / while conditions ----------------------------------------------
  putchar('D'); putchar(' ');
  if (a && c > 3) printf("%d ", 1); else printf("%d ", 0);
  if (b || c < 3) printf("%d ", 1); else printf("%d ", 0);
  x = 0;
  while (x < 10 && x != 4) x++;
  printf("%d ", x);
  putchar('\n');

  // --- ^^: true when exactly one side is ---------------------------------------
  putchar('E'); putchar(' ');
#ifdef __MC65__
  printf("%d ", a ^^ b);
  printf("%d ", a ^^ c);
  printf("%d ", b ^^ b);
  printf("%d ", b ^^ c > 2);
#else
  printf("%d ", !a != !b);
  printf("%d ", !a != !c);
  printf("%d ", !b != !b);
  printf("%d ", !b != !(c > 2));
#endif
  putchar('\n');

  return 0;
}
