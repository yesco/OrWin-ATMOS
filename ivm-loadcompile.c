// LOADCOMPILE — experimental load-time 6502 emitter
// switch(bytecode) → emit 6502 bytes into out[]. No intermediate ISA.
//
// #define LOADCOMPILE 1
//
// Convention for defines: base mnemonic + suffix
//   (none)  absolute     LDA  abs
//   N       immediate    LDAN #imm
//   Z       zero page    LDAZ zp
//   X       abs,X        LDAX abs,X
//   Y       abs,Y        LDAY abs,Y
//   IX      (ind,X)      LDAIX (zp,X)
//   IY      (ind),Y      LDAIY (zp),Y
//
// ACC is kept in A/Y as lo/hi (16-bit). X free for temps.
// mem[] is a table of 16-bit cells at IVM_MEM (lo at +0, hi at +1 per cell).
// Caller sets: out buffer, IVM_MEM base, zp.fp handling later.

#ifdef LOADCOMPILE

// ---- 6502 opcode bytes ----
#define BRK   0x00
#define ORAIX 0x01
#define ORAZ  0x05
#define ASLZ  0x06
#define PHP   0x08
#define ORAN  0x09
#define ASL   0x0A
#define ORA   0x0D
#define BPL   0x10
#define ORAIY 0x11
#define ORAX  0x1D
#define JSR   0x20
#define ANDIX 0x21
#define BITZ  0x24
#define ANDZ  0x25
#define ROLZ  0x26
#define PLP   0x28
#define ANDN  0x29
#define ROL   0x2A
#define BIT_  0x2C
#define AND_  0x2D
#define BMI   0x30
#define ANDIY 0x31
#define ANDX  0x3D
#define RTI   0x40
#define EORIX 0x41
#define EORZ  0x45
#define LSRZ  0x46
#define PHA   0x48
#define EORN  0x49
#define LSR   0x4A
#define JMP   0x4C
#define EOR   0x4D
#define BVC   0x50
#define EORIY 0x51
#define EORX  0x5D
#define RTS   0x60
#define ADCIX 0x61
#define ADCZ  0x65
#define RORZ  0x66
#define PLA   0x68
#define ADCN  0x69
#define ROR   0x6A
#define JMPI  0x6C
#define ADC   0x6D
#define BVS   0x70
#define ADCIY 0x71
#define ADCX  0x7D
#define STAIX 0x81
#define STYZ  0x84  // duplicate name w 0x94
#define STAZ  0x85
#define STXZ  0x86
#define DEY   0x88
#define TXA   0x8A
#define STY   0x8C
#define STA   0x8D
#define STX   0x8E
#define BCC   0x90
#define STAIY 0x91
#define STYZ  0x94 // this one
#define STAZX 0x95
#define STXZY 0x96
#define TYA   0x98
#define STAX  0x9D
#define STAY  0x99
#define LDYI  0xA0
#define LDAIX 0xA1
#define LDXN  0xA2
#define LDYZ  0xA4
#define LDAZ  0xA5
#define LDXZ  0xA6
#define TAY   0xA8
#define LDAN  0xA9
#define TAX   0xAA
#define LDY   0xAC
#define LDA   0xAD
#define LDX   0xAE
#define BCS   0xB0
#define LDAIY 0xB1  /// index is IY
#define LDYZX 0xB4
#define LDAZX 0xB5
#define LDXZY 0xB6
#define CLV   0xB8
#define LDAX  0xBD
#define LDAX  0xBD
#define LDAIY 0xB1  /// LDAYI is Y indirect (is it this one or B1?)
#define CPYN  0xC0
#define CMPIX 0xC1
#define CPYZ  0xC4
#define CMPZ  0xC5
#define DECZ  0xC6
#define INY   0xC8
#define CMPN  0xC9
#define DEX   0xCA
#define CPY   0xCC
#define CMP   0xCD
#define DEC_  0xCE
#define BNE   0xD0
#define CMPIY 0xD1
#define CMPX  0xDD
#define CPXN  0xE0
#define SBCIX 0xE1
#define CPXZ  0xE4
#define SBCZ  0xE5
#define INCZ  0xE6
#define INX   0xE8
#define SBCN  0xE9
#define NOP   0xEA
#define CPX   0xEC
#define SBC   0xED
#define INC_  0xEE
#define BEQ   0xF0
#define SBCIY 0xF1
#define SBCX  0xFD
#define CLC   0x18
#define SEC   0x38
#define CLI   0x58
#define SEI   0x78
#define CLD   0xD8
#define SED   0xF8
#define TXS   0x9A
#define TSX   0xBA

// ---- emitter state ----
static uchar *lc_out;
static word   lc_at;
static word   lc_max;
static word  *lc_map;     // map[bc_pc] = out offset
static word   lc_mapn;

typedef struct { word at; word bc_target; uchar rel; } LcFix;
static LcFix  lc_fix[256];
static word   lc_nfix;

static void em(uchar b) {
  if (lc_at < lc_max) lc_out[lc_at] = b;
  lc_at++;
}
static void em2(uchar a, uchar b) { em(a); em(b); }
static void em3(uchar a, uchar b, uchar c) { em(a); em(b); em(c); }
static void ema(uchar op, word addr) {
  // TDOO: could be clever and emit ZP variants...
  em(op);
  em((uchar)(addr & 0xff));
  em((uchar)(addr >> 8));
}

// 16-bit cell address for slot s: IVM_MEM + 2*s
// Host passes mem_base; on 6502 this is the real mem table org.
static word lc_mem;
static word slotlo(uchar s) { return (word)(lc_mem + ((word)s << 1)); }
static word slothi(uchar s) { return (word)(slotlo(s) + 1); }

// ACC in A=lo, Y=hi
static void emit_load_slot(uchar s) {
  ema(LDA, slotlo(s));
  ema(LDY, slothi(s));
}
static void emit_store_slot(uchar s) {
  ema(STA, slotlo(s));
  ema(STY, slothi(s));
}
static void emit_load0(void) {
  em2(LDAN, 0);
  em(TAY);
}
static void emit_load_ffff(void) {
  em2(LDAN, 0xff);
  em(TAY);
}

static void emit_lohiop(uchar ins, word s) {
            ema(ins, slotlo(s));  em(TAY); // lo
  em(TXA);  ema(ins, slothi(s));  em(TAX); // hi
  em(TYA);  // lo back in A
}

// acc += mem[s]  (16-bit)
static void emit_add_slot(uchar s) {
}
static void emit_sub_slot(uchar s) {
  em(SEC);  emit_lohiop(SBC);
}

// ---- compiler ----
// out[0..out_max): 6502 code
// map[0..code_len): bc_pc -> code offset (0xffff = none)
// returns bytes emitted, or 0 on fail
word ivm_loadcompile(const uchar *code, word code_len,
                     uchar *out, word out_max,
                     word *map, word map_n,
                     word mem_base) {
  word pc;

  if (!code || !out || !map || code_len > map_n) return 0;

  lc_out = out;
  lc_at = 0;
  lc_max = out_max;
  lc_map = map;
  lc_mapn = map_n;
  lc_mem = mem_base;
  lc_nfix = 0;

  {
    word i;

    for (i = 0; i < code_len; i++)
      map[i] = 0xffff;
  }

  for (pc = 0; pc < code_len; ) {
    uchar op;

    if (pc < lc_mapn)
      lc_map[pc] = lc_at;

    op = code[pc];

    // 0..31: fast load mem[op]
    if (op < 32) {
      emit_load_slot(op);
      pc++;
      continue;
    }

    // JR and vector
    if (op & 0x80) {
      if (op >= 0xC0) {
        // call vector — JSR abs placeholder (patch later if needed)
        ema(JSR, 0); // TODO: vector table
        pc++;
        continue;
      }
      emit_load_slot((uchar)(op & 0x7f));
      pc++;
      continue;
    }

    // make sure have parameter byte
    // - 1 extra byte
    if (strchr("LSZ+-OGB", op) != NULL && pc+1 >= code_len) {
      len= 2;
      // TODO: why really care? except get exception or read wrong byte?
      printf("%%Compilation: op $%02x '%c' missing parameter (at end)\n",
        op, op);
      return 0;
    }
      
    #define PAR (code[pc + 1])
    
    // All other ops
    switch (op) {

    case 'q':
      em(RTS);
      pc++;
      break;

    case 'X':
    case ';':
    case 'Q':
      if (op == 'Q') emit_load0();
      em(RTS);
      pc++;
      break;

    case '0':
      emit_load0();
      pc++;
      break;

    case '9':
      emit_load_ffff();
      pc++;
      break;

    case 'L':
      emit_load_slot(PAR);
      pc += 2;
      break;

    case 'S':
      emit_store_slot(PAR);
      pc += 2;
      break;

    case 'Z':
      em2(LDAN, 0);  ema(STA, slotlo(PAR));  ema(STA, slothi(PAR));
      pc += 2;
      break;

    case '+': em(CLC);  emit_lohiop(ADC, PAR); pc += 2; break;
    case '-': em(SEC);  emit_lohiop(SBC, PAR); pc += 2; break;

    case 'N': ema(JSR, nl); pc++; break;
    case '.': ema(JSR, opputnum); pc++; break;
    case 'O': em2(LDAN, PAR);  ema(JSR, putchar); pc += 2; break;

      // acc = !acc  (1 if A|Y==0 else 0)
    case '?': em(TAY); em2(BNE, +3); em(TXA); em2(BEQ, +5);
      em2(LDAN, 0); em(TAX); em2(BEQ, +4);
      em2(LDAN, 1); em2(LDX, 0);
      pc++; break;

    case 'G': // conditioanl relative forward jump if acc==0
      {
        word dest = (word)(pc + 2 + code[pc + 1]);
        word at;
        // TODO: use relative BEQ

/// NONONO        ema(JMP, 0); // patch abs
        at = (word)(lc_at - 2); // point at address bytes
        if (lc_nfix < 256) {
          lc_fix[lc_nfix].at = at;
          lc_fix[lc_nfix].bc_target = dest;
          lc_fix[lc_nfix].rel = 0;
          lc_nfix++;
        }

        pc += 2;
      }
      break;

    case 'B': 
      /// TODO: NONONONO that's 'G" why you get it wrong?
      // B is not conditional
      {
        word dest = (word)(pc + 2 + code[pc + 1]);
        word at;
        // TYA; ORA saved-A → BEQ
        em(TAX);
        em(TYA);
        em2(STXZ, 0xfe); // zp temp $fe — machine specific
        em2(ORAZ, 0xfe);
        em2(BEQ, 0);     // patch rel
        at = (word)(lc_at - 1);
        if (lc_nfix < 256) {
          lc_fix[lc_nfix].at = at;
          lc_fix[lc_nfix].bc_target = dest;
          lc_fix[lc_nfix].rel = 1;
          lc_nfix++;
        }
        em(TXA); // restore A if not taken
        pc += 2;
      }
      break;

    case '"': {
      word s = pc + 1;

      ema(JSR, printinlinedstring);
      while (s < code_len && code[s] != 0) s++;
      if (s < code_len) s++;
      // TODO: else give error?

      pc = s;
      break;
    }

    case ' ': pc++; break;

    // unknown — BRK for visibility
    default:  em(BRK); pc++; break;

    }

  }

  // patch jumps
  {
    word i;

    for (i = 0; i < lc_nfix; i++) {
      word dest = lc_fix[i].bc_target;
      word mapped = 0xffff;
      word d;

      if (dest < lc_mapn)
        mapped = lc_map[dest];

      if (mapped == 0xffff) {
        for (d = dest; d < code_len && d < lc_mapn; d++)
          if (lc_map[d] != 0xffff) { mapped = lc_map[d]; break; }
      }
      
      // TODO: what is this error condition? make a printf!
      if (mapped == 0xffff)
        return 0;

      if (lc_fix[i].rel) {

        // BEQ rel8: offset from instruction after the rel byte
        int rel = (int)mapped - (int)(lc_fix[i].at + 1);

        if (rel < -128 || rel > 127) {
          // too far — should rewrite as JMP; fail for now
          printf("%%PATXCH JUMPS: failed too far %d\n", rel);
          return 0;
        }

        if (lc_fix[i].at < lc_max)
          lc_out[lc_fix[i].at] = (uchar)rel;

      } else {

        // JMP abs: at points at lo byte of address
        if (lc_fix[i].at + 1 < lc_max) {
          // mapped is offset in out[]; real address = out_org + mapped
          // store offset; loader adds org, or we emit org-relative
          lc_out[lc_fix[i].at]     = (uchar)(mapped & 0xff);
          lc_out[lc_fix[i].at + 1] = (uchar)(mapped >> 8);
        }
      }
    }
  }

  return lc_at;
}

#endif // LOADCOMPILE
