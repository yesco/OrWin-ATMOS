#!/bin/sh
# run.sh - check the MeteoriC-65 manual's examples (MeteoriC-man.md)
#
#   cd Tests/Compiler && ./run.sh [man-expr.c ...]     (default: all)
#
# man-NAME.c      must compile; every case prints "name:<TAB>expected"
#                 then (indented) what it got: the two must agree. gcc's
#                 output must be ours too, unless the file says "MC65 only"
#                 (16 bit ints, our quirks: the manual's NOTEs)
# man-NAME-err.c  the lines marked ERR (in a // comment) are the errors
#                 reported, no other, and no .ivm is written
#
# The compiler is built in a temp dir (clang or gcc, -DRUN: it runs the
# program on the host VM); nothing is written here.

ROOT=$(cd ../.. && pwd)
HERE=$(pwd)
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT

CC=clang
command -v clang >/dev/null || CC=gcc
$CC -DRUN -O2 -w "$ROOT/parse.c" -o "$T/parse.x" || exit 1

files=${*:-man-*.c}
fails=0

for f in $files; do
  b=$(basename "$f" .c)
  (cd "$T" && rm -f "$b.ivm" && ./parse.x "$ROOT/c.bnf" "$HERE/$f" > "$b.out" 2>&1)

  # error tests: reported lines == ERR lines
  case $b in *-err)
    want=$(grep -n '// ERR' "$f" | cut -d: -f1 | tr '\n' ' ')
    got=$(grep -o "$b.c:[0-9]*: error" "$T/$b.out" | cut -d: -f2 | sort -nu | tr '\n' ' ')
    if [ "$want" = "$got" ] && [ ! -f "$T/$b.ivm" ]; then
      echo "ok    $b (errors: $want)"
    else
      echo "FAIL  $b: want [$want] got [$got]"; fails=$((fails + 1))
    fi
    continue;;
  esac

  # it compiled, ran: each case expected == got
  if ! grep -q -- "--- ACCEPT ---" "$T/$b.out"; then
    echo "FAIL  $b: does not compile"; grep "error" "$T/$b.out" | head -5
    fails=$((fails + 1)); continue
  fi
  sed -n '/^--- RUN ---$/,/^--- END/p' "$T/$b.out" | sed '1d;$d' > "$T/$b.run"
  bad=$(awk -F'\t' '
    /^[^\t]/ { name = $1; want = $0; sub(/^[^\t]*\t+/, "", want); next }
    /^\t\t/  { got = $0; sub(/^\t+/, "", got); sub(/ +$/, "", got)
               sub(/ +$/, "", want); n++
               if (got != want) print name " want [" want "] got [" got "]" }
    END      { if (!n) print "no cases" }
  ' "$T/$b.run" || echo "awk failed")
  if [ -n "$bad" ]; then
    echo "FAIL  $b:"; echo "$bad" | sed 's/^/        /'; fails=$((fails + 1)); continue
  fi

  # gcc agrees (unless MC65 only)
  if ! grep -q "MC65 only" "$f" && command -v gcc >/dev/null; then
    gcc -w -o "$T/$b.g" "$f" 2>/dev/null && "$T/$b.g" > "$T/$b.gcc"
    if ! diff "$T/$b.run" "$T/$b.gcc" >/dev/null; then
      echo "FAIL  $b: gcc differs"; diff "$T/$b.run" "$T/$b.gcc" | head -6
      fails=$((fails + 1)); continue
    fi
    echo "ok    $b (= gcc)"
  else
    echo "ok    $b"
  fi
done

echo "--- fails: $fails ---"
[ $fails = 0 ]
