
 ██████╗ ██████╗ ██╗    ██╗██╗███╗   ██╗     █████╗ ████████╗███╗   ███╗ ██████╗ ██████╗
██╔═══██╗██╔══██╗██║    ██║██║████╗  ██║    ██╔══██╗╚══██╔══╝████╗ ████║██╔═══██╗██╔════╝
██║   ██║██████╔╝██║ █╗ ██║██║██╔██╗ ██║    ███████║   ██║   ██╔████╔██║██║   ██║███████╗
██║   ██║██╔══██╗██║███╗██║██║██║╚██╗██║    ██╔══██║   ██║   ██║╚██╔╝██║██║   ██║╚════██║
╚██████╔╝██║  ██║╚███╔███╔╝██║██║ ╚████║    ██║  ██║   ██║   ██║ ╚═╝ ██║╚██████╔╝██████╔╝
 ╚═════╝ ╚═╝  ╚═╝ ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝    ╚═╝  ╚═╝   ╚═╝   ╚═╝     ╚═╝ ╚═════╝ ╚═════╝

# OrWin ATMOS

An experimential C-based retro text-based windowing system
and multitasking environment for the ORIC ATMOS.


## Goals

- Simple, low effort window system
- Compile + run several C program together
- Mimic several (fixed size/position) ORIC text screens
- A process is a window!
- FUNCT-key for control
- YIELD(); (TODO: Pre-emptying multitasking)

- Override: <tt>putc puts puti clrscr clreol gotoxy</tt>
- Minimal API: <tt>window setwin wstatus</tt>
- Terminal Special Characters: \n (\r \t \h) inc:0-7 paper:16-23 inverse|128
- Wraparound instead of scroll (optional scroll)

- VT100/52 compat

- Simple Emacs-style editor
- Simple script/interactive programming language, maybe minipanda
- Simple telnet


## Non-Goals

- No Overlapping terminals
- No Movable terminal
- No Graphics, at least for now, or make it All graphics!
- No separate binaries
- No generic relocation
- No for pre-exising programs


## OrWin Key-Controls

```
HELP       : FUNC-H

Go #-window: FUNCT-number
Next window: FUNCT-spc FUNCT-N   FUNCT-rightarrow
Prev window:           FUNCT-P   FUNCT-leftarrow
Last window: FUNCT-ESC FUNCT-T   FUNCT-I

KILL window: FUNCT-K   FUNCT-Q   FUNCT-DEL
List window: FUNCT-L   
Run new app: FUNCT-R   FUNCT-RETURN
```

TODO: List/Run

## Building

1. Install cc65
2. ./tap window
3. Assumes ORIC symlink to directory where to copy the  window.tap file
4. Run it in your emulator, or upload it


## Applications

Applications are relatively simple to build.

```
// app-atmos.c - a simple app for OrWin-ATMOS
#include <stdlib.h>

#include "orwin.h"

int atmos_main() {
  char j, i= 0;
  
  while(1) {
    if (++i >= 64) i= 0;
    for(j= i & 31; j--; ) putc(' ');
    putc(i & 7); puts("Atmos");
  }
  
  return 0;
}
```

*caveats:*

- *DON'T* use *globals*!
- if you use globals you can only run one instance of your program
- `puts() puti() kbkhit() getc()` implicitly calls `yield()` which makes it all tick
- if you do long calculations, it'll block
- you then need to insert `yield()` at convenient locations


## "YIELD THE POWER" - Interactivety

In order for a decent user experience, interactive
applications should be called more often. Typically,
you it would be the app that has the focus and where
you currently type. When typing, you'd want it to respond
*immediatly*.

We have made the following design choices:
- if the app has focus:
- `kbhit` will return immediately if there is a keypress
- otherwise, it will yield
- `getc` will call `kbhit` in a loop and behave as expected
- it returns without yielding if there is a key to read

=> If there is no keypress, the app will yield(), with
the assumption that it's waiting, but maybe has some
"background" task of it's own to do. When its turn comes
again, it will return from `kbhit` allow for that.

It could be possible that a user "typing" so fast, and/or
that the program is so slow that it totally "monopolizes"
the CPU. Not sure if we need safe-guards against that.
As long as it's waiting on real keyboard presses.

If it reads from other source, it may be a different
consideration.


TODO:
- mark if an app `getc` is waiting/checking for input
- if not in focus, don't run at all (waiting in getc)


## Throughput

Interrupting a flow of data printed may feel "chunky".
`putchar` employs the trick of yielding when it prints a
newline or wraps around to next line. This mimics old
terminals and feels natural.

Even so, we want to limit, so we do a timming test since
last yield; if we "exhausted" our quota putchar will yield
automatically.

Bulk operations like `puts putz` may do a lot of work;
you use them because they are efficient, but since they
are unbounded and may make other programs hickup.

We have implemented a minimal-overhead `putz` (called by puts),
enabled by OPTPUTZ, and it has maybe 3x higher througput than
repeated calls to `putchar`. Still, We will yield after
ca 64 (MAXPUTZ) characters, or explicit newlines.

**TODO:**
- 


## Ideas

As we wnt to make it as simple as possible, we avoid keeping history
of terminal data, it's assumed that if it's an interactive program
that pressing CTRL-L would redraw the screen. We may, however, take
the visible text and use it to redraw new sized window.

Overlapping window is where all the complex update problems come from,
so maybe just ignore them by not allowing it; maybe make some default using
tiling, or just allow resize/move throughout empty space on screen!

Possibly, have a Maximize mode for current window, but when swapping out
it minimied to old size.

How to handle variable type clashes. We're not using C++ namespaces in cc65.
Maybe just have a file be required to compile by itself and ONLY do export
on external visible functions. (Severe limitiation for porting). I guess one
could do renames in .o files, .o65 library files?




