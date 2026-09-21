// Gemeni generated, crap!

#include <stdio.h>

// 8.8 Fixed-Point Configuration
// 1.0 in fixed point is 1 << 8 = 256
#define SHIFT 8
#define ONE   256

// Convert a normal number to fixed point at compile time
#define TO_FIX(x) ((int)((x) * ONE/10))


#if 1

  #define MUL(a, b)   (((long)(a)*(b))>>SHIFT)
  #define SQR(a)      (((long)(a)*(a))>>SHIFT)
  #define ADD(a, b)   ((a)+(b))
  #define SUB(a, b)   ((a)-(b))

#endif



#ifndef __CC65__
int main() {
  int argc; char** argv;
#else
int main(int argc, char** argv) {
#endif  
  
  int x, y;
  int max_iter = 16; // Low iterations for fast 8-bit rendering

  // Screen bounds mapped to the Mandelbrot complex plane
  int x_start = TO_FIX(-20);
  int x_end   = TO_FIX(5);
  int y_start = TO_FIX(-125);
  int y_end   = TO_FIX(125);

  // Calculate step sizes across our 40x28 grid
  int x_step = (x_end - x_start) / 40;
  int y_step = (y_end - y_start) / 28;

  int cr, ci; // Complex constant C (Real, Imaginary)
  int zr, zi; // Complex number Z (Real, Imaginary)
  int zr2, zi2; // Z squared components
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
        zr2 = MUL(zr, zr);
        zi2 = MUL(zi, zi);

        // Escape check: Length squared > 4.0 (4 * 256 = 1024)
        if ((ADD(zr2, zi2)) > 1024) break;

        // Z = Z^2 + C
        // zi = 2*zr*zi + ci -> 2*zr*zi is rewritten as (zr*zi) >> 7
        zi = (MUL(zr, zi) >> (SHIFT - 1)) + ci;
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

      cr += x_step;
    }
    printf("\n"); // Move to the next screen line
    ci += y_step;
  }

  return 0;
  (void)argc; (void)argv;
}
