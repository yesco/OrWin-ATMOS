// test-fixipc-op.c - long straight code: the compiler's FIXIPC ops (DEL)
//
// ivm-fixed.asm keeps the low byte of ipc in Y and never checks it in
// the ops; ivm-gen.c gen_fixipc() puts a DEL (127, re-bases ipc) every
// FIX_RUN bytes of straight code. Here: ~800 bytes without a call or a
// string (no re-base), and a loop body (~200 bytes, the loop limit is
// 255) with them: B back over DELs, if-jumps landing between them.
// No 16-bit overflow (^ not +): gcc gives the reference output.

#include <stdio.h>

int main() {
  unsigned int a, b, c, i, j;

  a = 1; b = 3; c = 0; j = 0;

  // straight: no call, no string
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;
  b = b + 1; if (a > c) c = c + 7;
  a = a + b; c = c ^ a;

  // loop body with DELs in it
  for (i = 0; i < 20; i++) {
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    b = b + 1; if (a > c) c = c + 7;
    a = a + b; c = c ^ a;
    j = j ^ c;
  }

  printf("%d ", a); printf("%d ", b); printf("%d ", c); printf("%d\n", j);

  return 0;
}
