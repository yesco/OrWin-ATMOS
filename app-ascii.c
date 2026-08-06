#include "orwin.h"

static int i= 0;

void ascii_init() {
}

void ascii_loop() {
  putc((i++ % 96)+32);
}  

int ascii_main() {
  ascii_init();
  
  while(1) ascii_loop();
  
  return 0;
}
