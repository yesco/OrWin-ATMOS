#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

#include "shell.c"

typedef struct APP {
  char* line;
} APP;

void* app_shell(APP* app, char* line) {
  if (!app) {
    app= calloc(sizeof(APP), 1);
    if (!app) return 0;
    
    // TODO: remove testing
    if (!line) line= "iota 1 10|tail -5|head -2|terminal";

    app->line= strdup(line);

    window(-1, -1, 20-6, 11, green, black);
    wstatus(-1, "Shell");
    
    putchar('>');
    putchar(blue);
    puts(line);
    putchar(black);
    system(line);

    putchar('>');

    return app;

  } else if (line <= EVENTS)
    return WAITKEY;

  // have a new line!
  putchar('>');
  putchar(blue);
  puts(line);
  putchar(black);
  system(line);

  putchar('>');

  return WAITKEY;
}
