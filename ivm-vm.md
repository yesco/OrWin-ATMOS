# JSK thinking

Actually the ALTCODE=1 and INLINE of FIXIPC: I think the dispatch is
very expensive,a ND ALTCODE was one attempt to improve on that, I
still think copying and writing the jump is somewhat too expensive,
dispatch should be kept at around 21 cycles I think. Now it's
40+. Selfmodifying the JMP isn't really saving any if need modify both
bytes, it still is about 11c (write+JMP), PHAPHARTS is 11c not much
better. My worry is that keeping AX in all the time is that any mem op
is using it (and maybe X). Even writing the dispatch without using AX
is going to be difficult. The indirectg memory is a must for several
processes... We could make a single variant, running in a fixed
memory, or even zeropage, but because its indexed zero page doesn't
win any, only for pointers. They are 4c if page aligned for fixed
addresses. LDA (zp),y is surprisingly cheap 5c. So only 1c more, still
need to get the varindexinto Y (or X) so same cost. Only real
improvement would be to JIT and generate (simple) templetaized
variants of all the OPS, relatively trivial... But then, cannot
multitask and reloate... For (1) to be effective maybe the dispatch
needs to be in page zero. (2) Is half-done, only need to adopt the
loop. (4) is a lot of work and maybe not guaranteed to yield much. (1)
Maybe negatees the need for (2), which is why I haven't committed to
ALTCODE=1 yet. (reflection upon the following text)

# Grok suggestions

JSK: You have the .asm do you see anything other that could be improved?
It's currently about 5x slower than a naively generated asm code.

Yes. Looking at the current `ivm-vm.asm` (ALTCODE=0 path), the ~5× gap
vs naive native asm is almost entirely in the **per-op tax**, not in
the arithmetic itself.

### Where the cycles go (rough, per op)

| Step                            | Cycles (approx) | Notes |
|------                           |------------------|-------|
| `ldy #0` + `lda (ipc),y`        |   10 | fetch opcode |
| `inc ipc` / possible page cross | 8–12 | |
| `tax` + `bmi`                   |    6 | |
| load disphi/lofx,`sta jmpvec`   |   18 | |
| `jmp (jmpvec)`                  |    5 | |
| **Handler entry**               |      | |
| `PARAX` / `LOAD_SLOT_Y` (mostly)|21–30 | another indirect mem access |
| real work (add/sub/…)           |10–20 | |
| `jmp pnext` / `xnext`           |    3 + loop | |

Total: (+ 10 8 6 18 5 3) = 50-54 c


A dead-simple op (`L`, `+`, `I`, …) is often **60–90 cycles** before
any useful work. Native code for the same is typically 10–20
cycles. That alone explains most of the 5×.

### Highest-leverage improvements (in order)

1. **Self-modifying dispatch (biggest single win)**  

   The comment already notes it (`selfmodify can do -5c`). Instead of:
   ```asm
   lda disphi,x / sta jmpvec+1
   lda displo,x / sta jmpvec
   jmp (jmpvec)
   ```

   write the low byte of the handler address into a `jmp $xxxx` in ZP
   or page 0 and just `jmp that`. Saves the table loads and the
   indirect jump setup every op. Classic 6502 interpreter trick; can
   reclaim ~10–15 cycles per op.

2. **Keep the low byte of `ipc` in Y (or X)**  

   The ALTCODE=1 experiment tried this and only gained 1.5 % because
   the rest of the handlers were not fully converted. Done properly
   (every handler that touches ipc uses Y as the offset, FIXIPC only
   on control-flow ops) you eliminate the `ldy #0` + most of the `inc
   ipc` cost on the straight-line path. This is the second biggest
   win.

3. **Faster path for the 0–31 loads**  

   They already special-case in the table, but still go through the
   full dispatch. A quick `cmp #32 / bcc op_31_fast` *before* the
   table lookup (or a second, denser table) removes one indirection
   for the most common ops.

4. **ACC in A/X instead of ZP**  

   Many handlers do `sta _ivm_acc / stx _ivm_acc+1` only to reload
   them on the next op. Keeping the live ACC in A+X (or A+Y) across
   ops that don’t need the ZP copy saves 8–12 cycles on long
   arithmetic chains. The cost is that every handler that currently
   assumes ZP ACC must be audited.

5. **Specialised / fused ops you already started**  

   `i`/`n` (and `j`/`m`) already help. More of the same for the
   
   hottest patterns:
   - `L v / + u / S v` → single “add-to-var” op  
   - `L v / I v` style increments that stay in the same slot  
   - the Pascal-style counting loop you already sketched as `Y`

6. **Page-align the bytecode or insert a NEXTPAGE op**  

   Comment (c) in the source: if a page cross is impossible (or
   handled by a special op at $xxFF), the `inc ipc+1` path disappears
   and the common `inc ipc` becomes 5 cycles instead of 8–12.

7. **Smaller / denser handlers for the top 10 ops**  

   `L`, `S`, `+`, `-`, `I`, `J`, `G`, `B`, `0–31 loads`, and the new
   loop ops dominate. Every byte and every branch in those paths is
   paid on almost every iteration of a tight loop.

### What will *not* close the gap

- More C-level optimisations in the bytecode generator help a bit, but
  the interpreter dispatch overhead remains.

- ALTCODE=1 as currently written is only a partial conversion;
  finishing it is necessary before it beats ALTCODE=0 by more than a
  couple of percent.

- Going to a threaded-code or direct-threading model (addresses
  instead of opcodes) is possible but changes the bytecode format and
  the compiler.

### Realistic target

With (1) + (2) + (4) carefully applied you should be able to get from
~5× down to roughly **2–2.5×** native on tight arithmetic loops. The
remaining factor is inherent to any bytecode interpreter that still
has a fetch/dispatch step; specialised loop ops and denser encoding of
the hottest sequences are the way to attack that residual.

I can sketch a concrete self-modifying dispatch + Y-as-ipc-low
implementation next if you want to try it.



## Respoonse to the top summary:

Given the numbers and constraints, here is a realistic path that stays
multi-process / relocatable and does not require a full JIT.

### What is actually expensive right now

From your measurements:

- ~40+ cycles just to get from one op to the next handler
- Average ~106 cycles/op on non-loop code
- Target ~21-cycle dispatch is aggressive but the right direction

The dominant costs are:

1. Fetch + increment ipc (10–12 c)
2. Table lookup + build jmpvec + indirect jump (~23 c)
3. Handler prologue that reloads slots / ACC from ZP

Self-modifying a two-byte JMP address every op is indeed only a small
win (still ~11–14 c of stores + the JMP). Classic next-address
threading (PHA/PHA/RTS) is in the same ballpark and trashes the stack.

### Highest-ROI changes that stay compatible

**1. Finish ALTCODE=1 properly (Y = ipc low offset)**  

This is still the best pure-interpreter lever you have. The 1.5 % you
saw was because only part of the handlers were converted. Once *every*
straight-line op uses Y and only control-flow / page-cross ops call
FIXIPC, you remove:

- the `ldy #0` every op
- most of the `inc _ivm_ipc` traffic on the common path

Expected gain: 8–15 cycles per op on straight-line code. Do this
before anything more exotic.

**2. Shrink the dispatch to a single indirection**  

Keep the 256-byte table, but make the common path:

```asm
xnext:
        lda (ipc),y     ; or with Y-offset model
        tax
        lda displo,x
        sta jmpvec
        lda disphi,x
        sta jmpvec+1
        jmp (jmpvec)
```

That is already close to minimal for a table-driven interpreter. The
only cheaper table form is a pure low-byte table in page zero + fixed
high byte (if all handlers live in one page). That saves the high-byte
load and is worth doing if you can keep the handler page dense.

**3. Special-case the top 5–8 opcodes before the table**  

`L`, `S`, `+`, `-`, `I`, `J`, 0–31 loads, and the new loop ops
dominate. A short compare ladder (or a second, denser table for 0–63)
avoids the full indirect jump for the hottest ops. This is often worth
5–10 cycles on the common path and is easy to measure.

**4. Keep ACC in ZP, but stop reloading it when the previous op left
  it live**

A single sticky “acc_valid” bit (or just careful fall-through between
related ops) removes a lot of the `lda _ivm_acc / ldx _ivm_acc+1`
pairs inside arithmetic chains. This is cheaper than trying to keep
ACC in A/X permanently.

**5. More fused / specialised ops (you already started)**  

Every time you turn a 4–6 op sequence into one op you amortise the
dispatch. The `i`/`n` (and `j`/`m`) pairs already showed 18–37 % on
loop-heavy code. The next candidates are the ones that appear in the
inner loops of `test.c`:

- add-to-var / sub-from-var
- load + compare + branch
- the remaining counting patterns

These do not require any change to the dispatch.

### What to defer

- Full ACC-in-registers: high effort, fights with `(zp),y` memory and multi-process save/restore.
- True JIT / templated handlers: great speed, breaks the “several processes, one engine, relocatable blob” model you are using.
- Heavy self-modifying dispatch every op: the write cost cancels most of the gain unless the dispatch itself lives in zero page and you only rewrite one byte.

### Suggested order of work

1. Finish ALTCODE=1 (Y-offset) cleanly for all non-control-flow ops.  
2. Measure again.  
3. Add a short hot-path special case for 0–31 loads + `L`/`S`/`+`/`I`.  
4. Add 2–3 more fused ops that show up in the cycle profiles.  
5. Only then consider a page-zero low-byte dispatch table or further micro-optimisations.

That sequence should get you from ~2.7× cc65 toward ~1.8–2.2× on the
current test suite without giving up multitasking or relocation. The
residual gap is the inherent cost of a bytecode fetch/dispatch;
specialised ops are the practical way to keep eating into it.

Want me to sketch the finished ALTCODE=1 control structure (the FIXIPC
/ USEY / pnext / xnext contract that every handler must obey)?
