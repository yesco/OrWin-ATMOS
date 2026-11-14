// from wikipedia byte sieve
// -

#include <stdio.h>
#include <stdlib.h>

const int SIZE = 8190;
const int SIZEPL = 8191;

char *flags;

int main() {
  int i, prime, k, count, iter; 

  flags = malloc(SIZEPL << 1);
  
  printf("10 iterations\n");

  for (iter = 1; iter <= 10; iter++) {

    count = 0; 

    // recognized as memset! save bytes and time!
    for (i = 0; i < SIZEPL; i++)
      flags[i] = 1;

    for (i = 0; i <= SIZE; i++) { 

      if (flags[i]) {
        prime = i + i + 3; 
        k = i + prime; 

        while (k <= SIZE) { 
          flags[k] = 0;
          k += prime; 
        }

        count = count + 1;

      }
    }
  }

  putz("primes: "); printf("%u \n", count);
}
