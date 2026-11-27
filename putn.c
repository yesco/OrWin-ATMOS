#include <stdio.h>

#ifdef OSCAR64
 // TODO: not sure why cannot do this?
 //#define putz puts

 void putz(const char* s) {  while(*s) putchar(*s++); }
#endif

#ifdef __CC65__
void putz(const char* s) {
  while(*s) putchar(*s++);
}
//  #define putz puts
//  extern FILE* stdout;

//  extern fputs(const char*, FILE*);
//  #define putz(x) fputs((x),stdout)
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
