// test-peekpoke.c - raxiss's conio.h / types.h, our own versions
//
// - peek(a) poke(a, v): a byte at address a
// - deek(a) doke(a, v): a word (little endian)
// - uint8_t uint16_t: unsigned char, int
// - MEMPTR(a): a as a byte pointer
//
// On a malloc'd buffer: gcc can't run raw addresses, the .out is written
// by hand. The C ivm writes 2 bytes for a byte store (poke: its w op,
// see test-incdec), so its "ABC" check is done before the doke.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef __MC65__
  #define peek(a) (*(uint8_t*)(a))
  #define poke(a,v) (*(uint8_t*)(a)=(v))

  #define deek(a) (*(uint16_t*)(a))
  #define doke(a,v) (*(uint16_t*)(a)=(v))

// TODO: not right for oscar
  #define MEMPTR(p) ((uint16_t)(p))
#endif

uint8_t c;
uint16_t w;

int main() {
  char *b;
  int a;

  b = malloc(8);
  a = (uint16_t)b;

  putz("poke peek:\tABC\n\t\t");
  poke(a, 65);
  poke(a + 1, 66);
  poke(a + 2, 67);
  poke(a + 3, 0);
  c = peek(a);
  putchar(c);
  putchar(peek(a + 1));
  putchar(peek(a + 2));
  putz("\n");

  putz("_puts MEMPTR:\tABC|\n\t\t");
// TODO: oscar struggles
  putz(MEMPTR(a));
  putz("|\n");

  putz("doke deek:\t4660 52 18\n\t\t");
  doke(a + 4, 4660);
  w = deek(a + 4);
  putn(w);
  putn(peek(a + 4));
  putn(peek(a + 5));

  return 0;
}
