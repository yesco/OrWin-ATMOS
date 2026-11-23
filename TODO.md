# OrWIN-ATMOS

# MeteoriC-65 (parse.c c.bnf): next, after the manual (MeteoriC-man.md)
- recursion: report a call of a function to itself (directly or through
  others) as an error. Later: tail calls (a loop), recursion with frames
  copied to a stack
- local arrays with an initializer: require "static" (then it is standard
  C: the initializer runs once)
- ptr + n in an expression is not scaled: report it as an error for int *
  and word struct pointers (until it is scaled; p++ p += n are)
- #if NAME is always true: evaluate a #define'd name (#ifdef works)
- char struct fields hold a word: make them bytes (striped: the lo plane
  alone, [ ] ops; plain: a byte offset)
- ivm-asm: prints usage before parsing its arguments (argv broken in the
  -DIVMASM build)

# tpi.c parse.c
- parse: save .ivm
- parse: add optimize
- emit optimized (recognize gen: L S 1-8 a-h)
- poophole optimizer generalize
- FIX: string with \0 "embedded will exectute byte code?
  stop copy and skip at first \0 ?

# ivm
