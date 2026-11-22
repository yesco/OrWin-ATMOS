# jsk notes:

- 0.1 Mc: `i+i`: `asl; tay; lda; rol; tax; tya`
  (6 cycles of shuffling). A doubling that writes into a slot or leaves Y/A would fix it,
  but it only runs 18,990 times (about 0.1M).

- -0.12 Mc (0.9%): +20 B Bmemset "word" 
  duff style loop: odd count +8 B +13c
  cost second self-modify `sta abs,y`:
  - jit write o r (+6 B per call),
    or lib copy(about +12 B and +20c per call)
  - It also needs a second `dec` per page.
  - Lib cost: about +20 B in `_opmemset`.
  - Gain: 10c => 8.5c / byte, about 0.12M on the sieve (0.9%).
  - skip. or last percent;  the sieve's memset is 6% of the run.

Outside the inner loop we spend 5.73M cycles. That's 43% of the 13.24M total.

| part | runs | cycles/run | total |
|---|---|---|---|
| inner loop (for comparison) | 149,990 | 50 | 7.51M |
| `if (flags[i])`: pointer calc, load, `beq` | 81,910 | 24 | **1.95M** |
| `i <= SIZE` test | 81,920 | 11 | 0.92M |
| `i++` | 81,910 | 8 | 0.66M |
| `prime = i+i+3; k = i+prime` + loop entry | 18,990 | **65** | **1.23M** |
| memset body | 10 × 8,191 B | 10 per byte | 0.82M |
| `count++` | 18,990 | 8 | 0.15M |

3. **The `flags[i]` test is already tight**
   (`sta fe; txa; clc; adc df; sta ff; ldy de; lda (fe),y; beq`)
   The 17 cycles of address setup is the same "flags + index
   recomputed every pass" problem as the inner loop. It needs
   either pointer caching or strength reduction.

4. **The `i++` plus test costs 19 cycles per pass.**
   Making it cheaper needs loop versioning: a fast path while
   `hi < $1F` with only `inc lo; bne body`.
   That's the same "test only when the high byte changes"
   trick Oscar uses. It would save about 5 cycles per pass, about 0.4M,
   but it's a bigger change in `n`.

- 16 B: free(); include and doing F in code gen???
- 
-  ------- 6d 'm' 00 0d 5a 05 ------
   ;; 21 B => store ptr in acc? Let the routine do the add?
   ;; size in XY (hi, lo)
   B0AE ldx #$1f
   B0BE ldy #$ff

   ;; ADDR.lo = ptr.lo
   B0B0 lda $de
   B0B2 sta $0458

   ;; ADDR.hi = ptr.hi + x
   B0B5 txa 
   B0B6 clc 
   B0B7 adc $df
   B0B9 sta $0459

   ;; fill
   B0BC lda #$01

   B0C0  jsr $0452


TODO: 'B' opcode redundant, if loop back to 'i' j k then use n m!
      the idea is that 'n' behaves different depending on destination op!

BUG: 'I' and 'J' doesn't wrap around will give 16 bit number
    and when used as direct arg will read hibyte... 

--- 

DONE:
- saved  7 B? combine
    ------- 6e 'n' 25 6e 33 22 ------
       B155 inc $e8
       B157  bne +2	=> $B15B
       B159 inc $e9
       B15B lda $e8
       B15D ldx $e9
       B15F cpx #$1f
       B161  bne +2	=> $B165
       B163 cmp #$ff  '�'
       B165  bcc -113	=> $B0F6
    ------- 6e 'n' 33 22 70 72 ------
       B167 inc $f0
       B169  bne +2	=> $B16D
       B16B inc $f1
       B16D lda #$0a
       B16F cmp $f0
       B171 lda #$00
       B173 sbc $f1
       B175  bcc +3	=> $B17A
       B177  jmp $b0d2

- saved 17c: +byteconst +vvSdd
- saved  5 B: m - malloc sizes constant, don't need to LDA xx TAY ...  CMP BEQ,
           just delay and do ... LDY xx BEQ (- 3 bytes)
- saved  5 B: a 27 B: memset (+ 4 B load size deferred)
  - lots of setup (clc+adc)
  - possibly use shared library?
- saved  4 B: reorder: "prime = i + i + 3;  k = i + prime;" => "...; k = prime + i;"
- saved  2 B: 12 B (16) -> 10 B (12) i+i == i<<1
- saved  8 B: [ zjmp - it's a byte simplify zjmp
- saved  5 B: forward jumps: bxx + jmp = if can shorten save 2 byte per forward
- saved 12 B! 364-356: 8 B: for (i = 0; i <= SIZE; i++) { 
- saved 21 B: i n duplicates the test - fixed
- saved  3 B: I J lazy load - 2.8% in speed!
- saved  2 B: U - less juggling just save A directly, reload cheaper than TAY TYA
- saved  4 B: for should leave index in ACC


=============================================================
# bubble.c and qsort.c

Oscar's 130 cycles per inner pass is impressive. Your bubble has n =
1000, so the inner loop runs 499,500 times:

| | cycles per inner pass |
|---|---|
| Oscar | 130 |
| ivm-jit | 384 |
| cc65 | 695 |

I profiled our inner loop (`while (j < (n - 1 - i)) { if (a[j] > a[j +
1]) swap; j++; }`):

| part | cycles per pass | what it does now |
|---|---|---|
| `a[j+1]` load | 75 | `j+1` into a temp, then ×2, + base, 2 × `lda ($fe),y` |
| loop test | 69 | copies `j` to a temp, recomputes `n-1-i`, builds a 0/1, then branches |
| `a[j]` load | 56 | ×2, + base, load |
| `>` and branch | 32 | builds a 0/1 (`lda #0; tax; rol; eor #1`), then `bne` |
| swap (about half the passes) | 146 averaged | 4 more full address calculations |
| `j++`, `jmp` | 11 | |

Oscar most likely computes `&a[j]` once and reads `a[j]` and `a[j+1]`
as `(ptr),y` with Y = 0..3, and branches straight on the compare.

**What I'd do, in order:**

1. **Element pointer cache in the JIT**
   (about −150 cycles per pass).
   
   Remember that `$fe/$ff` = `&a[j]`. While neither `a` nor `j` is
   written and nothing else uses `$fe`, the next `a[j]`, `a[j]=` and
   `a[j+1]` reuse it with `ldy #0..3`. That covers the swap's reads
   and writes and both loads. It's general, since array code keeps
   touching the same element and its neighbour. It also needs `a[j+1]`
   to stay recognisable as "j plus a constant": a grammar rule
   `\i[\i+\d]` instead of going through a temp.

2. **Fused compare and branch**

   (about −25 cycles per pass, and in every `if` and `while` of every
   program). For `<`/`>`/`=` followed by a zjp: `cmp; sbc; bcs exit`
   with no 0/1. This is the one I've been talking about.

3. **`\i < (E)` in the grammar** 

   about −15 cycles per pass). Emit it as `E; > j` so `j` isn't copied
   to a temp first.

Together that's about 384 → 190 cycles per pass, roughly 95M. That's
still 1.5× Oscar, but 3.6× cc65. Hoisting the loop-invariant `n-1-i`
out of the loop would need real analysis; writing `m = n - 1 - i;` in
the source does the same for free.

qsort at 1.7× Oscar has the same causes: `a[i]`/`a[j]` swaps and
comparisons that build 0/1.

Shall I start with 1, the pointer cache? It's the biggest of the three
and helps every program that uses arrays.

=============================================================


## 6502 C-compiler bencharks out there

In benchmarks targeting the MOS 6502 (specifically variants of the
classic BYTE Sieve prime-number test), Oscar64 and LLVM-MOS
consistently lead in execution speed, while vbcc produces the smallest
compiled binaries. Traditional compilers like cc65 lag significantly
behind in cycle counts due to heavy reliance on a software stack.

[1](https://www.c64-wiki.com/wiki/oscar64),
[2](https://raxiss.com/article/id/30-MOS6502-compiler-benchmark),
[3](https://github.com/kassane/mos-toolchains-research/blob/main/Research.md),
[4](https://www.youtube.com/watch?v=ejbTKtgSZI0&vl=en),
[5](https://www.reddit.com/r/retrogamedev/comments/1xhp8dk/i_wrote_a_compiler_that_turns_typescript_into/)

Because different benchmark suites use varying iteration counts, array
sizes, and runtime libraries, results are best evaluated through the
two most comprehensive modern 6502 C compiler test suites: C-Bench-64
and the Raxiss MOS6502 Benchmark.

[1](https://thred.github.io/c-bench-64/),
[2](https://raxiss.com/article/id/30-MOS6502-compiler-benchmark)

### 1. Execution Speed & Cycles (C-Bench-64 / GGLabs Suite)

The C-Bench-64 suite (adapted from the GGLabs benchmarks) measures
real-world execution time on a ~1 MHz Commodore 64 (where 1 second ≈
1,000,000 clock cycles). It tests both the standard array-based Byte
Sieve (sieve) and a bitwise-packed variant (sieve-bit). [1]
(https://www.c64-wiki.com/wiki/oscar64), [2]
(https://thred.github.io/c-bench-64/), [3]
(https://6502.org/forum/viewtopic.php?t=8140)

Compiler	Standard Sieve Time (sec)
                        Approx. Cycles (~1 MHz)
			        Bitwise Sieve Cycles (sec)
Oscar64	9.5 s	~9.5M	16.3 s	~16.3M
LLVM-MOS	16.7 s ~16.7M   ~19.6M
cc65	23.1 s	~23.1M	70.7 s	~70.7M

• Oscar64 Dominates Loop Logic. Oscar64 performs whole-program
  call-chain analysis to statically allocate local variables and
  aggressively narrows 16-bit integer math down to native 8-bit
  operations. [1] (https://news.ycombinator.com/item?id=38901089)

• LLVM-MOS Excels at Complex Control Flow. LLVM-MOS uses static
  zero-page allocation instead of a software stack. While it trails
  Oscar64 slightly on tight 8-bit array indexing like the Sieve, it
  often matches or beats Oscar64 in heavier math and cryptography
  benchmarks like AES-256. [1]
  (https://news.ycombinator.com/item?id=46098359), [2]
  (https://llvm-mos.org/wiki/Findings), [3]
  (https://www.c64-wiki.com/wiki/oscar64)

• cc65 Suffers from Stack Overhead. cc65 maintains a 16-bit software
  stack pointer in memory, requiring roughly 20 cycles of overhead per
  function call and 8 cycles just to read a local variable off the
  stack. [1] (https://www.youtube.com/watch?v=ejbTKtgSZI0&vl=en)

2. Binary Size Comparison (Raxiss MOS6502 Suite)

The Raxiss MOS6502 Compiler Benchmark measures total compiled executable size (including minimal runtime startup overhead) when compilers are tuned specifically for code size. [1] (https://raxiss.com/article/id/30-MOS6502-compiler-benchmark)

Comp  OptFlag	Sieve Size	Notes
====  =======   ==========	=================
vbcc  -O=991	  914 bytes	Smallest complete binary; strong size optimizer.
cc65  -O	1,108 bytes	Compact due to calling reusable runtime subroutines.
osdk-lcc65 -O3	1,160 bytes	Oric-focused LCC port; competitive binary footprint.
LLVM-MOS   -Oz	1,162 bytes	Slightly larger due to aggressive loop/inlining transforms.
SDCC --opt-code-size
               10,012 bytes	Includes heavy standard library/startup overhead.
KickC	--Failed to compile--	Lacks full standard C compliance for unmodified benchmarks.

• Size vs. Speed Trade-off. Compilers like vbcc and cc65 produce
  smaller binaries because they emit calls to shared runtime helper
  routines.
  
  LLVM-MOS and Oscar64 trade a small amount of ROM/RAM footprint for
  unrolled loops and inlined zero-page operations that cut cycle
  counts in half.
  
  [1] (https://www.reddit.com/r/Compilers/comments/1xukomg/introduce_llvm_z80_backend/),
  [2] (https://raxiss.com/article/id/30-MOS6502-compiler-benchmark),
  [3] (https://news.ycombinator.com/item?id=46098359),
  [4] (https://www.c64-wiki.com/wiki/oscar64)

Key Takeaways for Choosing a 6502 C Compiler

• Best Overall Performance. Choose Oscar64 (especially for Commodore
  targets) or LLVM-MOS (for multi-platform 6502 targets like NES,
  Atari, and C64) if cycle count is your primary bottleneck. [1]
  (https://www.c64-wiki.com/wiki/oscar64), [2]
  (https://www.youtube.com/watch?v=ejbTKtgSZI0&vl=en&t=25)

• Smallest Footprint. Choose vbcc if you are targeting strict size
  limits (such as 4K/8K cartridge builds), though note that its
  license restricts commercial redistribution. [1]
  (https://sgadrat.itch.io/super-tilt-bro/devlog/219534/benchmark-c-compilers-for-the-6502-cpu),
  [2] (https://raxiss.com/article/id/30-MOS6502-compiler-benchmark)

• Legacy Compatibility. Choose cc65 only if you rely on its mature,
  built-in target libraries (conio, platform drivers) and do not need
  maximum execution speed. [1]
  (https://www.youtube.com/watch?v=xkolkC1RgOY), [2]
  (https://www.cc65.org/oldindex.php)

If you are targeting a specific 6502 system (such as C64, NES, Atari,
or Apple II) or have strict ROM/RAM constraints, let me know and I can
help you compare compiler flags and C coding patterns (like zero-page
usage and Structure-of-Arrays) to minimize cycle counts.

=============================================================


## - 'n' INC optimiation -1c +2 B - LOL

Let me count it exactly rather than guess:

**The inner `while (k <= SIZE)` loop (the one with `U`) runs 149,990
  times** over the 10 iterations. So 1 cycle saved there is 150k
  cycles, about **0.7%** of the 22.2 M total.

**Your `n` trick would go on the outer `i` loop, which runs 81,910
  times.** The inner loop has no `n`; it's `U` + `B`.

- **Now:** `INC lo; BNE +2` (taken, 3c); `INC hi`.

- **Your version:** `INC lo; BEQ stub` (not taken, 2c), so 1c saved
    255 times out of 256. At each wrap you pay about 4c extra for the
    taken BEQ, `INC hi` and `JMP` back, and there are 32 wraps per
    pass.

- **Net:** about 8,000 cycles per pass, so roughly **80k cycles
    (0.36%) for +3 bytes**, plus the work of emitting stubs after the
    code. It isn't worth it yet.

**The real cost is the inner loop itself.** These are my estimates
  from the trace, per run of the loop:

| op | cycles |
|---|---|
| `07` reload k | 6 |
| `>` build a 0/1 result | 20 |
| `?` invert it | 13 |
| zjp test it | 5 |
| `]` flags[k]=0 | 28 |
| `U` k += prime | 20 |
| `B` | 3 |
| **total** | **~95 × 150k = 14.3 M** |

So the inner loop is about **64% of the whole run**. The test `k <=
SIZE` alone takes `07 > ? zjp`, about 44 cycles, to build a 0/1
result, invert it, and test it. A fused compare-and-branch would be
about 12 cycles, still with k in zp:

```
lda k ; cmp #<8191 ; lda k+1 ; sbc #>8191 ; bcs exit
```

That saves about 32 cycles per run, roughly **4.8 M cycles, about
20%**. By the same scaling as before, that puts it around **1.25×
Oscar**, and it also saves bytes, since `?` and the zjp sequence go
away.

It could be done in either of two places. Either the JIT spots `>
?`/`< ?`/`= ?` followed by a zjp and emits one branch, or
ivm-gen/c.bnf emits a single compare-and-jump op. If you want, I'll do
the JIT peephole next, since it fits the ACC API: the fused op is "in,
KEEP" and needs no bool in AX.






# Claude analysis compare with oscar64 asm:

## Matching the parts

- 1..10: oscar uses an 8-bit counter, because it knows iter ≤ 10
- oscar keeps count in a register
- `if (flags[i])` | **08d5–08d9: 3 instructions** | **7079–70B1: about 30** | Biggest difference outside the hot loop |
-`k += prime` | 0909–0914 | 711C–712D | |
- count++ - 0922–0925 (`INX`) - `I` reloads ACC for nothing |

## What oscar does that the JIT doesn't

1. **Strength reduction on `flags[i]`.** oscar keeps a pointer T1 and
does `INC T1` once per `i`. The JIT's `(` recomputes `flags + i`
every time, about 30 cycles.

2. **Compare and branch fused.** oscar does `LDA (T1),y / BEQ`. The
JIT builds a boolean with `?` and then tests it again in `G`, which is
the `??` cost you measured. The same happens with `>` then `G`.

3. **A cheaper store in the hot loop.** For `flags[k] = 0`, oscar uses
`LDY flags_lo` and adds only the high bytes: one `STA (T5),y`. The JIT
does a 16-bit add plus two stores.

## The hot loop (`while`, roughly 17k passes per iteration)

These cycle counts are hand estimates:

- **JIT:** about 132 cycles per pass. 

  -- 6-cycle load of k
  -- `>` 22
  -- `G` 15

  -- `)` 58
  -- `U` 28

  -- `B`  3

- **oscar:** about 58 cycles per pass.

So the hot loop is about 2.3× slower, which matches 31s vs 16s
overall. These changes would close most of the gap:

- "d >s G"  ==> CMP #lo / TXA / SBC #hi / BCS exit 10c instead of 37c!
- k reload - both paths into it "S 0b' and "U 0b 0a" give AX = k
  Extending the `kc` tracking to "AX == slot s" would remove it.

- ") CONST" => LDY lo / STA (ptr),y   +hi-byte add, 20c instead of 58c


- "I0c" count++ reloads ACC: LDA $18/LDX $19 but `n` comes
  next and overwrites AX Your existing "TODO: lazy load!" covers
  this.

# ----------------------------------------------------------

# Gemini Analysis of Assembly generated

Looking directly at the 6502 disassembly of your ivm-jit inner loop
($6FFE to $7070), your JIT is already remarkably clean:

  1. espeically deferred constant load "Lf8 folded straight into ")",
     and the inlined 16-bit Zero Page additions

65 % !
     However, because each VM instruction is currently compiled
     independently without tracking CPU flags or register state
     across boundaries, there are 4 specific hotspots inside
     that inner loop ($6FFE–$7070) that execute 149,870 times
     and account for roughly 65% of your total runtime
     (~26 seconds out of 40.39 s).

1. Fuse "> + ? + ? + G"  ($7002–$703D) — Saves ~13.5 Seconds

> fa ? ? G 0a materializes a boolean 0/1 into A,X, negates
it twice with ? stores it in $FE and checks sign/zero just to
perform a single conditional branch
38 bytes / ~46 cycles every iteration):


6502

; CURRENT (38 bytes, ~46 cycles per hit)
7002: sec / eor #$ff / adc #$fe / txa / eor #$ff / adc #$1f
700C: lda #$00 / tax / rol / eor #$01
7012: stx $fe / ora $fe / beq ...   ; first '?'
7022: stx $fe / ora $fe / beq ...   ; second '?'
7032: sta $fe / txa / bmi ... / ora $fe / beq +3 / jmp $0000

Since > is immediately followed by ? (and an optional second ? + G),
your JIT can emit a direct 16-bit comparison + branch, eliminating the
entire 0/1 boolean materialization:

6502

; FUSED '> fa ? ? G' (13 bytes, ~13 cycles per hit — saves ~33 cycles × 149,870 = ~4.95M cycles!)

lda $1a
cmp #$fe
lda $1b
sbc #$1f
bcc +3          ; if k < 8190, continue loop
jmp $7071       ; else exit inner loop

2. Keep k ($1A/$1B) in AX Across B + <0d> — Saves ~1.5 Seconds

k is already sitting in AX when you store it to $1A/$1B
hen B11 jumps back to (<0d>), which immediately executes
LDA $1A / LDX $1B to load the exact same values back!

• Fix: If U leaves the updated value in AX and your JIT tracks that
  ACC == reg[0d], <0d> becomes a 0-byte, 0-cycle no-op (saving 4 bytes
  and 6 cycles per hit = ~0.9M cycles).

3. Eliminate TYA / TAX Shuffling in 16-Bit Add ($7061–$7069) — Saves
~1.5 Seconds

In U0d0c ($7061), because A holds the low byte of k and X holds the
high byte, you have to stash the low sum in Y (TAY), move X to A
(TXA), add the high byte, move back to X (TAX), and restore A
(TYA)—burning 4 register transfers (8 cycles) on every 16-bit add:

6502

; CURRENT (16 bytes, ~24 cycles)
clc
lda $1a
adc $18
tay
txa
adc $19
tax
tya
sta $1a
stx $1b

; DIRECT MEMORY ADD (12 bytes, ~18 cycles — saves 6 cycles × 149,870 = ~0.9M cycles!)

clc
lda $1a
adc $18
sta $1a
lda $1b
adc $19
sta $1b

(Bonus: Notice that after ADC $19 / STA $1B, A already holds k_hi and
Carry is valid—which feeds straight into the fused > check!)


;; 8-BIT ELEMENT STORE ')' (15 bytes, ~19 cycles — saves 23 cycles × 149,870 = ~3.45M cycles!)
;; 18 B  29c
clc
lda $1a
adc $0c
sta $fe
lda $1b
adc $0d
sta $ff
ldy #$00
tya             ; A = 0 (from deferred Lf8)
sta ($fe),y


;; TODO: keep an $fc == 0, $fd - to be used and then
;; 16 B  26c
clc
lda $1a
adc $0c
tay
lda $1b
adc $0d
sta $fd
lda #$00        ; value
sta ($fc),y     ; ($fb)==0

Projected Impact on Your Benchmark Table

Optimization Cycles Saved (10 Passes) New Code Size New Runtime (1
MHz) Speed vs. Hand ASM (7.0 s) Current ivm-jit — 450 B 40.39 s 5.77x

+ Fuse > ? ? G + Direct U + ACC tracking ~6.8M cycles ~412 B ~33.6 s
4.80x

+ 8-bit ) / ( Byte-Array Ops ~4.2M cycles ~382 B ~29.4 s 4.20x

just fusing > ? ? G and simplifying U0d0c will drop your JIT below 34
seconds (~4.8x)

and shrink the emitted 6502 binary from 450 bytes down to ~412 bytes!

Also spotted two tiny JIT parser bugs in your log

WOA --- now it's down to 95 BYTES!
- recognize constants
- inline ( )
- byte [ ] - see constant write
- memset
- peephole (mostly for less BC)
- Copy byte code

--- BYTECODE (256 bytes) ---
<02> *fc M07

-- TODO: recognize memset, lol                  -- MEMSETppnn
Lfb S08                                         -- COPYfb08 (4->3)
<02> S0d                                        -- OK, is 3 but overhead
J0d S0e
i080d07 
  Lfa )0708 
n0b 

Lfb )07fb                                       -- why loading 0 use 0 
Lfb )07fa                                       --       ...

Z0b Lfc S08 <02> S0f J0f S10                    -- lots of copy in loop init
i080f3f 
  (0708 ? G37                                   -- LONGEST JUMP dec: 53

    -- then
    I0b <08> <f9 ? G19                          -- because of < in loop use generic while
      <08> *08 
      S09 <09>                                  -- S09 <09>    redundant
      <02 ? G0a
        Lfb )0709 U0908
      B10 0  

    -- else
    ? G16
      L08 +08
      S09 <09>                                  -- S09 <09>    again...
      <02 ? G0a                                 -- because of < in loop use generic while
        Lfb )0709 U0908
      B10 
n43 

"LIMIT \0
<02> . N 

"primes \0
<0b> . N 

Z0a 

"last: \0

<02> -fa S08 <08> >fa                           -- S08 <08>   
? G1f (0708
  ? G15
    <08> . O20 L0a +fa S0a <0a> <f8 
      ? ? G04 Lfa S08 J08
B25 N 

-- return
F07 <0b> X 
X 


--------- OTHER CODES:

-- sum += complicated value:

i15180a 
  <16> S19 (00) +19 S16        -- sum = sum + Fun(): DO a Vss (add acc to ss)
n0e 
