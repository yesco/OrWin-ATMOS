// JIT — load-time 6502 emitter
//
// switch(bytecode) → 6502 bytes into out[]. No intermediate ISA.
//
// ACC: A=lo X=hi (cc65 style). Y is temp.
// Pointer walks with op=*p++; PAR advances. No pc++ / pc+=2.
// JSR helpers share symbols with ivm-vm.asm / C.
//
// Opcode naming: base mnemonic + suffix
//   (none) abs   N imm   Z zp   X abs,X   Y abs,Y   A accumulator
//   ZX zp,X   ZY zp,Y   IX (zp,X)   IY (zp),Y   I (abs) indirect
//   Exception: the plain abs forms of AND/INC/DEC are AND_ INC_ DEC_
//
// #define JIT 1
#define JIT_TRACE 1   // per-op trace + disassembly; comment out for quiet


/*

INSTRUCTIONS: Keeps this section but keep updated for status

### Status

| Feature                  | Status |
|--------------------------|--------|
| Calls (vector table)     | **Done** — JMP + word table at start, JSR abs |
| Call frame               | **Done** — fixed addresses from frame_base, AX stored at fp+FR_ACC on entry |
| Compares `<` `=` `>`     | **Done** — boolean in ACC, UNSIGNED (needs testing) |
| Loops `i` / `n` / `G` / `B` | **Present** — i/n signed via N flag (+-16K), needs testing |
| `U`                      | Present |
| `j` (down-loop)          | **Missing** — warns only |
| Array `(` `)`            | Present (needs testing) |
| / *                      | JSR helpers; still need **PARAX → zp_par** |
| ZP optimisation          | **Done, one place** — emop2() + zpmap[] (see below) |
| Branch emit              | **Done** — emrel/empatch (local), emjump (bytecode target) |

### Zero page

No lc_zp flag any more. Every abs-form instruction goes through
emop2(op, addr): if addr < 256 and zpmap[op] != 0 the zero-page opcode
is emitted (2 bytes), else the absolute form (3 bytes).  zpmap[] is a
256 slot table indexed by the abs opcode, filled in zpmap_init().
Only plain abs → zp is mapped; abs,X / abs,Y are NOT (zp,X wraps at
$FF, abs,X does not).

### Branches

Sizes vary (zp/abs), so never hard-code a skip distance across an
emop2()/emit_alub() instruction.

  fix = emrel(BNE); ...variable size code...; empatch(fix);
      Local forward skip inside one op's template. Short branch,
      range checked (lc_err if > 127; cannot happen for templates).

  emjump(BNE, bc_target);  emjump(JMP, bc_target);
      Jump to a BYTECODE pc.
      - target already emitted (backward, i.e. loop close): exact
        distance known → short branch if it fits, else reversed
        branch + JMP abs. Unconditional is JMP abs. No fixup.
      - target not emitted yet (forward): long form (reversed branch
        over a JMP abs), absolute operand patched at the end (lc_fix).
      Future -O: forward short form + "this one failed" bitmap keyed
      by bytecode pc, re-run the whole pass until no new failures.

### Call vectors

Simplyfy function calling code:
- WHOLE code should start with NOP till you get odd address
- emit a JMP (opocode, single byte)
- now mark this location as subroutine addresses
- each 2 bytes is a jump vector directly to each funciton
- this is patched as we compie them in order, except the 0th which
  is the "main"
- for now, ingore the Pascall assign return value, but the slot is
  still there
- we don't need the fp, no need to save/restore, as we transpilie the
  code knowing the fp already!
- first in funciton: store AX to it's accumulator (fp+ accpos), fixed addresss
- codes '1'..'7' can be resolved directly,to addresses,
  as is '8' to first arg location
- function use JSR (mark+2*vector)

- implement '<' '=' and '>' please. '=' is trivial. '<' and '>' is just
  subtraction and test not ==

*/

#ifdef JIT

#include <stdio.h>
#include <string.h>
#include <ctype.h>

// INSTRUCTION: Keep all OPS defined on 6502 here
//   Don't randomly remove because not used "today"
//   Example: DEX you removed, why?
// Complete documented NMOS 6502 set. No raw opcode bytes in the code below.

// ADC
#define ADCN  0x69
#define ADCZ  0x65
#define ADCZX 0x75
#define ADC   0x6D
#define ADCX  0x7D
#define ADCY  0x79
#define ADCIX 0x61
#define ADCIY 0x71
// AND
#define ANDN  0x29
#define ANDZ  0x25
#define ANDZX 0x35
#define AND_  0x2D
#define ANDX  0x3D
#define ANDY  0x39
#define ANDIX 0x21
#define ANDIY 0x31
// ASL
#define ASLA  0x0A
#define ASLZ  0x06
#define ASLZX 0x16
#define ASL   0x0E
#define ASLX  0x1E
// branches
#define BCC   0x90
#define BCS   0xB0
#define BEQ   0xF0
#define BMI   0x30
#define BNE   0xD0
#define BPL   0x10
#define BVC   0x50
#define BVS   0x70
// BIT
#define BITZ  0x24
#define BIT   0x2C
// BRK
#define BRK   0x00
// flags
#define CLC   0x18
#define CLD   0xD8
#define CLI   0x58
#define CLV   0xB8
#define SEC   0x38
#define SED   0xF8
#define SEI   0x78
// CMP
#define CMPN  0xC9
#define CMPZ  0xC5
#define CMPZX 0xD5
#define CMP   0xCD
#define CMPX  0xDD
#define CMPY  0xD9
#define CMPIX 0xC1
#define CMPIY 0xD1
// CPX / CPY
#define CPXN  0xE0
#define CPXZ  0xE4
#define CPX   0xEC
#define CPYN  0xC0
#define CPYZ  0xC4
#define CPY   0xCC
// DEC / DEX / DEY
#define DECZ  0xC6
#define DECZX 0xD6
#define DEC_  0xCE
#define DEC_X 0xDE
#define DEX   0xCA
#define DEY   0x88
// EOR
#define EORN  0x49
#define EORZ  0x45
#define EORZX 0x55
#define EOR   0x4D
#define EORX  0x5D
#define EORY  0x59
#define EORIX 0x41
#define EORIY 0x51
// INC / INX / INY
#define INCZ  0xE6
#define INCZX 0xF6
#define INC_  0xEE
#define INC_X 0xFE
#define INX   0xE8
#define INY   0xC8
// JMP / JSR
#define JMP   0x4C
#define JMPI  0x6C
#define JSR   0x20
// LDA
#define LDAN  0xA9
#define LDAZ  0xA5
#define LDAZX 0xB5
#define LDA   0xAD
#define LDAX  0xBD
#define LDAY  0xB9
#define LDAIX 0xA1
#define LDAIY 0xB1
// LDX
#define LDXN  0xA2
#define LDXZ  0xA6
#define LDXZY 0xB6
#define LDX   0xAE
#define LDXY  0xBE
// LDY
#define LDYN  0xA0
#define LDYZ  0xA4
#define LDYZX 0xB4
#define LDY   0xAC
#define LDYX  0xBC
// LSR
#define LSRA  0x4A
#define LSRZ  0x46
#define LSRZX 0x56
#define LSR   0x4E
#define LSRX  0x5E
// NOP
#define NOP   0xEA
// ORA
#define ORAN  0x09
#define ORAZ  0x05
#define ORAZX 0x15
#define ORA   0x0D
#define ORAX  0x1D
#define ORAY  0x19
#define ORAIX 0x01
#define ORAIY 0x11
// stack
#define PHA   0x48
#define PHP   0x08
#define PLA   0x68
#define PLP   0x28
// ROL
#define ROLA  0x2A
#define ROLZ  0x26
#define ROLZX 0x36
#define ROL   0x2E
#define ROLX  0x3E
// ROR
#define RORA  0x6A
#define RORZ  0x66
#define RORZX 0x76
#define ROR   0x6E
#define RORX  0x7E
// RTI / RTS
#define RTI   0x40
#define RTS   0x60
// SBC
#define SBCN  0xE9
#define SBCZ  0xE5
#define SBCZX 0xF5
#define SBC   0xED
#define SBCX  0xFD
#define SBCY  0xF9
#define SBCIX 0xE1
#define SBCIY 0xF1
// STA
#define STAZ  0x85
#define STAZX 0x95
#define STA   0x8D
#define STAX  0x9D
#define STAY  0x99
#define STAIX 0x81
#define STAIY 0x91
// STX
#define STXZ  0x86
#define STXZY 0x96
#define STX   0x8E
// STY
#define STYZ  0x84
#define STYZX 0x94
#define STY   0x8C
// transfers
#define TAX   0xAA
#define TAY   0xA8
#define TSX   0xBA
#define TXA   0x8A
#define TXS   0x9A
#define TYA   0x98

// Branch conditions come in pairs differing by this bit (BNE<->BEQ etc).
#define BRFLIP 0x20

extern ivmvar ivm_acc;
extern ivmvar ivm_par;

extern void opputnum(void);
extern void opmul(void);
extern void opdiv(void);
extern void opmod(void);

extern void printinlinedstring(void);
void jit_puts(void) {
  // print the string at ivm_acc (set by the P emitter)
  unsigned char *s = (unsigned char *)(unsigned)ivm_acc;
  if (!s) return;
  while (*s) putchar(*s++);
}

// JSR target. Return address on stack points at last byte of the JSR;
// the inline string follows. Walk it, print, then JMP to the byte after 0.
// Uses ZP $fe/$ff (same as tmp1/tmp2 in the emitter).
void printinlinedstring(void) {
  __asm__ ("pla");
  __asm__ ("sta $fe");
  __asm__ ("pla");
  __asm__ ("sta $ff");
  __asm__ ("inc $fe");
  __asm__ ("bne %g", pis_noinc);
  __asm__ ("inc $ff");
pis_noinc:
  __asm__ ("ldy #0");
  __asm__ ("lda ($fe),y");
  __asm__ ("beq %g", pis_done);
  __asm__ ("jsr _putchar");
  __asm__ ("inc $fe");
  __asm__ ("bne %g", pis_noinc);
  __asm__ ("inc $ff");
  __asm__ ("bne %g", pis_noinc);
pis_done:
  __asm__ ("inc $fe");
  __asm__ ("bne %g", pis_out);
  __asm__ ("inc $ff");
pis_out:
  __asm__ ("jmp ($fe)");
}

extern char* disasm(char* mc, char* end, char indent);

extern ivmvar ivm_zalloc(ivmvar n);
extern void   ivm_free(ivmvar p);

#define REF(f) ((word)(unsigned long)(void *)(f))

// zero-page scratch word (lo at tmp1, hi at tmp2) — single named base, not magic 0xfe elsewhere
static uchar tmp1 = 0xfe;
static uchar tmp2 = 0xff;

static uchar *lc_out;
static word   lc_at, lc_max;
static word  *lc_map, lc_mapn, lc_mem;
static uchar  lc_err;         // set when a short branch is out of range

typedef struct { word at, bc_target; } LcFix;

static LcFix lc_fix[256];
static word  lc_nfix;

// jump vector table at start of code (word per vector)
static word  lc_vecmark;      // offset in out[] of vector table
static word  lc_main_at;      // output offset where main trampoline landed
static word  lc_main_bc;      // bytecode offset of main (set by caller)
static word  lc_jmp_at;       // output offset of the startup JMP operand
static word  lc_out_org;      // absolute base of out[]
static uchar lc_ncall;
static uchar lc_cur_frame;    // frame_base of function being compiled
static const uchar *lc_fbase; // frame_base[v]
static const word  *lc_vec_bc;// bytecode entry pc per vector


// abs opcode → zero-page opcode, indexed by opcode. 0 = no zp variant.
static uchar zpmap[256];

static void zpmap_init(void) {
  if (zpmap[LDA]) return;     // already done
  zpmap[ORA]  = ORAZ;
  zpmap[ASL]  = ASLZ;
  zpmap[BIT]  = BITZ;
  zpmap[AND_] = ANDZ;
  zpmap[ROL]  = ROLZ;
  zpmap[EOR]  = EORZ;
  zpmap[LSR]  = LSRZ;
  zpmap[ADC]  = ADCZ;
  zpmap[ROR]  = RORZ;
  zpmap[STY]  = STYZ;
  zpmap[STA]  = STAZ;
  zpmap[STX]  = STXZ;
  zpmap[LDY]  = LDYZ;
  zpmap[LDA]  = LDAZ;
  zpmap[LDX]  = LDXZ;
  zpmap[CPY]  = CPYZ;
  zpmap[CMP]  = CMPZ;
  zpmap[DEC_] = DECZ;
  zpmap[CPX]  = CPXZ;
  zpmap[SBC]  = SBCZ;
  zpmap[INC_] = INCZ;
}

static void em(uchar b) {
  if (lc_at < lc_max) lc_out[lc_at] = b;
  lc_at++;
}
static void em2(uchar a, uchar b) { em(a); em(b); }

// forced absolute form (JSR / JMP and anything without a zp variant)
static void ema(uchar op, word addr) {
  em(op);
  em((uchar)(addr & 0xff));
  em((uchar)(addr >> 8));
}

// op + address: abs opcode given; zero-page form used when addr is 0..255
// and the opcode has one.
static void emop2(uchar op, word addr) {
  uchar z = zpmap[op];
  if (z && addr < 256) em2(z, (uchar)addr);
  else ema(op, addr);
}

// Reverse a branch opcode (flip the condition bit).
static uchar rev_branch(uchar bop) { return (uchar)(bop ^ BRFLIP); }

// ---- local forward branch (inside one op's template) -------------------
//   fix = emrel(BNE);  ...code of any size...;  empatch(fix);
static word emrel(uchar bop) {
  em(bop); em(0);
  return (word)(lc_at - 1);          // offset of the operand byte
}
static void empatch(word fix) {
  word d = (word)(lc_at - fix - 1);  // distance from after the operand
  if (d > 127) { lc_err = 1; return; }
  if (fix < lc_max) lc_out[fix] = (uchar)d;
}

// ---- jump to a bytecode pc ---------------------------------------------
// bop = a branch opcode, or JMP for unconditional.
// Already-emitted target (loop close): exact, short if it fits, no fixup.
// Not emitted yet (forward): long form, absolute address patched at the end.
static void emjump(uchar bop, word bc_target) {
  word t = (bc_target < lc_mapn) ? lc_map[bc_target] : 0xffff;
  word at;

  if (t != 0xffff) {
    if (bop == JMP) { ema(JMP, (word)(lc_out_org + t)); return; }
    {
      int off = (int)t - (int)(lc_at + 2);
      if (off >= -128 && off <= 127) { em2(bop, (uchar)off); return; }
    }
    em2(rev_branch(bop), 3);                  // far: skip over the JMP
    ema(JMP, (word)(lc_out_org + t));
    return;
  }

  if (bop != JMP) em2(rev_branch(bop), 3);    // opposite cond skips the JMP
  em(JMP);
  at = lc_at;
  em(0); em(0);
  if (lc_nfix < 256) {
    lc_fix[lc_nfix].at = at;
    lc_fix[lc_nfix].bc_target = bc_target;
    lc_nfix++;
  }
}

static word slotlo(uchar s) { return (word)(lc_mem + ((word)s << 1)); }
static word slothi(uchar s) { return (word)(slotlo(s) + 1); }

static uchar lc_constidx;     // first constant slot (255 = none)
static word *lc_constmem;     // mem array with constant values filled

static int is_const(uchar s) {
  return lc_constidx != 255 && s >= lc_constidx && lc_constmem;
}
static word const_val(uchar s) { return lc_constmem[s]; }

static void emit_load_slot(uchar s) {
  if (is_const(s)) {
    word v = const_val(s);
    em2(LDAN, (uchar)(v & 0xff));
    em2(LDXN, (uchar)(v >> 8));
    return;
  }
  emop2(LDA, slotlo(s));
  emop2(LDX, slothi(s));
}
static void emit_store_slot(uchar s) {
  emop2(STA, slotlo(s));
  emop2(STX, slothi(s));
}
static void emit_load0(void) { em2(LDAN, 0); em(TAX); }
static void emit_load_ffff(void) { em2(LDAN, 0xff); em(TAX); }

// abs opcode → immediate form
static uchar immop(uchar ins) {
  switch (ins) {
  case ADC:  return ADCN;
  case SBC:  return SBCN;
  case AND_: return ANDN;
  case ORA:  return ORAN;
  case EOR:  return EORN;
  case CMP:  return CMPN;
  case CPX:  return CPXN;
  case CPY:  return CPYN;
  }
  return 0;
}

// ACC = ACC OP mem[s]  (ADC/SBC/AND/ORA/EOR abs).
// CLC/SEC emitted by caller when needed (+/-). Reuse for logical ops too.
static void emit_lohiop(uchar ins, uchar s) {
  if (is_const(s)) {
    word v = const_val(s);
    uchar iop = immop(ins);

    em2(iop, (uchar)(v & 0xff)); em(TAY);
    em(TXA); em2(iop, (uchar)(v >> 8)); em(TAX);
    em(TYA);

    return;
  }
  emop2(ins, slotlo(s));
  em(TAY); em(TXA); emop2(ins, slothi(s));
  em(TAX); em(TYA);
}

static void emit_ax_to_tmp1(void) {
  em2(STAZ, tmp1);
  em2(STXZ, tmp2);
}

// reg OP (lo or hi byte of mem[s]); const-aware. For ADC/SBC/AND/ORA/EOR
// (on A) and CMP (A) / CPX (X). CLC/SEC by caller.
static void emit_alub(uchar ins, uchar s, uchar hi) {
  if (is_const(s)) {
    word v = const_val(s);
    em2(immop(ins), (uchar)(hi ? (v >> 8) : (v & 0xff)));
  } else {
    emop2(ins, hi ? slothi(s) : slotlo(s));
  }
}

// A = hi(mem[lim] - mem[slot]); N = sign (valid while |lim-slot| < 32768).
// Only A is clobbered. Used by 'i' and 'n'.
static void emit_lim_minus_slot(uchar lim, uchar slot) {
  if (is_const(lim)) {
    word v = const_val(lim);
    em2(LDAN, (uchar)(v & 0xff));
    em(SEC); emop2(SBC, slotlo(slot));
    em2(LDAN, (uchar)(v >> 8));
    emop2(SBC, slothi(slot));
  } else {
    emop2(LDA, slotlo(lim));
    em(SEC); emop2(SBC, slotlo(slot));
    emop2(LDA, slothi(lim));
    emop2(SBC, slothi(slot));
  }
}

// tmp1/tmp2 = mem[base] + 2*mem[idx]   (clobbers A, X, Y)
// TODO: this is very expensive, we should pass in the size
//   if we know it, also, have a byte operator. If size < 256 bytes
//   then we can simplify to simple LDA (base),Y !
static void emit_elem_ptr(uchar base, uchar idx) {
  if (is_const(idx)) {
    // constant index folds into one add
    word v = (word)(const_val(idx) << 1);
    emit_load_slot(base);
    em(CLC);
    em2(ADCN, (uchar)(v & 0xff)); em2(STAZ, tmp1);
    em(TXA);
    em2(ADCN, (uchar)(v >> 8));   em2(STAZ, tmp2);
    return;
  }
  // idx*2 (lo kept in Y, hi in tmp2), then + base
  emop2(LDA, slotlo(idx)); em(ASLA); em(TAY);
  emop2(LDA, slothi(idx)); em(ROLA); em2(STAZ, tmp2);
  em(CLC); em(TYA);
  emit_alub(ADC, base, 0); em2(STAZ, tmp1);
  em2(LDAZ, tmp2);
  emit_alub(ADC, base, 1); em2(STAZ, tmp2);
}


// Layout at out_org:
//   NOPs until odd address
//   JMP + vec[0..n)   ; JMP operand is vec[0] (main), filled at main entry
//   code...
// No forward calls / recursion: callee is compiled first, so vec[v] is already set.
// Call = JSR abs from vec[v]. No call fixup list.
// Frames: frame_base[v] known at transpile; entry stores AX to fp+FR_ACC.
#define FR_ACC 3

word ivm_jit(const uchar *code, word code_len,
  uchar *out, word out_max,
  word *map, word map_n,
  word mem_base,
  word out_org,
  const word *vec_bc, const uchar *frame_base, uchar ncall)
{
  const uchar *p, *end;
  word i, bc_pc;
  uchar op;
#ifdef JIT_TRACE
  word ostart;
#endif

  if (!code || !out || !map || code_len > map_n) return 0;

  zpmap_init();

  lc_out = out; lc_at = 0; lc_max = out_max;
  lc_map = map; lc_mapn = map_n; lc_mem = mem_base;
  lc_nfix = 0;
  lc_err = 0;
  lc_out_org = out_org;
  lc_ncall = ncall;
  lc_fbase = frame_base;
  lc_vec_bc = vec_bc;
  lc_cur_frame = frame_base ? frame_base[0] : 0;

  for (i = 0; i < code_len; i++)
    map[i] = 0xffff;

  // NOP until odd absolute address
  while (((out_org + lc_at) & 1) == 0)
    em(NOP);

  // JMP + vector table: JMP uses first word (main = vector 0)
  em(JMP);
  lc_jmp_at = lc_at;            // operand starts here
  lc_vecmark = lc_at;
  for (i = 0; i < ncall; i++) {
    em(0); em(0); // patched at function entry
  }

  p = code;
  end = code + code_len;

  while (p < end) {
    bc_pc = (word)(p - code);
    if (bc_pc < lc_mapn)
      lc_map[bc_pc] = lc_at;
    if (bc_pc == lc_main_bc)
      lc_main_at = lc_at;

    // function entry: patch vector word, set frame, store AX to ACC home
    if (vec_bc && frame_base) {
      for (i = 0; i < ncall; i++) {
        if (vec_bc[i] == bc_pc) {
          word abs = (word)(out_org + lc_at);
          word at = (word)(lc_vecmark + i * 2);
          if (at + 1 < lc_max) {
            lc_out[at]     = (uchar)(abs & 0xff);
            lc_out[at + 1] = (uchar)(abs >> 8);
          }
          lc_cur_frame = frame_base[i];
          // first in function: mem[fp+FR_ACC] = AX (incoming arg / ACC home)
          emit_store_slot((uchar)(lc_cur_frame + FR_ACC));
          break;
        }
      }
    }

    op = *p++;

    // 0..31: fast load mem[op]
    if (op < 32) { emit_load_slot(op); continue; }

    // hibit: vector call ($C0+) or const-slot load
    if (op & 0x80) {

      if (op < 0xC0) {

        // One byte slot load
        emit_load_slot((uchar)(op & 0x7f));

      } else {

        // One byte vector call.
        // Only vectors that exist in the call table are real calls.
        uchar v = (uchar)(op - 0xC0);
        if (v < lc_ncall) {
          word dest = 0;
          if (lc_vecmark + v * 2 + 1 < lc_max)
            dest = (word)(lc_out[lc_vecmark + v * 2]
                       | (lc_out[lc_vecmark + v * 2 + 1] << 8));
          if (!dest)
            printf("%%JIT: call vec %u before definition\n", v);
          ema(JSR, dest);
        } else {
          // not a real call — dump context so we can see what this byte is
          word pc = (word)(p - code - 1);
          printf("%%JIT: not-a-call 0x%02X (vec %u) at bc %u  ctx:",
            op, v, (unsigned)pc);
          {
            int k;
            word kstart = pc > 4 ? pc - 4 : 0;
            word kstop  = pc + 5 < code_len ? pc + 5 : code_len;
            for (k = (int)kstart; k < (int)kstop; k++)
              printf(" %02X%s", code[k], k == (int)pc ? "*" : "");
          }
          printf("\n");
        }

      }
      continue;
    }

#ifdef JIT_TRACE
    printf("------- %02x '%c' %02x %02x %02x %02x ------\n",
      op, isprint(op) ? op : ' ', p[0], p[1], p[2], p[3]);
    ostart = lc_at;
#endif

    // PAR: one operand byte, advances p
    #define PAR (*p++)

    switch (op) {
    // control
    case 'q': em(RTS); break;

    // returns
    case 'Q': emit_load0(); // fall into return
    case 'X': case ';': em(RTS); break;

    // constants
    case '0': emit_load0(); break;
    case '9': emit_load_ffff(); break;

    // load/store
    case 'L': emit_load_slot(PAR); break;
    case 'S': emit_store_slot(PAR); break;
    case 'Z': emit_load0(); emit_store_slot(PAR); break;

    // frame args: fixed addresses from frame_base (no runtime fp)
    // '8' = first arg / ACC home at fp+FR_ACC
    // '1'..'7' = fp+FR_ACC+(op-'0')
    case '8': emit_load_slot((uchar)(lc_cur_frame + FR_ACC)); break;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7':
      emit_load_slot((uchar)(lc_cur_frame + FR_ACC + (op - '0')));
      break;

    // arith / logic (emit_lohiop)
    case '+': em(CLC); emit_lohiop(ADC, PAR); break;
    case '-': em(SEC); emit_lohiop(SBC, PAR); break;
    case '&':          emit_lohiop(AND_, PAR); break;
    case '|':          emit_lohiop(ORA, PAR); break;
    case '^':          emit_lohiop(EOR, PAR); break;

    // compares → boolean ACC (0 or 1), X = 0.  UNSIGNED.
    // '=': CMP lo, CPX hi; 14-16 B (+1 per non-zp operand)
    case '=': {
      uchar s = PAR;
      word fx1, fx2, fx3;
      emit_alub(CMP, s, 0); fx1 = emrel(BNE);      // lo differs → ne
      emit_alub(CPX, s, 1); fx2 = emrel(BNE);      // hi differs → ne
      em2(LDAN, 1);         fx3 = emrel(BNE);      // Z=0 after LDA #1: always
      empatch(fx1); empatch(fx2);                  // ne:
      em2(LDAN, 0);
      empatch(fx3);
      em2(LDXN, 0);
      break;
    }
    // '<': ACC < mem  <=>  borrow in ACC - mem  <=>  !C
    case '<': {
      uchar s = PAR;
      emit_alub(CMP, s, 0); em(TXA);               // C from lo compare
      emit_alub(SBC, s, 1);                        // C = ACC >= mem
      em2(LDAN, 0); em(TAX); em(ROLA); em2(EORN, 1);   // A = !C, X = 0
      break;
    }
    // '>': ACC > mem  <=>  mem - ACC borrows  <=>  !C of (mem + ~ACC + 1)
    case '>': {
      uchar s = PAR;
      em(SEC); em2(EORN, 0xff); emit_alub(ADC, s, 0);
      em(TXA); em2(EORN, 0xff); emit_alub(ADC, s, 1);
      em2(LDAN, 0); em(TAX); em(ROLA); em2(EORN, 1);   // A = !C, X = 0
      break;
    }

    // * / % — helpers as in ivm-vm.asm (ACC op PAR); TODO: PARAX into zp_par
    case '*': case '/': case '%': {
      uchar s = PAR;
      emop2(STA, (word)(unsigned)&ivm_acc);
      emop2(STX, (word)((unsigned)&ivm_acc + 1));
      emit_load_slot(s);
      emop2(STA, (word)(unsigned)&ivm_par);
      emop2(STX, (word)((unsigned)&ivm_par + 1));
      if (op == '*') ema(JSR, REF(opmul));
      else if (op == '/') ema(JSR, REF(opdiv));
      else ema(JSR, REF(opmod));
      emop2(LDA, (word)(unsigned)&ivm_acc);
      emop2(LDX, (word)((unsigned)&ivm_acc + 1));
      break;
    }

    // I: INC lo; BNE skip; INC hi; reload ACC
    case 'I': {
      uchar s = PAR;
      word fx;
      emop2(INC_, slotlo(s));
      fx = emrel(BNE);
      emop2(INC_, slothi(s));
      empatch(fx);
      emit_load_slot(s);
      break;
    }
    // J: LDA lo; BNE skip; DEC hi; DEC lo; reload ACC
    case 'J': {
      uchar s = PAR;
      word fx;
      emop2(LDA, slotlo(s));
      fx = emrel(BNE);
      emop2(DEC_, slothi(s));
      empatch(fx);
      emop2(DEC_, slotlo(s));
      emit_load_slot(s);
      break;
    }

    // I/O — same symbols as ivm-vm.asm
    case 'N': em2(LDAN, 10); ema(JSR, REF(putchar)); break;
    case 'E': ema(JSR, REF(putchar)); break;  // emit ACC as char
    case '.':
      emop2(STA, (word)(unsigned)&ivm_acc);
      emop2(STX, (word)((unsigned)&ivm_acc + 1));
      ema(JSR, REF(opputnum));
      break;
    case 'P':
      emop2(STA, (word)(unsigned)&ivm_acc);
      emop2(STX, (word)((unsigned)&ivm_acc + 1));
      ema(JSR, REF(jit_puts));
      break;
    case 'O': em2(LDAN, PAR); ema(JSR, REF(putchar)); break;

    // heap
    case 'M': { uchar s = PAR; ema(JSR, REF(ivm_zalloc)); emit_store_slot(s); break; }
    case 'F': { uchar s = PAR; emit_load_slot(s); ema(JSR, REF(ivm_free)); emit_load0(); emit_store_slot(s); break; }

    // byte slot
    case 'l': { uchar s = PAR; emop2(LDA, slotlo(s)); em2(LDXN, 0); break; } // ACC = lo(mem[s])
    case 's': { uchar s = PAR; emop2(STA, slotlo(s)); em2(LDAN, 0); emop2(STA, slothi(s)); break; }

    case 'r': {
      uchar s = PAR;
      emit_load_slot(s); emit_ax_to_tmp1();
      em2(LDYN, 0); em2(LDAIY, tmp1); em2(LDXN, 0);
      break;
    }
    case 'w': {
      uchar s = PAR;
      em(TAY); emit_load_slot(s); emit_ax_to_tmp1();
      em(TYA); em2(LDYN, 0); em2(STAIY, tmp1);
      break;
    }

    // ': ACC = *(uchar*)ptr; ptr++   (post-increment byte read)
    case '\'': {
      uchar s = PAR;
      emit_load_slot(s); emit_ax_to_tmp1();
      em2(LDYN, 0); em2(LDAIY, tmp1); em(PHA);
      em(CLC);
      em2(LDAZ, tmp1); em2(ADCN, 1); em2(STAZ, tmp1);
      em2(LDAZ, tmp2); em2(ADCN, 0); em2(STAZ, tmp2);
      em2(LDAZ, tmp1); emop2(STA, slotlo(s));
      em2(LDAZ, tmp2); emop2(STA, slothi(s));
      em(PLA); em2(LDXN, 0);
      break;
    }
    // ,: *(uchar*)ptr = ACC; ptr++
    case ',': {
      uchar s = PAR;
      em(TAY); emit_load_slot(s); emit_ax_to_tmp1();
      em(TYA); em2(LDYN, 0); em2(STAIY, tmp1);
      em(CLC);
      em2(LDAZ, tmp1); em2(ADCN, 1); em2(STAZ, tmp1);
      em2(LDAZ, tmp2); em2(ADCN, 0); em2(STAZ, tmp2);
      em2(LDAZ, tmp1); emop2(STA, slotlo(s));
      em2(LDAZ, tmp2); emop2(STA, slothi(s));
      break;
    }

    // @: ACC = *(word*)mem[s]
    case '@': {
      uchar s = PAR;
      emit_load_slot(s); emit_ax_to_tmp1();
      em2(LDYN, 0); em2(LDAIY, tmp1); em(PHA);
      em(INY); em2(LDAIY, tmp1); em(TAX); em(PLA);
      break;
    }
    // !: *(word*)mem[s] = ACC
    case '!': {
      uchar s = PAR;
      // AX=value, need *mem[s]=AX.  Save value, load ptr to tmp1, store via (tmp1),Y
      em(TAY);                             // lo in Y
      em(TXA); em(PHA);                    // hi on stack
      em(TYA); em(PHA);                    // lo on stack
      emit_load_slot(s); emit_ax_to_tmp1();
      em(PLA); em2(LDYN, 0); em2(STAIY, tmp1);
      em(INY); em(PLA); em2(STAIY, tmp1);
      break;
    }

    // (: ACC = ((word*)base)[idx]
    case '(': {
      uchar base = PAR, idx = PAR;
      emit_elem_ptr(base, idx);            // tmp1/tmp2 = &base[idx]
      // load word: hi first, then lo, no stack traffic
      em2(LDYN, 1); em2(LDAIY, tmp1); em(TAX);
      em(DEY);      em2(LDAIY, tmp1);
      break;
    }
    // ): ((word*)base)[idx] = ACC   (ACC preserved)
    // TODO: constant stores (#0/#1): skip the push/pop and store the
    //   immediate directly via (tmp1),Y. Needs "ACC is constant" tracking.
    case ')': {
      uchar base = PAR, idx = PAR;
      em(PHA); em(TXA); em(PHA);           // save ACC: lo, then hi
      emit_elem_ptr(base, idx);            // tmp1/tmp2 = &base[idx]
      // pop hi → store, pop lo → store; AX ends up = original ACC
      em(PLA); em2(LDYN, 1); em2(STAIY, tmp1); em(TAX);
      em(PLA); em(DEY);      em2(STAIY, tmp1);
      break;
    }

    // ?: ACC = !ACC
    case '?':
      em2(STXZ, tmp1); em2(ORAZ, tmp1);
      em2(BEQ, 6); em2(LDAN, 0); em2(LDXN, 0); em2(BEQ, 4);
      em2(LDAN, 1); em2(LDXN, 0);
      break;
    // ~: ACC = ~ACC
    case '~':
      em2(EORN, 0xff); em(TAY); em(TXA); em2(EORN, 0xff); em(TAX); em(TYA);
      break;

    // G: if ((int16_t)ACC > 0) pc += rel8
    case 'G': {
      word rel = PAR;
      word dest = (word)((p - code) + rel);
      word fn, fz;
      // sign must come from the HIGH byte only (ORA of lo|hi would put
      // lo bit 7 into N)
      em2(STAZ, tmp1); em(TXA);
      fn = emrel(BMI);             // negative → skip
      em2(ORAZ, tmp1);             // Z if hi|lo == 0
      fz = emrel(BEQ);             // zero → skip
      emjump(JMP, dest);
      empatch(fn); empatch(fz);
      break;
    }
    // B: unconditional back
    case 'B': {
      word rel = PAR;
      word dest = (word)((p - code) - rel);
      emjump(JMP, dest);
      break;
    }

    // " inline string — helper prints until 0
    case '"':
      ema(JSR, REF(printinlinedstring));
      while (p < end && *p) { em(*p); p++; }
      em(0);
      if (p < end) p++;
      break;

    case ' ': break; // spacer

    // C: copy mem[s] -> mem[d], result in ACC. Two slot operands.
    case 'C': {
      uchar d = PAR;
      uchar s = PAR;
      emit_load_slot(s);
      emit_store_slot(d);
      break;
    }

    // i: FOR slot lim skip
    // if mem[slot] > mem[lim]: branch forward past body; else fall through.
    case 'i': {
      uchar slot = PAR;
      uchar lim  = PAR;
      uchar skip = PAR;
      word dest = (word)((p - code) + skip);
      emit_lim_minus_slot(lim, slot);
      emjump(BMI, dest);           // lim - slot < 0 → skip body
      break;
    }

    // n: NEXT back
    // increment the loop slot (from the matching i) and branch back
    // if still <= lim; else fall through.
    case 'n': {
      uchar back = PAR;
      // the i is `back` bytes before this operand
      // i layout: slot, lim, skip — we need those
      const uchar *ip = p - back;       // operand_pos - back + 1
      uchar slot, lim;
      word loop_top;
      word fx;
      if (ip < code || *ip != 'i') {
        // can't find matching i; just consume
        break;
      }
      slot = ip[1];
      lim  = ip[2];
      loop_top = (word)((ip + 4) - code);  // bc pc of body start
      // increment slot
      emop2(INC_, slotlo(slot));
      fx = emrel(BNE);
      emop2(INC_, slothi(slot));
      empatch(fx);
      // slot <= lim  <=>  lim - slot >= 0  <=>  N clear → loop
      // (backward: distance known, short branch when it fits)
      emit_lim_minus_slot(lim, slot);
      emjump(BPL, loop_top);
      break;
    }

    // U: mem[a] += mem[b]; ACC = result. Two slot operands.
    case 'U': {
      uchar a = PAR;
      uchar b = PAR;
      emit_load_slot(a);
      em(CLC);
      emit_lohiop(ADC, b);
      emit_store_slot(a);
      break;
    }

    // j: down-loop — 2 operands (not yet implemented)
    case 'j': {
      uchar slot = PAR;
      uchar skip = PAR;
      printf("%%Warning: unimpl op 'j' (slot %u skip %u) — skipping\n",
        slot, skip);
      break;
    }

    default:
      printf("%%ERROR: unimpl op 0x%02X '%c' at bc %u — continuing\n",
        op, (op >= 32 && op < 127) ? op : '.',
        (unsigned)(p - code - 1));
      break;

    }

    #undef PAR

#ifdef JIT_TRACE
    if (lc_at <= lc_max)
      disasm((char *)(lc_out + ostart), (char *)(lc_out + lc_at), 0);
#endif

  }

  if (lc_err) {
    printf("%%JIT: short branch out of range\n");
    return 0;
  }

  // patch forward jumps (G/i and any unmapped target). Calls need no fixups.
  for (i = 0; i < lc_nfix; i++) {
    word dest = lc_fix[i].bc_target;
    word mapped = 0xffff;
    word d;
    if (dest < lc_mapn) mapped = lc_map[dest];

    // mid-op target → snap forward to next op start
    if (mapped == 0xffff)
      for (d = dest; d < code_len && d < lc_mapn; d++)
        if (lc_map[d] != 0xffff) { mapped = lc_map[d]; break; }

    if (mapped == 0xffff) {
      printf("%%JIT: bad fixup %u\n", (unsigned)dest);
      return 0;
    }

    // write absolute JMP target (long form)
    if (lc_fix[i].at + 1 < lc_max) {
      word abs = (word)(lc_out_org + mapped);
      lc_out[lc_fix[i].at]     = (uchar)(abs & 0xff);
      lc_out[lc_fix[i].at + 1] = (uchar)(abs >> 8);
    }
  }

  return lc_at;
}

#endif
