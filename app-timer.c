#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

// TODO: uptime unix call? lol
#define TIMER (*(unsigned int*)0x304)

int timer_main(int argc, char** argv) {
  unsigned int last, now= TIMER;
  char buf[16]= {0};

  while(1) {
    last= now;
    now= TIMER;
    sprintf(buf, "%u\t", (last-now)/1000);
    putz(buf);
  }

  return 0;
}
