// test-arrayinit.c - initialized static arrays
//
// An initialized array lives in the data (the file, like a string): its
// name a const slot holding its address. = {0} or none: zeroed (bss, not
// in the file). Arrays are static: initialized once (a local one too: C
// wants "static" there):
//
//   int w[] = {1, 2, 3};      the size from the initializer
//   char s[8] = "abc";        zeros after it, up to the size
//   char t[3] = "abc";        exactly the size: no 0 (as C)
//
// Each case: name and the values C gives, (indented) what we got:
//
//   int list:       3 100 300
//                   3 100 300
//
// - int list:   int w[] = {100, 200, 300,}: sizeof / 2, w[0], w[2]
// - char list:  char c[5] = {'a', 66, 3}: c[0] c[1] c[2] c[4], sizeof
// - string:     char s[8] = "abc": s[0] s[3] s[7], sizeof
// - exact:      char t[3] = "xyz": t[2], sizeof
// - auto size:  char u[] = "hello": sizeof (6: the 0 too), u[4]
// - zero:       int z[4] = {0}: z[3], sizeof
// - write:      w[1] = 7, read back
// - static:     a local static char k[] = {5}: k[0]++ each call: 5 6 7
// - const:      #define N 3, const two = 2: int q[N] = {two, N}: q[0] q[1]

#include <stdio.h>

int w[] = {100, 200, 300,};
char c[5] = {'a', 66, 3};
char s[8] = "abc";
char t[3] = "xyz";
char u[] = "hello";
int z[4] = {0};

#define N 3

#ifdef __CC65__
// LOL a constant expression in CC65 isn't a constant lol
#define two 2 // lol
#else
const int two = 2;
#endif
int q[N] = {two, N};

int counter() {
  static char k[] = {5};
  int r;
  r = k[0];
  k[0] = k[0] + 1;
  return r;
}

int main() {
  int n;

  printf("int list:\t3 100 300\n\t\t");
  n = sizeof(w) / 2; printf("%u ", n);
  n = w[0]; printf("%u ", n);
  n = w[2]; printf("%u\n", n);

  printf("char list:\t97 66 3 0 5\n\t\t");
  n = c[0]; printf("%u ", n);
  n = c[1]; printf("%u ", n);
  n = c[2]; printf("%u ", n);
  n = c[4]; printf("%u ", n);
  n = sizeof(c); printf("%u\n", n);

  printf("string:\t\t97 0 0 8\n\t\t");
  n = s[0]; printf("%u ", n);
  n = s[3]; printf("%u ", n);
  n = s[7]; printf("%u ", n);
  n = sizeof(s); printf("%u\n", n);

  printf("exact:\t\t122 3\n\t\t");
  n = t[2]; printf("%u ", n);
  n = sizeof(t); printf("%u\n", n);

  printf("auto size:\t6 111\n\t\t");
  n = sizeof(u); printf("%u ", n);
  n = u[4]; printf("%u\n", n);

  printf("zero:\t\t0 8\n\t\t");
  n = z[3]; printf("%u ", n);
  n = sizeof(z); printf("%u\n", n);

  printf("write:\t\t7\n\t\t");
  w[1] = 7;
  n = w[1]; printf("%u\n", n);

  printf("static:\t\t5 6 7\n\t\t");
  n = counter(); printf("%u ", n);
  n = counter(); printf("%u ", n);
  n = counter(); printf("%u\n", n);

  printf("const:\t\t2 3\n\t\t");
  n = q[0]; printf("%u ", n);
  n = q[1]; printf("%u\n", n);
  return 0;
}
