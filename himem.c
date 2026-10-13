#include <stdio.h>
#include <ctype.h>


volatile char* via_port_b    = (char*)0x0300;
volatile char* microdisc_cfg = (char*)0x0314;

void enable_overlay_ram(void) {
    // Standard Oric VIA banking example:
    // Writing to the VIA Port B register to disconnect BASIC ROM and connect Overlay RAM
    
    // Exact bits vary depending on whether you are doing a pure overlay write 
    // or full mapping, but the hardware latch must be flipped:
    *via_port_b &= ~0x10; // Example bit flip to swap out the BASIC ROM mapping
}

#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F

unsigned int i=0;

void pc(char c) {
  TEXTSCREEN[i]= c;
  if (++i >= 28*40) i= 0;
}

int main() {
  char c;
  unsigned int a;
  
  i= 0;

  enable_overlay_ram();

// Writing 0xFD disables the internal Oric BASIC ROM and activates Overlay RAM
  *microdisc_cfg = 0xFD;
  
#if 1
  for(a=0xc000; a; ++a) {
    c= *(char*)a;
    if (isalnum(c)) pc(c);
  }
#else
  printf("Foobar!\n");

  for(a=0xc000; a; ++a) {
    c= *(char*)a;
    if (isalnum(c)) putchar(c);
  }

  printf("\nFiefum!\n");
#endif  


  return 0;
}
