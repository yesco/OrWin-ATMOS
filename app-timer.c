#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

int timer_main(int argc, char** argv) {
  clock_t last, now= clock();
  char buf[16]= {0};

  while(1) {
    last= now;
    now= clock();
    //sprintf(buf, "%u\t", (last-now)/1000);
    sprintf(buf, "%u\t%u\n", now, now-last);
    putz(buf);
  }

  return 0;
}
