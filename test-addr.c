// test-addr.c - &x: the address of a variable (the ` op), and &arr
//
// Plain C: gcc test-addr.c && ./a.out gives the reference output.
// Only the printf forms the grammar knows: "%d " and "%d\n".

#include <stdio.h>

char buf[4];
int g;

void set(int *p, int v) { *p = v; }

void swap(int *a, int *b) {
  int t;
  t = *a;
  *a = *b;
  *b = t;
}

int main(void) {
  int x, y, n;
  int *p;
  char c;
  char *q;

  // --- int: through a pointer to a local -----------------------------------
  putchar('A'); putchar(' ');
  x = 5;
  p = &x;
  *p = 7;
  printf("%d ", x);
  printf("%d ", *p);
  putchar('\n');

  // --- a pointer as an argument: set, swap ----------------------------------
  putchar('B'); putchar(' ');
  set(&y, 42);
  printf("%d ", y);
  swap(&x, &y);
  printf("%d ", x);
  printf("%d ", y);
  set(&g, 9);
  printf("%d ", g);
  putchar('\n');

  // --- the value changes behind the variable's back -------------------------
  // (x + 1 again, after *p = ..: not the earlier x + 1)
  putchar('C'); putchar(' ');
  p = &x;
  y = x + 1;
  printf("%d ", y);
  *p = 100;
  n = x + 1;
  printf("%d ", n);
  putchar('\n');

  // --- char: through a char pointer -----------------------------------------
  putchar('D'); putchar(' ');
  c = 'a';
  q = &c;
  *q = 'z';
  putchar(c);
  putchar(*q);
  putchar('\n');

  // --- &arr of a static array: its address ----------------------------------
  putchar('E'); putchar(' ');
  q = (char*)&buf;                       // C: &buf is a char (*)[4]
  q[1] = 'k';
  putchar(buf[1]);
  putchar('\n');

  return 0;
}
