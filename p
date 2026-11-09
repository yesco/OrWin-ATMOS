clang -g -DSYMS -DVERBOSE -DRUN parse.c -o parse.x && time ./parse.x -v tpi.bnf test.p
echo
echo "=== parse.c: `./wcode parse.c` LOC ==="
echo
