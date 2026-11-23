#!/usr/bin/env python3
# bnfconv.py IN OUT : rule calls A..Z -> $A..$Z (outside [ ] productions,
# not after a \), up to the "." line; comments, definitions' "X=" and the
# rest of the file unchanged
import sys
src = open(sys.argv[1]).read().split('\n')
out = []
done = False
for line in src:
    if done or line.startswith('#') or not line.strip():
        out.append(line); done = done or line == '.'; continue
    if line == '.':
        out.append(line); done = True; continue
    i = 0; o = ''
    if len(line) > 1 and line[0].isalpha() and line[1] == '=':
        o = line[:2]; i = 2
    inprod = False
    while i < len(line):
        ch = line[i]
        if ch == '\\' and i + 1 < len(line):
            n = line[i + 1]
            k = 2
            if n == '#' or (n == 'x' and i + 3 < len(line) and all(c in '0123456789abcdefABCDEF' for c in line[i+2:i+4])):
                k = 4
            o += line[i:i + k]; i += k; continue
        if ch == '[' and not inprod: inprod = True
        elif ch == ']' and inprod: inprod = False
        elif ch.isupper() and not inprod: o += '$'
        o += ch; i += 1
    out.append(o)
open(sys.argv[2], 'w').write('\n'.join(out))
