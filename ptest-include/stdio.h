// local stdio.h — shadows the real one 

#if 1

#ifndef putz
  // dependding onplatform puts adds newline (mostly) or not...
//  #define putz puts
#endif

#ifdef __CC65__
  int putchar(int c);
//extern void putz(const char* s);
  int puts(const char*);
  void putz(const char*);
  extern int getchar();

// Somehow not working with cc65, does't respect/use full path
// TODO: this is a problem for like "printf" etc... we can extern them?

//  #include "/data/data/com.termux/files/usr/share/cc65/include/stdio.h"

#endif

#ifdef __OSCAR64__
  int putchar(char c);
  int puts(const char*);

  #include "~/GIT/oscar64/include/stdio.h"
#endif
#endif 


extern void putn(unsigned int n);
extern void putnl(unsigned int n);
extern void nl(void);

