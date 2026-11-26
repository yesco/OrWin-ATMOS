// man-quirks.c - MeteoriC-man.md: the NOTE (silent) cases, what our
// compiler does where C differs. MC65 only: the expected values are ours
//
// Each case: its name and the values we give, then (indented) what we got.

#include <stdio.h>

int w[3] = {1, 2, 3};

int keep(void) {
  static int t[2] = {5, 0};
  t[0] = t[0] + 1;
  return t[0];
}

int main(void) {
  int x;
  int *p;

  // 16 bit, unsigned: %d prints unsigned, compares are unsigned
  printf("unsigned:\t65535 pos\n\t\t");
  x = 0 - 1;
  printf("%d ", x);
  if (x < 0) printf("neg\n"); else printf("pos\n");

  // an initialized local array needs static (C too: set once)
  printf("static array:\t6 7\n\t\t");
  printf("%d ", keep());
  printf("%d\n", keep());

  // ptr + n in an expression: bytes, not elements (p++ p += n scale)
  printf("ptr + 1:\t512 2\n\t\t");
  p = w + 1; printf("%d ", *p);
  p = w; p++; printf("%d\n", *p);
  return 0;
}
