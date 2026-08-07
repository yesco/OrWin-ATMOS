#include <stdlib.h>

#include "orwin.h"

int atmos_main() {
  char j, i= 0;
  
  while(1) {
    if (++i >= 64) i= 0;
    for(j= i & 31; j--; ) putc(' ');
    putc(i & 7); puts("Atmos");
  }
  
  return 0;
}
