#include <stdlib.h>

#include "orwin.h"

int atmos_main(int argc, char** argv) {
  char j, i= 0;
  
  while(1) {
    if (++i >= 64) i= 0;
    for(j= i & 31; j--; ) putchar(' ');
    putchar(i & 7); putz("Atmos");
  }
  
  (void)argc; (void)argv;
  return 0;
}
