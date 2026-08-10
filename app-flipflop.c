#include <stdlib.h>

#include "orwin.h"

int flipflop_main(int argc, char** argv) {
  // TODO: functions to get geometry
  int z= 14*14;
  char* a= calloc(z+1, 1);
  char* spc= calloc(z+1, 1);
  int i;
  
  for(i= z; i--; ) {
    a[i]= 'a'+(i%10);
    spc[i]= 32;
  }
      
  while(1) {
    putz(a);
    putz(spc);
  }
  
  return 0;
}
