# OrWIN-ATMOS


# MeteoriC-65 (parse.c c.bnf): next, after the manual (MeteoriC-man.md)

-- ./pc TMP/test-rax-06-sieve.c                    --- parse ---
    TMP/test-rax-06-sieve.c:18: error: cannot parse (function not defined: sizeof)                                                              TMP/test-rax-06-sieve.c:18:   for(j=0; j < sizeof(buf)/sizeof(char);) TMP/test-rax-06-sieve.c:18:                                         ^ TMP/test-rax-06-sieve.c:18: error: cannot parse (function not defined: sizeof)                                                              TMP/test-rax-06-sieve.c:18:   for(j=0; j < sizeof(buf)/sizeof(char);)
    TMP/test-rax-06-sieve.c:18:                                         ^
    TMP/test-rax-06-sieve.c:18: error: cannot parse                       TMP/test-rax-06-sieve.c:18:   for(j=0; j < sizeof(buf)/sizeof(char);) TMP/test-rax-06-sieve.c:18:                                         ^ TMP/test-rax-06-sieve.c:49: error: cannot parse                       TMP/test-rax-06-sieve.c:49:         else
    TMP/test-rax-06-sieve.c:49:             ^

According to RAXIS: not sure how cc65 gets that!!!

(size, bytes)
                                          osdk     osdk
                 cc65  llvm-mos    vbcc  lcc65  ...-1.x  kickc             sdcc
\BYTES             -O       -Oz  -O=991    -O3      -O2         --opt-code-size

sieve            1108      1162     914   1160     9745      C            10012

sieve            10,318407   6,786242    7,559299   14,943291   16,502355         C   20,788032

Fastest: llvm-mos (6.79s) and vbcc (7.56s) are fastests
cc65: can't replicate... It's at 22s! see below

./ptest 06-sieve
=== ./ptest rax-06-sieve                                              =          - FAILED                - pc    - test-rax-06-sieve        =            ./pc TMP/test-rax-06-sieve                               = 12,568,166 cycles ( 1509 bytes) - cc65  - test-rax-06-sieve
=  4,130,355 cycles ( 1057 bytes) - oscar - test-rax-06-sieve
~/GIT/OrwinClaude_1 $

./ptest byte-sieve-array
=== ./ptest byte-sieve-array
= 334,348,469 cycles (  131 bytes) - ivm   - byte-sieve-array
= 104,314,344 cycles (  131 bytes) - asm   - byte-sieve-array
=  81,481,026 cycles (  131 bytes) - fix   - byte-sieve-array
=  11,584,333 cycles (  478 bytes) - zpjit - byte-sieve-array  3.65 B/BC
=       norun cycles (  465 bytes) - ztap  - byte-sieve-array  3.55 B/BC
=  12,814,781 cycles (  274 bytes) - hjit  - byte-sieve-array  2.09 B/BC
=  25,385,189 cycles ( 1106 bytes) - cc65  - byte-sieve-array  8.44 B/BC
(-Cl -Oirs)
=  22,792,649 cycles ( 1110 bytes) - cc65  - byte-sieve-array  8.47 B/BC
BEFORE (printf)
=  12,639,850 cycles ( 4337 bytes) - oscar - byte-sieve-array  33.11 B/BC
=  12,822,517 cycles (  660 bytes) - oscar - byte-sieve-array  5.04 B/BC

./ptest byte-sieve-prime
=== ./ptest byte-sieve-prime
= 333,853,144 cycles (  130 bytes) - ivm   - byte-sieve-prime
= 104,322,336 cycles (  130 bytes) - asm   - byte-sieve-prime
=  81,489,119 cycles (  130 bytes) - fix   - byte-sieve-prime             
=  13,052,144 cycles (  707 bytes) - zpjit - byte-sieve-prime  5.44 B/BC  
=       norun cycles (  696 bytes) - ztap  - byte-sieve-prime  5.35 B/BC
=  14,981,309 cycles (  299 bytes) - hjit  - byte-sieve-prime  2.30 B/BC  
=  26,598,152 cycles ( 1916 bytes) - cc65  - byte-sieve-prime  14.74 B/BC 
=  15,047,706 cycles ( 1054 bytes) - oscar - byte-sieve-prime  8.11 B/BC  ~/GIT/OrwinClaude_1 $l


-- NOT FAIR: we have fake malloc() and no free() lol

-- fold function calls with constants (if called once?)
    cat TMP/test-rax-05-0xcafe.c
    // test-rax-05-0xcafe.c - raxiss's sample 05-0xcafe (made by ptest)
    //
    // From ../oricCompilerBenchmark/playground/samples/05-0xcafe.
    // Changed: its #include "..." inlined, <conio.h> <types.h>
    // as "conio.h" "types.h" (theirs, in TMP).

    #include "conio.h"
											       static char hex[] = "0123456789ABCDEF";                                                    static void puthex(unsigned int val)                                                       {                                                                                            unsigned char t;                                                                           t = (val >> 12) &0x0f; _putc(hex[t]);
      t = (val >>  8) &0x0f; _putc(hex[t]);
      t = (val >>  4) &0x0f; _putc(hex[t]);                                                      t = (val >>  0) &0x0f; _putc(hex[t]);
    }
											       static unsigned int Oxcafe = 0b1100101011111110;
    int main(void)
    {                                                                                            _puts("0XCAFE == 0X");
      puthex(Oxcafe);                                                                            _puts(Oxcafe == 0xcafe? " :)":" :(");
      _putc(LF);                                                                                                                                                                            return 0;                                                                                }


-- FIX: char* a[]= {"ab","cd"} is now a compile error.
        It used to compile to bad pointers and crash.
	I guess these values need to be "relocated", hmmm, complicated!
	(generic case needs a FIXit list..., bah...)
	FIXIT-LIST: byte offsets: 3 4 7 99 3 5 7 (99 -> 3 means add 256!)
        This avoids add stuff!
	How to end? last number repeated. or 0 0, however that's valid seq?
	
    Efficient RELOCATION scheme (1 byte per fix)

    Using $00 as an escape character solves both the 256-byte stride problem and gaps larger than 256 bytes in a very clean, compact way.

    How $00 Escape Encoding Works

    Instead of treating $00 as a normal offset, treat $00 as a control prefix followed by a second byte (param):

    Byte 1	Byte 2 (param)	Meaning	Action
    $01–$FF	(none)	Normal offset	If new_Y <= prev_Y, INC base+1. Then patch (base),Y.
    $00	$00	End of stream	Terminate relocation loop (DONE).
    $00	$01–$FF	Page skip / Reset	Add param to base+1 (base_hi += param) and reset prev_Y = 0 without patching.


    How This Handles Every Edge Case

    1. Patching offset $00 (including 256-byte strides):
	    • Because $00 is reserved as the escape prefix, you never emit a raw $00 to patch offset 0 of a page.
	    • Instead, you emit $00, $01 (which advances base+1 by 1 page and sets prev_Y = 0)—or if you want to patch offset $00 on the current page without advancing, you simply pre-bias base by -1 at the start of the routine!
	    • The -1 Pre-Bias Trick: If you initialize base = target_address - 1 (and prev_Y = 0), then offset $00 on a page becomes $01! Now $01 through $FF covers offsets 0 through 254 on the page, and offset 255 ($FF of the real page) wraps naturally to $00 of the next page (which is $01 after INC base+1). Every single byte in memory is addressable as $01–$FF, leaving $00 100% free as an escape code.
    2. Gaps larger than 256 bytes (up to 65 KB!):
	    • If the next relocation is 10 pages away, you don't need 10 dummy bytes. You just emit $00, $0A (base+1 += 10, prev_Y = 0), followed by the next $01–$FF offset.
    3. End of stream:
	    • Emit $00, $00.

    Compact 6502 Decoder (~30 Bytes)

    Assuming base starts at target_address (pre-biased for the high byte of pointers, minus 1 so $01 = byte 0 of the page):
    6502
	    LDY #$00
	    STY prev_y        ; prev_y = 0

    loop:
	    JSR next_byte     ; Reads next byte from relocation stream into A
	    BEQ escape        ; If A == $00, handle escape sequence

	    CMP prev_y        ; Compare new_Y with prev_Y
	    BEQ next_page     ; new_Y == prev_Y -> 256-byte stride! Advance page
	    BCS do_patch      ; new_Y > prev_Y  -> Same page
    next_page:
	    INC base+1        ; new_Y <= prev_Y -> Advance 1 page (+256)

    do_patch:
	    STA prev_y        ; Update prev_y
	    TAY
	    LDA (base),Y      ; Patch high byte
	    CLC
	    ADC delta_hi
	    STA (base),Y
	    JMP loop

    escape:
	    JSR next_byte     ; Read second byte of escape sequence
	    BEQ done          ; $00, $00 -> End of relocation list!
	    CLC
	    ADC base+1        ; $00, N   -> Skip N pages (up to 255 pages / 65,280 bytes!)
	    STA base+1
	    LDA #$00
	    STA prev_y        ; Reset prev_y so next $01..$FF won't trigger an extra INC
	    JMP loop

    done:
	    RTS
    Use code with caution.

    Why This Encoding Is Hard to Beat on 6502

    • 1 byte per relocation for all normal fixups, even with a 256-byte stride ($05, $05, $05 works seamlessly because BEQ next_page catches new_Y == prev_Y).
    • 2 bytes total to skip anywhere from 256 bytes up to 65 KB of code/data that has no relocations ($00, $N).
    • 2 bytes total for the terminator ($00, $00).
    If you'd like to optimize the next_byte stream reader or adapt this for arbitrary (non-page-aligned) byte relocations where the low byte also needs patching, let me know!

    # has it been done before?
    
    Yes, variations of this exact technique have been used in classic and modern 6502 systems, though your specific variant (Y-offset wrap detection + $00 escape) is a particularly tight, clever synthesis of them.

    Notable Real-World Examples on 6502

    • The o65 Binary Format (used by cc65, LUnix, and GeckOS):
    Designed by André Fachat in 1997, the standard o65 relocation table uses 1-byte delta offsets between fixups. Instead of $00 as an escape, o65 reserves $FF (255) as an escape value meaning "advance the pointer by 254 bytes without patching, and read the next byte," and uses $00 to terminate the relocation table. (Unlike your approach, o65 adds the delta to a 16-bit base pointer each time rather than storing raw low bytes for Y.) [1] (https://6502.org/users/andre/o65/fileformat.html), [2] (https://6502.org/users/andre/o65/)
    • C64 OS Page-Aligned Relocator:
    C64 OS uses a virtually identical concept for page-aligned relocatable drivers and modules. Its relocation table stores the raw low-byte Y offsets of every high byte that needs patching, using $00 as a terminator (and placing the code right after the table so offset $00 on page 0 is never a valid fixup). [1] (https://c64os.com/post/relocatable_6502)
    • Apple II / ProDOS & GEOS Relocators:
    Early 6502 operating systems (like GEOS on the C64 and Apple II) routinely generated page-aligned relocation tables by assembling a program twice at two different page-aligned addresses (e.g., $1000 and $1100), XORing or diffing the two binaries, and recording the low-byte Y offsets where the high bytes differed.

    What Makes Your Variant Distinct

    Most historical 6502 formats either:
    1. Stored delta offsets (like o65), which requires doing a 16-bit addition (CLC / ADC base / BCC / INC base+1) on every single fixup.
    2. Stored raw Y offsets per page (like C64 OS), which avoids 16-bit addition via LDA (base),Y. [1] (https://6502.org/users/andre/o65/fileformat.html), [2] (https://c64os.com/post/relocatable_6502)
    Combining raw Y offsets with wrap-around detection (new_Y <= prev_Y -> INC base+1) and a $00 page-skip escape avoids 16-bit addition on the 6502 while still supporting multi-page binaries, 256-byte strides, and arbitrary gaps—something even o65 needs multiple $FF bytes to skip.
    If you're building a toolchain around this (for example, with LLVM-MOS or ca65), let me know which assembler or compiler you're using—I can show you how to generate this relocation table automatically by assembling at two base addresses and diffing the binaries.

    # efficiency comparisions and extra cost
    
    To compare your Y-wrap + $00 escape method against the standard o65 delta-addition method, your approach saves ~7–12 CPU cycles per relocation at the cost of roughly 6 to 10 extra bytes of decoder code.
    Here is the exact breakdown for standard NMOS 6502 (assuming zero-page pointers and an inlined stream reader like LDA (reloc_ptr,X) / INC reloc_ptr / BNE / INC reloc_ptr+1).

    Side-by-Side Comparison


    Metric	Your Method (Y-Wrap + $00 Escape)	Standard o65 (Delta Addition + $FF Escape)	Winner
    Decoder Code Size	~44–48 bytes	~36–40 bytes	o65 (~8 bytes smaller)
    Cycles per Relocation (same page)	~33 cycles	~40 cycles	Yours (~7 cycles faster)
    Cycles per Relocation (page wrap)	~37 cycles	~42 cycles	Yours (~5 cycles faster)
    Table Size: Normal Fixup	1 byte	1 byte	Tie
    Table Size: 1 KB Gap (no fixups)	2 bytes ($00, $04)	5 bytes ($FF, $FF, $FF, $FF, $08)	Yours (60% smaller)
    Table Size: 256-Byte Stride	1 byte per entry	2 bytes per entry ($FF, $02)	Yours (50% smaller)


    Why Your Method Is Faster (Cycle Breakdown)

    In the o65 delta approach, the CPU has to update a 16-bit pointer on every single step and reset Y = 0:
    6502
    ; --- Standard o65 per-relocation overhead (~18 cycles) ---
    CLC                 ; 2
    ADC base            ; 3  (Add delta to 16-bit base pointer)
    STA base            ; 3
    BCC no_inc          ; 2/3
    INC base+1          ; 5
    no_inc:
    LDY #$00            ; 2  (Must zero Y every time!)
    LDA (base),Y        ; 5  (Always incurs 5-cycle indirect read)
    Use code with caution.
    In your method, base never gets its low byte touched, and Y carries the low-byte offset directly into (base),Y:
    6502
    ; --- Your per-relocation overhead (~11 cycles) ---
    CMP prev_y          ; 3
    BEQ next_page       ; 2/3
    BCS do_patch        ; 2/3
    next_page:
    INC base+1          ; 5  (Only runs once per 256 bytes!)
    do_patch:
    STA prev_y          ; 3
    TAY                 ; 2
    LDA (base),Y        ; 5  (Note: no page-crossing penalty since base is page-aligned!)
    Use code with caution.
    • Speed Gain: On a medium-sized 6502 program with 500 relocations, your method saves roughly 3,500–4,000 cycles (~3.5 to 4 milliseconds on a 1 MHz 6502).
    • Data Size Gain: Whenever your binary has large lookup tables, bitmaps, or string data with no pointers (e.g., a 4 KB tilemap), your $00, $10 escape encodes that gap in 2 bytes, whereas o65 needs 17 bytes of $FF chain codes.

    The Only Trade-Off

    • Decoder Footprint: Because you have to handle both new_Y <= prev_Y (page wrap) and the two-byte $00, $N escape sequence, your loader routine is about 8 bytes larger than a bare-bones o65 loop.
    • Net Break-Even: As soon as your relocation table has just one 2 KB data gap or eight 256-byte strided pointers, the table savings pay back those 8 extra bytes of decoder code—and everything after that is pure memory and speed profit.
    If you want to shave another ~10 cycles off every relocation and ~6 bytes off the decoder, would you like to see how to replace prev_y in Zero Page with self-modifying code (CPY #$00) or using the X register directly?



- ./pc test-peekpoke
    --- parse --- 
    test-peekpoke.c:47: error: cannot parse (function not defined: MEMPTR)
    test-peekpoke.c:47:   puts(MEMPTR(a)); 
    test-peekpoke.c:47:              ^                                            
    --- 1 error ---

--- # - Casting # TODO: - casting is tighter!
    #|(I)$E  - not work (type annotation require a symbol?
    #|(T*)E - not work
    |(char*)$E
    |(int)$E
    |(word)$E
    # raxiss's types.h: MEMPTR(a) is (uint8_t *)(a)
    |MEMPTR($E)
    |(int*) $E
    |(word*)$E
    # (char): binds tighter than the operators: (char)x + y casts x only
    |(char)$R\#ff[&\2]$O
    |(char)$S\#ff[\l1&\2]$O


- zero page out of order, lol
======== MEMMAP test-empty.tap (atmos, from test-empty.ivm) ======== 
file: 65 bytes: a 17 byte header, then $0509-$0538 (48 bytes), starts at $0509     
$0000 --- zero page ---      
  $00-$01  NULL: 00 00 (a NULL pointer reads 0)                     
  $E2-$FB  ivm-lib: cc65's zero page (ptr1 .. tmp4)         
  $DA-$E1  mem[] (-z)                               
  $FC-$FD  JIT: array element pointer       
  $FE-$FF  JIT: scratch (tmp1 tmp2)
$0500 --- not loaded: the lib's startup parts this imag

- What the samples need next:

  ┌──────────────────────────────────┬─────────┐
  │             feature              │ samples │
  ├──────────────────────────────────┼─────────┤
  │ long                             │ 4       │
  ├──────────────────────────────────┼─────────┤
  │ prototypes                       │ 3       │
  ├──────────────────────────────────┼─────────┤
  │ array size expressions ([N + 1]) │ 3       │
  └──────────────────────────────────┴─────────┘

  The rest come up in one or two samples each, and are listed in
  raxis-compiler-test.txt.
  

- Signed compares: 14-insertion-sort now compiles. On the VMs it reads
  array[65535], because while (j >= 0 && …) is always true when compares are
  unsigned. Making int compares signed (unsigned staying unsigned) is a
  design choice for you.
  >> parse.x -v -v crashes (Trap) on that file. That's the VM's bad-access check
  firing at run time, not the parser.

TODO: make compares with <0 <=0 and >0 >=0 be "signed" give a warning
      with our \"Warning: directive, assuming SIGNED compare w 0."


- for (int j = 0; j < 3; j++) a++;              // ERR no declaration in for
  well, why not? we do "recognize it for memset" (not expect i set after)

TODO: just wrap all for  in \{ and \} - no harm?


- I'm assuming that a[i]=a[i]+1; becomes quite efficient JIT code? Adress
  calc only once and reused. Particular for small fixed char arrays one
  can use INC $addr,X
  

- Is our ./parse generating 2--8 for local params load, any speed benefit
  (not need if it's already slot 0-31 so do that first and only change Lii)
  
- realloc; maybe user code Mnnzz Vnnppoo Cppnn - 10 bytes
  curiously if pp always is guaranteed to be 0, then Mppzz could 
  just see pp= 0 => malloc, if !0 ==> realloc!
  TODO: wouldn't work for local var which may be anything? Analyze if
        have value, or force init ptr?
  TODO: how about array element assign, use local var/tmp first?

- if a function is never called remove it and all under not used!

- do we support:  a?:b; - never seen but nice shortcut opt!

- ivm-lib.asm: add memcpy, for <= 256 generate simple inline code
  otherwise call a library routine

- assignement expressions

- #define META (ESC+'A'-1)    // can we evaluate/reduce constant expr?
- #define CTRL -64            // negative unare numbers and vars 
- #define ESC 96 // haha! ESC A or M-A! gives error on:
  if (c == 27) return ESC + toupper(getch_());

- typedef struct imacsbuffer xyz;

- better error message when pririoty goes wrong:
error: cannot parse (function not defined: while)
imacs.c:63:     while (r > 0 && *p && p < end) if (*p++ == '\n') r--;     imacs.c:63:                     ^
imacs.c:64: error: cannot parse (function not defined: while)             imacs.c:64:     while (c > 0 && *p && p < end && *p++ != '\n') c--;
imacs.c:64:                     ^                                         imacs.c:65: error: cannot parse
imacs.c:65:     return p < end ? p                                        imacs.c:65:                    ^                                          imacs.c:71: error: cannot parse (function not defined: while)             imacs.c:71:     while (*p && p < b->end) if (*p++ == '\n') c++;
imacs.c:71:                   ^                                           imacs.c:81: error: cannot parse (function not defined: while)             imacs.c:81:     while (*p != '\n' && *p && p < b->end) p++;

- atoi, strlen, strcpy, strcmp, strcat, is..., toupper, tolower, strncpy


** tests failing **:

- Against the .out files, ptest now reports these differences:
  - incdec: ivm
  - shifts: ivm and hjit; jit fails to build it
  - loops: oscar
  - dispatch-pon: asm prints a stray %
  - ifdef: cc65 and oscar. That's expected, because they don't define
    MeteoriC's predefined macros, so they print 0 0 instead of 1 19712.

- jit fails to build test-loops and test-struct ("JIT: compilation
  failed"), and test-shifts ("alloc fail (map 4426, code 512 bytes)").
- ivm char* *q = v in test-incdec prints 0 where everyone else prints
  67.
- test.shfits.c: x << 16 / x >> 16 asm prints 0, the others print x
  unchanged. hjit is wrong in 240 of 249 lines there.
- Investigate: FIX: string with \0 "embedded will exectute byte code?
  stop copy and skip at first \0 ?

- recursion: report a call of a function to itself (directly or through
  others) as an error. Later: tail calls (a loop), recursion with frames
  copied to a stack
- local arrays with an initializer: require "static" (then it is standard
  C: the initializer runs once)
- ptr + n in an expression is not scaled: report it as an error for int *
  and word struct pointers (until it is scaled; p++ p += n are)
- int compares: ints are unsigned, so < <= > >= on an int give C's answer
  only for values below 32768. Warn on them (the type is in the capture)
  and recommend "word"; recognize v < 0 / v > 0 (the top bit: v & 0x8000)
  with a warning. Needs a grammar directive \"text" that prints text as a
  warning (file:line) when its alt is accepted
- shifts: x = x << n is ACC = x << n ("{ n", then S x); the 6502 shifts in
  memory (ASL / LSR zp, ROL / ROR): ops mem[s] <<= ACC / >>= ACC (commented
  out in ivm.c: op_shlm op_shrm) for x <<= n, x = x << 1, also by 1 alone
- break / continue (done, test-break.c): a switch, once there is one,
  needs its own \( \) for break, but a continue in it must go to the
  loop around it (now: the innermost \( only)

- JIT: '=' (and '&') + G / zjp are not fused into a branch like < >
  (fuse_branch). if (a == b) is the 'v' op now; left: if (a == call()),
  a == b in a bigger E

- G and B, as you asked:
  - <cmp> G rr: that's 2 bytes for G, the same size as <cmp> ? zjp, but one dispatch
    fewer. In the JIT, fuse_branch turns < > ? … G/zjp into a single branch either
    way, so G costs nothing extra there. = is not fused, but 'v' now covers the
    common == case. Without G, every forward jump would have to be zjp, and zjp only
    reaches 63 bytes. G is today's only conditional forward jump that reaches 64–255
    bytes.
  - What I'd change instead: I'd keep G but make it "ACC ≠ 0" rather than "ACC > 0
    signed":
    - Ints are unsigned in MC, so > 0 already means ≠ 0. The i > 0 and i-- > 0 loop
      rules would still work.
    - if (x) break; becomes just x G, for any x, and the ??G workaround and my cmp ?
      ? G peephole go away.
    - The JIT's G, which today needs about 7 instructions for the signed test when X
      is unknown, becomes ORA/BNE.
    - The cost is one semantic change, to G in the VM, the asm VM, the JIT and the
      peephole.
  - B → n: n only works when the loop head is a loop op (t/u/v), because it jumps
    back and re-runs that op's test. while (E) loops end in B back to an arbitrary
    point, and a single forward branch over a B-loop needs it too. Removing B would
    mean either making every while loop take a t/u-style head, or a 1-byte back jump.
    The only room for one is splitting the 0xC0 call range: 32 calls, plus 32 back
    jumps of up to 31 bytes. Many inner loops are under 32 bytes, so that would save
    a byte per loop.

- if (a == b) break;: rule Y's v form wins (v a b 02 0 zjp, 6 bytes) over
  a =b G (5 bytes, but 3 dispatches when not taken)


# gemini

- What's missing (Forth / C / Dis VM): != (Not-Equal) Loop Skip (w or
  a dedicated char): mem[a] != k else skip (i.e., exit when mem[a] ==
  k).

- Even better for your VM (' + !=0): You already have ' (ACC = *vv++)
  and $ (//? $ - string ptr at end? $ == \0). In Inferno's Dis VM and
  Forth, scanning strings/buffers is one of the hottest loops. Fusing
  ACC = *vv++; if (ACC == 0) skip into $ gives you single-instruction
  C-string/buffer iteration with zero extra comparisons.

1. 

	• them turns while ((c = *p++)) (lexers, string lengths,
          parsers, print loops) into a single bytecode per character.

2. Non-Clobbering Bit/Flag Loop Test (& + skip)

	• Semantics: Test (var & mask) == 0 (or != 0) without touching
          ACC, skipping out of the loop on failure.

	• Why: Your t and u only test < and <=. Hardware/emulator
          loops, bitsets, and tag checks constantly loop while a flag
          bit is set or clear, and doing it without clobbering ACC
          avoids save/restore spills.

4. Bonus Ergonomic Observations on Your Table

• Byte-order / range overlap: Notice $80-$bf is listed as 64
  conditional forward jumps, and $bf-$ff starts at $bf (65 values,
  overlapping $bf by 1 byte—you probably meant $c0-$ff for the 64
  vectorized calls).

• Unified V (ACC as source): Your note ? V - TODO: same using ACC as
  value? Vss == +ssSss is basically 2-address accumulator accumulation
  (var += ACC), which is one of the highest-density bytecodes in
  classic 8-bit/16-bit VMs (like Sweet16 and the 6502-era UCSD
  p-System). Worth implementing right alongside U
  
● Agreed, 'v' is of little use as it stands. To find something better, I measured
  instead of guessing. The profiler counts how often each bytecode op executes in 24
  test programs, and a script ranks fusible op sequences both ways: by bytes in the
  code and by executions. Sequences that cross a jump target can't be fused, so
  they're excluded. Output ops are filtered out, since printing swamps the tests.

  Notation: v/k is a variable or constant slot, r a jump offset, ldk a 1-byte const
  load.

  1. The sieve-style stride fill dominates execution: 22% of all executed ops.
  - The pattern: t k lim r; ldk; ] a k; U k p; B r, which is for (k = ...; k <= n; k
    += p) a[k] = 0;. That's 4 dispatches per element.
  - The fused op: a stride-fill ("memset with step") would make the whole loop one
    op. The while-loop ++ version appears too (t; ldk; ]; I; B, 5% of the non-sieve
    programs), which the memset rule misses because it isn't a for.
  - Benefit: large on ivm/asm, near zero on the JIT, which already compiles stride
    loops tightly.

  2. The skip-on-equal op you meant: the data supports it.
  - 'v' sites: 21 of the 'v' uses are directly followed by 0 zjp, i.e. if (a == b)
    break/continue, which is the "jump when equal" case.
  - The opposite op: a jump-on-equal op X a b r makes that 4 bytes instead of 6. It's
    the same op that gives you if (a != b) S, while (a != b) and for (...; i != n;
    ...).
  - Recommendation: replace 'v' with this one, or keep both.

  3. Test a byte array element and branch: 3% of executed ops.
  - The pattern: t i k r; [ a i; zjp, i.e. for (...) if (flags[i]) ....
  - The fused op: fusing [ with zjp ("skip if a[i] == 0") saves one dispatch per
    iteration.

  4. Compare an element with a constant: 6.7% of executed ops in the non-sieve
  programs.
  - The pattern: ( a i; >k; zjp (bubble-sort style). The JIT already fuses this into
    one branch; the VM pays 3 dispatches.

  5. Byte-wise there's not much to win. The largest non-I/O items are
  3-address forms like x = y >> K and x = y + K (ldv }k Sv, ldv +k Sv,
  about 15–18 sites each, 5 bytes) and runs of C v k assignments. Each
  would save about 1 byte per site.

  Your if ((x & 1) || (x >>= 1, 0)): a "shift x right, skip if the bit
  shifted out was 0" op maps perfectly onto 6502 (LSR x then BCC). It
  would turn if (x & 1) ...; x >>= 1;, about 5–6 ops, into one. It
  fits multiply/divide by shift-add, CRC, LFSR, popcount and binary
  exponentiation. But nothing in the current corpus contains that
  loop, so it doesn't show up in the counts. To measure it we'd first
  need a test like a shift-add multiply or CRC-8.

  My suggestion, in order:

  1. Turn 'v' into skip-on-equal: small, and it pays off immediately.
  2. The stride-fill op: the biggest win for the interpreters.
  3. Your bit-test/shift op, together with a CRC or multiply benchmark
     to show what it's worth.

  Which of these should I do?


## Approach 1: Fat Iterator State (Zero Dispatch Bytes, No Object Header Bloat)

Instead of putting a next function pointer inside every data structure
(which bloats cons cells, strings, and arrays), put the next
C-function pointer inside the iterator state struct itself when the
loop initializes.

• How it works:

	• Your loop setup instruction (ITER_INIT) inspects the type
          once before the loop starts and populates a small 2–3 word
          iterator struct (held in a local var vv):c

typedef struct {
    word (*next_fn)(struct Iter *self, word *out_val);
    word cursor;   // pointer, index, or current cons cell
    word limit;    // end pointer, length, or mask
} Iter;

Use code with caution.

	• Inside the loop, your single generic opcode (I vv skip or
          similar) just does:c

if (!iter->next_fn(iter, &ACC)) pc += skip;
Use code with caution.

• Why this fits your VM:
	• 1 opcode handles arrays, strings, cons lists, ranges, and
          hash/assoc tables.
	• Zero extra bytes in the bytecode stream (op vv skip,
          identical in shape to your t and u).
	• Existing data types stay untouched—strings stay raw
          \0-terminated bytes, cons cells stay 2 words, arrays stay
          raw memory.

## Approach 2: Tag-Bit / Pointer-Alignment Polymorphism (Zero Indirect Calls)

If your VM runs on a 16-bit/32-bit/64-bit host where pointers are
aligned (or values are NaN-boxed / low-bit tagged like
Lisp/Smalltalk/BEAM), the value itself tells the NEXT opcode what to
do in 2 CPU instructions without any function pointer:

• Example (2-bit low tag on pointers/ints):
	• ...00 = Byte/String pointer → ACC = *(char*)p++; if (!ACC) skip;
	• ...01 = Cons cell → if (!p) skip; ACC = car(p); p = cdr(p);
	• ...10 = Small integer range → if (p <= 0) skip; ACC = p--;
	• ...11 = Boxed object / vtable → dereference obj->vtable->next(obj)

• Why this wins: 90% of iterations (strings, lists, countdowns)
  execute inline inside the VM switch-case with no indirect call
  overhead, while custom objects still work via the boxed fallback.

## Approach 3: Quickening / Self-Modifying Bytecode (Smalltalk-80 / Python 3.11)

Keep one generic ITER_NEXT opcode in your compiler (ivm-gen.c), and
let the VM overwrite the opcode byte in-place on the first execution
(since your bytecode is in RAM!):

1. Compiler always emits generic I (ITER_NEXT vv skip).

2. On iteration 0, I checks the type of vv and patches the bytecode
   stream in-place to a specialized internal opcode:

	• I_STR (string/byte pointer)
	• I_LIST (cons cell car/cdr)
	• I_ARR (word array)

3. Every subsequent iteration runs the specialized fast path directly
with zero type checks—and if the type ever changes on a later call, a
guard inside I_STR deoptimizes back to I.  (Note: Since your bytecode
uses printable ASCII for the compiler, you have unused control bytes
0x01–0x1F or 0x7F that ivm-gen.c never emits—those make ideal
internal-only "quickened" opcodes!)

My Recommendation

Go with Approach 1 (Fat Iterator Struct) if you want the cleanest,
smallest C implementation with zero pointer-tagging gymnastics. Go
with Approach 3 (Quickening into 0x01–0x1F) if you want maximum speed
on tight loops without changing how variables or iterators are
represented in memory.

To see which fits best, tell me:

• How values/pointers are tagged or sized in your VM (e.g.,
  16-bit/32-bit untagged words, or low-bit tagged Lisp values)

• Whether your bytecode lives in writable RAM (allowing quickening) or
  ROM/read-only memory