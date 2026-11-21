// test-incdec.c - ++ -- += = x+1 on char, int and pointers, C semantics
//
// Plain C: gcc test-incdec.c && ./a.out is the reference; the IVM VM and
// the JIT must print exactly the same.
// Each case: its name and the values C gives, then (indented) what we got:
//   char inc:       255 0 1
//                   255 0 1
// - unsigned char: wraps at 255 / 0 (our char is unsigned; gcc's plain
//   char is signed, so the test says unsigned char)
// - int *p: p++ moves one element (2 bytes), *p reads / writes a word
// - char *p: p++ moves one byte, *p reads / writes a byte
// Values stay below 32768 (gcc int is 32 bit, ours 16).

#include <stdio.h>
#include <stdlib.h>

int main(void) {
  unsigned char c, d;
  int i, k, n;
  int *w;
  int *p;
  char *b;
  char *q;

  // --- unsigned char: ++ -- wrap ------------------------------------------
  printf("char inc:\t255 0 1\n\t\t");
  c = 254;
  c++; printf("%d ", c);
  c++; printf("%d ", c);
  c++; printf("%d ", c);
  printf("\n");

  printf("char dec:\t0 255 254\n\t\t");
  c = 1;
  c--; printf("%d ", c);
  c--; printf("%d ", c);
  c--; printf("%d ", c);
  printf("\n");

  printf("char ++c --c:\t0 255 0 255\n\t\t");
  c = 255;
  ++c; printf("%d ", c);
  --c; printf("%d ", c);
  c = c + 1; printf("%d ", c);
  c = c - 1; printf("%d ", c);
  printf("\n");

  // += and plain assignment truncate too
  printf("char += =:\t4 44 1 144\n\t\t");
  c = 250; k = 10;
  c += k; printf("%d ", c);
  c = 300; printf("%d ", c);
  k = 513;
  c = k; printf("%d ", c);
  d = 200;
  c = d + d; printf("%d ", c);
  printf("\n");

  // --- int: plain ++ -- (sanity) ---------------------------------------------
  printf("int ++ --:\t10 11 10 9 10 9\n\t\t");
  i = 9;
  i++; printf("%d ", i);
  ++i; printf("%d ", i);
  i--; printf("%d ", i);
  --i; printf("%d ", i);
  i = i + 1; printf("%d ", i);
  i = i - 1; printf("%d ", i);
  printf("\n");

  // --- int *p: moves by elements, *p is a word -------------------------------
  w = malloc(40);
  for (i = 0; i <= 9; i++) w[i] = 100 * i + 7;
  printf("int* ++ --:\t7 107 207 107 7\n\t\t");
  p = w;
  k = *p; printf("%d ", k);
  p++; k = *p; printf("%d ", k);
  ++p; k = *p; printf("%d ", k);
  p--; k = *p; printf("%d ", k);
  --p; k = *p; printf("%d ", k);
  printf("\n");

  printf("int* += +1 -1:\t307 407 307\n\t\t");
  p = w;
  n = 3;
  p += n; k = *p; printf("%d ", k);
  p = p + 1; k = *p; printf("%d ", k);
  p = p - 1; k = *p; printf("%d ", k);
  printf("\n");

  //   *p = v writes a word: neighbours unchanged
  printf("int* *p = v:\t7 1234 207\n\t\t");
  p = w;
  p++;
  *p = 1234;
  k = w[0]; printf("%d ", k);
  k = w[1]; printf("%d ", k);
  k = w[2]; printf("%d ", k);
  printf("\n");

  // --- char *q: moves by bytes, *q is a byte -----------------------------------
  b = malloc(20);
  for (i = 0; i <= 9; i++) b[i] = 65 + i;
  printf("char* ++ -- +=:\t65 66 67 66 69\n\t\t");
  q = b;
  k = *q; printf("%d ", k);
  q++; k = *q; printf("%d ", k);
  ++q; k = *q; printf("%d ", k);
  q--; k = *q; printf("%d ", k);
  q += n; k = *q; printf("%d ", k);
  printf("\n");

  //   *q++: read a byte, then step one byte
  printf("char* *q++:\t65 66 67\n\t\t");
  q = b;
  k = *q++; printf("%d ", k);
  k = *q++; printf("%d ", k);
  k = *q; printf("%d ", k);
  printf("\n");

  //   *q = v writes one byte: the next byte unchanged
  printf("char* *q = v:\t65 90 67\n\t\t");
  q = b;
  q++;
  *q = 90;
  k = b[0]; printf("%d ", k);
  k = b[1]; printf("%d ", k);
  k = b[2]; printf("%d ", k);
  printf("\n");

  // --- unsigned char as a loop variable ----------------------------------------
  //   250 .. 255 0 1 2 3: the != test needs the wrap
  printf("char loop:\t10 4\n\t\t");
  n = 0;
  for (c = 250; c != 4; c++) n++;
  printf("%d ", n); printf("%d ", c);
  printf("\n");

  return 0;
}
