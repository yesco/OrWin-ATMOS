
● The benchmark builds cc65 with its own 11-byte startup, --target none, a
  custom none.cfg, and a raw binary with no load header. I'm rebuilding dummy
  that way next to reproduce the 15 B.

  Ran 1 shell command

● Reproduced the bug at 15 bytes using their boot code and main routine; next
  I'll compare against the 226 bytes cc65 produces for the same empty test.

  Ran 1 shell command

● I reproduced the 15 B. It's real, but it's a bare build that leaves out
  everything a runnable cc65 program normally carries, so it isn't comparable
  to ptest's cc65 column.

  How raxiss builds cc65 (../oricCompilerBenchmark/playground/share/cc65/):
  - cc65 -O --target none, then ld65 -C none.cfg with _start = $0800.
  - Its own boot0.s as the whole startup: lda #<__stack / sta sp / lda
    #>__stack / sta sp+1 / jmp _main, which is 11 bytes.
  - x-bin.sh just copies the result, so the binary has no load header.

  Doing exactly that in the scratchpad gives dummy.prg = 15 bytes:
  a9 00 85 00 a9 80 85 01 4c 0b 08   boot0: sp = $8000, jmp _main
  a2 00 8a 60                        main:  ldx #0 / txa / rts
  So that number is cc65's code for main (4 bytes) plus a minimal hand-written
  entry. There's no runtime at all: no BSS clear, no constructors, no exit, and
  main's rts returns to whatever called $0800.

  What ptest measures: cl65 -t sim6502 -Cl -Os on test-empty gives 191 bytes
  (the ptest line is "226 cycles (191 bytes)"). From the linker map:

  ┌───────────────────────────────────────────────────────┬───────┐
  │                         part                          │ bytes │
  ├───────────────────────────────────────────────────────┼───────┤
  │ sim65 header (exehdr)                                 │ 12    │
  ├───────────────────────────────────────────────────────┼───────┤
  │ crt0 (STARTUP)                                        │ 29    │
  ├───────────────────────────────────────────────────────┼───────┤
  │ zerobss                                               │ 35    │
  ├───────────────────────────────────────────────────────┼───────┤
  │ condes: constructor/destructor code, table data, ONCE │ 61    │
  ├───────────────────────────────────────────────────────┼───────┤
  │ pushax (pulled in by callmain)                        │ 26    │
  ├───────────────────────────────────────────────────────┼───────┤
  │ callmain (argc/argv)                                  │ 27    │
  ├───────────────────────────────────────────────────────┼───────┤
  │ main                                                  │ 1     │
  └───────────────────────────────────────────────────────┴───────┘

  That's the full runnable sim65 program, which is also how the IVM sizes are
  measured. So 15 vs 191  Jump to bottom (ctrl+End) ↓ ytes of boot0 against 190

That's the full runnable sim65 program, which is also how the IVM sizes are    measured. So 15 vs 191  Jump to bottom (ctrl+End) ↓ ytes of boot0 against 19

● Right, for dummy it's essentially the size of main plus their 11-byte
  startup.

  And no, their hello-world doesn't use printf. It's:

  #include <conio.h>
  int main(void) { _puts("HELLO 6502 WORLD!\x0a"); return 0; }

  _puts and _putc are their own tiny C library in libmos6502vm/, compiled by
  each compiler along with the program:
  - _putc(c): just poke(0xFFFF, c), a byte stored to the mos6502vm emulator's
    console port. No ROM call, no OS, no file I/O.
  - _puts(s): while (*s) _putc(*s++);

  I built it their way with our cc65 (2.18) and got 205 bytes, against their
  197; the difference is probably the cc65 version. The breakdown:

  ┌─────────────────────────────────────────────────────────────────┬───────┐
  │                              part                               │ bytes │
  ├─────────────────────────────────────────────────────────────────┼───────┤
  │ boot0                                                           │ 11    │
  ├─────────────────────────────────────────────────────────────────┼───────┤
  │ main                                                            │ 11    │
  ├─────────────────────────────────────────────────────────────────┼───────┤
  │ the string                                                      │ 19    │
  ├─────────────────────────────────────────────────────────────────┼───────┤
  │ _puts                                                           │ 42    │
  ├─────────────────────────────────────────────────────────────────┼───────┤
  │ _putc                                                           │ 18    │
  ├─────────────────────────────────────────────────────────────────┼───────┤
  │ cc65 runtime helpers (pushax, pusha, incsp2, staxsp, ldaxsp,    │ 104   │
  │ incax1, incsp1)                                                 │       │
  └─────────────────────────────────────────────────────────────────┴───────┘

  Even then, half the size is cc65's software-stack helpers, which come from
  passing a pointer argument in C.

  test-hello.c uses printf("Hello World!\n"). For cc65 that pulls in the whole
  printf formatter plus stdio and the sim65 file I/O, which is where its 2287
  bytes come from. oscar64's 4101 bytes is the same thing, its own printf. So
  our cc65/oscar hello numbers and theirs measure different programs.

  Our side does better because the compiler turns that printf into the " op,
  and the JIT into jsr _opprintstr with the string inline:
  - hjit (36 B): the code plus the string, no library.
  - jit (211 B): a full runnable image with lib startup and
    putchar/printstr/puts over sim65's write().

  Their 197 B sits between those two. It's closest to our hjit plus a
  two-instruction putc, because a store to a console port is about as cheap as
  output can get.

  For a like-for-like comparison, we could compile their 02-hello-world (with
  _puts/_putc implemented as "/E or a putchar op) through our compiler and the
  JIT. Their sample set would also make a good benchmark suite for ptest.
