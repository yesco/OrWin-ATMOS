// 1,000,000 NOP; ' ' dispatch;
// So every second (1000000 cycles) is equivalant to one cycle
// in dispatch! test-dispatch-pon.c tests C max dispatch time

// - using switch-case - see BTABLE
// --- IVM ---
//    66,959,281 - dispatch  
//    71,260,332    - 2026-11-16, slower 5c more in dispatch
//   465,003,214 - inc
//   408,959,336 - pon
//   414,059,992    - 2026-11-16, slower, more ops?
// 1,371,959,628 - fun - last! and supser expensive!
// 8,914,999,378 - loop 10 M
// 8,964,065,027    - 2026-11-16, got somewhat slower...
//   751,977,998 byte-sieve
//
//  68 ops
// (/ (- 409 66) 68.0) 5.044 c/ops! (linear search CMP #xx / BEQ xx)


// --- IVM USE_BTABLE dispatch ---
//    94,686,785 - dispatch
//    92,786,560 - nop
//   118,786,560 - pon
//   327,800,453 - inc
//   830,787,333 - fun                     
// 6,510,594,290 - loop 10M (/ 6510 10) = 651 c


// --- IVM-ASM ---
//    44,034,108 -dispatch  // using ALTCODE+0
//    95,074,135 -inc
//   214,054,457 -fun (call + return)
// 1,603,014,148 -loop


// --- IVM-ASM: UPDATED with FASTDISPATCH ALTCODE=0

//    36,413,525 - dipatch
//    85,964,138 - inc
//   204,884,444 - fun
// 1,512,825,132 - loop 10 M ops

//   184,577,647 - byte-sieve


// --- IVM-ASM: AND ALTCODE=1 ---
//    28,133,473 - dispatch
//    79,184,563 - inc
//   218,974,445 - fun  SLOWER! :-(
// 1,302,964,133 - loop ALTCODE=1 

//   ? crash     - byte-sieve


// TODO: time to do ivm-asm again!!!


// -- JIT w const ----
//       598,503 - dispatch    47. x  !
//    17,508,004 - inc          4.5x
//    20,468,599 - fun         10.7x  !
//   550,272,728 - loop         2.4x

//    58,556,915 - byte-sieve   3.15x

// -- with i + n loop opt --
//       558,449 - dispatch
//    14,284,034 - inc
//    18,248,449 - fun
//   210,381,273 - loop 10M
//    30,253,218 - byte-sieve



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

#ifndef __MC65__
#define NOP 0
#endif

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
