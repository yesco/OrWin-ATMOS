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
char NL     = '\n';
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
  putn(a);
  putchar(a);
  putchar('>');        // op: O>
  putchar(spc);        // op: LvvE
  putz(h);             // op: LvvP
  putchar(32);         // op:"O "
  putz(world);         // op: LvvP
  putchar('!');        // op: O!
  putchar('\n');       // op: N
  putchar(NL);         // op: N
 
  putchar('"');
  putz(mylen);
  putz("\" has ");
  putn(my_strlen(mylen));
  putz("characters\n");
  
  return 42;
}
