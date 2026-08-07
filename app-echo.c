#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

int echo_main() {

  while(1) {
    char c= getc();

    // Terminal stuff
    if (c==127) { // Del key
      putc(8);
      putc(' ');
      putc(8);
    } else {
      putc(c);
      if (c==13) putc(10); // CR-LF
    }
  }

  return 0;
}
