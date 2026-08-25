#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

typedef struct APP {
  char sel;
} APP;

void* app_charset(APP* app, char* line) {
  if (!app) {
    int c;

    window(-1, -1, 18, 11, green, black);
    wstatus(-1, "Charset (edit)");
    
    gotoxy(0, 0);
    for(c= 0; c<256; ++c) {
      if ((c & 0x7f) < 32) continue;
      // beginning of line
      if ((c & 0b1111)==0) putcraw(c<=128? 8: 9);
      if (c < 128+32+80) putcraw(c & 0x7f);
      // end of line
      if ((c & 0b1111)==0b1111) { putcraw(8); putchar(' '); }
    }

    return calloc(sizeof(APP), 1);
  }
  
  return 0;
}
