// test-ifeq.c - if (a == b): the fused 'v' op, every form of rule Y
//
// Plain C: gcc test-ifeq.c && ./a.out gives the reference output.
// a == const, a == var, a == call (into a temp), with and without
// else, values that differ only in the hi byte, char vars, and a small
// decoder (as test-dispatch-switch-ifif.c, on defined data).
// Only the printf forms the grammar knows: "%d " and "%d\n".

#include <stdio.h>

int twice(int v) {
  return v + v;
}

int len;
int mode;
int flag = 1;   // a global = 1: Y in main's init

void decode(unsigned char low) {
  int hit;
  hit = 0;
  if (low == 0x00) hit = 1;
  if (low == 0x02) hit = 1;
  if (low == 0x09) hit = 1;
  if (hit) { len = 2; mode = 1; return; }
  hit = 0;
  if (low == 0x01) hit = 1;
  if (low == 0x05) hit = 1;
  if (low == 0x11) hit = 1;
  if (hit) { len = 2; mode = 2; return; }
  if (low == 0x0C) { len = 3; mode = 3; return; }
  len = 1;
  mode = 4;
}

int main(void) {
  int i, a, b, n, x, s;
  unsigned char c;

  // --- a == const, a == var ----------------------------------------------
  putchar('A'); putchar(' ');
  a = 5; b = 5; n = 7;
  if (a == 5) printf("%d ", 1);
  if (a == 6) printf("%d ", 2);
  if (a == b) printf("%d ", 3);
  if (a == n) printf("%d ", 4);
  if (n == 7) printf("%d ", 5);
  putchar('\n');

  // --- with else ------------------------------------------------------------
  putchar('B'); putchar(' ');
  if (a == 5) printf("%d ", 1); else printf("%d ", 2);
  if (a == 9) printf("%d ", 3); else printf("%d ", 4);
  if (a == b) printf("%d ", 5); else printf("%d ", 6);
  if (a == n) printf("%d ", 7); else printf("%d ", 8);
  putchar('\n');

  // --- only the hi byte differs -----------------------------------------------
  putchar('C'); putchar(' ');
  a = 0x1234; b = 0x0034; n = 0x1234;
  if (a == b) printf("%d ", 1); else printf("%d ", 2);
  if (a == n) printf("%d ", 3); else printf("%d ", 4);
  if (a == 0x0034) printf("%d ", 5); else printf("%d ", 6);
  if (a == 0x1234) printf("%d ", 7); else printf("%d ", 8);
  a = 0x0100;
  if (a == 0) printf("%d ", 9); else printf("%d ", 10);
  a = 0;
  if (a == 0) printf("%d ", 11); else printf("%d ", 12);
  putchar('\n');

  // --- a == call (R: into a temp) ----------------------------------------
  putchar('D'); putchar(' ');
  a = 12; b = 6; n = 3;
  if (a == twice(b)) printf("%d ", 1); else printf("%d ", 2);
  if (a == twice(n)) printf("%d ", 3); else printf("%d ", 4);
  if (a == twice(6)) printf("%d ", 5);
  if (b == twice(n)) printf("%d ", 6);
  putchar('\n');

  // --- char var, in a loop, nested ------------------------------------------
  putchar('E'); putchar(' ');
  s = 0;
  for (i = 0; i < 300; i++) {
    c = i;
    if (c == 7) s = s + 1;
    if (i == 7) s = s + 100;
    if (c == 44) { if (i == 300) s = s + 1000; else s = s + 10; }
  }
  printf("%d ", s);
  putchar('\n');

  // --- a decoder on defined data --------------------------------------------
  putchar('F'); putchar(' ');
  x = 1; s = 0;
  for (i = 0; i < 40; i++) {
    x = (x * 13 + 7) & 31;
    decode(x);
    n = len * 10;
    s = s + n + mode;
    printf("%d ", mode);
  }
  printf("%d\n", s);

  // --- = 1 (Y): global init, int, char ---------------------------------------
  putchar('G'); putchar(' ');
  printf("%d ", flag);
  a = 7; a = 1; printf("%d ", a);
  c = 9; c = 1; printf("%d ", c);
  if (c == 1) printf("%d ", 2);
  putchar('\n');

  return 0;
}
