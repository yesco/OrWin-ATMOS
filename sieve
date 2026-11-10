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
# --- (from mc) ---
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
#
# ivm/cc65 : (/ 731302528 85204077.0) ivm is 8.58x slower than cc65
# asm/cc65 : (/ 199459095 85204077.0) ivm is 2.34x slower than cc65
# cc65/osc : (/  85204077 21078607.0)        4.04x LOL!
#            (/ 199954374 198149017.0) 0.91% faster w pagealign table
# FASTDISP.: (/ 198149017.0 181680540) 9.06% faster
#
# TODO: looking at sieve.bytes - maybe 20% savings possible...





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
# ./ivm C-interpr    ** |      697 s  ~99.6x    | simplier code
#
#
# **) = multi-tasking several processes, at least 2x overhead?
#
#
# The sieve's inner loop: 149,870 times in 10 iterations,
# adding a 16-bit prime to a 16-bit index k (up to 8190) and writing a byte to memory.
