## --- sieve benchmark ----
##
##
##
# --- performance
#
#   cc65          85,204,077 cycles         4194 bytes
#   oscar64       21,078,607 cycles         5772 bytes 682 b main+primes

# --- IVM ---
#
#   ivm          731,302,528 cycles          303 bytes
#   ivm-asm      199,459,095 cycles          303 bytes
#
# --- (byte-sieve-prime.c from mc) ---
#
# - byte-sieve.. 696,771,860     ./ivm       122 bytes
# - byte .. asm  199,637,579     ./ivm-asm   122 bytes
#
# 199637579/2208207 = 90.41 cycles / op in asm vm
#          (/ 1000000 90.41       ) = 11060  ops / s
# We'll do (/ 1000000 (- 90.41 22)) =    14K ops / s
#
# ivm: 11664  imv-asm: 9580 (- 9580 6772) = 2808 VM asm bytes
#
# --- FASTDISPATCH ---
#
# ivm/cc65 : (/ 731302528 85204077.0) ivm is 8.58x slower than cc65
# asm/cc65 : (/ 199459095 85204077.0) ivm is 2.34x slower than cc65
# cc65/osc : (/  85204077 21078607.0)        4.04x LOL!
#            (/ 199954374 198149017.0) 0.91% faster w pagealign table
# FASTDISP.: (/ 198149017.0 181680540) 9.06% faster
#
# --- JIT ASM ---
#
# (values are too big, as they include compile time!)
#./ivm-jit byte-sieve-prime
#
# jit@byte.: (/ 198149017 60969255.0) 3.25x faster w JIT
# jit/cc65 : (/ 60969255 85204077.0)  71.6% time of cc65!
#
# loaded byte-sieve-prime.ivm (162 bytes)
#  --- JIT compiling IVM (blob 162) ---
#  --- hdr: call=24482 con=24484 fp=24498 code=24499 ncall=1 ---
#  --- JIT: code_len=122 ncall=1 memsz=254 ---
#  --- JIT: emitted 748 bytes of 6502 at org 0x6347 ---
#
# - from 122 bytes of bytecode
# (/ 748 122.0) = 6.13x blow up in size
#
# (/ 60969255 2208207.0) 27.6 cycles / "op"
# 1029876275186 = 35.08 c / "op"
#
# (/ 60969255 60816464.0) = 0.25% cycles 152791





echo "-- `uname -a` --"
clang -Dputz=printf sieve.c && time ./a.out
echo

echo "-- cc65 --"
cl65 -Dputz=printf -t sim6502 -Oirs sieve.c -o sieve.sim && time sim65 -c sieve.sim
echo

echo "-- oscar64 --"
time ./oscar -Oz -Dputz=printf -ep sieve.c 
echo

echo "-- pc ; ivm ; ivm-asm --"
./pc sieve && ./ivm sieve && ./ivm-asm sieve
echo


# BYTE SIEVE BENCHMARK
# ====================
# 
# according to Gemini AI
#
# Implementation / VM	| Time (Seconds)	| Relative Speed - Notes
# -------------------------------------------------------------------
# Hand-tuned 6502 asmy	|  ~4 –  7 s    1x Base	| self-modifying code/zp indir ptr
# fig-FORTH/VolksForth)	| ~25 – 45 s   ~6x  	| (in)direct/indirect fst 16bt stk ops
# (cc65 / Aztec C)	| ~45 – 75 s  ~10x 	| 16bitsw stk, arr[]
# SWEET16 (Est / Equiv)	| ~60 – 90 s  ~12x–15x	| ~10x says Woz
# Applesft/C64 BASIC	|~200 –250 s  ~40x 	| fplopvar, parsing
# UCSD-Pas (p-code)  **	|      516 s ~100x	| Official BYTE Magazine result; heavy stack-frame and bounds-checking overhead.
#        (/ 516 7.0)  =                74x

#                        - my measurements - (/ xxx 7.0)
#
# oscar64 -Os		|       21 s   ~3x      | best C for 6502
# cc65 -Oirs		|       85 s  ~12x	| std compliant, slow
#
# ./ivm-asm          **	|      182 s  ~26x	| written in asm/ca65
# ./ivm C-interprter **	|      735 s ~105x	| written in cc65
#
#                       - byte-sieve-prime.c -
#
# ./ivm-asm		|      180 s  ~25x      |
# ./ivm C-interpr    ** |      697 s  ~99.6x    | simplier code
#
# ./ivm-jit             |       61 s   ~8.7x    |

# **) = multi-tasking several processes, at least 2x overhead?
#
#
# The sieve's inner loop: 149,870 times in 10 iterations,
# adding a 16-bit prime to a 16-bit index k (up to 8190) and writing a byte to memory.
