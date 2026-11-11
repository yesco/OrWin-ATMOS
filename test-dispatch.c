// 1,000,000 NOP; ' ' dispatch;
// So every second (1000000 cycles) is equivalant to one cycle
// in dispatch! test-dispatch-pon.c tests C max dispatch time

// --- IVM ---
//    66,959,281 ./ivm test-dispatch
//   465,003,214 ./imv test-dispatch-inc
//   408,959,336 ./ivm test-dispatch-pon
// 1,371,959,628 ./ivm test-dispatch-fun - last! and supser expensive!
// 8,914,999,378 ./ivm-test-dispatch-loop 10 M
//
//  68 ops
// (/ (- 409 66) 68.0) 5.044 c/ops! (linear search CMP #xx / BEQ xx)

// --- IVM-ASM ---
//    44,034,108 ./ivm-asm test-dispatch  // using ALTCODE+0
//    95,074,135 ./ivm-asm test-dispatch-inc
//   214,054,457 ./ivm-asm test-dispatch-fun (call + return)
//
// 1,603,014,148 ./ivm-asm test-dispatch-loop

// --- IVM-ASM: UPDATED with FASTDISPATCH

//    36,413,525 - dipatch
//    85,964,138 - inc
//     .......   - test-dispatch-fun.c  - fail compile
// 1,512,825,132 - loop 10 M ops

// --- IVM-ASM: AND ALTCODE ---
//    28,133,473 - dispatch
//    79,184,563 - inc
//     .......   - test-dispatch-fun.c  - fail compile
// 1,302,964,133 - loop ALTCODE=1 (print problem

// -- JIT ----
//     2,384,325 - dispatch
///   47,568,070 - inc
//   550,272,728 - loop


// n++; == "Ivv" one dispatch, read param, read memory, inc, write
// (- 95.07 44.03) 51.04  44      16           -- 34 --  3 jmp
// (+ 44 16 34 3) = 97 ! (+ 16 34 3) = 53 c "work"
// optimal fixed 6502 zp: 13c ops (7 inc/bne, 6: lda+ldx) 4.1x!
// (/ 95 13.0) = 7.3x slower w bad asm dispatch than "compile"

// (/ 1000000 44) = 22,727 dis / s in asm " "
// (/ 1000000 95) = 10,52t ops / s in asm "Ivv"

// 8,914,999,378
// 1,603,014,148 (/ 8914999378 1603014148.0) 5.56x
// 891 c / loop turn
// 160 c / loop turn (- 160 44) 116 c (w/o disptch)

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
