// 1,000,000 NOP; ' ' dispatch;
// So every second (1000000 cycles) is equivalant to one cycle
// in dispatch! test-dispatch-pon.c tests C max dispatch time

//  66,959,281 ./ivm test-dispatch
// 465,003,214 ./imv test-dispatch-inc
// 408,959,336 ./ivm test-dispatch-pon
//
//  68 ops
// (/ (- 409 66) 68.0) 5.044 c/ops! (linear search CMP #xx / BEQ xx)

//  44,034,108 ./ivm-asm test-dispatch  // using ALTCODE+0
//  95,074,135 ./ivm-asm test-dispatch-inc
//
// n++; == "Ivv" one dispatch, read param, read memory, inc, write
// (- 95.07 44.03) 51.04  44      16           -- 34 --  3 jmp
// (+ 44 16 34 3) = 97 ! (+ 16 34 3) = 53 c "work"
// optimal fixed 6502 zp: 13c ops (7 inc/bne, 6: lda+ldx) 4.1x!
// (/ 95 13.0) = 7.3x slower w bad asm dispatch than "compile"

#include <stdio.h>

int main(void) {
  int i;
  
  // 1 million NOP

  //                              	// ALTCODE=0    ALTCODE=1
//  for(i = 1; i <= 10000; i++) {	// 44,034,108    - (i,n)
  i = 10000; while(i) { i--;    	// 45,843,614   36,573,572
    //                                                  31c / loop
    // (- 45843614 36573572) 9,270,042 = 9c cheaper doing ALTCODE=1

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
