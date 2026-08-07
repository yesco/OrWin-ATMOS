#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

int echo_main() {

  while(1) {

    // TODO: remove
  next:
    if (kbhit()) {
      char c= cgetc();

      // TODO: hid inside kbhit and wfocus
      switch(c) {
      case ' '+128: setfocus(); goto next;
      }

      if (c==127) { // Del key
	putc(8);
	putc(' ');
	putc(8);
      } else {
	putc(c);
	if (c==13) putc(10); // CR-LF
      }

    } else yield();
  }

  return 0;
}
