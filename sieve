## --- sieve benchmark ----
##
##
## TODO: ./ivm gives wrong result!
##
##
## 898506779/93822576   9.57665859654077287325
## ivm.c    /    cc65 = 9.58x slower UNOPTIMIZED bytecode
## ivm-asm  /    cc65 = ... malloc/free missing 3.1x expected? (sqrt)

## (/ 93822576 23185738.0) = 4.05x slower cc65 / oscar64
#     cc65     oscar64



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
