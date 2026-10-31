// char and char* tests

#include <stdio.h>
//#include <whatever.h>

// Global vars init, delayed till main!
// TODO: consider a more efficient initilizer opcode?
//   (like a stream <init><count><var#><const#>
//   use Y(nit) or H(mmm)?
char *h     = "Hello";
char spc    = 32;
char *world = "World";
char nl     = '\n';
int  a      = 65;

char* mylen = "Guess my length!";

int my_strlen(char* s) {
  int len = 0;

  // TODO: ???
  //for( ; *s++ ; 0) ++len;

  while(*s++) len++;

  return len;
}

int main(void) {
  printf("%d", a);
  putchar(a);
  putchar('>');        // op: O>
  putchar(spc);        // op: LvvE
  printf("%s", h);     // op: LvvP
  putchar(32);         // op:"O "
  printf("%s", world); // op: LvvP
  putchar('!');        // op: O!
  putchar('\n');       // op: N

  printf("\"%s\" has %d characters\n",
    mylen, my_strlen(mylen));

  return 42;
}
