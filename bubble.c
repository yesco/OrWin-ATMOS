// bubble sort of N pseudo random numbers (xorshift16)

int seed = 1;

int rnd() {
  seed = seed ^ (seed << 7);
  seed = seed ^ (seed >> 9);
  seed = seed ^ (seed << 8);
  return seed;
}

int main() {
  int i, j, n, k, x, y, t, sum, bad;
  int *a;

  n = 1000;
  a = malloc(n * 2);
  for (i = 0; i < n; i++) a[i] = rnd() % 10000;

#pragma TIMED "init"
  for (i = 0; i < n; i++) {
    j = 0;
    while (j < (n - 1 - i)) {
      x = a[j]; k= j+1; y= a[k];
      if (x > y) {
        a[j] = y;
        a[k] = x;
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
