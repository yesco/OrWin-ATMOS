// man-func.c - MeteoriC-man.md 7: functions (define before use; no
// recursion: use loops)
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>

int g;

int add(int a, int b) {
  return a + b;
}

int sum5(int a, int b, int c, int d, int e) {
  return a + b + c + d + e;
}

void set(void) {
  g = 7;
}

int dbl(int a) {
  return a * 2;
}

// a loop instead of recursion
int fact(int n) {
  int r;
  r = 1;
  while (n > 1) { r = r * n; n--; }
  return r;
}

int main(void) {
  int x;

  printf("calls:\t\t5 15 7\n\t\t");
  printf("%d ", add(2, 3));
  printf("%d ", sum5(1, 2, 3, 4, 5));
  set();
  printf("%d\n", g);

  printf("in exprs:\t6 8 6 1\n\t\t");
  x = dbl(1) + dbl(2); printf("%d ", x);
  x = dbl(dbl(2)); printf("%d ", x);
  x = add(dbl(1), dbl(2)); printf("%d ", x);
  if (dbl(2) == 4) printf("1\n"); else printf("0\n");

  printf("fact loop:\t120 5040\n\t\t");
  printf("%d ", fact(5));
  printf("%d\n", fact(7));
  return 0;
}
