#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

int echo_main() {

  while(1) {
    char c= getc();

    // Terminal stuff
    if (c==127) { // Del key
      putchar(8);
      putchar(' ');
      putchar(8);
    } else {
      putchar(c);
      if (c==13) putchar(10); // CR-LF
    }
  }

  return 0;
}
