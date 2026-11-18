# jsk notes:

- 21 B: 7x: forward jumps: bxx + jmp = if can shorten save 21 B!
- 27 B: memset (+ 4 B load size deferred)
  - lots of setup (clc+adc)
  - possibly use shared library?
- 4 B: for should leave index in ACC
-     'B' opcode redundant, if loop back to 'i' j k then use n m!
      the idea is that 'n' behaves different depending on destination op!
- 2 B: U - less juggling just save A directly, reload cheaper than TAY TYA

BUG: 'I' and 'J' doesn't wrap around will give 16 bit number
    and when used as direct arg will read hibyte... 

--- 

DONE:
- saved 12 B! 364-356: 8 B: for (i = 0; i <= SIZE; i++) { 
- saved 21 B: i n duplicates the test - fixed
- saved  3 B: I J lazy load - 2.8% in speed!


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
