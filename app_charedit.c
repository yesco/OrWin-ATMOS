#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "orwin.h"

// TODO: make an "oric.h"
#define CHARSET    ((char*)0xB400) // $B400-B7FF
#define CHARDEF(C) ((char*)CHARSET+(C)*8)
#define ALTSET     ((char*)0xB800) // $B800-BB7F


typedef struct APP {
  char sel, *def;
} APP;

void draw(char *def) {
  int r, c;

  clrscr();
  for(r=0; r<8; ++r) {
    char v= def[r];
    for(c=0; c<6; ++c) {
      putchar( (v & 0b100000)? 127: 32);
      v<<= 1;
    }
    nl();
  }
}

void* app_charedit(APP* app, char* line) {
  if (!app) {
    window(-1, -1, 8, 10, white, black);
    wstatus(-1, "Char Editor");
    
    app->sel= isdigit(*line)? atoi(line): *line;
    app->def= CHARDEF(app->sel);

    draw(app->def);

    return calloc(sizeof(APP), 1);
  }
  
  // TODO: make edit with cursor keys
  
  return 0;
}
