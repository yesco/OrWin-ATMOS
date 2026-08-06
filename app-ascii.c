#include "orwin.h"

static int i= 0;

void ascii_init() {
}

void ascii_loop() {
  putc((i++ % 96)+32);
}  

int ascii_main() {
  ascii_init();
  
  do { ascii_loop(); } while(YIELD());
  
  return 0;
}
