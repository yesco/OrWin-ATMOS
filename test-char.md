/////////////////
#  why is ./ivm-asm slower than ./ivm ??????? 4x?


--- ACCEPT ---
--- BYTECODE (96 bytes) ---
Lf7 S0b '0a ? G04 I0b B09 <0b> X 
X 
Lfd S01 Lfc S02 Lfb S03 Lfa S04 Lf9 S05 
Lf8 S06 <05> . L05 E O3e L02 E L01 
P O20 L03 P O21 N 
O22 <06> P 
"" has \0
<06> (00) . O20 
"characters
\0
Lf3 
X 
X 
q 
4c f7 53 0b 27 0a 3f 47 04 49 0b 42 09 0b 58 58
4c fd 53 01 4c fc 53 02 4c fb 53 03 4c fa 53 04
4c f9 53 05 4c f8 53 06 05 2e 4c 05 45 4f 3e 4c
02 45 4c 01 50 4f 20 4c 03 50 4f 21 4e 4f 22 06
50 22 22 20 68 61 73 20 00 06 c0 2e 4f 20 22 63
68 61 72 61 63 74 65 72 73 0a 00 4c f3 58 58 71
--- BLOB (183 bytes) main=16 nvector=2 ---
--- SAVED test-char.ivm (183 bytes) ---
--- SAVED .sym ---
  frame V00 fp=7
  frame V01 fp=12
--- RUN ---
65A> Hello World!
"Guess my length!" has 16 characters
--- END acc=42 ---

=== test-char.c: 26 LOC ===
=== parse.c: 1337 LOC ===
=== test-char.c: 26 LOC ===



--- IVM BLOB dump (183 bytes) ---
0000  H  magic:      IVM 
0004  H  len:        00b7  (183)
0006  H  memsz:      00fe  (254 cells)
0008  H  constidx:   00f3
000a  H  main:       0098
000c  H  code:       0038
000e  H  call:       0016
0010  H  fp:         0036
0012  H  con:        001c
0014  H  data:       009a
-- vectors (until 0000) --
0016  V  vector00:   0038  code+0
0018  V  vector01:   0048  code+16
001a  Z  0000          (end reloc)
-- constants (11 words @ 001c) --
001c  C  constant00: 002a  slot=f3 :    42  $002a
001e  C  constant01: 0022  slot=f4 :    34  $0022
0020  C  constant02: 0021  slot=f5 :    33  $0021
0022  C  constant03: 003e  slot=f6 :    62  $003e
0024  C  constant04: 0000  slot=f7 :     0  $0000
0026  S  string05:   000c  slot=f8  "Guess my length!"
0028  C  constant06: 0041  slot=f9 :    65  $0041
002a  C  constant07: 000a  slot=fa :    10  $000a
002c  S  string08:   0006  slot=fb  "World"
002e  C  constant09: 0020  slot=fc :    32  $0020
0030  S  string10:   0000  slot=fd  "Hello"
-- reloc list (4 bytes @ 0032) --
0032  R  count:      03  (3 strings)
0033  R  idx[0]:     05  -> constant05  val=000c  "Guess my length!"
0034  R  idx[1]:     08  -> constant08  val=0006  "World"
0035  R  idx[2]:     0a  -> constant10  val=0000  "Hello"
-- frame bases --
0036  F  frame00:    07
0037  F  frame01:    0c
-- bytecode (98 bytes @ 0038) --
0038  B  +0000  4c f7 53 0b 27 0a 3f 47  |L.S.'.?G|
0040  B  +0008  04 49 0b 42 09 0b 58 58  |.I.B..XX|
0048  B  +0010  4c fd 53 01 4c fc 53 02  |L.S.L.S.|
0050  B  +0018  4c fb 53 03 4c fa 53 04  |L.S.L.S.|
0058  B  +0020  4c f9 53 05 4c f8 53 06  |L.S.L.S.|
0060  B  +0028  05 2e 4c 05 45 4f 3e 4c  |..L.EO>L|
0068  B  +0030  02 45 4c 01 50 4f 20 4c  |.EL.PO L|
0070  B  +0038  03 50 4f 21 4e 4f 22 06  |.PO!NO".|
0078  B  +0040  50 22 22 20 68 61 73 20  |P"" has |
0080  B  +0048  00 06 c0 2e 4f 20 22 63  |....O "c|
0088  B  +0050  68 61 72 61 63 74 65 72  |haracter|
0090  B  +0058  73 0a 00 4c f3 58 58 71  |s..L.XXq|
0098  B  +0060  c1 71                    |.q|
-- data (29 bytes @ 009a) --
009a  D  +0000  48 65 6c 6c 6f 00 57 6f  |Hello.Wo|
00a2  D  +0008  72 6c 64 00 47 75 65 73  |rld.Gues|
00aa  D  +0010  73 20 6d 79 20 6c 65 6e  |s my len|
00b2  D  +0018  67 74 68 21 00           |gth!.|
--- end blob ---




--- IVM BLOB dump (183 bytes) ---
0000  H  magic:      IVM 
0004  H  len:        00b7  (183)
0006  H  memsz:      00fe  (254 cells)
0008  H  constidx:   00f3
000a  H  main:       0098
000c  H  code:       0038
000e  H  call:       0016
0010  H  fp:         0036
0012  H  con:        001c
0014  H  data:       009a
-- vectors (until 0000) --
0016  V  vector00:   0038  code+0
0018  V  vector01:   0048  code+16
001a  Z  0000          (end reloc)
-- constants (11 words @ 001c) --
001c  C  constant00: 002a  slot=f3 :    42  $002a
001e  C  constant01: 0022  slot=f4 :    34  $0022
0020  C  constant02: 0021  slot=f5 :    33  $0021
0022  C  constant03: 003e  slot=f6 :    62  $003e
0024  C  constant04: 0000  slot=f7 :     0  $0000
0026  S  string05:   000c  slot=f8  "Guess my length!"
0028  C  constant06: 0041  slot=f9 :    65  $0041
002a  C  constant07: 000a  slot=fa :    10  $000a
002c  S  string08:   0006  slot=fb  "World"
002e  C  constant09: 0020  slot=fc :    32  $0020
0030  S  string10:   0000  slot=fd  "Hello"
-- reloc list (4 bytes @ 0032) --
0032  R  count:      03  (3 strings)
0033  R  idx[0]:     05  -> constant05  val=000c  "Guess my length!"
0034  R  idx[1]:     08  -> constant08  val=0006  "World"
0035  R  idx[2]:     0a  -> constant10  val=0000  "Hello"
-- frame bases --
0036  F  frame00:    07
0037  F  frame01:    0c
-- bytecode (98 bytes @ 0038) --
0038  B  +0000  4c f7 53 0b 27 0a 3f 47  |L.S.'.?G|
0040  B  +0008  04 49 0b 42 09 0b 58 58  |.I.B..XX|
0048  B  +0010  4c fd 53 01 4c fc 53 02  |L.S.L.S.|
0050  B  +0018  4c fb 53 03 4c fa 53 04  |L.S.L.S.|
0058  B  +0020  4c f9 53 05 4c f8 53 06  |L.S.L.S.|
0060  B  +0028  05 2e 4c 05 45 4f 3e 4c  |..L.EO>L|
0068  B  +0030  02 45 4c 01 50 4f 20 4c  |.EL.PO L|
0070  B  +0038  03 50 4f 21 4e 4f 22 06  |.PO!NO".|
0078  B  +0040  50 22 22 20 68 61 73 20  |P"" has |
0080  B  +0048  00 06 c0 2e 4f 20 22 63  |....O "c|
0088  B  +0050  68 61 72 61 63 74 65 72  |haracter|
0090  B  +0058  73 0a 00 4c f3 58 58 71  |s..L.XXq|
0098  B  +0060  c1 71                    |.q|
-- data (29 bytes @ 009a) --
009a  D  +0000  48 65 6c 6c 6f 00 57 6f  |Hello.Wo|
00a2  D  +0008  72 6c 64 00 47 75 65 73  |rld.Gues|
00aa  D  +0010  73 20 6d 79 20 6c 65 6e  |s my len|
00b2  D  +0018  67 74 68 21 00           |gth!.|
--- end blob ---


loaded test-char.ivm (183 bytes)
--- RUN IVM (blob 183) ---
65A> Hello World!
"Guess my length!" has 16 characters

--- END ---
acc = 42
133661 cycles


-rw-------. 1 11782 Nov  7 04:19 ivm.sim
-rw-------. 1  6771 Nov  6 02:53 ivm-noivm.sim
-rw-------. 1   183 Nov  2 02:14 test-char.ivm

=== ivm.c: 636 LOC ===

loaded test-char.ivm (183 bytes)
--- RUN IVM (blob 183) ---
65A> Hello World!
"Guess my length!" has 16 characters

--- END ---
acc = 42
424039 cycles


-rw-------. 1 9619 Oct 27 02:51 ivm-asm.sim
-rw-------. 1 6771 Oct 31 03:14 ivm-asm-noivm.sim
-rw-------. 1  183 Nov  2 02:14 test-char.ivm

=== ivm.c     : 636 LOC ===
=== ivm-vm.asm: 1381 LOC ===

loaded test-char.ivm (183 bytes)
--- RUN IVM (blob 183) ---
65A> Hello World!
"Guess my length!" has 16 characters

--- END ---
acc = 42
424039 cycles


-rw-------. 1 9619 Oct 27 02:51 ivm-asm.sim
-rw-------. 1 6771 Oct 31 03:14 ivm-asm-noivm.sim
-rw-------. 1  183 Nov  2 02:14 test-char.ivm

=== ivm.c     : 636 LOC ===
=== ivm-vm.asm: 1381 LOC ===

