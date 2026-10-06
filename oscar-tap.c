// oscartap.c - and oscar compiled program

// inspired by locifilemanagerv2
// - https://github.com/xahmol/locifilemanager-v2

#include "conio.h"
#include <stdio.h>

#include <string.h>

#include "keyboard.h"
#include "loci.h"

//#include "oric.h"
//#include "ijk.h"



#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F

// Boot
// -------------------------------------------------------------------------

// Boot from active mounts (disk > tape > ROM), via mia_call_boot()
// with a flag byte built from the autoload bit b11 tap flopoy on
// mount-status flags (auto-load tape takes preference if no disk is
// mounted on drive A). On success this never returns (LOCI reboots
// the machine); on failure shows an error popup and returns.
// 
// Returns: not at all on success, if returns then it failed!
static void boot(void) {
  char autoload= 1;
  char bit     = 1; // ??
  char tap     = 1; // loading tap file
  char b11     = 0; // ??
  char floppy  = 1; // it's ona a floppy

  if (mia_call_boot((uint8_t)(0x80 | (autoload << 4) | (bit << 3) | (b11 << 2) | (tap << 1) | floppy)) < 0)
    sprintf(TEXTSCREEN+40*3, "%% Boot failed!\n");
}

// It seems that the bs routines are "lowcode" and can be overridden
#define TEXTSCREEN ((char*)0xbb80)

//char screeni= 0;

//void putchar(char c) { TEXTSCREEN[screeni++]= c; }

// fm_getkey() is polled and
int main(void) {
  int i;

#if 1

  for(i= 0; i<32; ++i) 
   putchar('a'+i);
//    bsout('a'+i);
  
  printf("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  
  return 0;
  
#else

  sprintf(TEXTSCREEN+40*1, "Hello APP!\n");

  //printf("Hello APP!\n");

  // LOCI required for overlay RAM save/restore; gracefully absent in Oricutron
  if (!loci_present())
    sprintf(TEXTSCREEN+40*2, "%%No loci\n");
#endif
  
  return 0;
}

#endif // dummy
