// man-pointer.c - MeteoriC-man.md 9: pointers and strings (int * moves
// 2 bytes in p++ / p += n; char * reads and writes bytes)
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>
#include <stdlib.h>

int w[4] = {10, 20, 30, 40};
char s[] = "abc";

void swap(int *a, int *b) {
  int t;
  t = *a; *a = *b; *b = t;
}

int main(void) {
  int *p;
  int *q;
  char *c;
  int n, x;

  printf("int *:\t\t10 20 40 42\n\t\t");
  p = w;
  printf("%d ", *p);
  p++; printf("%d ", *p);
  n = 2; p += n; printf("%d ", *p);
  *p = 42; printf("%d\n", w[3]);

  printf("p[i]:\t\t30 7\n\t\t");
  p = w;
  printf("%d ", p[2]);
  p[1] = 7; printf("%d\n", w[1]);

  printf("swap:\t\t7 10\n\t\t");
  q = w; q++;
  swap(w, q);
  printf("%d ", w[0]);
  printf("%d\n", w[1]);

  // a string walk, *c++
  printf("strings:\t3 97 98\n\t\t");
  c = s; n = 0;
  while (*c) { n++; c++; }
  printf("%d ", n);
  c = s;
  x = *c++; printf("%d ", x);
  printf("%d\n", *c);

  printf("literal:\t104 101\n\t\t");
  c = "hello";
  printf("%d ", c[0]);
  printf("%d\n", c[1]);

  printf("malloc:\t\t7 null\n\t\t");
  p = malloc(4);
  p[1] = 7; printf("%d ", p[1]);
  free(p);
  p = 0;
  if (p == 0) printf("null\n");
  return 0;
}
