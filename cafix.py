#!/usr/bin/env python3
# cafix.py MAP MODULE LISTING  > absolute listing
#
# Rewrites ca65 listing offsets ("001234r") to absolute addresses using an
# ld65 map (-m): address = segment start + module's offset in that segment
# + listing offset. Tracks .segment/.code/.rodata/.data/.bss/.zeropage in
# the listing, since offsets are per segment.
#
#   python3 cafix.py ivm-jit.map ivm.o     ivm.raw.lst     > ivm.lst
#   python3 cafix.py ivm-jit.map ivm-lib.o ivm-lib.raw.lst > ivm-lib.lst
#
# -s: group by segment (CODE first, then RODATA, DATA, BSS, ZEROPAGE, ...).
#     cc65 interleaves segments per function; inside one segment the listing
#     is already in address order, so this IS the sorted view (don't `sort`:
#     it tears C source lines away from their code).

import re, sys

bysegment = '-s' in sys.argv[1:]
args = [a for a in sys.argv[1:] if a != '-s']
mapf, module, lst = args[0:3]

out = {}        # segment -> lines (for -s)
order = ['STARTUP', 'LOWCODE', 'ONCE', 'CODE', 'RODATA', 'DATA', 'BSS', 'ZEROPAGE']

def emit(s):
    if bysegment:
        out.setdefault(seg, []).append(s)
    else:
        print(s)

segstart = {}   # "CODE" -> absolute start
offs     = {}   # "CODE" -> module offset inside segment
cur = None

for line in open(mapf):
    # module header: "ivm.o:"  or  "sim6502.lib(write.o):"  (any, so cur resets)
    m = re.match(r'^(\S[^:]*):\s*$', line)
    if m:
        cur = m.group(1)
        continue
    m = re.match(r'^\s+(\w+)\s+Offs=([0-9A-Fa-f]+)', line)
    if m:
        if cur == module:
            offs[m.group(1)] = int(m.group(2), 16)
        continue
    # segment list: "CODE   000234  0074D3  0072A0  00001"
    m = re.match(r'^(\w+)\s+([0-9A-Fa-f]{6})\s+[0-9A-Fa-f]{6}\s+[0-9A-Fa-f]{6}\b', line)
    if m:
        segstart[m.group(1)] = int(m.group(2), 16)

if not offs:
    sys.exit(f"cafix: module {module} not found in {mapf}")

short = {'code': 'CODE', 'rodata': 'RODATA', 'data': 'DATA',
         'bss': 'BSS', 'zeropage': 'ZEROPAGE'}
seg = 'CODE'

# cc65 --add-source emits, per C statement:
#     .dbg line, "ivm-jit.c", 803
#   ;
#   ; for (k = ...)
#   ;
# Shown instead as one tagged line with blank lines around it:
#
# ; ivm-jit.c:803  for (k = ...)
#
where = None    # "file:line" from the last .dbg line
bare  = False   # previous line was a bare ";"

for line in open(lst):
    m = re.match(r'^([0-9A-Fa-f]{6})r(\s+\d+)(.*)$', line.rstrip())
    if not m:
        if not bysegment:
            print(line.rstrip())
        continue
    off, mid, src = int(m.group(1), 16), m.group(2) + ' ', m.group(3)[1:]
    text = src.strip()

    if seg in segstart and seg in offs:
        addr = f"{segstart[seg] + offs[seg] + off:04X}  "
    else:
        addr = f"{seg[:4]}+{off:04X}"           # segment not in map (e.g. empty)

    # switch segment; the directive line itself is noise
    s = re.search(r'\.segment\s+"(\w+)"', src)
    if not s:
        s = re.search(r'(?:^|\s)\.(code|rodata|data|bss|zeropage)\b', src.split(';')[0])
    if s:
        seg = s.group(1) if s.group(1).isupper() else short[s.group(1)]
        continue

    if not text:
        continue                                # empty listing lines
    d = re.search(r'\.dbg\s+line\s*,\s*"([^"]*)"\s*,\s*(\d+)', src)
    if d:
        where = f"{d.group(1)}:{d.group(2)}"
    if text.startswith('.dbg'):
        continue                                # .dbg lines are noise here
    if text == ';':
        bare = True
        continue
    if bare and where and text.startswith(';'):
        emit(f"\n; {where}  {text[1:].strip()}\n")    # flush left
        bare = False
        continue
    bare = False

    emit(f"{addr}{mid}{src}")

if bysegment:
    for name in order + sorted(k for k in out if k not in order):
        if name in out:
            print(f"\n;;; ==================== {name} ====================\n")
            print("\n".join(out[name]))
