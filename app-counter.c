#include "orwin.h"

static int i= 0;

void counter_init() {
}

void counter_loop() {
  if (++i % 25 == 0) {
    puti(i); putc('\t');
  }
}  

int counter_main() {

  counter_init();
  
  while(1) counter_loop();
  
  return 0;
}
