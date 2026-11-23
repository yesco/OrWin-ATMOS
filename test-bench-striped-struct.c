// test-bench-striped-struct.c - an array of structs, plain SoA vs striped
//
//   ./parse c.bnf test-bench-striped-struct.c      ( ): plain SoA
//   ./parse -S c.bnf test-bench-striped-struct.c   a b: striped
//
// 32 particles (4 int fields), 200 rounds: move, bounce at 1000. Prints
// the sum of all x + y (gcc: the same).
// Measured (sim65 cycles, the run only; the JIT with -z):
//                    plain       striped
//   JIT -z          2.90 M      1.43 M     6502 code 764 -> 463 B
//   asm VM         12.76 M     13.67 M
//   cc65 C VM      32.07 M     42.39 M
// Striped: LDY i; LDA lo,Y; LDX hi,Y on the JIT; the interpreters do two
// byte loads instead of one word: a bit slower there.

#include <stdio.h>

struct p { int x; int y; int dx; int dy; };
struct p ps[32];

int main(void) {
  int i, t, s;

  // start: spread out, speeds 1..4, 2..3
  for (i = 0; i < 32; i++) {
    ps[i].x = i * 7; ps[i].y = i * 3;
    ps[i].dx = 1 + (i & 3); ps[i].dy = 2 + (i & 1);
  }

  // move, bounce
  for (t = 0; t < 200; t++)
    for (i = 0; i < 32; i++) {
      ps[i].x += ps[i].dx;
      ps[i].y += ps[i].dy;
      if (ps[i].x > 1000) ps[i].dx = 0 - ps[i].dx;
      if (ps[i].y > 1000) ps[i].dy = 0 - ps[i].dy;
    }

  // result
  s = 0;
  for (i = 0; i < 32; i++) s = s + ps[i].x + ps[i].y;
  printf("%u\n", s);
  return 0;
}
