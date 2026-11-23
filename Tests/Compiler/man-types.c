// man-types.c - MeteoriC-man.md 3: types. MC65 only: int is 16 bit and
// unsigned here (gcc: 32 bit, signed), the expected values are ours
//
// Each case: its name and the values we give, then (indented) what we got.

#include <stdio.h>

int main(void) {
  int x;
  unsigned int u;
  unsigned char c;
  char d;
  word w;

  // 16 bit, unsigned: wraps at 65536
  printf("int 16 bit:\t65535 0 24464 40000\n\t\t");
  x = 0 - 1; printf("%u ", x);
  x = x + 1; printf("%u ", x);
  x = 300 * 300; printf("%u ", x);
  u = 40000; printf("%u\n", u);

  // char: 0..255, unsigned
  printf("char 8 bit:\t4 200 255\n\t\t");
  c = 250; c = c + 10; printf("%d ", c);
  d = 200; printf("%d ", d);
  c = 0; c--; printf("%d\n", c);

  printf("word:\t\t65535\n\t\t");
  w = 65535; printf("%u\n", w);

  // literals: decimal hex binary octal (0o17 or, as in C, 017) char
  printf("literals:\t31 5 15 15 97 10 92\n\t\t");
  x = 0x1f; printf("%d ", x);
  x = 0b101; printf("%d ", x);
  x = 0o17; printf("%d ", x);
  x = 017; printf("%d ", x);
  x = 'a'; printf("%d ", x);
  x = '\n'; printf("%d ", x);
  x = '\\'; printf("%d\n", x);

  printf("sizeof:\t\t2 1 2 2\n\t\t");
  x = sizeof(int); printf("%d ", x);
  x = sizeof(char); printf("%d ", x);
  x = sizeof(char *); printf("%d ", x);
  x = sizeof(int *); printf("%d\n", x);

  // the sign: test the top bit
  printf("sign bit:\tneg\n\t\t");
  x = 0 - 3;
  if (x & 0x8000) printf("neg\n"); else printf("pos\n");
  return 0;
}
