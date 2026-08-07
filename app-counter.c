#include <stdlib.h>

#include "orwin.h"

typedef struct APP {
  int i;
} APP;

static void* init() {
  return calloc(sizeof(APP), 1);
}

static void loop(APP* app) {
  if (++app->i % 25 == 0) {
    puti(app->i); putchar('\t');
  }
}  

int counter_main() {

  APP* app= init();
  
  do { loop(app); } while(YIELD());
  
  return 0;
}
