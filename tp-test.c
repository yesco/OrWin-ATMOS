// equivalent of tp-test.p for genchmark/speed

// - ./tp => 6502 compiled
// 127,395 sum                                   127 c / loop


// - ./tpi => ivm
// 1,373,911                                    1373 c / loop

// - cc65 -Oirs
//  38,072 cycles no loop
// 110,545 sum   (/ (- 110545 38072) 1000) =      72 c / loop
// 285,548 fsum  (/ (- 285548 38072) 1000) =     247 c / loop

// - oscar
// 15525  no loop
// 17748  sum  - I guess precomputed
// 17748  fsum -

// - oscar -bc 
//    222,524
//    238,840  (/ (-  238840 222524) 1000)        16 c / loop
//  1,078,343  (/ (- 1078343 222524) 1000)       855 c / loop

// - oscar -O0 -bc
//    328,531 nothing
//    991,996 sum  (/ (-  991996 328531) 1000)   663 c / loop
//  1,413,474 fsum (/ (- 1413474 328531) 1000)  1084 c / loop

#include <stdio.h>

int Mul2(int val) {
  return val+val;
}

int AddTen(int val) {
  return val + Mul2(5);
}

int Two(int a, int b) {
 return a + b;
}

int alpha, beta, i, sum, fsum;
   
int main(void){
  // Test: Direct literal string compilation
  printf("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!\n");

  // Test: 16-Bit variable immediate assignment
  alpha= 7;

  // Test: Complex math addition using our functional JSR routine
  beta= AddTen(Mul2(alpha)+1) + Mul2(11);

  // Test: Printing numeric 47 value from memory
  printf("%d\n", beta);

  // Test: 54 = Two parameter function calling
  printf("Two:\n");
  printf("%d\n", Two(alpha, beta));

  //  printf("Magical injected input parameter X=\n");
  //  printf("%d\n", X); // is 42 }

  // Test: looping sum
  sum= 0;
  for(i=1; i<=0; i++) sum+= i;
//  for(i=1; i<=1000; i++) sum+= i;
  printf("Sum:\n");
  printf("%u\n", sum);

  //Test: looping funsum }
  fsum= 0;
//  for(i=1; i<=0; i++) fsum= Two(fsum, i);
  for(i=1; i<=1000; i++) fsum= Two(fsum, i);
  printf("FumSum:\n");
  printf("%u\n", fsum);

#if 0
// no care for now
  { Test Case 6: Literal subtraction }
   writeln("Subtraction tests:");
   writeln(20 - 3);       { expected 17 }
   writeln(100 - 10 - 5); { expected 85 }

   { Test Case 7: Subtract from a variable }
   writeln(alpha - 2);    { expected 5 }

   { Test Case 8: Function call on the left }
   writeln(Mul2(alpha) - 3); { expected 11 }

   { Test Case 9: Function call on the right }
   writeln(100 - Mul2(7));   { expected 86 }

   { Test Case 10: Subtraction with a function on each side }
   writeln(Mul2(10) - Mul2(3)); { expected 14 }
#endif

   return 0;
}
