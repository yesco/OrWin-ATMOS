// man-decl-err.c - MeteoriC-man.md 3, 4, 8, 9, 11: types, declarations,
// arrays, pointers, printf forms that do not compile
//
// Each line marked ERR (in a // comment) must be reported, no other, and
// no .ivm written.

#define N 3
const int two = 2;

typedef int T;                       // ERR no typedef
enum e { A, B };                     // ERR no enum
int g = 2 + 3;                       // ERR global initializer: a constant only
long l;                              // ERR no long
short sh;                            // ERR no short
float fl;                            // ERR no float
signed char sc;                      // ERR no signed
int m[3][3];                         // ERR no 2D arrays

int main(void) {
  int a, x;
  int w[4];
  int v[4];
  int *p;

  a = 1;
  x = N;
  N = 4;                             // ERR a #define is a const
  two = 3;                           // ERR a const
  w = v;                             // ERR an array name is a const
  p = &a;                            // ERR & only on arrays and structs
  x = *(p + 1);                      // ERR write p[1]
  w[1]++;                            // ERR only = on elements
  w[1] += 2;                         // ERR the same
  printf("%d %d\n", a, x);           // ERR one number per printf
  p = w;
  x = p[1];
  return x;
}
