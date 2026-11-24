// test-static.c - static in a function: kept between calls, initialized
// once (ours: a global slot, the name local; the init at the start)
//
// Plain C: gcc test-static.c && ./a.out gives the reference output.
// Ours, not C: an initialized array in a function needs static (it is
// set once), a static's initializer is a constant, the function comes
// before main.
// Only the printf forms the grammar knows: "%d " and "%d\n".

#include <stdio.h>

// another function's locals: with -O its frame may share slots
int other(int a) {
  int t, u, v;
  t = a + 1;
  u = t * 2;
  v = u + 3;
  return v;
}

// a counter: n starts at 0 once, not on every call
int counter(void) {
  static int n = 0;
  n = n + 1;
  return n;
}

// several in one declaration, an array, a char
int two(void) {
  static int a, b = 10;
  static char tab[4] = { 1, 2, 3, 4 };
  static char c = 'a';
  a++;
  b++;
  tab[0] = tab[0] + 1;
  c++;
  putchar(c);
  putchar(' ');
  return a + b + tab[0];
}

// the same name in two functions: two variables
int third(void) {
  static int n = 100;
  n = n + 10;
  return n;
}

int main(void) {
  putchar('A'); putchar(' ');
  printf("%d ", counter());
  printf("%d ", other(5));
  printf("%d ", counter());
  printf("%d ", counter());
  putchar('\n');

  putchar('B'); putchar(' ');
  printf("%d ", two());
  printf("%d ", two());
  printf("%d ", two());
  putchar('\n');

  putchar('C'); putchar(' ');
  printf("%d ", third());
  printf("%d ", counter());
  printf("%d\n", third());
  return 0;
}
