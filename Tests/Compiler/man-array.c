// man-array.c - MeteoriC-man.md 8: arrays (static: zeroed or initialized
// at load), indexing, the memset idiom
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference (sizeof in ints / chars: the same).

#include <stdio.h>
#include <string.h>

#define N 16

char buf[80];
int w[10];
int t[] = {1, 2, 3};
int z[4] = {1, 2};
char msg[] = "hello";
int q[N];

int sum(int *p, int n) {
  int i, s;
  s = 0;
  for (i = 0; i < n; i++) s += p[i];
  return s;
}

int main(void) {
  int i, x;

  printf("zeroed:\t\t0 0\n\t\t");
  printf("%d ", buf[5]);
  printf("%d\n", w[9]);

  printf("initialized:\t6 2 0 101\n\t\t");
  x = t[0] + t[1] + t[2]; printf("%d ", x);
  printf("%d ", z[1]);
  printf("%d ", z[3]);
  printf("%d\n", msg[1]);

  printf("sizeof:\t\t80 3 6\n\t\t");
  x = sizeof(buf); printf("%d ", x);
  x = sizeof(t) / sizeof(int); printf("%d ", x);
  x = sizeof(msg); printf("%d\n", x);

  // index: a var, a const, an expression; = only on elements
  printf("index:\t\t300 9 7 18\n\t\t");
  i = 2;
  w[i] = 300; printf("%d ", w[i]);
  w[i + 1] = 9; printf("%d ", w[3]);
  w[3] = w[3] - 2; printf("%d ", w[i + 1]);
  q[N - 1] = 18; printf("%d\n", q[15]);

  // an array is its address: an int * parameter
  printf("as pointer:\t6\n\t\t");
  printf("%d\n", sum(t, 3));

  // the memset idiom (recognized: one op) and memset()
  printf("memset:\t\t7 7 0 2\n\t\t");
  for (i = 0; i < 80; i++) buf[i] = 7;
  printf("%d ", buf[0]);
  printf("%d ", buf[79]);
  for (i = 0; i < 10; i++) w[i] = 0;
  printf("%d ", w[2]);
  memset(buf, 2, 80);
  printf("%d\n", buf[40]);
  return 0;
}
