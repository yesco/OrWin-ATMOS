#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"


typedef struct APP {
  unsigned int i;
} APP;


void* app_atmos(void* voidapp, char* line) {
  unsigned int j;
  
  if (!voidapp) {
    app= calloc(sizeof(APP), 1);
    wstatus(-1, "Oric ATMOS");
    window(3, 12, 30, 15, black, white);
    return app;
  }

  if (++app->i >= 64) app->i= 0;
  for(j= app->i & 31; j--; ) putchar(' ');
  putchar(app->i & 7); putz("Atmos");
  
  return 0;
  (void)line;
}
