// quicksort of N pseudo random numbers (xorshift16), no recursion:
// a stack of ranges (lo, hi) still to sort

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
  int i, j, n, t, lo, hi, p, sp, sum, bad, maxsp;
  int *a;
  int *slo;
  int *shi;

  n = 10000;
  a = malloc(n * sizeof(int));
  slo = malloc(32 * sizeof(int));
  shi = malloc(32 * sizeof(int));
  for (i = 0; i < n; i++) a[i] = rnd();

#pragma TIMED "init"
  sp = 0; maxsp = 0;
  slo[0] = 0; shi[0] = n - 1; sp = 1;
  while (sp > 0) {
    sp--;
    lo = slo[sp]; hi = shi[sp];
    {
      // Lomuto partition, pivot = middle element moved to hi
      p = (lo + hi) >> 1;
      t = a[p]; a[p] = a[hi]; a[hi] = t;
      p = a[hi];
      i = lo;
      j = lo;
      while (j < hi) {
        if (a[j] < p) {
          t = a[i]; a[i] = a[j]; a[j] = t;
          i++;
        }
        j++;
      }
      t = a[i]; a[i] = a[hi]; a[hi] = t;
      // push the bigger part first: stack stays small (log n).
      // Only ranges of 2 or more (unsigned: no i - 1 below 0)
      if ((i - lo) > (hi - i)) {
        if ((lo + 1) < i) { slo[sp] = lo; shi[sp] = i - 1; sp++; }
        if ((i + 1) < hi) { slo[sp] = i + 1; shi[sp] = hi; sp++; }
      } else {
        if ((i + 1) < hi) { slo[sp] = i + 1; shi[sp] = hi; sp++; }
        if ((lo + 1) < i) { slo[sp] = lo; shi[sp] = i - 1; sp++; }
      }
      if (sp > maxsp) maxsp = sp;
    }
  }
#pragma TIMED "qsort"
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
  printf("max stack: "); printf("%u\n", maxsp);
  return 0;
}
