#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

char wkbhit() {
  if (!kbhit()) yield();
  return kbhit();
}

char wgetc() {
  char c;

 again:
  while(!wkbhit()) yield();

  c= cgetc();

  // Capture FUNC keys Window Keys
  switch(c) {
  case ' '+128: setfocus(); goto again;
  }

  return c;
}


#define kbhit wkbhit
#undef getc
#define getc wgetc



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
