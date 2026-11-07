# data usage

Memory map for **`parse.c` + `ivm-gen.c`** during a compile run.

### Caps (defaults; parse can override before include)

| Symbol | Default | Notes |
|--------|---------|--------|
| `MAX_BIN_LEN` | 2048 (gen) / 4096 (parse) | bytecode buffer |
| `MAX_CONSTS`  |  128 | const pool entries |
| `MAX_DATA`    | 2048 | string/data bytes |
| `MAX_VECTORS` |   64 | functions |
| `MAX_GINIT`   |   64 | deferred global inits |
| `MAX_SYMBOLS` |  250 | hard ceiling; grows by `SYM_GROW=16` |
| `MAX_PARAMS`  |    4 | per function |
| `MAX_STACK`   |   64 | parse frames |
| `MAX_SLOTS` / `CONST_TOP` | 256 / 253 | runtime mem image in **blob**, not parse RAM |
| `STR_MATCH_MAX` | 128 (cc65) / 512 (host) | static string match buf |
| `NAME_LEN`    |   32 | id buffers |
| `NRULES` | 26 | A–Z rule pointers |
| `MAX_LINE` / `MAX_ACCUM` | 1024 / 4096 | **BNF load only** (temps) |

Suggested knobs for sizing: **`NBYTECODE`**, **`NCONST`**, **`NBYTESDATA`**, **`NVECTORS`**, **`NSYMS`**, **`NFRAMES`**, **`INPUT_SIZE`** (source bytes / live window).

---

### Fixed / max-sized globals (always reserved)

| Object                         | Size (bytes) |
|--------                        |----------------|
| `bin[MAX_BIN_LEN]`             | `MAX_BIN_LEN` |
| `const_pool[MAX_CONSTS]`       | `2 * MAX_CONSTS` |
| `const_is_reloc[MAX_CONSTS]`   | `MAX_CONSTS` |
| `data_pool[MAX_DATA]`          | `MAX_DATA` |
| `vectors[MAX_VECTORS]`         | `2 * MAX_VECTORS` |
| `frame_base[MAX_VECTORS]`      | `MAX_VECTORS` |
| `ginit_slot/src[MAX_GINIT]`    | `2 * MAX_GINIT` |
| `str_match_buf[STR_MATCH_MAX]` | `STR_MATCH_MAX` |
| `id_buf` + `last_undef`        | `2 * NAME_LEN` |
| `far_stack` + `far_alt`        | `2 * MAX_STACK` |
| `rule[26]` + `blobs[26]` (ptr) | `26 * 2 * sizeof(void*)` |
| SYMS-only:`sym_r/w[256]`,`frame_sz[]`| optional |

**Fixed ≈**
```text
MAX_BIN_LEN
+ 3*MAX_CONSTS          // pool words + reloc flags  (words = 2 B → use 2*MAX_CONSTS + MAX_CONSTS)
+ MAX_DATA
+ 3*MAX_VECTORS         // vectors as words: 2*MAX_VECTORS + frame_base
+ 2*MAX_GINIT
+ STR_MATCH_MAX
+ 2*NAME_LEN
+ 2*MAX_STACK
+ ~100                  // scalars / small
```

With parse defaults (`MAX_BIN_LEN=4096`, rest ivm-gen defaults, cc65 `STR_MATCH_MAX=128`):

```text
≈ 4096 + 384 + 2048 + 192 + 128 + 128 + 64 + 64 + ~200
≈ 7.3 KB  (static/BSS, not counting malloc)
```

---

### Dynamic (grows with input / grammar)

| Object | Growth | Peak ≈ |
|--------|--------|--------|
| **BNF** `blobs[r]` + `rule[r][]` | once at load | size of grammar text + pointers |
| **`line_buf`**      | doubles from 256 | live window after `\C` commits; ≤ remaining file, often ≪ `INPUT_SIZE` |
| **`stack` (Frame)** | +8 frames to `MAX_STACK` | `NFRAMES * sizeof(Frame)` |
| **`syms[]`**        | +16 `Symbol` to `MAX_SYMBOLS` | `NSYMS * sizeof(Symbol)` + Σ `strlen(name)+1` |
| **blob output**     | `malloc` in `build_blob` | header + vectors + consts + fp + bytecode + data ≈ **O(NBYTECODE + NCONST + NBYTESDATA + NVECTORS)** |

**`sizeof(Frame)`** (6502, `word` = 2): about **50–56 bytes**  
(`prod_cap[10]`, `patch[4]`, saves, marks).

**`sizeof(Symbol)`** without name: about **16–20 bytes** + `malloc(name)`.

---

### Formula (parse RAM, peak)

```text
MEM ≈
    MAX_BIN_LEN
  + 2*MAX_CONSTS + MAX_CONSTS
  + MAX_DATA
  + 2*MAX_VECTORS + MAX_VECTORS
  + 2*MAX_GINIT
  + STR_MATCH_MAX
  + 2*NAME_LEN + 2*MAX_STACK
  + BNF_BYTES
  + LINE_CAP                    // ≤ INPUT_SIZE, usually much less after \C
  + NFRAMES * sizeof(Frame)     // ≤ MAX_STACK * ~56
  + NSYMS * sizeof(Symbol) + NAME_BYTES
  + BLOB_BYTES                  // build_blob malloc (can free after save)
  + O(1)
```

In “workload” variables:

```text
MEM ≈ FIXED(MAX_*)
    + O(BNF)
    + O(LINE_WINDOW)            // not full INPUT_SIZE if \C used
    + O(NFRAMES)
    + O(NSYMS + name chars)
    + O(NBYTECODE + NCONST + NBYTESDATA + NVECTORS)  // output blob
```

Runtime **IVM** image size (what you ship), not parse BSS:

```text
BLOB ≈ 22
     + 2*NVECTORS
     + 2*(NCONST)               // includes 0 sentinel in const image
     + NVECTORS                 // fp bytes
     + NBYTECODE
     + NBYTESDATA
```

`memsz` in the header is still **254** until slot compaction; that is **target VM cells**, not parse memory.

---

### What dominates on 6502

1. **`bin[4096]`** — biggest fixed hit; lower `MAX_BIN_LEN` if
   programs are small
2. **`data_pool[2048]`** + **`const_pool`**  
3. **Parse `Frame` stack** — depth × ~56 bytes  
4. **Symbols** — light if names are short and `SYM_GROW` is small  
5. **`line_buf`** — only dangerous if `\C` is rare and the whole file
   sits in RAM

`INPUT_SIZE` alone does **not** force `MEM`; only **uncommitted**
input + BNF + tables + codegen buffers do.




# Candidate Example Workloads


So the program has to be array-heavy, loop-heavy, integer-only, and preferably non-recursive (or only shallow).
Good candidate workloads

Program

Why it fits

What it stresses

Sieve of Eratosthenes (e.g. primes to 8k–16k)
Tight loops, byte array, counting + zero tests
S, L, n/m, indexed loads/stores
Integer matrix multiply (e.g. 16×16 or 32×32)
Triple loop, lots of + / * / S
Exactly the +S / S hot path you already see
Checksum / CRC-16 over a buffer
Sequential scan, shift/xor, inner loop
Bit ops, sequential access, long straight-line
Insertion or bubble sort on ~256–1k ints
Nested loops, swaps, comparisons
if, indexing, L/S
Bresenham line (plot into a small framebuffer)
Mixed arithmetic, branches, useful “graphics”
Realistic branch mix, not pure counting
Word count / histogram on a built-in string
Char scan, classifiers, small tables
Byte loads, if chains, useful output
Recommendation
Start with two:
Sieve — classic, easy to write in the subset, dominated by the specialised loop ops and indexed stores. Good for measuring n/m and memory traffic.
Small matrix multiply — forces the + / S / * patterns that showed up in the synthetic profile, but in a ratio that looks like real numeric code.
Both are short enough to rewrite by hand, produce a verifiable result (prime count / checksum of the matrix), and give a profile that is no longer “two loops + noise”.


# Optimizations Possible

deepseek:

Analyzing parse.c for 6502 Code Size Reduction

I've read through parse.c. There are indeed several very common idioms
repeated many times. Let me go through the top candidates, ranked by
(repetitions × savings) — because on cc65, a call costs ~3 bytes (JSR)
plus argument-passing overhead, so a helper only pays off if the call
sites outnumber the helper body enough times.

Important caveat up front: cc65 passes args on a software stack. So "3
bytes for JSR + 7 bytes for arg" is roughly right for one arg, but
many of these idioms already pass/consume zero args, so a zero-arg
helper call is just 3 bytes (JSR), and no-arg helpers are the most
profitable. Also, cc65 will often inline if/return patterns into
~10–15 bytes; a JSR saves less than naive counting suggests unless the
pattern is ≥ 12 bytes.

Here are the top idioms I found.

---

1. skip_ws(); if (!line_need()) return 0; — repeated 5×

Locations:

· match_lit (line ~"skip_ws(); if (!line_need()) return 0;")
· match_ident_raw
· match_num
· match_str_const (skip_ws(); if (!line_need() || line_buf[line_pos]
  != '"') return 0;)
· match_newline (skip_hspace variant)

cc65 cost inline: JSR _skip_ws (3) + load line_need result & test (≈5)
+ branch (2) + return 0 (≈2) ≈ 12 bytes each.

Helper:

```c
static int skip_need(void) { skip_ws(); return line_need(); }
```

Body ≈ 8 bytes + JSR per call (3) + test/branch (4) = ~7 bytes per
call site + ~10 one-time.

Savings: ~5 bytes × 5 sites = ~25 bytes, minus ~10 helper = net ~15
bytes. Modest.

---

2. if (!match_ident_raw()) return 0; — repeated 6×

· match_ident_same
· match_ident
· match_ident_decl
· match_ident_func_decl
· match_param_decl
· (plus match_ident_raw itself contains the skip_ws above)

Inline: JSR _match_ident_raw (3) + TAX/BNE (4) + return (2) ≈ 9 bytes.

Not really a helper opportunity — the pattern is already a call. Skip.

---

3. record_capture(MAKE_CAP(slot, ...)); return 1; — 4×

· match_ident_same, match_ident, match_ident_decl, match_param_decl,
  match_num

MAKE_CAP + record_capture is itself 2 calls; combining them into
record_slot_cap(slot, type) saves one JSR + arg setup per site.

Inline cost: MAKE_CAP expansion
(build word: ~8 bytes) + push 2 args (12 bytes) + JSR (3) ≈ 23 bytes.

Combined helper call: push 2 args (12) + JSR (3) ≈ 15 bytes.

Helper body ≈ 15 bytes.

Savings: ~8 × 4 = 32 bytes, minus 15 ≈ net ~17 bytes.

---

4. sym_find(...) → if (SYM_NONE) { note_undef; return 0; } blocks

These are bigger blocks (8–15 bytes each) but only occur 2–3
times. Low yield.

---

5. The emit() / emitop2() pairs — the biggest win

Inside run_production, the pattern emitop2('L', ...) / emitop2('S',
...) / emit(...) appears 8+ times. Each emit call in cc65 is: load arg
into A (3) + JSR (3) = 6 bytes; emitop2 = load 2 args (8–10) + JSR (3)
≈ 13 bytes.

These are already functions though, so this is not duplication to fix
by merging — it's fine as-is.

However, there's real duplication here:

· emitop2('S', b); emitop2('J', b); in case 'e' — vs — emitop2('L',
  src); emitop2('S', dest); in case 'g' — vs — emitop2('S', tmp); in
  force_addr. Not the same, no merge.

---

6. The bytecopy / string-walk loops — big win in load_bnf and line_commit

Two nearly identical loops:

```c
for (i = 0; i < line_pos; i++)
  if (line_buf[i] == '\n') line_no_base++;
...
for (i = 0; i < rest; i++)
  line_buf[i] = line_buf[line_pos + i];
```

vs build_one_rule's escape-copy loop, and load_bnf's acc[acc_len++] =
*p++ accumulator loop. These are large inline loops, each ~30–50 bytes
on cc65.

Extracting count_nl(buf, n) and memmove_reuse(buf, from, len):

· count_nl inline ≈ 20 bytes × 2 sites = 40 → helper ≈ 15 + 2×3 =
  21. Save ~19 bytes.

· But the line_commit shift is only used once — no win.

---

7. tolower(c) / isalpha / isalnum / isxdigit — cc65 emulation overhead

These map to __ctype_* lookups. Each ≈ 8–10 bytes inline. In tight
inner loops (match_num, match_ident_raw, match_lit, hexval) they cost
more than a hand-rolled check. Rewriting isalpha(c) as ((c|32)-'a' <
26) (≈5 bytes) in those 4 hot spots saves ~15–20 bytes and is
faster. Not a function call, but the same "tighten duplicated idiom"
idea.

---

8. The eprintf/eputc shim already exists — good, keep it. But
err_prefix and show_line are called from a loop in report_reject; that
loop is already compact.

---

9. if (n < 1 || n > cur->prod_n) return 0; and similar bounds guards —
4×

Small (~8 bytes each) but trivial to extract as capture_valid(n). Inline ≈ 8 bytes; helper call ≈ 3 + return-check ≈ 6. Marginal (~5–8 bytes total).

---

10. if (!line_need()) return 0; alone — 6×

· match_newline inner loop
· match_lit
· match_ident_raw inner loop
· match_ident_func_decl? no
· match_num × 2
· match_str_const
· match_str_emit
· decode_escape
· next_body_char
· read_quoted_body

Roughly 8–10 occurrences. Inline ≈ 5 bytes each (load, test, branch, epilogue). A helper can't save much — a call is the same 3 bytes, and you still need the test. No win unless merged into the skip_need helper above.

---

Summary table (conservative net byte estimates on cc65 for a 6502 target)

# Idiom Sites Est. inline cost/site Helper body Net saving
1 skip_ws(); if (!line_need()) return 0; 5 ~12 ~10 ~15
3 record_capture(MAKE_CAP(...)); return 1; 4 ~23 ~15 ~17
6 count '\n' in loop (only the reusable part) 2 ~20 ~15 ~19
7 hand-rolled isalpha/isalnum in hot paths 4 ~5 saved each 0 ~20
9 capture_valid(n) guard 4 ~8 ~6 ~8
— everything else — — — ~5–10

Realistic total: roughly 80–100 bytes saved out of what is likely a
5–8 KB binary. That's a real but modest win — the file is already
fairly disciplined about emit/eprintf shims and skip_ws.

---

The thing that would actually save hundreds of bytes (but isn't "dedupe")

A few structural wins dwarf the above:

1. decide_escape / next_body_char / read_quoted_body use a function
   pointer (put) — on cc65, indirect calls through function pointers
   cost ~8 bytes each and are hard to optimise. Three call
   sites. Replacing the callback with an enum (PUT_EMIT, PUT_BUF) and
   a switch inside read_quoted_body saves the function-pointer setup
   at each site (~10 bytes × 3) and lets cc65 use direct calls inside
   the switch. ~25–30 bytes.

2. Frame is huge (~46 bytes). On a 6502 with 64 frames that's ~3 KB
   of stack allocation — but it's realloc'd on demand in STACK_GROW=8
   chunks, so the resident cost is smaller. Still, prod_cap[10] as
   word = 20 bytes/frame. If captures were uchar (you already noted
   "prefer uchar if variable fits"), each frame shrinks by 10 bytes →
   at 8 frames resident that's 80 bytes, plus possibly eliminating
   some word conversions. ~50–80 bytes and much better cache behaviour
   on 6502.

3. report_reject builds a whole stack dump that's debug-only. #ifdef
   VERBOSE-guard it. Hundreds of bytes.

4. #ifdef SYMS block is ~200 lines of formatting code. It's already
   ifdef'd — just make sure it's off by default for the 6502 build.

5. hexval/hex2 and the esc_in/esc_out strchr — strchr on cc65 is a
   real call with a real loop; the decode_escape path calls it per
   character. ~20 bytes and faster to replace strchr with a short switch
   (there are only 7 escapes).

If you're serious about bytes, #3 and #4 alone likely give you more
than all the dedupe efforts combined. Would you like me to sketch
concrete patches for items 1, 3 and 6 — the three with the best
byte-per-effort ratio?