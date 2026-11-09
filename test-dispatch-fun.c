// see test-dispatch.c

#include <stdio.h>

void f() {
}

int main(void) {
  int i;
  
  // 1 million PON (only for ivm.c dispatch)
  for(i = 1; i <= 10000; i++) {
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  

    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
    f();f();f();f();f();  f();f();f();f();f();  
  };
  
  return 42;
}
