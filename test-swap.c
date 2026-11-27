// test-swap.c - the array swap idiom and the 'A' op
//
// t = a[i]; a[i] = a[j]; a[j] = t;  with int *a and plain variables
// i j is compiled to one op: A a i j (then S t: t gets the old a[i],
// as in C). Other forms stay 3 statements and must give the same.
// Each case: its name and the values C gives, then (indented) what we
// got:
//   swap 2 7:       70 20 20
//                   70 20 20
// - swap 2 7: a[2] a[7] t afterwards
// - swap 3 3: i == j: nothing moves, t = a[3]
// - swap 0 9: the ends (pointer math at both ends of the array)
// - j + 1:    a[j + 1] is no plain variable: not A, 3 statements
// - char:     a char* array: not A (A swaps words), 3 statements
// - twice:    swapping back gives the start again (70 20 from above)

#include <stdio.h>
#include <stdlib.h>

int main() {
  int i, j, t, k;
  int *a;
  char *b;
  char c;
  a = malloc(40);          // 10 ints: 40 for gcc's 32 bit int
  b = malloc(10);
  for (k = 0; k < 10; k++) a[k] = k * 10;
  for (k = 0; k < 10; k++) b[k] = k + 65;

  putz("swap 2 7:\t70 20 20\n\t\t");
  i = 2; j = 7;
  t = a[i]; a[i] = a[j]; a[j] = t;
  putn(a[2]); putn(a[7]); putn(t);
  putz("\n");

  putz("swap 3 3:\t30 30\n\t\t");
  i = 3; j = 3;
  t = a[i]; a[i] = a[j]; a[j] = t;
  putn(a[3]); putn(t);
  putz("\n");

  putz("swap 0 9:\t90 0 0\n\t\t");
  i = 0; j = 9;
  t = a[i]; a[i] = a[j]; a[j] = t;
  putn(a[0]); putn(a[9]); putn(t);
  putz("\n");

  putz("j + 1:\t\t50 40 40\n\t\t");
  j = 4;
  t = a[j]; a[j] = a[j + 1]; a[j + 1] = t;
  putn(a[4]); putn(a[5]); putn(t);
  putz("\n");

  putz("char:\t\t71 66 66\n\t\t");
  i = 1; j = 6;
  c = b[i]; b[i] = b[j]; b[j] = c;
  putn(b[1]); putn(b[6]); putn(c);
  putz("\n");

  putz("twice:\t\t70 20\n\t\t");
  i = 2; j = 7;
  t = a[i]; a[i] = a[j]; a[j] = t;
  t = a[i]; a[i] = a[j]; a[j] = t;
  putn(a[2]); putn(a[7]);
  putz("\n");

  return 0;
}
