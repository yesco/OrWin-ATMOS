// test-opts.c - the bytecode optimizations, one small case each
//
// Each case is done ROUNDS times: so their speed shows. Each line: its
// name and the values C gives, then (indented) what we got.
//
// The cases:
//
// - load:   globals and locals read a lot: with -O a one byte load
// - jump:   if (!x) and if (x == 0): with -O a one byte jump
// - acc:    x = ...; then x again: ACC still holds it, no load
// - vn:     a[j + 1] twice: j + 1 only once (value numbering)
// - swap:   t = a[i]; a[i] = a[j]; a[j] = t: one A op (CSE)
// - const:  if (0) ..., an unused constant: no code, no slot
// - frames: sq() and add3() locals: frames packed by the call tree
//
// Its impact, each one added to the ones above (./ivm-asm, cycles less
// the startup):
//
//   optimization             .ivm bytes          cycles
//   none                     399                 9465624
//   + value numbering        395   -1.0%         9438229   -0.3%
//   + CSE / swap             394   -0.3%         9427621   -0.1%
//   + -O (layout, peephole)  373   -5.3%         9380793   -0.5%
//   all                            -6.5%                   -0.9%
//
// The g++ loop is most of the time, and -O doesn't make its I op faster.
// More in MeteoriC-man.md, Optimizations (all the tests: -6.3% bytes).

#include <stdio.h>
#include <stdlib.h>

#define ROUNDS 200
#define UNUSED 12345

int g;


// frames: add3 calls sq, their locals don't overlap (-O: packed)

int sq(int x) {
  int y;
  y = x * x;
  return y;
}

int add3(int v) {
  int w;
  w = v + 3;
  return sq(w);
}


int main() {
  int r, k, j, s, t, i, n;
  int *a;

  a = malloc(40);

  printf("load:\t\t40000\n\t\t");
  g = 0;
  for (r = 0; r < ROUNDS; r++)
    for (k = 0; k < ROUNDS; k++)
      g++;
  printf("%u\n", g);

  printf("jump:\t\t100 100\n\t\t");
  s = 0; t = 0;
  for (r = 0; r < ROUNDS; r++) {
    k = r & 1;
    if (!k) s++;
    if (k == 0) t++;
  }
  printf("%u ", s); printf("%u\n", t);

  printf("acc:\t\t20100\n\t\t");
  s = 0;
  for (r = 0; r < ROUNDS; r++) {
    n = r + 1;
    s = s + n;
  }
  printf("%u\n", s);

  printf("vn:\t\t12000\n\t\t");
  for (k = 0; k < 10; k++) a[k] = k * 10;
  s = 0;
  for (r = 0; r < ROUNDS; r++) {
    j = 2;
    t = a[j + 1]; n = a[j + 1];
    s = s + t + n;
  }
  printf("%u\n", s);

  printf("swap:\t\t0 90\n\t\t");
  for (r = 0; r < ROUNDS; r++) {
    i = 0; j = 9;
    t = a[i]; a[i] = a[j]; a[j] = t;
  }
  printf("%u ", a[0]); printf("%u\n", a[9]);

  printf("const:\t\t7\n\t\t");
  s = 7;
  if (0) s = UNUSED;
  printf("%u\n", s);

  printf("frames:\t\t64\n\t\t");
  s = 0;
  for (r = 0; r < ROUNDS; r++) s = add3(5);
  printf("%u\n", s);

  free(a);
  return 0;
}
