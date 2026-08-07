#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "orwin.h"

typedef struct APP {
  int i;
} APP;

void* init() {
  return calloc(sizeof(APP), 1);
}

void loop(APP* app) {
  // TODO: not nice
  if (kbhit()) {
    char c= cgetc();

    // TODO: abstract focus etc...
    if (c==9) { setfocus(); return; }
		     
    togglecursor();

    putc(c);
    if (c==13) putc(10); // CR-LF

    togglecursor();
  }
}  

int echo_main() {
  void* app= init();
  
  togglecursor();
  
  do { loop(app); } while(YIELD());
  
  return 0;
}
