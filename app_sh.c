#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

#include "shell.c"

typedef struct APP {
  char*        line;
  cmdtrain*    train;
  cmdtrain*    loco;
  unsigned int cleanbits;
  char         n;
} APP;

void prompt() {
  // TODO: buggy write!!!
  //puts(RED "$" BLUE);
  putchar(red);
  putchar('$');
  putchar(blue);
}

void run(APP* app, char* cmd) {
  prompt(); puts(cmd); // we own the string!
  putchar(black);
  
  app->line= EOS;
  app->train= wsysparse(cmd, &app->n, &app->cleanbits);
  // move past NULL
  app->loco= app->train + 1;
  free(cmd);

#if 0
  // TODO: remove
  wrunsystrain(app->loco);
  shellcleanup(app->train, app->n, app->cleanbits);
  app->train= 0;
#endif
}

void* app_sh(APP* app, char* line) {
  if (!app) {
    app= calloc(sizeof(APP), 1);
    if (!app) return 0;
    
    // TODO: remove -c
    // TODO: otherwise is supposed to be script file to run!
    
    // TODO: remove testing
    //    if (!line) line= "iota 1 10|tail -5|head -2|terminal";
    if (!line)
      //line= "iota 1 1000|grep 0|grep 7|terminal";

#if 1
      // 14s - full interpret no yield (wrapper)
      // 13s - full interpret no yield (reuse lfree lstrdup)
      //   ??? no savings!
      // 11s - reuse but no print (print: 2s)

      line= "iota 1 1000|grep 7|terminal";
#else

#if 0      
      // 10s - with reuse!
      //
      // (7s + logic)
      // (2s + printing)
      // (4s + strdup/free) ==> 0s by simple single reuse cache!



      //  9s - optimal
      //  7s - if not printing
    {
      int i, n= 0;
      char s[10];
      for(i=1; i<1000; ++i) {
	snprintf(s, sizeof(s), "%d", i);
	if (strstr(s, "7")) {
	  puts(s);
	  //++n;
	}
      }
      //printf("%d results", n);
      return app;
    }
#else
      // 11s - no interpret overhead
      // 9s with reuse last string
    {
      int i;
      char n, * line;
      for(i=1; i<1000; ++i) {
	char s[10]; // no measurable cost (even if moved out)
	n= snprintf(s, sizeof(s), "%d", i);
	line= lstrdup(s);
	// 11s too: (but should be more efficient)
	// line= memdup(s, 1+snprintf(s, sizeof(s), "%d", i));
	if (strstr(line, "7")) {
	  puts(line);
	}
	lfree(line);
      }
      return app;
    }
#endif

#endif // OPTIMAL    

    // doesn/t do antyhing if already defined
    //    window(-1, -1, 20-6, 10, green, black);
    wstatus(-1, line? line: "Shell");

    run(app, strdup(line));

    return app;

  } //else if (line <= EVENTS)
  //return WAITKEY;

  // TODO: do we have a new line!
  //   need to save?
  
  // running command
  if (app->train) {
    //  14s wsystem()
    // ~22s one step per call here
    //char n= 1; // ~22s
    char n= 10; // ~16s 
    //char n= 100; // ~14w

    // TODO: make it run 2-4 ms, then YIELD
    // TODO: since it's in FOREGROUND, should get MORE cycles!

    n= 0;
    do {
      app->line= wtrainstep(&app->loco, app->line);
      //} while(--n && app->line != EOS);
      ++n;
    } while(KEEPRUNNING && app->line != EOS);
    //    putchar('0'+n);

    // TODO: also crashes if run a second time+
    //   but works w several at the ssame time!

    // finshed?
    if (app->line == EOS) {
      //putz("[EOS]");
      shellcleanup(app->train, app->n, app->cleanbits);
      free(app->train); app->train= 0;
      //putz("[/EOS]");
      return EOS;
    }

    // TODO: this isn't correct, as we may want to receive the
    //   output lines... LOL

    // CALLAGAIN
    return 0;
  }

  prompt();
  // TODO: read from script/stdin"

  return WAITKEY;
}
