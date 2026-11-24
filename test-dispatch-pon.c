// see test-dispatch.c

#ifndef __MC65__
// not really relevant test, tests bad disptch of long serial switch
// on cc65 PON (127) would be the last case... so the worst case
#define PON 0
#endif

#include <stdio.h>

int main(void) {
  int i;
  
  // 1 million PON (only for ivm.c dispatch)
  for(i = 1; i <= 10000; i++) {
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  

    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
    PON;PON;PON;PON;PON;  PON;PON;PON;PON;PON;  
  };
  
  return 42;
}
