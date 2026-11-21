#!/usr/bin/env python3
# ivm-tree.py - write ivm-btable.c from the label table in ivm-jtable.c
#   python3 ivm-tree.py ivm-jtable.c > ivm-btable.c
import re, sys
src = re.sub(r"//.*", "", open(sys.argv[1]).read())   # skip comments
lab = {}
for c, l in re.findall(r"t\[('(?:\\.|.)'|\d+)\]\s*=\s*&&(\w+);", src):
    if l != 'op_unk':
        lab[int(c) if c.isdigit() else ord(eval(c))] = l
segs = []                       # [first op, label] runs over 32..127
for c in range(32, 128):
    l = lab.get(c, 'op_unk')
    if not (segs and segs[-1][1] == l): segs.append([c, l])

def ch(c):
    if c == 39: return "'\\''"
    if 32 < c < 127 and c != 92: return "'%s'" % chr(c)
    return "0x%02X" % c

out = []
def gen(s, ind):
    if len(s) == 1:
        out.append(ind + "goto %s;" % s[0][1]); return
    m = len(s) // 2
    if m == 1:
        out.append(ind + "if (op < %s) goto %s;" % (ch(s[m][0]), s[0][1]))
    else:
        out.append(ind + "if (op < %s) {" % ch(s[m][0]))
        gen(s[:m], ind + "  ")
        out.append(ind + "}")
    gen(s[m:], ind)
gen(segs, "  ")
print("""// ivm-btable.c - for run_ivm in ivm.c
// 
// Dispatch by binary search, plain gotos.
//
// #included in run_ivm() with -DUSE_BTABLE.
// 
// Works on any compiler (no labels as values, no switch):
//   %d op ranges, %d-%d compares for any op.
//
// GENERATED from ivm-jtable.c:
// 
//   python3 ivm-tree.py ivm-jtable.c > ivm-btable.c
// 

  if (op & 0x80) goto op_hibit;
""" % (len(segs), (len(segs)).bit_length()-1, (len(segs)-1).bit_length()))
print("\n".join(out))
