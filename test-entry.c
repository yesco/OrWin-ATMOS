// test-entry.c - the first statement of main: const expressions with
// * / % & | <<
//
// Plain C: gcc test-entry.c && ./a.out is the reference.
// main's entry is set by its first code. A failing alternative (rule O
// tries "* [S\j] R" first: \j drops the load before it) took the code
// back to main's start for a moment; the entry was armed again, and set
// after the restored first load: "x = 7 * 4" first in main gave 0.
// Fixed in frame_rewind (parse.c): only re-armed if main's global init
// code is really gone.

#include <stdio.h>

int main(void) {
  int x;

  x = 7 * 4 + 35;
  putz("first:\t\t63\n\t\t");
  putn(x);

  putz("const exprs:\t63 28 21 1 3 4 7 28 24464\n\t\t");
  x = 35 + (7 * 4); putn(x);
  x = 7 * 4; putn(x);
  x = 3 * 7; putn(x);
  x = 7 / 4; putn(x);
  x = 7 % 4; putn(x);
  x = 7 & 4; putn(x);
  x = 7 | 4; putn(x);
  x = 7 << 2; putn(x);
  x = 300 * 300; putn(x & 0xffff);
  return 0;
}
