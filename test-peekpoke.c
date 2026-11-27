// test-peekpoke.c - raxiss's conio.h / types.h, our own versions
//
// - peek(a) poke(a, v): a byte at address a
// - deek(a) doke(a, v): a word (little endian)
// - _putc(c) _puts(s): putchar, puts without the newline
// - uint8_t uint16_t: unsigned char, int
// - MEMPTR(a): a as a byte pointer
//
// On a malloc'd buffer: gcc can't run raw addresses, the .out is written
// by hand. The C ivm writes 2 bytes for a byte store (poke: its w op,
// see test-incdec), so its "ABC" check is done before the doke.

#include <stdio.h>
#include <stdlib.h>

uint8_t c;
uint16_t w;

int main() {
  char *b;
  int a;

  b = malloc(8);
  a = b;

  putz("poke peek:\tABC\n\t\t");
  poke(a, 65);
  poke(a + 1, 66);
  poke(a + 2, 67);
  poke(a + 3, 0);
  c = peek(a);
  _putc(c);
  _putc(peek(a + 1));
  _putc(peek(a + 2));
  _puts("\n");

  putz("_puts MEMPTR:\tABC|\n\t\t");
  _puts(MEMPTR(a));
  _puts("|\n");

  putz("doke deek:\t4660 52 18\n\t\t");
  doke(a + 4, 4660);
  w = deek(a + 4);
  putn(w);
  putn(peek(a + 4));
  putn(peek(a + 5));

  return 0;
}
