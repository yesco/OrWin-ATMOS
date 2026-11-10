// see test-dispatch.c

#include <stdio.h>

int j;

int main(void) {
  int i;
  
  // 10 million loops
  for(i = 1; i <= 1000; i++) {
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;

    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
    for(j = 1; j<= 1000; j++) ;
  };
  
  return 42;
}
