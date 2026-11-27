// test-loops.c - every loop form of c3.bnf, with C semantics as the truth
//
// Plain C: gcc test-loops.c && ./a.out gives the reference output; the
// IVM VM and the JIT must print exactly the same.
// One line per case: a letter, the values the body saw (or count / sum),
// and "= v": the loop variable AFTER the loop. C leaves it at the first
// value that failed the test (i <= 10 -> 11); down loops see below.
// Body starts vary on purpose: the JIT treats a body that starts by reading
// the loop var (print i, k = p[i]) differently from one that does not
// (cnt++), and p[i] = v (a store, var as index) is a third case.

#include <stdio.h>
#include <stdlib.h>

// "= v ": the loop variable after the loop
int eq(int v) {
  putchar('=');
  putchar(' ');
  putn(v);
  return 0;
}

int main(void) {
  int i, j, k, n, s, cnt, sum, last;
  char *p;

  p = malloc(700);

  // --- for: i <= const (i/n), body starts with the var ------------------
  putchar('A'); putchar(' ');
  for (i = 7; i <= 12; i++) putn(i);
  eq(i); putchar('\n');

  // i < const (limit - 1 as the i/n limit)
  putchar('B'); putchar(' ');
  for (i = 7; i < 12; i++) putn(i);
  eq(i); putchar('\n');

  // start and limit expressions
  putchar('C'); putchar(' ');
  for (i = 3 + 4; i < 11 + 1; i++) putn(i);
  eq(i); putchar('\n');

  // start from a variable, limit a variable
  putchar('D'); putchar(' ');
  j = 5; n = 9;
  for (i = j; i <= n; i++) putn(i);
  eq(i);
  for (i = j; i < n; i++) putn(i);
  eq(i); putchar('\n');

  // --- zero trips: the body must not run --------------------------------
  putchar('E'); putchar(' ');
  cnt = 0;
  for (i = 9; i <= 5; i++) cnt++;
  putn(cnt); eq(i);
  cnt = 0;
  for (i = 5; i < 5; i++) cnt++;
  putn(cnt); eq(i);
  cnt = 0; n = 0;
  for (i = 1; i <= n; i++) cnt++;
  putn(cnt); eq(i);
  cnt = 0; n = 3;
  for (i = n; i < n; i++) cnt++;
  putn(cnt); eq(i); putchar('\n');

  // --- limits around a page: 255 256 511, body not reading the var -------
  putchar('F'); putchar(' ');
  cnt = 0;
  for (i = 0; i <= 255; i++) cnt++;
  putn(cnt); eq(i);
  cnt = 0;
  for (i = 0; i <= 256; i++) cnt++;
  putn(cnt); eq(i);
  cnt = 0;
  for (i = 0; i < 256; i++) cnt++;
  putn(cnt); eq(i);
  cnt = 0;
  for (i = 250; i <= 511; i++) cnt++;
  putn(cnt); eq(i); putchar('\n');

  // --- the same, body starts with k = p[i] (read, var as index) ----------
  for (i = 0; i <= 600; i++) p[i] = 1;
  putchar('G'); putchar(' ');
  sum = 0;
  for (i = 0; i <= 255; i++) { k = p[i]; sum = sum + k; }
  putn(sum); eq(i);
  sum = 0;
  for (i = 0; i <= 511; i++) { k = p[i]; sum = sum + k; }
  putn(sum); eq(i);
  sum = 0;
  for (i = 250; i <= 260; i++) { k = p[i]; sum = sum + k; }
  putn(sum); eq(i); putchar('\n');

  // --- body starts with p[i] = v (store, var as index) -------------------
  putchar('H'); putchar(' ');
  for (i = 10; i <= 300; i++) p[i] = 2;
  sum = 0;
  for (i = 0; i <= 310; i++) { k = p[i]; sum = sum + k; }
  putn(sum); eq(i); putchar('\n');

  // --- memset form: for (i = 0; i < n; i++) p[i] = v ----------------------
  putchar('I'); putchar(' ');
  n = 100;
  for (i = 0; i < n; i++) p[i] = 3;
  k = p[99]; putn(k);
  k = p[100]; putn(k);
  eq(i); putchar('\n');

  // --- nested ------------------------------------------------------------
  putchar('J'); putchar(' ');
  cnt = 0; sum = 0;
  for (i = 1; i <= 4; i++) {
    for (j = i; j <= 5; j++) { cnt++; sum = sum + j; }
  }
  putn(cnt); putn(sum); eq(i); putn(j);

  // --- generic for, step 2: the increment runs AFTER the body -------------
  // TODO grammar: for (k = 0; k < 20; k = k + 2) is not parsed (the
  //   generic rule wants expressions, k = .. is an assignment; and the
  //   increment's code must come after the body: needs a 2nd loop mark)
  //   gcc: K 18 = 20 10 = 20
  // putchar('K'); putchar(' ');
  // last = 99;
  // for (k = 0; k < 20; k = k + 2) { last = k; }
  // putn(last); eq(k);
  // cnt = 0;
  // for (k = 0; k < 20; k = k + 2) cnt++;
  // putn(cnt); eq(k); putchar('\n');

  // --- for with += s (U) ---------------------------------------------------
  putchar('L'); putchar(' ');
  s = 3; last = 99;
  for (k = 1; k <= 10; k += s) { last = k; }
  putn(last); eq(k); putchar('\n');

  // --- generic for with i++ / i-- and a general condition ------------------
  putchar('M'); putchar(' ');
  for (i = 0; i * 2 < 9; i++) putn(i);
  eq(i);
  for (i = 5; i != 1; i--) putn(i);
  eq(i); putchar('\n');

  // --- nested generic for (the old embryo's G) -------------------------------
  // TODO grammar: same as K.   gcc: N 6 = 6 6
  // putchar('N'); putchar(' ');
  // cnt = 0;
  // for (i = 0; i < 6; i = i + 2) {
  // for (j = 0; j < 6; j = j + 3) cnt++;
  // }
  // putn(cnt); eq(i); putn(j);

  // --- down loops (C semantics) ----------------------------------------------
  //   i--   and i-- > 0 : body sees N-1 .. 0, i ends at -1
  //   --i               : body sees N-1 .. 1, i ends at 0
  //   i; i--  and i > 0; i-- : body sees N .. 1, i ends at 0
  //   (the VM prints unsigned, so "= i + 1": -1 shows as 0, 0 as 1) ???
  putchar('O'); putchar(' ');
  for (i = 3; i--; ) putn(i);
  eq(i + 1);
  for (i = 3; i-- > 0; ) putn(i);
  eq(i + 1); putchar('\n');
  putchar('P'); putchar(' ');
  for (i = 3; --i; ) putn(i);
  eq(i + 1);
  for (i = 3; i; i--) putn(i);
  eq(i + 1);
  for (i = 3; i > 0; i--) putn(i);
  eq(i + 1); putchar('\n');
  putchar('Q'); putchar(' ');
  cnt = 0;
  for (i = 0; i; i--) cnt++;
  putn(cnt); eq(i + 1);
  cnt = 0;
  for (i = 0; i > 0; i--) cnt++;
  putn(cnt); eq(i + 1); putchar('\n');

  // --- while -----------------------------------------------------------------
  //   while (i <= const) / (i <= var): 'i' as the test, 'B' back
  putchar('R'); putchar(' ');
  i = 3;
  while (i <= 7) { putn(i); i++; }
  eq(i);
  i = 9;
  while (i <= 7) { putn(i); i++; }
  eq(i);
  i = 2; n = 5;
  while (i <= n) { putn(i); i++; }
  eq(i); putchar('\n');

  //   general condition
  putchar('S'); putchar(' ');
  i = 0;
  while (i < 5) { putn(i); i++; }
  eq(i);
  i = 10;
  while (i != 0) { i = i - 3; if (i < 3) i = 0; putn(i); }
  eq(i); putchar('\n');

  //   stride: body ends with k += s (the JIT's Y/A stride loop)
  putchar('T'); putchar(' ');
  s = 7; k = 3; cnt = 0; last = 99;
  while (k <= 100) { p[k] = 5; last = k; cnt++; k += s; }
  putn(cnt); putn(last); eq(k);
  s = 7; k = 3; cnt = 0;
  while (k <= 2) { cnt++; k += s; }
  putn(cnt); eq(k);
  s = 100; k = 0; cnt = 0;
  while (k <= 600) { p[k] = 6; cnt++; k += s; }
  putn(cnt); eq(k);
  k = p[3]; putn(k);
  k = p[10]; putn(k);
  k = p[600]; putn(k);

  //   stride, body reads p[k] first
  putchar('U'); putchar(' ');
  s = 50; k = 0; sum = 0;
  while (k <= 300) { j = p[k]; sum = sum + j; k += s; }
  putn(sum); eq(k); putchar('\n');

  return 42;
}

/* gcc test-loops.c && ./a.out  (K and N: see the TODOs above)
A 7 8 9 10 11 12 = 13 
B 7 8 9 10 11 = 12 
C 7 8 9 10 11 = 12 
D 5 6 7 8 9 = 10 5 6 7 8 = 9 
E 0 = 9 0 = 5 0 = 1 0 = 3 
F 256 = 256 257 = 257 256 = 256 262 = 512 
G 256 = 256 512 = 512 11 = 261 
H 602 = 311 
I 3 2 = 100 
J 14 50 = 5 6
L 10 = 13 
M 0 1 2 3 4 = 5 5 4 3 2 = 1 
O 2 1 0 = 0 2 1 0 = 0 
P 2 1 = 1 3 2 1 = 1 3 2 1 = 1 
Q 0 = 1 0 = 1 
R 3 4 5 6 7 = 8 = 9 2 3 4 5 = 6 
S 0 1 2 3 4 = 5 7 4 0 = 0 
T 14 94 = 101 0 = 3 7 = 700 5 5 6
U 31 = 350 
*/
