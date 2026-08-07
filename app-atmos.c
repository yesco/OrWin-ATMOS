#include "orwin.h"

static int i= 0, j= 0, z= 0;

void atmos_init() {
}

void atmos_loop() {
  if (++i>64) {
    i= 0;
    ++z; z&= 31;
  }

  for(j=z; j--; ) putc(' ');

  putc(i&7); puts("Atmos");
}  

int atmos_main() {
  atmos_init();
  
  do { atmos_loop(); } while(YIELD());
	  
  //while(1) atmos_loop();
  
  return 0;
}
