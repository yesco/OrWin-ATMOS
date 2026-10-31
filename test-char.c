// char and char* tests

#include <stdio.h>
//#include <whatever.h>

char *h     = "Hello";
char spc    = 32;
char *world = "World";
char nl     = '\n';

int main(void) {
  putchar('>');        // op: O>
  printf("%s", h);     // op: LvvP
  putchar(spc);        // op: LvvE
  printf("%s", world); // op: LvvP
  putchar('!');        // op: O!
  putchar('\n');       // op: N

  return 42;
}
