// from wikipedia byte sieve: flags a static array (byte-sieve-primes.c:
// malloc). Expected: 10 iterations, primes: 1899
// -

#include <stdio.h>

#define SIZE   8190
#define SIZEPL 8191

unsigned char flags[SIZEPL];

int main() {
  unsigned int i, prime, k, count, iter; 

  
  putz("10 iterations\n");

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

        // TODO: for-loop with clear stride...
        while (k <= SIZE) { 
          flags[k] = 0;
          k += prime; 
        }

        count = count + 1;

      }
    }
  }

  putz("primes: "); putnl(count);

  return 0;
}
