#include <stdio.h>

//#ifndef __CC65__
  // other compilers remove if not needed.
#define NEED_PUTN
//#endif

#ifdef NEED_PUTN

#ifdef OSCAR64
  #define putz putc
#else
void putz(const char* s) {
  while(*s) putchar(*s++);
}
#endif

void putnraw(unsigned int u) {
    char buf[6];
    char *p = buf + 5;
    
    *p = 0;
    do { *--p = '0' + (u % 10); u /= 10; } while (u);
    putz(p);
}

void putn(unsigned int u) { putnraw(u); putchar(' '); }

void nl() { putchar('\n'); }

void putnl(unsigned int u) { putnraw(u); nl(); }

#endif
