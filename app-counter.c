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
  
  do { counter_loop(); } while(YIELD());
  
  return 0;
}
