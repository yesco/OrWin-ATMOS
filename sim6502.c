// sim6502.c - abstracted ORIC ATMOS machine
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// This is for the OrWIN-ATMOS project,
// to allow it to compile and run under a simulator.
//
// All ORIC specific "pokes" have been abstracted
// this this file and API and can be implemented
// for other platforms
// 
// This file targets to run under sim65 (cc65).

//#define SHADE "░" // 25% (U+2591 — Light shade)
#define SHADE "▒" //  50% (U+2592) — Medium shade)
//#define SHADE "▓" //  75% (U+2593) — Dark shade)
#define FULL  "█" // 100% (U+2588) — 100% Filled)

clock_t clock() {
  // map to simulator register... 
}
