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

void draw(char x, char *def) {
  int r, c;

  //clrscr();
  printf("%c $%02x %d\n\n", x, x, x);
  for(r=0; r<8; ++r) {
    char v= def[r];
    putchar(black); putchar(BG|yellow);
    for(c=0; c<6; ++c) {
      putchar((v & 0b100000)? BG|white: BG|black);
      v<<= 1;
    }
    putchar(BG|yellow); nl();
  }
}

void* app_charedit(APP* app, char* line) {
  if (!app) {
    char c;
    // TODO: reduce by one once attributes OPT at beginning line
    window(-1, -1, 8, 11, yellow, black);
    wstatus(-1, "Char Edit");
    
    c= isdigit(*line)? atoi(line): *line;
    if ((c & 0x7f) < 32) c= '`'; // copyright - useless!?
    app->sel= c;
    app->def= CHARDEF(c);

    draw(c, app->def);

    return calloc(sizeof(APP), 1);
  }
  
  // TODO: make edit with cursor keys
  
  return 0;
}
