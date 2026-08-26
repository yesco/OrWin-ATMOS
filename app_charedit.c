#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "orwin.h"

// TODO: make an "oric.h"
#define CHARSET    ((char*)0xB400) // $B400-B7FF
#define CHARDEF(C) ((char*)CHARSET+(C)*8)
#define ALTSET     ((char*)0xB800) // $B800-BB7F


typedef struct APP {
  char x, y, c, *def;
} APP;

void draw(APP* app) {
  int r, c;

  gotoxy(0,0);
  printf("%c $%02x %d\n", app->c, app->c, app->c);
  printf(" (%d, %d)", app->x, app->y);
  nl();
  for(r=0; r<8; ++r) {
    char v= app->def[r];
    putchar(black); putchar(BG|yellow);
    for(c=5; c>=0; --c) {
      if (c == app->x && r == app->y)
	putcraw((v & 0b100000)? BG|red: BG|yellow);
      else
	putcraw((v & 0b100000)? BG|black: BG|white);
      v<<= 1;
    }
    putchar(BG|yellow); clnl();
  }
}

void* app_charedit(APP* app, char* line) {
  char c;

  if (!app) {
    // TODO: reduce by one once attributes OPT at beginning line
    window(-1, -1, 10, 11, yellow, black);
    wstatus(-1, "Char Edit");
    
    c= isdigit(*line)? atoi(line): *line;
    if ((c & 0x7f) < 32) c= '`'; // copyright - useless!?

    app= calloc(sizeof(APP), 1);
    app->c= c;
    app->def= CHARDEF(c);

    draw(app);

    return app;
  }
  
  // move around with arrow keys, space to toggle
  // TODO: takes some code, can generalize?
  //   can have a general cursor movement passthrough
  //   through putchar, other keys filterred.
  switch((c= getc())) {
  case 0: break; // no key
    // LEFT and RIGHT seems  swapped because bit pos!
  case KEYRIGHT: if (app->x > 0) --app->x; break;
  case KEYLEFT : if (app->x < 5) ++app->x; break;
  case KEYDOWN : if (app->y < 7) ++app->y; break;
  case KEYUP   : if (app->y > 0) --app->y; break;
  case ' '     : app->def[app->y]^= 1 << app->x; break;
  // TODO: undo
  }

  draw(app);
  printf("%d %02x", c, c);
  
  return WAITKEY;
}
