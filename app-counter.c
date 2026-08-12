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

int counter_main(int argc, char** argv) {

  APP* app= init();
  
  do { loop(app); } while(1);
  
  (void)argc; (void)argv;
  return 0;
}
