// test-cse.c - the bytecode CSE (ivm-gen.c gen_cse) and swap (gen_swap)
//
// "x OP y S t" computed twice (a[j + 1] twice, j + 1 into two temps):
// the 2nd is dropped and its reads use the 1st temp - but only when the
// value can't have changed. These cases must stay right:
// Each case: its name and the values C gives, then (indented) what we
// got:
//   same twice:     30 30
//                   30 30
// - same twice: a[j + 1] read twice: one j + 1
// - j changed:  j++ between: two different j + 1
// - store:      a[j + 1] = a[j + 1] + 1: read and write the same element
// - swap j+1:   t = a[j]; a[j] = a[j + 1]; a[j + 1] = t;  -> A (bytecode)
// - if:         the 2nd j + 1 inside an if body (a branch, no label)
// - loop:       the 2nd j + 1 in the next round (a label between: no CSE)

#include <stdio.h>
#include <stdlib.h>

int main() {
  int i, j, t, k, s;
  int *a;
  a = malloc(40);
  for (k = 0; k < 10; k++) a[k] = k * 10;

  putz("same twice:\t30 30\n\t\t");
  j = 2;
  s = a[j + 1]; t = a[j + 1];
  printf("%u ", s); printf("%u ", t);
  putz("\n");

  putz("j changed:\t30 40\n\t\t");
  j = 2;
  s = a[j + 1]; j++; t = a[j + 1];
  printf("%u ", s); printf("%u ", t);
  putz("\n");

  putz("store:\t\t51 51\n\t\t");
  j = 4;
  a[j + 1] = a[j + 1] + 1;
  printf("%u ", a[5]); printf("%u ", a[j + 1]);
  putz("\n");

  putz("swap j+1:\t70 60 60\n\t\t");
  j = 6;
  t = a[j]; a[j] = a[j + 1]; a[j + 1] = t;
  printf("%u ", a[6]); printf("%u ", a[7]); printf("%u ", t);
  putz("\n");

  putz("if:\t\t90 80\n\t\t");
  j = 8;
  if (a[j] < a[j + 1]) {
    t = a[j]; a[j] = a[j + 1]; a[j + 1] = t;
  }
  printf("%u ", a[8]); printf("%u ", a[9]);
  putz("\n");

  putz("loop:\t\t10 20 30\n\t\t");
  s = 0;
  for (i = 0; i < 3; i++) {
    printf("%u ", a[i + 1]);
  }
  putz("\n");

  return 0;
}
