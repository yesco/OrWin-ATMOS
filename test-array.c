// test-array.c - static arrays: char a[N]; int w[N];
//
// A static array is bss: zeroed memory the loader sets aside (not in
// the file), its name a const slot holding its address. The JIT knows
// that address, so an element needs no pointer:
//
//   char index:   LDY i; LDA a,Y                     (no calculation)
//
//   word index:   Y = i lo, A = i hi + a hi -> the hi byte of a
//                 self-modifying LDA a,Y / STA a,Y   (no zp pointer)
//
//   int array:    the AI pointer, a's address as immediates
//
// Each case: name and the values C gives, (indented) what we got:
//
//   char idx:       12 27
//                   12 27
//
// - char idx:   a[i] = i * 3, i a char: a[4] a[9]
// - word idx:   big[k] (k = 300, 1000, 2047): written, read back
// - zeroed:     big[500], w[3]: never written, 0 (bss is zeroed)
// - int array:  w[i] = i * 100: w[7] w[0]
// - const idx:  a[2] = 5: a[2]
// - sizeof:     char a[20], int w[10]: 20 20
// - pointer:    p = a; p[4]
// - local:      a char array in a function: t[1] = 9, returned

#include <stdio.h>

char a[20];

#define BIGSIZE 2048
char big[BIGSIZE];

const int ten = 10;
int w[ten];

// Cannot:
// #define ADD 2048+1
// int add[ADD];

int loc() {
  char t[4];
  t[1] = 9;
  return t[1];
}

int main() {
  char i;
  int k, n;
  char *p;

  printf("char idx:\t12 27\n\t\t");
  for (i = 0; i < 10; i++) a[i] = i * 3;
  n = a[4]; printf("%u ", n);
  n = a[9]; printf("%u\n", n);

  printf("word idx:\t3 10 7\n\t\t");
  k = 300;  big[k] = 3;
  k = 1000; big[k] = 10;
  k = 2047; big[k] = 7;
  k = 300;  n = big[k]; printf("%u ", n);
  k = 1000; n = big[k]; printf("%u ", n);
  k = 2047; n = big[k]; printf("%u\n", n);

  printf("zeroed:\t\t0 0\n\t\t");
  n = big[500]; printf("%u ", n);
  n = w[3]; printf("%u\n", n);

  printf("int array:\t700 0\n\t\t");
  for (k = 0; k < 10; k++) w[k] = k * 100;
  n = w[7]; printf("%u ", n);
  n = w[0]; printf("%u\n", n);

  printf("const idx:\t5\n\t\t");
  a[2] = 5;
  n = a[2]; printf("%u\n", n);

  printf("sizeof:\t\t20 20\n\t\t");
  n = sizeof(a); printf("%u ", n);
  n = sizeof(w); printf("%u\n", n);

  printf("pointer:\t12\n\t\t");
  p = a;
  n = p[4]; printf("%u\n", n);

  printf("local:\t\t9\n\t\t");
  n = loc(); printf("%u\n", n);
  return 0;
}
