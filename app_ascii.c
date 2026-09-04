#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

typedef struct APP {
  char c;
} APP;

void* app_ascii(void* voidapp, char* line) {
  if (!app) return calloc(sizeof(APP), 1);
  
  putchar((app->c++ % 96)+32);

  return 0;
  (void)line;
}
