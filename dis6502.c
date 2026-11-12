// A simple switch/case test for parse.c compiler (Gemini)

// Used for benchmarking switch/case optimizations of

char* code= (char*)0x400;

int disasm() {
  int sum = 0, pc = 0, len, mode;
  char op;

  while (pc < 256) {
    op = code[pc];
    switch (op & 0x1F) { // 32 perfectly dense cases: 0..31!
    case 0x00: case 0x02: case 0x09: case 0x0B: case 0x10:
    case 0x12: case 0x19: case 0x1B:
      len = 2; mode = 1; break; // #imm, rel, (zp),Y, abs,Y (2B)
    case 0x01: case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x11: case 0x14: case 0x15: case 0x16: case 0x17:
      len = 2; mode = 2; break; // zp, zp,X, (zp,X)
    case 0x0C: case 0x0D: case 0x0E: case 0x0F:
    case 0x1C: case 0x1D: case 0x1E: case 0x1F:
      len = 3; mode = 3; break; // abs, abs,X
    case 0x08: case 0x0A: case 0x18: case 0x1A:
      len = 1; mode = 0; break; // implied / accumulator
    default:
      len = 1; mode = 4; break; // JAM / unused
    }
    // Special 6502 exceptions (dense 0x00..0x60 step 0x20, or flat):
    switch (op) {
    case 0x00: case 0x40: case 0x60: len = 1; mode = 0; break; // BRK, RTI, RTS
    case 0x20: case 0x4C: case 0x6C: len = 3; mode = 3; break; // JSR, JMP
    }
    sum = (sum + op + (mode << 4) + len) & 0x7FFF;
    pc += len;
  }
  return sum;
}
