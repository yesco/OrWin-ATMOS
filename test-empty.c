// cl65 -Oirs -C sim6502.cfg -t sim6502 test-empty.c -o test-empty.sim && ls -l test-empty.sim
//   226 cycles
//   191 bytes - test-empty.sim 
//
// ./oscar -Os -ep test-const.c  && ls -l test-empty.prg
//   Total Cycles 89
//   137 bytes - test-empty.prg
//
// ./pc test-empty && ./ivm-jit ... 
//    37 cycles
//    33 bytes test-emtpy.ivm.sim

int main() {
}
