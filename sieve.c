#include <stdio.h>
#include <stdlib.h>

// Sieve of Eratosthenes — primes up to LIMIT-1
// LIMIT should stay within available IVM memory (flags[LIMIT] bytes)

int LIMIT= 8192;


int primes() {

// TODO: testif local/global makes difference in speed

  int* flags;
  int i;
  int j;
  int n;
  int count;
  int p;

  //flags = malloc(LIMIT * sizeof(*flags));
  flags = malloc(LIMIT * 2);
  
  // 1 = candidate, 0 = composite
  for (i = 0; i < LIMIT; i++) {
    flags[i] = 1;
  }
  flags[0] = 0;
  flags[1] = 0;

  count = 0;

  for (i = 2; i < LIMIT; i++) {

    if (flags[i]) {

      count++;

      // strike multiples: i*i, i*i+i, ...
      // (i*i may exceed 16-bit for large LIMIT; use running j)
      if (i < 128) { // TODO: what 128???
        
        for (j = i * i; j < LIMIT; j += i) {
          flags[j] = 0;
        }

      } else {

        // for larger i, start at 2*i to avoid i*i overflow
        for (j = i + i; j < LIMIT; j += i) {
          flags[j] = 0;
        }

      }
    }
  }

  printf("LIMIT "); printf("%u\n", LIMIT);
  printf("primes "); printf("%u\n", count);

  // print last new primes as a sanity check
  n = 0;
  printf("last: ");
  for (i = LIMIT - 1; i > 1; i--) {
    if (flags[i]) {
      printf("%u ", i);
      n = n + 1;
      if (n >= 5) {
        i = 1;          // break (no break keyword)
      }
    }
  }
  printf("\n");

  free(flags);
  
  return count;
}

int main() {
  int i, sum, n;

  sum = 0;
  n = 10;
  
//    sum += primes();
  
  for(i = 0; i <= n; i++)
    sum = sum + primes();
  
  printf("SUM "); printf("%u ", n); printf("times = ");
  printf("%u\n", sum);
  
  return sum;
}
