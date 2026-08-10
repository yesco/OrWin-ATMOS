#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

typedef struct APP {
  char c;
} APP;

void* ascii_init() {
  return calloc(sizeof(APP), 1);
}

void ascii_loop(APP* app) {
  putchar((app->c++ % 96)+32);
}  

int ascii_main() {
  void* app= ascii_init();
  
  do { ascii_loop(app); } while(1);
  
  return 0;
}
