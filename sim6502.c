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

#define HEAPMEM
size_t _heapmemavail(void);
size_t _heapmaxavail(void);

// TODO: define, need to fiddle with stty in starting script
#define KBHIT
#undef kbhit
char kbhit() {
  return 0;
}

