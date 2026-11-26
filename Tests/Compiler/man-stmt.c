// man-stmt.c - MeteoriC-man.md 6: statements and the loop forms
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>

int main(void) {
  int i, n, s, a, d;

  a = 3;
  printf("if else:\t3 y big\n\t\t");
  if (a == 1) printf("1 "); else if (a == 3) printf("3 "); else printf("x ");
  if (a) printf("y ");
  if (a > 1) { if (a > 5) printf("huge\n"); else printf("big\n"); }
  else printf("small\n");

  printf("while:\t\t6\n\t\t");
  i = 0; s = 0;
  while (i < 4) { s += i; i++; }
  printf("%d\n", s);

  // the counting for: one loop op (< and <= cost the same)
  printf("for up:\t\t10 15 6\n\t\t");
  s = 0; for (i = 0; i < 5; i++) s += i; printf("%d ", s);
  s = 0; for (i = 1; i <= 5; i++) s += i; printf("%d ", s);
  n = 4; s = 0; for (i = 0; i < n; i++) s += i; printf("%d\n", s);

  printf("for down:\t15 15 10\n\t\t");
  s = 0; for (i = 5; i; i--) s += i; printf("%d ", s);
  s = 0; for (i = 5; i > 0; i--) s += i; printf("%d ", s);
  s = 0; for (i = 5; i-- > 0; ) s += i; printf("%d\n", s);

  printf("for step var:\t20\n\t\t");
  d = 2; s = 0;
  for (i = 0; i < 10; i += d) s += i;
  printf("%d\n", s);

  printf("for steps:\t20 30 28 36\n\t\t");
  s = 0; for (i = 0; i < 10; i += 2) s += i; printf("%d ", s);
  s = 0; for (i = 12; i > 0; i -= 3) s += i; printf("%d ", s);
  s = 0; for (i = 0; i < 8; ++i) s += i; printf("%d ", s);
  s = 0; for (i = 8; i > 0; --i) s += i; printf("%d\n", s);

  printf("for parts out:\t5 7 10\n\t\t");
  i = 0; for (; i < 5;) i++; printf("%d ", i);
  i = 0; for (;;) { i++; if (i > 6) break; } printf("%d ", i);
  i = 0; s = 0; for (; i < 5; i++) s += 2; printf("%d\n", s);

  printf("break continue:\t12 3\n\t\t");
  s = 0;
  for (i = 0; i < 9; i++) { if (i == 3) continue; if (i == 6) break; s += i; }
  printf("%d ", s);
  n = 5;
  for (i = 0; i < n; i++) if (i * i == 9) break;
  printf("%d\n", i);
  return 0;
}
