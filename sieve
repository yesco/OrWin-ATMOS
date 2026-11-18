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
# jitzptmp       58,557,526
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
# -- const int foo = ... byte...
# (/ 60969255 60816464.0) = 0.25% cycles  152791
# (/ 60969255 58556915.0) = 4.12% cycles 2412340
#
# ==> 122 byte bytecode went down to: 113! -9 vars!

#name="sieve"

name="byte-sieve-prime"

echo "-- `uname -a` --"
clang -Dputz=printf $name.c && time ./a.out
echo

echo "-- cc65 --"
cl65 -Dputz=printf -t sim6502 -Oirs -vm -m $name.map.cc65 $name.c -o $name.sim && time sim65 -c $name.sim
echo

echo "-- oscar64 --"
time ./oscar -Os -Dputz=printf -ep $name.c 
echo

echo "-- pc --"
./pc $name
echo

echo "-- ivm --"
./ivm     $name
echo

echo "-- ivm-asm --"
./ivm-asm $name
echo

echo "-- ivm-jit --"
./ivm-jit $name
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

# **) = multi-tasking several processes, at least 2x overhead?


#      ============  byte-sieve-prime.c - 109 BC ============
# (/ 14.4 7.0)
# oscar64 -Os	  294 B	|       14.4s  ~2.06x 	| best C for 6502 -   (4594 B)
# cc65 -Oirs	  442 B	|       69.5s  ~9.93x	| std compliant, slow (3083 B)
#
# ./ivm-asm 	  109 BC|(182?)180 s  ~25x      |
# ./ivm C-interpr "  ** |(744?)697 s  ~99.6x    | simplier code
#
# ./ivm-jit       672 B |       61 s   ~8.7x    |
# ./ivm-jit const       |       58.7s  ~8.4x	| 4.12% cycles 2412340
# ./ivm-jit zptmp       |       58.6s  		| 58,557,526 
# ./ivm-jit zp    597 B	|       55.3   ~7.9x	| 55,267,670
#      - unopt:  -12.6%	|			|
#     - fix i ( ) 547 B | 	50.5    7.22x	| 50,546,185
# a = a + 1 == I  540 B	|       50.3    7.19x	| 50,300,804
#    better >     532 B |       47.6    6.79x	| 47,553,365
#     even better 526 B	|	47.4    6.77x 	| 47,402,930
#   saved 21.7% code    |			|
# jit: empatch    487 B |       44.8    6.40x	| 44,791,864 5% faster
# zp: Gemini Guess:	|      ~44.7    ...
# jit: const ')'  478 B |	41.4	5.91x	| 41,380,743
# jit: Lcc(bbcc   469 B	|       40.5    5.78x	| 40,453,871 2.3%
# jit: *2  -> <<1 450 B |       40.39	5.77x	| 40,389,838


# === TODO: no ?? was faster than after OP. some regressed?
# adding ./ivm -r 10 option so we can (minimize/remove) overhead
#
# --- df299c9548b538b5443378482bb86b238a1fb216
# ivm: memset     101 BC|      623 s   89x	|622,733,340
#	no ?? (41 instr)|      561 s   80x	|561,055,640
# asm: memset		|      154 s   22x	|154,385,371
#       no ??		|      148 s   21x	|148,471,071
# jit: memset     412 B	|	34.98   5.00x	| 34,984,658
#	no ??	  385 B	|	31.16   4.45x	| 31,158,066

# - old
# ivm: 0x		|			|      34,191
# ivm: 1x 		|			| 561,098,501

# - new
# asm:                  |      154		|154,385,591
# asm: ALTCODE=1 m ( )	|      139 s   19.9x	|139,257,315
#       no ??		|      133 s   19.0x	|133,343,015
# ivm: 0x 		|			|      237,623
# ivm: 5x 		|			|31,46,563,638


# --- 0395abf4215ecf7428f131351d35639072dd00a1 (zeropage)
# ivm: byte idx	  105 B	|			|625,684,958
# asm: byte idx         |      128 s   18.35x	|128,431,679
# jit: byte idx	  404 B	|      slower...        | 31,266,638
#       no ??     367 B |       25.56	3.65x	| 25,557,416

# --- started w OPT, maybe got worse! but 367 B and now 439 ???
#                 463 B |			| 33,155,238

# --- non opt is SLOWER and simplier to opt removed some opts
# ivm			| 			|686,483,586
# asm			|			|133,525,763
# jit			|			| 50,851,131

# --- peephole Lxx XX 0Q and -z
# ivm              95 B	|			|629,316,715
# asm		   95 B	|			|121,566,667
# jit		  429 B |			| 27,893,845
# jit -z 	  364 B	|			| 25,033,395
#
# ==> NOTE: saves byte-codes, doesn't make faster
# it's currently 101 bytes, 41 instr, can made 30 by remove redundant!

# ivm-oscar	  	|		44.20x	|309,383,290

# - opt jit byte-sieve-primes.c - sieve.md CMP oscar64
# oscar64 -Os	  294 B	|       14.4s   1x 	| best C for 6502 (4594 B)
# jit -z 	  364 B	|		1.74x	| 25,033,395
# jit for	  356 B	|		1.726x	| 24,856,097
# jit 'i' 'n'     335 B |		1.738x 	| 24,757,775

# (/ 25.033395 14.40)1.7384302083333332


# (/ 309.383290 7.0) 
# (/ 450 469.0)
# (/ 526 111.0)4.738738738738738 (/ 672 111.0)6.054054054054054

# === SPEED OVERHEAD === 
#
# Target / Variant        | Size  | Time   | vs ASM |vs Oscar| vs JIT
# -------------------------------------------------------------------
# Orig ASM (hand)         |  ?    |  7.00s |  1---x |  0.49x |  0.28x
#                         |       |        |        |        |
# oscar64 -Os            *| 294 B | 14.40s |  2.06x |  1---x |  0.58x
#                         |       |        |        |        |
# ivm-jit (-z peephole)  *| 364 B | 25.03s |  3.58x |  1.74x |  1---x
# ivm-jit (byte idx)      | 367 B | 25.56s |  3.65x |  1.77x |  1.02x
# ivm-jit (memset,no??)   | 385 B | 31.16s |  4.45x |  2.16x |  1.24x
# ivm-jit (const/*2)      | 450 B | 40.39s |  5.77x |  2.80x |  1.61x
# ivm-jit (zp baseline)   | 597 B | 55.30s |  7.90x |  3.84x |  2.21x
# ivm-jit (initial)       | 672 B | 61.00s |  8.71x |  4.24x |  2.44x
#                         |       |        |        |        |
# cc65 -Oirs             *| 442 B | 69.50s |  9.93x |  4.83x |  2.78x
#
# ivm-asm (byte idx)      | 105 BC| 128.4s | 18.34x |  8.92x |  5.13x
# ivm-asm (ALTCODE=1)     | 101 BC| 133.3s | 19.04x |  9.26x |  5.32x
# ivm-asm (memset)        | 101 BC| 148.5s | 21.21x | 10.31x |  5.93x
# ivm-asm (initial)       | 109 BC| 180.0s | 25.71x | 12.50x |  7.19x
#                         |       |        |        |        |
# ivm-oscar              *|     - | 309.4s | 44.20x | 21.48x | 12.36x
#                         |       |        |        |        |
# ivm (btable disp) +368 B|  94 BC| 385.8s | 55.11x | 26.79  | 15.41x
# ivm (peephole)          |  95 BC| 629.0s | 89.86x | 43.68x | 25.13x
# ivm C-interp (initial)  | 109 BC| 697.0s | 99.57x | 48.40x | 27.84x
#
# *) actual program size (file) is bigger:


# === THE BEST OF THE BEST - FASTER ===
#
# Target / Variant        |  Size |   Time | vs C   | vs ASM | vs JIT
# -------------------------------------------------------------------
# oscar64 -Os            *| 294 B | 14.40s | 26.79x |  8.92x |  1.74x
# ivm-jit (-z peephole)  *| 364 B | 25.03s | 15.41x |  5.13x |  1.00x
# cc65 -Oirs             *| 442 B | 69.50s |  5.55x |  1.85x |       
# ivm-asm (byte idx)      | 105 BC| 128.4s |  3.00x |  1.00x |       
# ivm-oscar              *|  94 BC| 309.4s |  1.25x |        |       
# ivm (btable disp) +368 B|  94 BC| 385.8s |  1.00x |        |       
#
#
#
#              FILE  ./ivm    -asm   -jit
#              ====  ======  =====  =====
#    oscar64 : 5772  11,518
#    cc65    : 4194  14,420  12,044 32,439
#    ivm     :  132
#    jit     :  749 -- gen 364 B (+ (* 6 2) 364 100 50 50 20 50 100)
#      libs  :      malloc free opmu opodiv opmod opputnum
#                   putchar getchar INIT

#
#
# The sieve's inner loop: 149,870 times in 10 iterations,
# adding a 16-bit prime to a 16-bit index k (up to 8190) and writing a byte to memory.


# Gemini guess:
#
# Where the Remaining ~33 Seconds Above Hand-Coded ASM Live
#
# In hand-written 6502 assembly (~7.0 s / ~47 cycles per inner-loop
# hit), the inner loop is literally ~8 instructions:
#
# loop:
#   STA (ptr),Y
#   TYA
#   CLC
#   ADC prime_lo
#   TAY
#   LDA ptr+1
#   ADC prime_hi
#   STA ptr+1
#   CMP end_hi
#   BCC loop
#
# Your ivm-jit is currently averaging ~269 cycles per inner-loop hit
# (40.39M / 149,870), which means a simple peephole pass in
# ivm-jit—specifically fusing U0d0c + <0d> >fa + B11 so A isn't reloaded
# between the 16-bit add and the bounds check—could realistically drop
# your JIT time from 40.39 s into the ~28–32 s (~4.0x–4.5x) range
# without touching the bytecode ISA.
#
# <09> +09 +fd S0a
# L09 +0a S0b <0b> >f9 G0a
#   Lf8 )040b U0b0a
# B11
