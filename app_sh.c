#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

#include "shell.c"

typedef struct APP {
  char*        origcmd;

  // TODO: make it's own control struct runtrain
  char*        line;
  cmdtrain*    train;
  char         n;
  unsigned int cleanbits;

  cmdtrain*    loco;
} APP;

void prompt() {
  // TODO: buggy write!!!
  //puts(RED "$" BLUE);
  putchar(red);
  putchar('$');
  putchar(blue);
}

void run(void* voidapp, char* cmd) {
  prompt(); puts(cmd);
  putchar(black);
  
  app->train= wsysparse(cmd, &app->n, &app->cleanbits);

  // start: move past NULL
  app->loco= app->train + 1;
  app->line= EOS;
}

void* app_sh(void* voidapp, char* line) {
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

      wins[wcur].args= line= strdup("iota 1 1000|grep 7|terminal");
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
    // TODO: or is it a terminal? LOL
    wstatus(-1, line? line: "Shell");

    // may not need store!
    // TODO: it's winp->args!!!!
    app->origcmd= strdup(line);
    run(app, line); // consumed!

    return app;

  } else if (line == CLEANUP) {
    // TODO: currently leaks as no cleanup is sent!
    free(app->origcmd); free(app->line); free(app->train);
  }
#if 0
  else if (KEYEVENT(line))
    ;
  else if (line < EOS)
    return WAITKEY;
#endif

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
      shellcleanup(app->train, app->n, app->cleanbits);
      app->train= NULL;
      return EOS;
    }

    // TODO: this isn't correct, as we may want to receive the
    //   output lines... LOL

    // CALLAGAIN
    return 0;
  }

  if (!KEYEVENT(line)) return WAITKEY;
  
  if (app->origcmd) {
    // we had command line -c argument
    // RERUN on every keypress!
    clrscr();
    run(app, app->origcmd);
    return 0;
  }
  
  // script input
  prompt();
  

  return WAITKEY;
}
