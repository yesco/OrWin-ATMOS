#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

typedef struct APP {
  char dummy;
} APP;

void* app_echo(void* voidapp, char* line) {
  char c;
  
  if (!app) return calloc(sizeof(APP), 1);
    
  c= getc();

  // Terminal stuff
  if (c==127) { // Del key
    putchar(8);
    putchar(' ');
    putchar(8);
  } else if (c) {
    putchar(c);
    if (c==13) putchar(10); // CR-LF
  }

  return WAITKEY;
  (void)app; (void)line;
}
