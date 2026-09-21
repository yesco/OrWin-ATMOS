#include <stdio.h>

#define MAIN

#ifdef USE_GLOAT
  #include "gloat16.c"
#endif

// 8.8 Fixed-Point Configuration
// 1.0 in fixed point is 1 << 8 = 256
#define SHIFT 8
#define ONE   256

// Convert a normal number to fixed point at compile time
// Maps dynamically through atog using stringification while matching your exact syntax layout
#ifdef GLOAT16_H

  #define FLOAT       gloat16

  // unit is "cUnit" (1/100th)
  #define FIXTOFLOAT(x) (atog(#x))

  #define MUL(a, b)   (gmul(a, b))
  #define DIV(a, b)   (gdiv(a, b))
  #define SQR(a)      (gmul(a, a))
  #define ADD(a, b)   (gadd(a, b))
  #define SUB(a, b)   (gsub(a, b))

  // Inverted sign check: if Bit 14 is 0, the result is positive (a > b)

// TODO: NOT handling alt format...

  #define CMP(a, b)   (((SUB(a, b) & 0x4000)? -1: +1))

//  #define CMP(a, b)   (((SUB(a, b) & 0x4000) == 0) && (SUB(a, b) != 0x8000) ? 1 : -1)

#else

  // fixpoint math
  #define FLOAT        int

  #define ONE          256
 
  // Convert a normal number to fixed point at compile time
  // unit is "cUnit" (1/100th)
  #define FIXTOFLOAT(x)  ((int)((long)(x) * ONE/100))

  #define MUL(a, b)   (((long)(a)*(b))>>SHIFT)
  #define DIV(a, b)   (((((long)(a))<<SHIFT)/(b))>>SHIFT)
  #define SQR(a)      (((long)(a)*(a))>>SHIFT)
  #define ADD(a, b)   ((a)+(b))
  #define SUB(a, b)   ((a)-(b))

  #define CMP(a, b)   ((a)<(b)?-1:(a)==(b)?0:+1)

  // TODO: add comparison using plain float for oscar64!

#endif



#ifndef __CC65__
int main() {
  int argc; char** argv;
#else
int main(int argc, char** argv) {
#endif  
  
  int max_iter = 16; // Low iterations for fast 8-bit rendering

  // Generic constants
  FLOAT onehundred= FIXTOFLOAT(10000); // 100
  FLOAT two       = FIXTOFLOAT(  200); //   2

  // Constants and step bounds right at the beginning of main

// THESE WERE WRONG, BUT GAVE IMAGE!
//  // Screen bounds mapped to the Mandelbrot complex plane
//  int x_start = TO_FIX(-20);
//  int x_end   = TO_FIX(5);
//  int y_start = TO_FIX(-125);
//  int y_end   = TO_FIX(125);

  FLOAT x_start   = FIXTOFLOAT(-200);  //  -2.00
  FLOAT x_end     = FIXTOFLOAT(  50);  //    0.5
  FLOAT y_start   = FIXTOFLOAT(-125);  //  -1.25
  FLOAT y_end     = FIXTOFLOAT( 125);  //   1.25
//  FLOAT y_start   = FIXTOFLOAT(-1250);  //  -1.25
//  FLOAT y_end     = FIXTOFLOAT( 1250);  //   1.25

  FLOAT rows      = FIXTOFLOAT( 2800); //  28
  FLOAT cols      = FIXTOFLOAT( 4000); //  40
  
  // Calculate step sizes across our 40x28 grid
  FLOAT x_step    = DIV(SUB(x_end, x_start), cols);
  FLOAT y_step    = DIV(SUB(y_end, y_start), rows);

  FLOAT escape    = FIXTOFLOAT( 400); //  4.0

  FLOAT cr, ci; // Complex constant C (Real, Imaginary)
  FLOAT zr, zi; // Complex number   Z (Real, Imaginary)
  FLOAT zr2, zi2; // Z squared components

  int x, y;
  int iter;
  int color;

  ci = y_start;
  for (y = 0; y < 28; y++) {
    cr = x_start;
    for (x = 0; x < 40; x++) {
      zr = 0;
      zi = 0;
      iter = 0;

      while (iter < max_iter) {
        // Fixed point multiplication requires shifting down by 8 bits
        // to correct the scale: (A * B) >> 8
        zr2 = SQR(zr);
        zi2 = SQR(zi);

        if ((CMP(ADD(zr2, zi2), escape)) > 0) break;

        // Z = Z^2 + C
        // zi = 2*zr*zi + ci -> 2*zr*zi is rewritten as (zr*zi) >> 7
        // Shift correction is handled natively inside the log multiplication domain
        zi = ADD(MUL(MUL(zr, zi), two), ci);
        zr = ADD(SUB(zr2, zi2), cr);
                
        iter++;
      }

      // Map iteration count down to a standard 0 to 7 value
      if (iter == max_iter) {
        color = 0; // Black inside the set
      } else {
        //color = 1 + (iter % 7); // Iteration maps to 1-7
        color = iter & 7;
      }

      // Output the raw single-digit character (0 to 7)
      printf("%d", color);

      // Fast, safe progression along the log line using your table-driven gadd primitive
      cr = ADD(cr, x_step);
    }
    printf("\n"); // Move to the next screen line
    ci = ADD(ci, y_step);
  }

  return 0;
  (void)argc; (void)argv;
}
