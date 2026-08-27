#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

#include "shell.c"

typedef struct APP {
  char* line;
} APP;

void prompt() {
  // TODO: buggy write!!!
  //puts(RED "$" BLUE);
  putchar(red);
  putchar('$');
  putchar(blue);
}

void run(char* line) {
  prompt(); puts(line);
  putchar(black);
  system(line);
}

void* app_shell(APP* app, char* line) {
  if (!app) {
    app= calloc(sizeof(APP), 1);
    if (!app) return 0;
    
    // TODO: remove testing
    //    if (!line) line= "iota 1 10|tail -5|head -2|terminal";
    if (!line)
      //      line= "iota 1 1000|grep 0|grep 7|terminal";
      line= "iota 1 1000|grep 7|terminal";

    app->line= strdup(line);

    window(-1, -1, 20-6, 10, green, black);
    wstatus(-1, "Shell");
    run(line);

    return app;

  } else if (line <= EVENTS)
    return WAITKEY;


  // have a new line!

  run(line);
  lfree(line);
  
  prompt();

  return WAITKEY;
}
