// test-vnalias.c - value numbering and &x: a slot kept over a store through
// a pointer, the & of a variable only later in the source. parse.c then
// compiles again (vn_again), the slot not kept: as with & known up front
//   before the &:  30 30
//   after it:      30 30 5

#include <stdio.h>
#include <stdlib.h>

int main() {
  int j, s, t, k;
  int *a;
  int *q;
  a = malloc(40);
  for (k = 0; k < 10; k++) a[k] = k * 10;
  q = malloc(2);

  // before the &: a[j + 1] twice, a store through a pointer between
  j = 2;
  s = a[j + 1]; *q = 5; t = a[j + 1];
  printf("%u ", s); printf("%u\n", t);

  // after it: q is j (the store changes j)
  q = &j;
  j = 2;
  s = a[j + 1]; *q = 5; t = a[j + 1];
  printf("%u ", s); printf("%u ", t); printf("%u\n", j);
  return 0;
}
