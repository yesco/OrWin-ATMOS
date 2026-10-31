// char and char* tests

#include <stdio.h>
//#include <whatever.h>

char *h     = "Hello";
word spc    = 32;
char *world = "World";
char nl     = '\n';

int main(void) {
  putchar('>');        // op: O>
  putchar(spc);        // op: LvvE
  printf("%s", h);     // op: LvvP
  putchar(32);         // op:"O "
  printf("%s", world); // op: LvvP
  putchar('!');        // op: O!
  putchar('\n');       // op: N

  return 42;
}
