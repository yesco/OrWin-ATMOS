// cpu6502.c - NMOS 6502 emulator with cycle counter, plus disassembler
//
// All 151 documented opcodes, exact cycle counts: page-crossing penalty on
// indexed reads, +1 taken branch, +1 more if it crosses a page. Decimal
// mode is not emulated (SED sets D, ADC/SBC stay binary): the JIT never
// uses it. Undocumented opcodes stop the run (cpu_step returns -1).
//
// #include it (static functions, no header). Needs uchar / word.
//
//   cpu_mem[]         the 64K address space
//   cpu_call(c, a, n) JSR a: run until it returns, or n cycles. 0 = ok
//   cpu_trap          if set: called when pc >= cpu_trap_lo; return 1 if
//                     it handled the call (cpu_call then does an RTS)
//   cpu_dis(pc, buf)  disassemble one instruction, returns its length

#ifndef CPU6502_C
#define CPU6502_C

typedef struct {
  uchar a, x, y, s, p;
  word pc;
  unsigned long cycles;
  unsigned long xpage;          // +1 cycles of taken branches crossing a page
} Cpu6502;

// per branch address: how often its taken branch crossed a page
static unsigned long cpu_xpage_at[65536];

// P flags
#define CPU_C 0x01
#define CPU_Z 0x02
#define CPU_I 0x04
#define CPU_D 0x08
#define CPU_B 0x10
#define CPU_V 0x40
#define CPU_N 0x80

static uchar cpu_mem[65536];
static int (*cpu_trap)(Cpu6502 *c);
static word cpu_trap_lo = 0xffff;

enum { IMP, ACC, IMM, ZP, ZPX, ZPY, ABS, ABX, ABY, IND, IZX, IZY, REL };

enum { M_ADC, M_AND, M_ASL, M_BCC, M_BCS, M_BEQ, M_BMI, M_BNE, M_BPL, M_BVC, M_BVS, M_BIT, M_BRK, M_CLC, M_CLD, M_CLI, M_CLV, M_CMP, M_CPX, M_CPY, M_DEC, M_DEX, M_DEY, M_EOR, M_INC, M_INX, M_INY, M_JMP, M_JSR, M_LDA, M_LDX, M_LDY, M_LSR, M_NOP, M_ORA, M_PHA, M_PHP, M_PLA, M_PLP, M_ROL, M_ROR, M_RTI, M_RTS, M_SBC, M_SEC, M_SED, M_SEI, M_STA, M_STX, M_STY, M_TAX, M_TAY, M_TSX, M_TXA, M_TXS, M_TYA, M_ILL };
static const char cpu_mnem[][4] = { "adc", "and", "asl", "bcc", "bcs", "beq", "bmi", "bne", "bpl", "bvc", "bvs", "bit", "brk", "clc", "cld", "cli", "clv", "cmp", "cpx", "cpy", "dec", "dex", "dey", "eor", "inc", "inx", "iny", "jmp", "jsr", "lda", "ldx", "ldy", "lsr", "nop", "ora", "pha", "php", "pla", "plp", "rol", "ror", "rti", "rts", "sbc", "sec", "sed", "sei", "sta", "stx", "sty", "tax", "tay", "tsx", "txa", "txs", "tya", "???" };
// per opcode: mnemonic, mode, base cycles | 0x80 = +1 on page cross
static const struct { uchar m, mode, cy; } cpu_op[256] = {
  {M_BRK,IMP,7}, {M_ORA,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 00
  {M_ILL,IMP,2}, {M_ORA,ZP,3}, {M_ASL,ZP,5}, {M_ILL,IMP,2},  // 04
  {M_PHP,IMP,3}, {M_ORA,IMM,2}, {M_ASL,ACC,2}, {M_ILL,IMP,2},  // 08
  {M_ILL,IMP,2}, {M_ORA,ABS,4}, {M_ASL,ABS,6}, {M_ILL,IMP,2},  // 0C
  {M_BPL,REL,2}, {M_ORA,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 10
  {M_ILL,IMP,2}, {M_ORA,ZPX,4}, {M_ASL,ZPX,6}, {M_ILL,IMP,2},  // 14
  {M_CLC,IMP,2}, {M_ORA,ABY,4|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 18
  {M_ILL,IMP,2}, {M_ORA,ABX,4|0x80}, {M_ASL,ABX,7}, {M_ILL,IMP,2},  // 1C
  {M_JSR,ABS,6}, {M_AND,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 20
  {M_BIT,ZP,3}, {M_AND,ZP,3}, {M_ROL,ZP,5}, {M_ILL,IMP,2},  // 24
  {M_PLP,IMP,4}, {M_AND,IMM,2}, {M_ROL,ACC,2}, {M_ILL,IMP,2},  // 28
  {M_BIT,ABS,4}, {M_AND,ABS,4}, {M_ROL,ABS,6}, {M_ILL,IMP,2},  // 2C
  {M_BMI,REL,2}, {M_AND,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 30
  {M_ILL,IMP,2}, {M_AND,ZPX,4}, {M_ROL,ZPX,6}, {M_ILL,IMP,2},  // 34
  {M_SEC,IMP,2}, {M_AND,ABY,4|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 38
  {M_ILL,IMP,2}, {M_AND,ABX,4|0x80}, {M_ROL,ABX,7}, {M_ILL,IMP,2},  // 3C
  {M_RTI,IMP,6}, {M_EOR,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 40
  {M_ILL,IMP,2}, {M_EOR,ZP,3}, {M_LSR,ZP,5}, {M_ILL,IMP,2},  // 44
  {M_PHA,IMP,3}, {M_EOR,IMM,2}, {M_LSR,ACC,2}, {M_ILL,IMP,2},  // 48
  {M_JMP,ABS,3}, {M_EOR,ABS,4}, {M_LSR,ABS,6}, {M_ILL,IMP,2},  // 4C
  {M_BVC,REL,2}, {M_EOR,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 50
  {M_ILL,IMP,2}, {M_EOR,ZPX,4}, {M_LSR,ZPX,6}, {M_ILL,IMP,2},  // 54
  {M_CLI,IMP,2}, {M_EOR,ABY,4|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 58
  {M_ILL,IMP,2}, {M_EOR,ABX,4|0x80}, {M_LSR,ABX,7}, {M_ILL,IMP,2},  // 5C
  {M_RTS,IMP,6}, {M_ADC,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 60
  {M_ILL,IMP,2}, {M_ADC,ZP,3}, {M_ROR,ZP,5}, {M_ILL,IMP,2},  // 64
  {M_PLA,IMP,4}, {M_ADC,IMM,2}, {M_ROR,ACC,2}, {M_ILL,IMP,2},  // 68
  {M_JMP,IND,5}, {M_ADC,ABS,4}, {M_ROR,ABS,6}, {M_ILL,IMP,2},  // 6C
  {M_BVS,REL,2}, {M_ADC,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 70
  {M_ILL,IMP,2}, {M_ADC,ZPX,4}, {M_ROR,ZPX,6}, {M_ILL,IMP,2},  // 74
  {M_SEI,IMP,2}, {M_ADC,ABY,4|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 78
  {M_ILL,IMP,2}, {M_ADC,ABX,4|0x80}, {M_ROR,ABX,7}, {M_ILL,IMP,2},  // 7C
  {M_ILL,IMP,2}, {M_STA,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 80
  {M_STY,ZP,3}, {M_STA,ZP,3}, {M_STX,ZP,3}, {M_ILL,IMP,2},  // 84
  {M_DEY,IMP,2}, {M_ILL,IMP,2}, {M_TXA,IMP,2}, {M_ILL,IMP,2},  // 88
  {M_STY,ABS,4}, {M_STA,ABS,4}, {M_STX,ABS,4}, {M_ILL,IMP,2},  // 8C
  {M_BCC,REL,2}, {M_STA,IZY,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 90
  {M_STY,ZPX,4}, {M_STA,ZPX,4}, {M_STX,ZPY,4}, {M_ILL,IMP,2},  // 94
  {M_TYA,IMP,2}, {M_STA,ABY,5}, {M_TXS,IMP,2}, {M_ILL,IMP,2},  // 98
  {M_ILL,IMP,2}, {M_STA,ABX,5}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // 9C
  {M_LDY,IMM,2}, {M_LDA,IZX,6}, {M_LDX,IMM,2}, {M_ILL,IMP,2},  // A0
  {M_LDY,ZP,3}, {M_LDA,ZP,3}, {M_LDX,ZP,3}, {M_ILL,IMP,2},  // A4
  {M_TAY,IMP,2}, {M_LDA,IMM,2}, {M_TAX,IMP,2}, {M_ILL,IMP,2},  // A8
  {M_LDY,ABS,4}, {M_LDA,ABS,4}, {M_LDX,ABS,4}, {M_ILL,IMP,2},  // AC
  {M_BCS,REL,2}, {M_LDA,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // B0
  {M_LDY,ZPX,4}, {M_LDA,ZPX,4}, {M_LDX,ZPY,4}, {M_ILL,IMP,2},  // B4
  {M_CLV,IMP,2}, {M_LDA,ABY,4|0x80}, {M_TSX,IMP,2}, {M_ILL,IMP,2},  // B8
  {M_LDY,ABX,4|0x80}, {M_LDA,ABX,4|0x80}, {M_LDX,ABY,4|0x80}, {M_ILL,IMP,2},  // BC
  {M_CPY,IMM,2}, {M_CMP,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // C0
  {M_CPY,ZP,3}, {M_CMP,ZP,3}, {M_DEC,ZP,5}, {M_ILL,IMP,2},  // C4
  {M_INY,IMP,2}, {M_CMP,IMM,2}, {M_DEX,IMP,2}, {M_ILL,IMP,2},  // C8
  {M_CPY,ABS,4}, {M_CMP,ABS,4}, {M_DEC,ABS,6}, {M_ILL,IMP,2},  // CC
  {M_BNE,REL,2}, {M_CMP,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // D0
  {M_ILL,IMP,2}, {M_CMP,ZPX,4}, {M_DEC,ZPX,6}, {M_ILL,IMP,2},  // D4
  {M_CLD,IMP,2}, {M_CMP,ABY,4|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // D8
  {M_ILL,IMP,2}, {M_CMP,ABX,4|0x80}, {M_DEC,ABX,7}, {M_ILL,IMP,2},  // DC
  {M_CPX,IMM,2}, {M_SBC,IZX,6}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // E0
  {M_CPX,ZP,3}, {M_SBC,ZP,3}, {M_INC,ZP,5}, {M_ILL,IMP,2},  // E4
  {M_INX,IMP,2}, {M_SBC,IMM,2}, {M_NOP,IMP,2}, {M_ILL,IMP,2},  // E8
  {M_CPX,ABS,4}, {M_SBC,ABS,4}, {M_INC,ABS,6}, {M_ILL,IMP,2},  // EC
  {M_BEQ,REL,2}, {M_SBC,IZY,5|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // F0
  {M_ILL,IMP,2}, {M_SBC,ZPX,4}, {M_INC,ZPX,6}, {M_ILL,IMP,2},  // F4
  {M_SED,IMP,2}, {M_SBC,ABY,4|0x80}, {M_ILL,IMP,2}, {M_ILL,IMP,2},  // F8
  {M_ILL,IMP,2}, {M_SBC,ABX,4|0x80}, {M_INC,ABX,7}, {M_ILL,IMP,2},  // FC
};

static const uchar cpu_len[] = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2 };

#define RD(a)    cpu_mem[(word)(a)]
#define RD16(a)  (cpu_mem[(word)(a)] | (cpu_mem[(word)((a) + 1)] << 8))
// zero page pointer: the hi byte wraps inside page 0
#define RDZ16(a) (cpu_mem[(uchar)(a)] | (cpu_mem[(uchar)((a) + 1)] << 8))

static void cpu_nz(Cpu6502 *c, uchar v) {
  c->p = (uchar)((c->p & ~(CPU_N | CPU_Z)) | (v & CPU_N) | (v ? 0 : CPU_Z));
}
static void cpu_push(Cpu6502 *c, uchar v) { cpu_mem[0x100 + c->s--] = v; }
static uchar cpu_pull(Cpu6502 *c) { return cpu_mem[0x100 + ++c->s]; }

static void cpu_cmp(Cpu6502 *c, uchar r, uchar v) {
  c->p = (uchar)((c->p & ~CPU_C) | (r >= v ? CPU_C : 0));
  cpu_nz(c, (uchar)(r - v));
}

static void cpu_adc(Cpu6502 *c, uchar v) {
  unsigned r = c->a + v + (c->p & CPU_C);
  c->p &= (uchar)~(CPU_C | CPU_V);
  if (r > 0xff) c->p |= CPU_C;
  if (~(c->a ^ v) & (c->a ^ r) & 0x80) c->p |= CPU_V;
  c->a = (uchar)r;
  cpu_nz(c, c->a);
}

// one instruction. 0 = ok, -1 = undocumented opcode
static int cpu_step(Cpu6502 *c) {
  word pc = c->pc, ea = 0, t;
  uchar op = RD(pc), b1 = RD(pc + 1), v, cy;
  uchar mode = cpu_op[op].mode, m = cpu_op[op].m;

  if (m == M_ILL) return -1;
  cy = cpu_op[op].cy & 0x7f;

  // effective address (+1 cycle on page cross where the table says so)
  switch (mode) {
  case ZP:  ea = b1; break;
  case ZPX: ea = (uchar)(b1 + c->x); break;
  case ZPY: ea = (uchar)(b1 + c->y); break;
  case ABS: ea = (word)RD16(pc + 1); break;
  case ABX: t = (word)RD16(pc + 1); ea = (word)(t + c->x);
            if ((cpu_op[op].cy & 0x80) && (t ^ ea) >> 8) cy++;
            break;
  case ABY: t = (word)RD16(pc + 1); ea = (word)(t + c->y);
            if ((cpu_op[op].cy & 0x80) && (t ^ ea) >> 8) cy++;
            break;
  case IND: t = (word)RD16(pc + 1);     // NMOS bug: hi byte from same page
            ea = (word)(RD(t) | (RD((t & 0xff00) | ((t + 1) & 0xff)) << 8)); break;
  case IZX: ea = (word)RDZ16((uchar)(b1 + c->x)); break;
  case IZY: t = (word)RDZ16(b1); ea = (word)(t + c->y);
            if ((cpu_op[op].cy & 0x80) && (t ^ ea) >> 8) cy++;
            break;
  }
  c->pc = (word)(pc + cpu_len[mode]);
  v = (mode == IMM) ? b1 : (mode == ACC) ? c->a : RD(ea);

  switch (m) {
  case M_LDA: c->a = v; cpu_nz(c, v); break;
  case M_LDX: c->x = v; cpu_nz(c, v); break;
  case M_LDY: c->y = v; cpu_nz(c, v); break;
  case M_STA: cpu_mem[ea] = c->a; break;
  case M_STX: cpu_mem[ea] = c->x; break;
  case M_STY: cpu_mem[ea] = c->y; break;
  case M_TAX: c->x = c->a; cpu_nz(c, c->x); break;
  case M_TAY: c->y = c->a; cpu_nz(c, c->y); break;
  case M_TXA: c->a = c->x; cpu_nz(c, c->a); break;
  case M_TYA: c->a = c->y; cpu_nz(c, c->a); break;
  case M_TSX: c->x = c->s; cpu_nz(c, c->x); break;
  case M_TXS: c->s = c->x; break;
  case M_ADC: cpu_adc(c, v); break;
  case M_SBC: cpu_adc(c, (uchar)~v); break;
  case M_AND: c->a &= v; cpu_nz(c, c->a); break;
  case M_ORA: c->a |= v; cpu_nz(c, c->a); break;
  case M_EOR: c->a ^= v; cpu_nz(c, c->a); break;
  case M_CMP: cpu_cmp(c, c->a, v); break;
  case M_CPX: cpu_cmp(c, c->x, v); break;
  case M_CPY: cpu_cmp(c, c->y, v); break;
  case M_BIT:
    c->p = (uchar)((c->p & ~(CPU_N | CPU_V | CPU_Z)) | (v & (CPU_N | CPU_V)) |
                   ((c->a & v) ? 0 : CPU_Z));
    break;
  case M_INC: case M_DEC:
    v = (uchar)(m == M_INC ? v + 1 : v - 1); cpu_mem[ea] = v; cpu_nz(c, v); break;
  case M_INX: cpu_nz(c, ++c->x); break;
  case M_INY: cpu_nz(c, ++c->y); break;
  case M_DEX: cpu_nz(c, --c->x); break;
  case M_DEY: cpu_nz(c, --c->y); break;
  case M_ASL: case M_LSR: case M_ROL: case M_ROR: {
    uchar ci = c->p & CPU_C, co;
    if (m == M_ASL || m == M_ROL) { co = v >> 7; v = (uchar)((v << 1) | (m == M_ROL ? ci : 0)); }
    else { co = v & 1; v = (uchar)((v >> 1) | (m == M_ROR && ci ? 0x80 : 0)); }
    c->p = (uchar)((c->p & ~CPU_C) | co);
    cpu_nz(c, v);
    if (mode == ACC) c->a = v; else cpu_mem[ea] = v;
    break;
  }
  case M_BCC: case M_BCS: case M_BEQ: case M_BNE:
  case M_BMI: case M_BPL: case M_BVC: case M_BVS: {
    static const uchar flag[] = { CPU_C, CPU_Z, CPU_N, CPU_V };
    uchar f, want, taken;
    switch (m) {
    case M_BCC: f = 0; want = 0; break;  case M_BCS: f = 0; want = 1; break;
    case M_BNE: f = 1; want = 0; break;  case M_BEQ: f = 1; want = 1; break;
    case M_BPL: f = 2; want = 0; break;  case M_BMI: f = 2; want = 1; break;
    case M_BVC: f = 3; want = 0; break;  default:    f = 3; want = 1; break;
    }
    taken = (uchar)(((c->p & flag[f]) != 0) == want);
    if (taken) {
      t = (word)(c->pc + (signed char)b1);
      cy += 1;
      if ((t ^ c->pc) >> 8) {           // page cross: layout dependent
        cy++; c->xpage++;
        cpu_xpage_at[(word)(c->pc - 2)]++;
      }
      c->pc = t;
    }
    break;
  }
  case M_JMP: c->pc = ea; break;
  case M_JSR:
    t = (word)(c->pc - 1);
    cpu_push(c, (uchar)(t >> 8)); cpu_push(c, (uchar)t);
    c->pc = ea;
    break;
  case M_RTS: t = cpu_pull(c); t |= (word)(cpu_pull(c) << 8); c->pc = (word)(t + 1); break;
  case M_RTI:
    c->p = (uchar)((cpu_pull(c) & ~CPU_B) | 0x20);
    t = cpu_pull(c); t |= (word)(cpu_pull(c) << 8); c->pc = t;
    break;
  case M_BRK:
    t = (word)(pc + 2);
    cpu_push(c, (uchar)(t >> 8)); cpu_push(c, (uchar)t);
    cpu_push(c, (uchar)(c->p | CPU_B | 0x20));
    c->p |= CPU_I;
    c->pc = (word)RD16(0xfffe);
    break;
  case M_PHA: cpu_push(c, c->a); break;
  case M_PHP: cpu_push(c, (uchar)(c->p | CPU_B | 0x20)); break;
  case M_PLA: c->a = cpu_pull(c); cpu_nz(c, c->a); break;
  case M_PLP: c->p = (uchar)((cpu_pull(c) & ~CPU_B) | 0x20); break;
  case M_CLC: c->p &= (uchar)~CPU_C; break;
  case M_SEC: c->p |= CPU_C; break;
  case M_CLI: c->p &= (uchar)~CPU_I; break;
  case M_SEI: c->p |= CPU_I; break;
  case M_CLV: c->p &= (uchar)~CPU_V; break;
  case M_CLD: c->p &= (uchar)~CPU_D; break;
  case M_SED: c->p |= CPU_D; break;
  case M_NOP: break;
  }
  c->cycles += cy;
  return 0;
}

// JSR addr and run until it returns (RTS to the pushed sentinel), or
// max_cycles more cycles have run (0 = no limit).
// 0 = returned, 1 = cycle limit, -1 = undocumented opcode at c->pc.
#define CPU_SENTINEL 0xfff0
static int cpu_call(Cpu6502 *c, word addr, unsigned long max_cycles) {
  unsigned long stop = c->cycles + max_cycles;

  cpu_push(c, (uchar)((CPU_SENTINEL - 1) >> 8));
  cpu_push(c, (uchar)(CPU_SENTINEL - 1));
  c->pc = addr;
  while (c->pc != CPU_SENTINEL) {
    if (cpu_trap && c->pc >= cpu_trap_lo && cpu_trap(c)) {
      // handled in C: return from it like an RTS
      word t = cpu_pull(c);
      t |= (word)(cpu_pull(c) << 8);
      c->pc = (word)(t + 1);
      continue;
    }
    if (cpu_step(c)) return -1;
    if (max_cycles && c->cycles >= stop) return 1;
  }
  return 0;
}

// one instruction at pc into buf ("lda $12,x"), returns its length
static int cpu_dis(word pc, char *buf) {
  uchar op = RD(pc), b1 = RD(pc + 1), mode = cpu_op[op].mode;
  word ab = (word)RD16(pc + 1);
  const char *mn = cpu_mnem[cpu_op[op].m];

  switch (mode) {
  case IMP: sprintf(buf, "%s", mn); break;
  case ACC: sprintf(buf, "%s a", mn); break;
  case IMM: sprintf(buf, "%s #$%02x", mn, b1); break;
  case ZP:  sprintf(buf, "%s $%02x", mn, b1); break;
  case ZPX: sprintf(buf, "%s $%02x,x", mn, b1); break;
  case ZPY: sprintf(buf, "%s $%02x,y", mn, b1); break;
  case ABS: sprintf(buf, "%s $%04x", mn, ab); break;
  case ABX: sprintf(buf, "%s $%04x,x", mn, ab); break;
  case ABY: sprintf(buf, "%s $%04x,y", mn, ab); break;
  case IND: sprintf(buf, "%s ($%04x)", mn, ab); break;
  case IZX: sprintf(buf, "%s ($%02x,x)", mn, b1); break;
  case IZY: sprintf(buf, "%s ($%02x),y", mn, b1); break;
  case REL: sprintf(buf, "%s %+d\t=> $%04X", mn, (signed char)b1,
                    (unsigned)(word)(pc + 2 + (signed char)b1)); break;
  }
  if (cpu_op[op].m == M_ILL) sprintf(buf, ".byte $%02x", op);
  return cpu_len[mode];
}

#undef RD
#undef RD16
#undef RDZ16

#endif // CPU6502_C
