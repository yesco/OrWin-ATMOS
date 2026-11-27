// test-logic.c - && || (short-circuit, 0 / 1) and ^^ (ours: logical xor)
//
// Plain C: gcc test-logic.c && ./a.out gives the reference output. ^^ is
// not C: under __MC65__ only (the others compute the same another way).

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
  putn(a && b);
  putn(a && c);
  putn(b || c);
  putn(b || b);
  putn(c && c);
  putchar('\n');

  // --- precedence: && over ||, compares over both, left to right ----------
  putchar('B'); putchar(' ');
  putn(a || b && b);
  putn(b && b || a);
  putn(c > 2 && c < 9);
  putn(c > 7 || c == 5);
  putn(a && b || c && a);
  putn(!b && (a || b));
  putchar('\n');

  // --- short-circuit: the right side only when needed ----------------------
  putchar('C'); putchar(' ');
  calls = 0; x = b && t(1); putn(x); putn(calls);
  calls = 0; x = a || t(1); putn(x); putn(calls);
  calls = 0; x = a && t(7); putn(x); putn(calls);
  calls = 0; x = b || t(0); putn(x); putn(calls);
  putchar('\n');

  // --- in if / while conditions ----------------------------------------------
  putchar('D'); putchar(' ');
  if (a && c > 3) putn(1); else putn(0);
  if (b || c < 3) putn(1); else putn(0);
  x = 0;
  while (x < 10 && x != 4) x++;
  putn(x);
  putchar('\n');

  // --- ^^: true when exactly one side is ---------------------------------------
  putchar('E'); putchar(' ');
#ifdef __MC65__
  putn(a ^^ b);
  putn(a ^^ c);
  putn(b ^^ b);
  putn(b ^^ c > 2);
#else
  putn(!a != !b);
  putn(!a != !c);
  putn(!b != !b);
  putn(!b != !(c > 2));
#endif
  putchar('\n');

  return 0;
}
