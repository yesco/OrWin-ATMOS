
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

- Have fun!
- Simple, low effort window system
- Compile + run several C program together sharing stack
- Cheap coopertive tasks, basically event-driven actor system
- Have several window terminals
- ORIC Terminal - transparantly taking unquoted inline INK/PAPER
- A process/task is a window (limited to 16)
- Keyboard FUNCT-key used for control

- Override: `putc puts puti clrscr clreol gotoxy`
- Minimal API: `window setwin wstatus`
- Terminal Special Characters: `\n` (`\r \t \h`) inc:`0-7` paper:`16-23` inverse:`|128`
- Wraparound instead of scroll (fast!)

- TODO: VT100/52 compat
- Simple VI+Emacs-style editor
- TODO: Simple script/interactive programming language, maybe minipanda
- TODO: integrate w LOCI storage device, and oric "native" `DSK`
- TODO: Simple telnet using LOCI/usb-web devices


## Non-Goals

- No Overlapping terminals
- No Graphics, at least for now
- No separate binaries loaded (only interpreted apps)
- No generic relocation of C programs
- No multi-tasking pre-exising programs, you may switch to it
- No Slicing the stack (limits you to 4 processes max)



## OrWin Key-Controls

```
HELP       : FUNC-H

Go #-window: FUNCT-1   FUNCT-2    ...
Next window: FUNCT-N   FUNCT-SPC
Prev window: FUNCT-P
Last window: FUNCT-ESC FUNCT-TAB

Kill window: FUNCT-K   FUNCT-DEL
List window: FUNCT-L
Run new app: FUNCT-R   FUNCT-RETURN

Terminal   : FUNCT-T

Info       : FUNCT-I
```

## OrWin Key Code getc() remap

OrWin uses CTRL and FUNCT in combination with the ARROW keys.
It seems newer version of CC65 has mapped FUNCT (ALT) to the
high-bit, which is a nice addition.

However, the default ORIC ATMOS BASIC ROM (used by CC65),
is still mapping ARROW keys to ASCII: 8-11 which means
you can distinguish CTRL-H, CTRL-I, CTRL-J, CTRL-K from
the arrow keys.

OrWin maps keys as follows:

```
CTRL-A ... CTRL-Z : ASCII 1-26
RETURN            : ASCII 13
KEYLEFT           : 28
KEYRIGHT          : 29
KEYDOWN           : 30
KEYUP             : 31

A                 : 65
...
a                 : 97
...

DEL               : ASCII 127

> 128 ::: HI-BIT SET (typially FUNCT)

FUNCT + SHIFT + A : 128+65 == 'A' | 128
FUNCT + A         : 128+97 == 'a' | 128

FUNCT + LEFTKEY   : 128+LEFTKEY == LEFTKEY | 128
        ...

In addition:

CTRL + LEFTKEY    : 128+8 == CTRL+LEFTKEY-20, LOL
       ...

```

Generally, the keys print printed (like plain ARROWKEYS)
will move accordingly, like "normal ORIC", modulo bugs.


## OrWin terminal codes

OrWin takes the Unix interpretation of most special
ASCII control codes when printed, however, we *blend*
them with the ink codes as they "fit"!

You can simply use:
```
// single putchar calls
putchar(green); putchar(white|BG); puts("Hello");

// or more simply by concatentation gives speed:
puts(GREEN BGWHITE "Hello!");
```

An OrWin window/terminal `putchar` the following codes:
- `INK 0-7`: change ink color of text printed
- `BS (8) - backspace (non-rubout)
- `TAB \t (9)' - tab to next n*8
- `CRLF \n (10) (CTRL-J)` - newline (with clreol!)
- `CRLF \n (11)` - newline (no clreol, maybe DO: scroll!)
- `CTRL-L (12)` - `clrscr`
- `CR CTRL-M (13)` - cursor to column 0
- `CTRL-N (14)` - `clreol`
- `CTRL-O (15)` - TODO: ???
- `BG 16-23` - change background color
- `CTRL-X` - (TODO: CANcel: cancel/erase line?)
- `CTRL-Y` - (TODO: EM: End of Medium: yank/paste buffer/repeat?)
- `CTRL-Z` - (TODO: SUBstitute: useless padding) 
- `ESC (27)` - TODO: quote next char, or VT100
 - TODO: ??
- `KEYLEFT KEYRIGHT KEYDOWN KEYUP (28-31!)` will move the cursor!
- `DEL (127)` - rubout!

**HIBIT SET:**
- `0-7`: byte as is (inverted ink attribute)
- `8`;
- `9`:
- `10`:
- `11`
- `12`: HOME: gotoxy(0,0);
- `13`: CLNL: clreol(); nl();
- `14`
- `15`
- `16-23`: byte as is (inverted paper atttribute)
- `24``
- `25`
- `26`
- `27`
- `28`
- `29`
- `30`
- `31`
- ALL OTHER PRINTABLE CHARS: byte as is (inverted)
- `127`: INK 0: WHITE ink (attributte 0 but can
't be inside a C-string!)

At newline, or line-wrap the next line is *cleared*.

At end of the screen, it rolls around to the first line,
this is much smoother, but possibly, unusual. LOL

The ORIC-atttributes are special as they cease effect at
the end of the line and are reset. Then typically, screens
have a column of paper color and then a column of ink seting
if for every line.

This is in contradition with most terminals/ANSI/vt100
where, changing forground or background changes the rest
of written text.

In case of OrWin we take a step in the ANSI direction.
Changing ink changes the ink for that screen for each
new line printed and thus remember by that window.
This has the effect that `clrscr` will use that color,
as well as `clreol`. The same holds for background color.

A possible TODO: (when wrapping, the whole line changes
to new forground/background color.)

TODO: vt52 vt100 cursor gotoxy


## Apps

We generally call everything executing, APPs.
In practice, it can be:
- a stack task
- an cooperative handler

So call yield() once in a while during heavy processing.
Like in sorting. The guideline would be to time
it to every 2-4 ms!

TODO: there is a macro that will yield if needed
and it's cheap to call frequently.


### StackTasks ("Processes")

As the ORIC ATMOS doesn't have a real pre-emptive
tasking, even one can use interrupts to get close,
the cost of sharing/dividing/copying the stack is
not negliable. Therefore, we have struck the
compromise of alowing 3 mostly unmodified "unixy"
style apps run with their own stacks. This may be
fragile. The  beauty is that as long as they call
system routines like `kbhit getc putc puts (putz)`
the system will multi-task nicelly. However,
a crash is a crash and if you run a long time
without explicitly calling `yield();` the system
is locked and other processes are waiting.


### Tasks

Tasks are meant to be cheaper to process and you
can in principle have an unlimited number--limited
only by memory and window space!

They will not automatically yield, for now, instead
the run by the main program loop/scheduler, potentialy
with slightly more stack space, if there are fewer
"StackTask".

Instead, they work more like functions called by
the idle loop, or an app generic event-handler.

The implementation is a simple function:

```
typedef struct APP {
  int calls;	
} APP;

void* hello_app(APP* app, char* event) {
  if (!app) return calloc(sizeof(APP), 1);

  app->calls++;
  puti(app->calls); putchar(' ');

  return 0;
}
```

There apps can request to get keyboard key
events, and will be called when appropriate.


### Events

The following events can be received:
- `NULL (0)` - a request for next item (ignore: return 0)
- `EOS (1)` - end of input stream (for like unix processes)
- `RESIZE (2)` - user resized screen, redraw if you can
- `IDLE (3)` - idle tick
- `CLEANUP (255)` - program+window is getting KILLED, cleanup/deallocate extra memory, not state though)

- `KEY0-127 ($100-$1ff)` - i.e. `getc()+0x100`

- `IDLE`
- TODO: `TICK`


The return values returned by a "loop" task:
- `NULL` - request for more input
- TODO: `WAITKEY` - call back when a key is availble
- TODO: `SLEEP0-15ds` - 1-16 ds (deco 1/10th seconds) wakeup request
- TODO: `SLEEP1-240s` - 1s to 4 minutes wakeup request



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


## Note on AI usage

The main code is handwritten, and files with (C) notices copyright
belongs to me.

Some applications files `app_X.c` without such header, may have be
generated by Gemini AI and edited by me. Those are NOT important or
vital codes. No copyright claims on generated code. AI is mostly used
as a searcch and design support/discussion/discovery tool.

*I prefer to write my own code.*


