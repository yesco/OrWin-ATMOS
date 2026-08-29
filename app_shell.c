#include <stdio.h>
#include <stdlib.h>

#include "orwin.h"

#include "shell.c"

typedef struct APP {
  //char* line;
  char dummy;
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
