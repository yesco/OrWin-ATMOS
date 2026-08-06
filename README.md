
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
- Pre-empting multitasking (stack ptr manipulation/copy)
- Mimic several (fixed size/position) ORIC text screens

- Override: <tt>putc puts puti clrscr clreol gotoxy</tt>
- Minimal API: <tt>window setwin wstatus</tt>
- Terminal Special Characters: \n (\r \t \h) inc:0-7 paper:16-23 inverse|128
- Wraparound instead of scroll (optional scroll)

- Simple Emacs-style editor
- Simple script/interactive programming language, maybe minipanda


## Non-Goals

- No Overlapping terminals
- No Movable terminal
- No Graphics, at least for now, or All graphics!
- No separate binaries
- No generic relocation
- No for pre-exising programs


## Building

1. Install cc65
2. ./tap window
3. Assumes ORIC symlink to directory where to copy the  window.tap file
4. Run it in your emulator, or upload it


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




