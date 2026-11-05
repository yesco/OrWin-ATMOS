## --- sieve benchmark ----
##
##
##
# --- performance
# - cc65          85204077 cycles         4194 bytes
# - oscar64       21078607 cycles         5772 bytes 682 b main+primes
# - ivm          731302528 cycles          303 bytes
# - ivm-asm      199459095 cycles          303 bytes
#
# ivm: 11664  imv-asm: 9580 (- 9580 6772) = 2808 VM asm bytes
#
#
# ivm/cc65 : (/ 731302528 85204077.0) ivm is 8.58x slower than cc65
# asm/cc65 : (/ 199459095 85204077.0) ivm is 2.34x slower than cc65
# cc65/osc : (/  85204077 21078607.0)        4.04x LOL!
#
# (+ 256 (* 2 7))

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
