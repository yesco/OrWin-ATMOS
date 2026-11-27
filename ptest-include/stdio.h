// local stdio.h — shadows the real one 

#if 0
#ifndef putz
  // dependding onplatform puts adds newline (mostly) or not...
  #define putz puts
#endif

#ifdef __CC65__
  #include "/data/data/com.termux/files/usr/share/cc65/include/stdio.h"
#endif

#ifdef __OSCAR64__
  #include "~/GIT/oscar64/include/stdio.h"
#endif
#endif 

int putchar(char c);
int puts(char*);

//extern void putz(const char *s);
#ifndef __OSCAR64__
extern void putz(const char* s);
#endif
extern void putn(unsigned int n);
extern void putnl(unsigned int n);
extern void nl(void);

