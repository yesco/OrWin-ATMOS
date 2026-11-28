# List of INNOVIATIONS in the OrWin-ATMOS project

- generic "any language" BNF-interpreter to write a minimal C compiler
- byte code output very compact (better than oscar64)
- bytecode to JIT for 6502 "smaller code than oscar64"
- bytecode is an accumulator register VM, collapsing constants/var/stack to 256 slots
- call-graph analysis makes a static optimal stack
- TODO.md: efficent 6502 relocation offsets without addition (-7 cycles/fix)
- oaq.c - new varint method, efficient on 6502, no shifting! and savs bytes!
- petitable: mini bigtable index replacing disk filesystem for 6502

