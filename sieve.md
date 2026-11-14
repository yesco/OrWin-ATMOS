# Claude analysis compare with oscar64 asm:

Before the comparison, there's a bug in the JIT trace: the first two
lines go wrong.

```
L fa  [ fb  M 04      ; size = 8191 << 1 ; flags = malloc(size)
%ERROR: unimpl op 0x5B '['
%JIT: not-a-call 0xFB
```

The JIT has no `[` (shift left), so `malloc` gets $1FFF = 8191 bytes,
not $3FFE. That's what oscar asks for, and what the word array
needs. The memset then writes 16380 bytes into an 8191-byte block. It
only gives the right answer because nothing else lives after it on the
heap. This lines up with your "`*2 → <<1`" codegen change in the
table. Add `[` and `]` to the JIT (`ASL`/`ROL` × const), or fold a
const-by-const shift at compile time.

## Matching the parts

| C | oscar64 | JIT | Difference |
|---|---|---|---|
| `flags = malloc(16382)` | 088a–089b | 7008–7011 | Size bug (above) |
| `puts("10 iterations")` | 089d–08a7 | 7013 | |
| `for (iter = 1; iter <= 10;)` | 08aa, test only at the bottom (093b) | 7025–7037, 714D–715D | oscar uses an 8-bit counter, because it knows iter ≤ 10 |
| `count = 0` | 08b6 (count lives in **X** + T7+1) | 703A | oscar keeps count in a register |
| `memset(flags, 1, …)` | 08ae–08cd | 7045–7066 | oscar fills 8191 bytes, JIT 16380 |
| `for (i = 0; i <= 8190;)` | 08cf, 0927–0939 | 7065–7078, 713A–714A | |
| `if (flags[i])` | **08d5–08d9: 3 instructions** | **7079–70B1: about 30** | Biggest difference outside the hot loop |
| `prime = i + i + 3` | 08db–08ea | 70B2–70CA | |
| `k = i + prime` | 08ef–08f7 | 70CC–70DB | |
| `while (k <= 8190)` | 0914–0920 | 70DD–70FC | |
| `flags[k] = 0` | 08fc–0907 | 70FD–711A | |
| `k += prime` | 0909–0914 | 711C–712D | |
| `count++` | 0922–0925 (`INX`) | 7130–7138 | `I` reloads ACC for nothing |
| `printf(count)` | 0946–0969 | 7160– | |

## What oscar does that the JIT doesn't

1. **Strength reduction on `flags[i]`.** oscar keeps a pointer T1 and
does `INC T1` once per `i`. The JIT's `(` recomputes `flags + 2*i`
every time, about 30 cycles.

2. **Compare and branch fused.** oscar does `LDA (T1),y / BEQ`. The
JIT builds a boolean with `?` and then tests it again in `G`, which is
the `??` cost you measured. The same happens with `>` then `G`.

3. **A cheaper store in the hot loop.** For `flags[k] = 0`, oscar uses
`LDY flags_lo` and adds only the high bytes: one `STA (T5),y`. The JIT
does ×2 plus a 16-bit add plus two stores.

4. **Bytes instead of words.** That halves the memset and the
stores. This one is just waiting on your `char[]` support.

## The hot loop (`while`, roughly 17k passes per iteration)

These cycle counts are hand estimates:

- **JIT:** about 132 cycles per pass. That's a 6-cycle load of k, `>`
    22, `G` 15, `)` 58, `U` 28 and `B` 3.

- **oscar:** about 58 cycles per pass.

So the hot loop is about 2.3× slower, which matches 31s vs 16s
overall. These changes would close most of the gap:

- **Fuse `> s G`** into `CMP #lo / TXA / SBC #hi / BCS exit`. That's
    about 10 cycles instead of 37.

- **Drop the k reload at the loop top.** Both paths into it (`S 0b`
    and `U 0b 0a`) already leave AX = k. Extending the `kc` tracking
    to "AX == slot s" would remove it.

- **Byte `)` with a constant value**: with `char[]` it becomes `LDY lo
    / STA (ptr),y` plus a high-byte add, about 20 cycles instead of
    58.

Two smaller items:

- The memset reads its fill byte with `lda $01f6` even though `fb` is
  a constant slot. Use `is_const` and emit `LDA #1`.


- `I 0c` (`count++`) reloads ACC with `lda $18/ldx $19`, but `n` comes
  next and overwrites AX. Your existing "TODO: lazy load!" covers
  this.


# ----------------------------------------------------------


# Gemini Analysis of Assembly generated

Looking directly at the 6502 disassembly of your ivm-jit inner loop
($6FFE to $7070), your JIT is already remarkably clean—especially the
deferred constant load (Lf8 folded straight into )!) and the inlined
16-bit Zero Page additions.  However, because each VM instruction is
currently compiled independently without tracking CPU flags or
register state across boundaries, there are 4 specific hotspots inside
that inner loop ($6FFE–$7070) that execute 149,870 times and account
for roughly 65% of your total runtime (~26 seconds out of 40.39 s).

1. Fuse > + ? + ? + G ($7002–$703D) — Saves ~13.5 Seconds

Right now, > fa ? ? G 0a materializes a boolean 0/1 into A,X, negates
it twice with ? (0x3F), stores it in $FE, and checks sign/zero just to
perform a single conditional branch ($7002–$703D = 38 bytes / ~46
cycles every iteration):



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

2. Keep k ($1A/$1B) in A,X Across B + <0d> ($706A–$7001) — Saves ~1.5
Seconds

At the end of U0d0c ($706A), k is already sitting in A,X when you
store it to $1A/$1B. Then B11 jumps back to $6FFE (<0d>), which
immediately executes LDA $1A / LDX $1B to load the exact same values
back!

• Fix: If U leaves the updated value in A,X and your JIT tracks that
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


4. Byte-Sized ) Store When Element Size = 1 Byte ($703E–$705C) — Saves ~6.5 Seconds

Your ) array store ($703E–$705C) currently treats the sieve array as
16-bit elements (ASL / ROL to double k, storing $00 to ($FE),Y at Y=1
and Y=0 = 31 bytes / ~42 cycles per hit).

• In the BYTE Sieve, flags[] is a 1-byte char array (8,192 bytes, not
  16,384 bytes).

• If you add a byte-store opcode (or know ) targets a byte array), you
  skip ASL / ROL, LDY #$01, TAX, and DEY completely:

6502
; 8-BIT ELEMENT STORE ')' (15 bytes, ~19 cycles — saves 23 cycles × 149,870 = ~3.45M cycles!)
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

(And in the initialization loop at $6F55, STA ($FE),Y runs half as
many stores too, saving another ~0.35 seconds!)

Projected Impact on Your Benchmark Table


Optimization Cycles Saved (10 Passes) New Code Size New Runtime (1
MHz) Speed vs. Hand ASM (7.0 s) Current ivm-jit — 450 B 40.39 s 5.77x

+ Fuse > ? ? G + Direct U + ACC tracking ~6.8M cycles ~412 B ~33.6 s
4.80x

+ 8-bit ) / ( Byte-Array Ops ~4.2M cycles ~382 B ~29.4 s 4.20x

Even keeping 16-bit elements so the bytecode stays 100% identical,
just fusing > ? ? G and simplifying U0d0c will drop your JIT below 34
seconds (~4.8x) and shrink the emitted 6502 binary from 450 bytes down
to ~412 bytes!  (Also spotted two tiny JIT parser bugs in your log
just in case you hadn't noticed them yet: 0x5B '[' at bc 2 logged
unimpl op—likely why the sieve bound defaulted to 8190—and 0x22 '"'
inline string literals at bc 6 and bc 103 were decoded as instructions
instead of skipping past the closing "!)  Want me to write out the
exact peephole matcher logic (in C or 6502) for fusing > reg ? [?] G
offset inside your JIT compiler?


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

Lfd S02   Z16   Lf7 S17   Lfa S15   <17> S18    -- COPYfftt save 3b, -4 ops

i15180a 
  <16> S19 (00) +19 S16        -- sum = sum + Fun(): DO a Vss (add acc to ss)
n0e 

"SUM \0
<17> . O20 
"times = \0
<16> . N 

-- return (exit)
<16> X 
X 
q 
