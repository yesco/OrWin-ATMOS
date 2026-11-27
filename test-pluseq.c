// test-pluseq.c - += -= and x = x + 2 on int, int* and char
//
// Plain C: the IVM VM and the JIT must print the expected values.
// Each case: its name and the values C gives (16 bit int), then
// (indented) what we got:
//   int += 2:       0 7 9 27
//                   0 7 9 27
// What it checks, and why:
// - int += 2, x = x + 2, x = 2 + x: one op, H (word += 2). 65534 + 2
//   wraps to 0. x += 20 is the generic += (any expression).
// - int* += 1 / 2 / n, p = p + 1: an int* moves 2 bytes per element
//   (H, H H, n doubled), so *p reads a[1], a[2], a[3], a[1].
// - char += -=: wraps at 255 / 0 (s keeps the hi byte 0), never H.
// - int -=: 5 - 7 wraps to 65534; x -= n + 2 subtracts the whole E.
// - int* -= 2 / n, += n + 2: the element count doubled, then - / +.
// gcc: int is 32 bit there, so it prints 65536 for 65534 + 2 and
// 4294967294 for 5 - 7 (compile with -Dint="unsigned short" to match).

#include <stdio.h>
#include <stdlib.h>

int main() {
  int x, n;
  int *p;
  int *a;
  unsigned char c;
  a = malloc(20);
  a[0] = 10; a[1] = 11; a[2] = 12; a[3] = 13; a[4] = 14;

  putz("int += 2:\t0 7 9 27\n\t\t");
  x = 65534;	x+= 2;		printf("%u ", x);
  x = 5;	x= x + 2; 	printf("%u ", x);
  x = 7; 	x = 2 + x; 	printf("%u ", x);
  x = 7;	x+= 20;		printf("%u ", x);
  putz("\n");

  putz("int* +=:\t11 12 13 11\n\t\t");
  p = a;	p+= 1;	 	printf("%u ", *p);
  p = a;	p+= 2;	 	printf("%u ", *p);
  p = a; n = 3;	p += n;		printf("%u ", *p);
  p = a;	p = p + 1;	printf("%u ", *p);
  putz("\n");

  putz("char, int -=:\t1 255 65534 40\n\t\t");
  c = 255;	c+= 2;	 	printf("%u ", c);
  c = 1;	c-= 2;	 	printf("%u ", c);
  x = 5;	x-= 7;	 	printf("%u ", x);
  x = 50; n = 8;	x -= n + 2;	printf("%u ", x);
  putz("\n");

  putz("int* -= += E:\t12 13 13\n\t\t");
  p = a; p+= 4;	p -= 2;		printf("%u ", *p);
  p = a; n = 1;	p += n + 2;	printf("%u ", *p);
  p = a; p+= 4;	p -= n;		printf("%u ", *p);
  putz("\n");

  return 0;
}
