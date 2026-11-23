# OrWIN-ATMOS

# MeteoriC-65 (parse.c c.bnf): next, after the manual (MeteoriC-man.md)

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
- x << 16 / x >> 16 in test-shifts: asm prints 0, the others print x
  unchanged. hjit is wrong in 240 of 249 lines there.
- Other output differences:
  - asm: dispatch-pon
  - hjit: memset
  - cc65 and oscar: ifdef
  - oscar: loops

- recursion: report a call of a function to itself (directly or through
  others) as an error. Later: tail calls (a loop), recursion with frames
  copied to a stack
- local arrays with an initializer: require "static" (then it is standard
  C: the initializer runs once)
- ptr + n in an expression is not scaled: report it as an error for int *
  and word struct pointers (until it is scaled; p++ p += n are)
- #if NAME is always true: evaluate a #define'd name (#ifdef works)
- ivm-asm: prints usage before parsing its arguments (argv broken in the
  -DIVMASM build)
- int compares: ints are unsigned, so < <= > >= on an int give C's answer
  only for values below 32768. Warn on them (the type is in the capture)
  and recommend "word"; recognize v < 0 / v > 0 (the top bit: v & 0x8000)
  with a warning. Needs a grammar directive \"text" that prints text as a
  warning (file:line) when its alt is accepted
- shifts: x = x << n is ACC = x << n ("{ n", then S x); the 6502 shifts in
  memory (ASL / LSR zp, ROL / ROR): ops mem[s] <<= ACC / >>= ACC (commented
  out in ivm.c: op_shlm op_shrm) for x <<= n, x = x << 1, also by 1 alone
- break / continue: cheaply, one jump per loop to patch at its end / its
  next (the patch stack, like \x \y). Or first label: / goto as a
  substitute; the JIT may take a backward goto for THE loop (its loop
  detection, lc_lbl): check it


# tpi.c parse.c
- parse: save .ivm
- parse: add optimize
- emit optimized (recognize gen: L S 1-8 a-h)
- poophole optimizer generalize
- FIX: string with \0 "embedded will exectute byte code?
  stop copy and skip at first \0 ?

# ivm
