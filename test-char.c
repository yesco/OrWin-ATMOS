// char and char* tests

#include <stdio.h>
//#include <whatever.h>


int main(void) {
  // Global vars cannot be initialized, do here...
  // TODO: consider a more efficient initilizer opcode?
  //   (like a stream <init><count><var#><const#>
  //   use Y(nit) or H(mmm)?
  char *h     = "Hello";
  char spc    = 32;
  char *world = "World";
  char nl     = '\n';
  word a      = 65;

  printf("%d", a);
  putchar(a);
  putchar('>');        // op: O>
  putchar(spc);        // op: LvvE
  printf("%s", h);     // op: LvvP
  putchar(32);         // op:"O "
  printf("%s", world); // op: LvvP
  putchar('!');        // op: O!
  putchar('\n');       // op: N

  return 42;
}
