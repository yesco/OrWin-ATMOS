// test-break.c - break; and continue; in every loop form of c.bnf
//
// Plain C: gcc test-break.c && ./a.out gives the reference output.
// One line per case: a letter, the values the body printed, and "= v":
// the loop variable after the loop (break leaves it where it was, a
// continue in a for still does the step).
// Only the printf forms the grammar knows: "%d " and "%d\n".

#include <stdio.h>

int eq(int v) {
  putchar('=');
  putchar(' ');
  printf("%d ", v);
  return 0;
}

int main(void) {
  int i, j, n, s;

  // --- for i <= const / i < const (t / u, closed by n) --------------------
  putchar('A'); putchar(' ');
  for (i = 0; i <= 10; i++) { if (i == 5) break; printf("%d ", i); }
  eq(i);
  for (i = 0; i < 10; i++) { if (i & 1) continue; printf("%d ", i); }
  eq(i); putchar('\n');

  // limit a variable
  putchar('B'); putchar(' ');
  n = 8;
  for (i = 2; i < n; i++) { if (i == 4) continue; if (i == 6) break; printf("%d ", i); }
  eq(i);
  for (i = 2; i <= n; i++) { if (i > 3) break; printf("%d ", i); }
  eq(i); putchar('\n');

  // body starts by reading the loop var (the JIT keeps it in AX)
  putchar('C'); putchar(' ');
  for (i = 1; i <= 9; i++) { printf("%d ", i); if (i == 3) continue; if (i == 7) break; }
  eq(i); putchar('\n');

  // --- while (v <= lim) / while (v < lim) (t / u closed by B) -------------
  putchar('D'); putchar(' ');
  i = 0;
  while (i <= 20) { i++; if (i == 3) continue; if (i == 6) break; printf("%d ", i); }
  eq(i);
  i = 0; n = 9;
  while (i < n) { i++; if (i & 1) continue; printf("%d ", i); }
  eq(i); putchar('\n');

  // --- while (E) ----------------------------------------------------------
  putchar('E'); putchar(' ');
  i = 10;
  while (i) { i--; if (i == 7) continue; if (i == 4) break; printf("%d ", i); }
  eq(i);
  i = 0;
  while (1) { i++; if (i > 4) break; printf("%d ", i); }
  eq(i); putchar('\n');

  // --- for (i = N; i--; ), for (i = N; i; i--) and friends ----------------
  putchar('F'); putchar(' ');
  for (i = 6; i--; ) { if (i == 4) continue; if (i == 1) break; printf("%d ", i); }
  eq(i);
  for (i = 6; i; i--) { if (i == 5) continue; if (i == 2) break; printf("%d ", i); }
  eq(i);
  for (i = 6; i > 0; i--) { if (i == 3) break; printf("%d ", i); }
  eq(i); putchar('\n');

  // --- for (i = E; E; i++) -------------------------------------------------
  putchar('G'); putchar(' ');
  for (i = 0; i != 9; i++) { if (i == 2) continue; if (i == 5) break; printf("%d ", i); }
  eq(i);
  s = 3;
  for (i = 0; i != 20; i += s) { if (i == 6) continue; if (i == 15) break; printf("%d ", i); }
  eq(i); putchar('\n');

  // --- nested: break / continue are the inner loop's ----------------------
  putchar('H'); putchar(' ');
  for (i = 0; i < 4; i++) {
    if (i == 2) continue;
    for (j = 0; j < 9; j++) {
      if (j == 1) continue;
      if (j == 3) break;
      printf("%d ", i * 10 + j);
    }
    eq(j);
  }
  eq(i); putchar('\n');

  // the outer loop breaks after the inner one ran
  putchar('I'); putchar(' ');
  s = 0;
  for (i = 0; i < 100; i++) {
    for (j = 0; j < 100; j++) { if (j == i) break; s = s + 1; }
    if (s > 10) break;
  }
  eq(i); eq(j); eq(s); putchar('\n');

  // statements after the loop see the right values
  putchar('J'); putchar(' ');
  n = 0;
  for (i = 0; i < 50; i++) { n = n + i; if (n > 20) break; }
  printf("%d ", n); eq(i); putchar('\n');

  return 0;
}
