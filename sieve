echo "-- `uname -a` --"
clang sieve.c && time ./a.out
echo

echo "-- cc65 --"
cl65 -t sim6502 -Oirs sieve.c -o sieve.sim && time sim65 -c sieve.sim
echo

echo "-- pc ; ivm ; ivm-asm --"
./pc sieve && ./ivm sieve && ./ivm-asm sieve
echo
