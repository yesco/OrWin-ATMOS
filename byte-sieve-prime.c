// from wikipedia byte sieve
// -

#include <stdio.h>
#include <stdlib.h>

const int SIZE = 8190;
const int SIZEPL = 8191;

char *flags;

int main() {
  int i, prime, k, count, iter; 

  flags = malloc(SIZEPL);
  
  printf("10 iterations\n");

  // < is incorect so use <= lol
  for (iter = 1; iter <= 10; iter++) {

    count = 0; 

    // recognized as memset! save bytes and time!
    // TODO: currently only using < ...
    for (i = 0; i < SIZEPL; i++)
      flags[i] = 1;

    // < is incorect so use <= lol
    for (i = 0; i <= SIZE; i++) { 

      if (flags[i]) {
        prime = i + i + 3; 
        k = i + prime; 

        printf("----------------");
        printf("%d\n", prime);
      
        // TODO: for-loop with clear stride...
        while (k <= SIZE) { 
          printf("%d\n", k);
          flags[k] = 0;
          k += prime; 
        }

        count = count + 1;

      }
    }
  }

  putz("primes: "); printf("%u \n", count);
}
