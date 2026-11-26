// bubble sort of N pseudo random numbers (xorshift16)

#include <stdio.h>
#include <stdlib.h>

int seed = 1;

int rnd() {
  seed = seed ^ (seed << 7);
  seed = seed ^ (seed >> 9);
  seed = seed ^ (seed << 8);
  return seed;
}

int main() {
  int i, j, n, t, sum, bad;
  int *a;

  n = 1000;
  a = malloc(n * sizeof(int));
  for (i = 0; i < n; i++) a[i] = rnd() % 10000;

#pragma TIMED "init"
  for (i = 0; i < n; i++) {
    j = 0;
    while (j < (n - 1 - i)) {
      if (a[j] > a[j + 1]) {
        t = a[j];
        a[j] = a[j + 1];
        a[j + 1] = t;
      }
      j++;
    }
  }
#pragma TIMED "bubble"
  sum = 0; bad = 0;
  for (i = 0; i < n; i++) {
    sum = sum + a[i];
    if (i > 0) {
      if (a[i - 1] > a[i]) bad++;
    }
  }
  printf("first last: "); printf("%u ", a[0]); printf("%u\n", a[n - 1]);
  printf("sum: "); printf("%u\n", sum);
  printf("unsorted pairs: "); printf("%u\n", bad);
  return 0;
}
