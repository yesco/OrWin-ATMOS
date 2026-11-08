// 1,000,000 NOP; ' ' dispatch;
// So every second (1000000 cycles) is equivalant to one cycle
// in dispatch! test-dispatch-pon.c tests C max dispatch time

//  66,959,281 ./ivm test-dispatch
// 408,959,336 ./ivm test-dispatch-pon
//  68 ops
// (/ (- 409 66) 68.0) 5.044 c/ops!

//  44,034,108 ./ivm-asm test-dispatch ;; using ALTCODE+0




#include <stdio.h>

int main(void) {
  int i;
  
  // 1 million NOP
  for(i = 1; i <= 10000; i++) {
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  

    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
    NOP;NOP;NOP;NOP;NOP;  NOP;NOP;NOP;NOP;NOP;  
  };
  
  return 42;
}
