# OrWIN-ATMOS

# MeteoriC-65 (parse.c c.bnf): next, after the manual (MeteoriC-man.md)

-  &a[i]  == a+i not right for word*

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