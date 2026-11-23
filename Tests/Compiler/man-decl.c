// man-decl.c - MeteoriC-man.md 4: variables and declarations
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>

#ifndef __MC65__
#define register
#endif

int g;
int h = 5;
const int K = 7;
register int r;

// a static local keeps its value between calls
int counter(void) {
  static int n;
  n++;
  return n;
}

int twice(int a) {
  return a * 2;
}

int main(void) {
  int a, b;
  int c = 3, d = 4;
  int e = twice(c) + 1;

  printf("globals:\t0 5 7\n\t\t");
  printf("%d ", g);
  printf("%d ", h);
  printf("%d\n", K);

  printf("several:\t3 7 7\n\t\t");
  a = 1; b = 2;
  printf("%d ", a + b);
  printf("%d ", c + d);
  printf("%d\n", e);

  // a declaration after statements, an initializer: any expression
  printf("later decl:\t8\n\t\t");
  int f = e + 1;
  printf("%d\n", f);

  printf("static local:\t3\n\t\t");
  counter(); counter();
  printf("%d\n", counter());

  printf("register:\t9\n\t\t");
  r = 9;
  printf("%d\n", r);
  return 0;
}
