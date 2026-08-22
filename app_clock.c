#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"


typedef struct APP {
  unsigned int slast;
} APP;

void* app_clock(APP* state, char* line) {
  clock_t now= clock();
  int s= now / CLOCKS_PER_SEC, m= s / 60, h= m / 60;

  if (!state) return calloc(sizeof(APP), 1);
  
  // print dots if second not change
  if (s == state->slast) putchar('.');
  else {
    char tmp[16];

    if (0) {
      sprintf(tmp, "\r%02d:%02d.%02d", h, m, s);
      putz(tmp);
    } else {
      // TODO: doesnt do right!
      //putchar('\r'); puti(s);

      putchar('\n'); puti(s);
    }

    state->slast= s;
  }

  return 0;
  (void)line;
}
