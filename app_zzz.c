#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"


typedef struct APP {
  unsigned int slast;
} APP;

// dummy
void* app_zzz(APP* state, char* line) {
  if (!state) return calloc(sizeof(APP), 1);
  
  return 0;
  (void)line;
}
