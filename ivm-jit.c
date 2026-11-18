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
| Loops `i` / `n` / `B` | **Present** — unsigned. `i` closed by a `B` back to it (`while (v <= lim)`, i_exit_b): plain test, exit if > lim, AX = v after. Closed by `n`, rotated: `i` = JMP to the test in `n` (patched there, lc_loop[]), one compare per loop. If the body starts by reading the loop var, the test leaves it in AX and that load / `[` index is free; a `B` back to such a body start loads it first (lc_ksa[]) |
| `G`                      | **Done** — keeps AX (CPX #$80 / TAY / CPX #0), as the contract says |
| `U`                      | Present |
| `j` (down-loop)          | **Missing** — warns only |
| Array `(` `)`            | Present (needs testing) |
| / *                      | JSR helpers in **ivm-lib.asm** (ACC/PAR in zp ivm_acc/ivm_par); still need **PARAX → zp_par** |
| ZP optimisation          | **Done, one place** — emop2() + zpmap[] (see below) |
| Branch emit              | **Done** — emrel/empatch (local), emjump (bytecode target); forward short by estimate (JIT_BC_SCALE), retry if too far |
| ACC state                | **Done** — acc_at AX/PEND/LOST + acc_s (slot AX equals) + acc_k (const). Every op declares its ivm-walk.c contract: acc_need (in: loads a pending const/slot, error if LOST), acc_keep, acc_drop/acc_lost, acc_ax, acc_ax_slot, acc_pk/acc_ps (lazy const/slot), acc_write. Undeclared op → `ACC use not declared`. Labels (prescan, lc_lbl) flush. Pending users: `+ - & \| ^` load from the source (no TXA), const folding (also `~`), `)` `]` store from the source. acc_dead(): KEEP ops (N O F) don't save a dead ACC. -z pointer ops `r w ' , @ !` use (slot),Y directly. Contract breaks fixed: Z N O G F s w , ! ; m |
| Runtime library          | **Done (untested)** — ivm-lib.asm: op* helpers, putchar/getchar trampolines, bump heap. Generated code calls ONLY these (+ zp), never cc65 lib |
| `-z[XX]` mem in zp       | **Done (untested)** — mem[] at XX, default: ends at target zp top |
| `-a XXXX` code org       | **Done (untested)** — implies -j (can't run code built for another address) |
| `-o file` save           | **Done (untested)** — [hdr][ivm-lib][mem image][strings][gap][JIT code], implies -j |
| `-t target`              | **Done (untested)** — sim6502 (sim65 hdr), atmos (.tap autorun), raw |
| zjp 0x80+rel             | **Done (untested)** — jump if ACC == 0; replaces the old 0x80..0xBF one-byte slot load |
| HOSTSIM (host)           | **Done** — not cc65: code runs in cpu6502.c (cycle exact), lib routines are C traps (0 cycles); sim_run_blob() |

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
      - target not emitted yet (forward): short branch if the bytecode
        distance * JIT_BC_SCALE fits, else long form (reversed branch
        over a JMP abs); patched at the end (lc_fix). A short one that
        doesn't fit marks its op in lc_long[] ("this one failed") and
        ivm_jit() runs ivm_jit1() again. JMP stays JMP.

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

#include "ivm-walk.c"     // ivm_oplen, ivm_jump: label prescan

// HOSTSIM: not built by cc65 (host): the generated code runs in cpu6502.c,
// and the ivm-lib.asm routines are traps implemented in C (see the end of
// this file: sim_run_blob). -DNOHOSTSIM turns it off.
// (Not "SIM": that is the sim65 target in ivm-lib.asm.)
#if !defined(__CC65__) && !defined(NOHOSTSIM)
  #define HOSTSIM
#endif

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

#ifndef HOSTSIM
extern ivmvar ivm_acc;
extern ivmvar ivm_par;
#endif



// TODO: emit as native asm to USR using tmp1 as first arg, ACC as second
//   result in ACC: * / % outputnum

#ifndef HOSTSIM
extern void opputnum(void); // there is like a 30 B variant!
extern void opmul(void);
extern void opdiv(void);
extern void opmod(void);

// ivm-lib.asm: generated code must only call these (they are inside the
// image saved by -o). putchar/getchar trampolines hide the target.
extern void opputchar(void);  // A = char; A X Y preserved
extern void opgetchar(void);  // AX = char, $FFFF = EOF
extern void opputs(void);     // print string at ivm_acc; AX = ivm_acc
extern void opprintstr(void); // JSR + inline string + 0; AX preserved
extern void opmalloc(void);   // AX = bytes -> AX = zeroed ptr (bump)
extern void opfree(void);     // AX = ptr (no-op)
#endif

// TODO: word oprandom(void): see my parse.asm in Meteoric C compiler


// TODO: this should be in asm,
//   this could be the base routine used by printinlinedstring
//   the asm should use ACC to be printing advancing it at END
//   it holds next byte, or for printinlined string,
//   the return address
#ifndef HOSTSIM
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
#endif // !HOSTSIM


#ifndef HOSTSIM
extern char* disasm(char* mc, char* end, char indent);
#endif


extern ivmvar ivm_zalloc(ivmvar n);
extern void   ivm_free(ivmvar p);


#ifdef HOSTSIM
  #include "cpu6502.c"

  // ivm-lib routines: trap addresses (cpu_trap runs them in C), and the
  // ivm_acc / ivm_par words in the 64K
  enum {
    SIM_opputnum = 0xFF00, SIM_opmul, SIM_opdiv, SIM_opmod,
    SIM_opputchar, SIM_opgetchar, SIM_opputs, SIM_opprintstr,
    SIM_opmalloc, SIM_opfree, SIM_end
  };
  #define REF(f) ((word)SIM_##f)
  // address of ivm_acc / ivm_par as the generated code sees it
  #define ACC_ADR 0x0200
  #define PAR_ADR 0x0202
  char *disasm(char *mc, char *end, char indent);   // HOSTSIM: cpu6502.c
#else
  #define REF(f) ((word)(unsigned long)(void *)(f))
  #define ACC_ADR ((word)(unsigned)&ivm_acc)
  #define PAR_ADR ((word)(unsigned)&ivm_par)
#endif

// zero-page scratch word (lo at tmp1, hi at tmp2) — single named base, not magic 0xfe elsewhere
static uchar tmp1 = 0xfe;
static uchar tmp2 = 0xff;

static uchar *lc_out;
static word   lc_at, lc_max;
static word  *lc_map, lc_mapn, lc_mem;
static uchar  lc_err;         // set when a short branch is out of range

// forward branch: short if the target looks near, estimated as bytecode
// distance * JIT_BC_SCALE code bytes. A short one that doesn't fit when
// patched marks its op in lc_long[] and the whole JIT runs again.
#define JIT_BC_SCALE 4

typedef struct { word at, bc_target, bc_from; uchar sh; } LcFix;

static LcFix lc_fix[256];
static word  lc_nfix;
static word  lc_bc;           // bytecode pc of the current op
static uchar *lc_long;        // ops whose forward branches must be long
#define IS_LONG(bc) (lc_long && (lc_long[(bc) >> 3] & (1 << ((bc) & 7))))

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
static void ksa_check(word bc_target);
static void emjump(uchar bop, word bc_target) {
  word t = (bc_target < lc_mapn) ? lc_map[bc_target] : 0xffff;
  word at;

  ksa_check(bc_target);

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

  if (lc_nfix >= 256) { lc_err = 1; return; }
  lc_fix[lc_nfix].bc_target = bc_target;
  lc_fix[lc_nfix].bc_from = lc_bc;
  lc_fix[lc_nfix].sh = 0;
  if (bop != JMP && !IS_LONG(lc_bc) &&
      (long)(bc_target - lc_bc) * JIT_BC_SCALE <= 127) {
    em2(bop, 0);                              // short, offset patched
    lc_fix[lc_nfix].at = (word)(lc_at - 1);
    lc_fix[lc_nfix].sh = 1;
  } else {
    if (bop != JMP) em2(rev_branch(bop), 3);  // opposite cond skips the JMP
    em(JMP);
    at = lc_at;
    em(0); em(0);
    lc_fix[lc_nfix].at = at;
  }
  lc_nfix++;
}


static word slotlo(uchar s) { return (word)(lc_mem + ((word)s << 1)); }
static word slothi(uchar s) { return (word)(slotlo(s) + 1); }

static uchar lc_constidx;     // first constant slot (255 = none)
static word *lc_constmem;     // mem array with constant values filled

static int is_const(uchar s) {
  return lc_constidx != 255 && s >= lc_constidx && lc_constmem;
}

static word const_val(uchar s) { return lc_constmem[s]; }


// ACC = constant. Equal bytes (0, $FFFF, $0101...) → LDA #; TAX.
static void emit_load_const(word v) {
  uchar lo = (uchar)(v & 0xff), hi = (uchar)(v >> 8);
  em2(LDAN, lo);
  if (lo == hi) em(TAX); else em2(LDXN, hi);
}

static void emit_load_slot(uchar s) {
  if (is_const(s)) { emit_load_const(const_val(s)); return; }
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

// mem[s]++ (word)
static void emit_inc(uchar s) {
  word fx;
  emop2(INC_, slotlo(s)); fx = emrel(BNE); emop2(INC_, slothi(s));
  empatch(fx);
}

// zp pointer holding mem[s] for (zp),Y: with -z the slot itself, else a
// copy in tmp1/tmp2 (clobbers A)
static uchar emit_ptr(uchar s) {
  if (slotlo(s) < 0xff) return (uchar)slotlo(s);
  emop2(LDA, slotlo(s)); em2(STAZ, tmp1);
  emop2(LDA, slothi(s)); em2(STAZ, tmp2);
  return tmp1;
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

// UNSIGNED 16-bit compare: C = (mem[lim] >= mem[slot]).
// CMP on the low byte starts the carry chain (same as SEC;SBC, 1 byte
// shorter), SBC on the high byte finishes it. Only A is clobbered.
static void emit_lim_minus_slot(uchar lim, uchar slot) {
  if (is_const(lim)) {
    word v = const_val(lim);
    em2(LDAN, (uchar)(v & 0xff));
    emop2(CMP, slotlo(slot));
    em2(LDAN, (uchar)(v >> 8));
    emop2(SBC, slothi(slot));
  } else {
    emop2(LDA, slotlo(lim));
    emop2(CMP, slotlo(slot));
    emop2(LDA, slothi(lim));
    emop2(SBC, slothi(slot));
  }
}

// for-loop test (in 'n' only, 'i' jumps to it): branch back to bytecode
// pc body while mem[slot] <= lim, else fall through (loop done). UNSIGNED.
// ax = 0: lim - slot (10 cycles), AX not touched.
// ax = 1: the body starts by reading the loop var: AX = mem[slot] first,
//   then CPX/CMP, which only read A and X (13-14 cycles, the body's load
//   is free):  CPX hi; BNE t; CMP lo; t:   C = AX >= lim, Z = AX == lim
//   Const lim: i <= lim  <=>  i < lim+1 (folded here): one BCC.
// 'i' closed by a 'B' back to it (while (v <= lim)): its exit, else 0xffff.
// Such an 'i' is a plain test; closed by 'n' it is the rotated for loop.
static word i_exit_b(const uchar *c, word i, word len) {
  word t = ivm_jump(c + i, i);
  if (t >= 2 && t <= len && c[t - 2] == 'B' && (word)(t - c[t - 1]) == i)
    return t;
  return 0xffff;
}

// AX = mem[slot]: jump to exit if AX > lim (unsigned), AX kept
static void emit_loop_exit(uchar slot, uchar lim, word exit) {
  word fx, fz;
  (void)slot;
  if (is_const(lim)) {
    word v = const_val(lim);
    if (v == 0xffff) return;                          // never
    v++;
    em2(CPXN, (uchar)(v >> 8)); fx = emrel(BNE);
    em2(CMPN, (uchar)v);        empatch(fx);
    emjump(BCS, exit);                                // >= lim + 1
  } else {
    emop2(CPX, slothi(lim)); fx = emrel(BNE);
    emop2(CMP, slotlo(lim)); empatch(fx);
    fz = emrel(BEQ);                                  // == lim: stay
    emjump(BCS, exit);                                // >
    empatch(fz);
  }
}

static void emit_loop_test(uchar slot, uchar lim, word body, uchar ax) {
  word fx;

  if (!ax) {
    emit_lim_minus_slot(lim, slot);
    emjump(BCS, body);
    return;
  }
  emit_load_slot(slot);
  if (is_const(lim)) {
    word v = const_val(lim);
    if (v == 0xffff) { emjump(JMP, body); return; }   // always true
    v++;
    em2(CPXN, (uchar)(v >> 8)); fx = emrel(BNE);
    em2(CMPN, (uchar)v);        empatch(fx);
    emjump(BCC, body);
  } else {
    emop2(CPX, slothi(lim)); fx = emrel(BNE);
    emop2(CMP, slotlo(lim)); empatch(fx);
    emjump(BEQ, body);                                // == (Z only if hi ==)
    emjump(BCC, body);                                // <
  }
}

// open loops: 'i' emits "JMP test" with an empty operand, its 'n' patches
// it (loops nest, so this is a stack). ibc = bytecode pc of the 'i'.
#define MAX_LOOPS 8
static struct { word ibc, at; uchar ax; } lc_loop[MAX_LOOPS];
static uchar lc_nloop;

// "AX == mem[slot]" at a label. lc_ksa[]: such labels; every jump there
// makes it true, so the load at the label is free:
// - loop body start: only entered from the 'n' test, which leaves the loop
//   var in AX ('i' adds these)
// - a label starting with a slot load, reached only by fall-through and
//   'B' (a while head), if the fall-in has AX == slot: each 'B' loads it
//   unless AX already has it (prescan adds, the label keeps or drops them)
static struct { word bc; uchar slot, b; } lc_ksa[MAX_LOOPS * 4];  // b: by 'B'
static uchar lc_nksa;


// Byte element pointer like emit_belem_ptr, index already in AX
static void emit_belem_ptr_ax(uchar base) {
  em2(STAZ, tmp1);
  em(TXA);
  em(CLC);
  emit_alub(ADC, base, 1);
  em2(STAZ, tmp2);
  if (is_const(base)) em2(LDYN, const_val(base));
  else                emop2(LDY, slotlo(base));
}

// Word indexing address calculation
//
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

// Byte indexing address calculation
//
// (tmp1),Y == &((uchar*)mem[base])[mem[idx]]   (clobbers A, Y; X kept)
// ptr = (idx_lo, base_hi+idx_hi), Y = base_lo: the indexed mode does the
// lo add + carry for free.
static void emit_belem_ptr(uchar base, uchar idx) {
  if (is_const(idx)) em2(LDAN, const_val(idx));
  else               emop2(LDA, slotlo(idx));
  em2(STAZ, tmp1);
  if (is_const(idx)) em2(LDAN, const_val(idx) >> 8);
  else               emop2(LDA, slothi(idx));
  em(CLC);
  emit_alub(ADC, base, 1);
  em2(STAZ, tmp2);
  if (is_const(base)) em2(LDYN, const_val(base));
  else                emop2(LDY, slotlo(base));
}

// ---- ACC state: where the IVM ACC is, and what is known about it --------
// Per op contract: ivm-walk.c "ACC contract" (in / KEEP SLOT NEW UNDEF).
//
//   acc_at  ACC_AX    ACC is in A/X
//           ACC_PEND  not loaded yet: it is the const acc_k (acc_hk), or
//                     mem[acc_s]. acc_need() emits the load, only if asked.
//           ACC_LOST  an op destroyed it (UNDEF): acc_need() reports it
//   acc_hk/acc_k  ACC == const acc_k        (in AX or pending)
//   acc_s         ACC == mem[acc_s], 0xff = no
//
// Every op says what it does with ACC; nothing is special-cased in the loop:
//   in:  acc_need()         ACC must be in A/X now (loads a pending one)
//        or look at acc_at / acc_hk / acc_s and do better (immediates...)
//   out: acc_keep()         unchanged: a pending ACC stays pending
//        acc_drop()         about to be overwritten: pending load not emitted
//        acc_ax()           new value in A/X, nothing known about it
//        acc_ax_slot(s)     new value in A/X, == mem[s]
//        acc_pk(v)          new value: the const v (load later, if needed)
//        acc_ps(s)          new value: mem[s] (load later, if needed)
//        acc_lost()         destroyed (UNDEF)
//   acc_write(s)  before the op writes mem[s]: a pending mem[s] is loaded
//                 first, and "ACC == mem[s]" is forgotten
// Labels (jump targets, function entries): acc_label() emits a pending load
// BEFORE the label (so only fall-through runs it), then ACC is in AX with
// nothing known. lc_lbl[] has every label (prescan, ivm-walk.c ivm_jump).
// Stores through pointers ( ! , w ) ] m ) are assumed never to hit mem[]
// (no & of a variable), so they keep what is known about ACC.
// An op that calls none of these is reported ("ACC not declared").
enum { ACC_AX, ACC_PEND, ACC_LOST };
static uchar acc_at, acc_hk, acc_s;
// with ACC in AX: acc_x0  X == 0 (ACC is a byte), kept by KEEP ops
//                 acc_z   Z == (ACC == 0): set by an op for the NEXT op only
//                 (lc_z: taken over at the start of each op)
static uchar acc_x0, acc_z, lc_z;
static word  acc_k;
static uchar lc_acc_said;   // acc_* calls by the current op
static uchar lc_op;         // current op (messages)
static uchar *lc_lbl;       // label bitmap, one bit per bytecode pc

static int is_label(word bc) {
  return lc_lbl && (lc_lbl[bc >> 3] & (1 << (bc & 7)));
}

// emit a pending load (no message if lost)
static void acc_flush(void) {
  if (acc_at == ACC_PEND) {
    if (acc_hk) emit_load_const(acc_k); else emit_load_slot(acc_s);
    acc_at = ACC_AX; acc_x0 = 0;
  }
}
static void acc_need(void) {
  lc_acc_said++;
  if (acc_at == ACC_LOST) {
    printf("%%JIT: '%c' at bc %u needs ACC, but it was lost\n",
      lc_op, (unsigned)lc_bc);
    acc_at = ACC_AX; acc_x0 = 0;
  }
  acc_flush();
}
static void acc_keep(void) { lc_acc_said++; }
static void acc_drop(void) {
  lc_acc_said++;
  acc_at = ACC_LOST; acc_hk = 0; acc_s = 0xff;
}
static void acc_lost(void) { acc_drop(); }
static void acc_ax(void) {
  lc_acc_said++;
  acc_at = ACC_AX; acc_hk = 0; acc_s = 0xff; acc_x0 = 0;
}
// AX = a byte result, X = 0 (z: and Z == (ACC == 0) for the next op)
static void acc_ax_byte(uchar z) { acc_ax(); acc_x0 = 1; acc_z = z; }
static void acc_ax_slot(uchar s) {
  acc_ax();
  acc_s = s;
  if (is_const(s)) { acc_hk = 1; acc_k = const_val(s); }
}
static void acc_pk(word v) {
  lc_acc_said++;
  acc_at = ACC_PEND; acc_hk = 1; acc_k = v; acc_s = 0xff;
}
static void acc_ps(uchar s) {
  lc_acc_said++;
  if (acc_at == ACC_AX && acc_s == s) return;     // already in AX
  if (is_const(s)) { acc_pk(const_val(s)); return; }
  acc_at = ACC_PEND; acc_hk = 0; acc_s = s;
}
static void acc_write(uchar s) {
  lc_acc_said++;
  if (acc_s != s) return;
  if (acc_at == ACC_PEND && !acc_hk) acc_flush();  // old mem[s] into AX
  acc_s = 0xff;
}
// ACC *= 2 (i+i, i*2, 2*i): from the pending slot, or in AX. NEW
static void emit_acc_double(void) {
  if (acc_at == ACC_PEND && !acc_hk) {
    emop2(LDA, slotlo(acc_s)); em(ASLA); em(TAY);
    emop2(LDA, slothi(acc_s)); em(ROLA); em(TAX); em(TYA);
  } else {
    acc_need();
    em(ASLA); em(TAY); em(TXA); em(ROLA); em(TAX); em(TYA);
  }
  acc_ax();
}

static void acc_label(void) {
  acc_flush();
  acc_at = ACC_AX; acc_hk = 0; acc_s = 0xff; acc_x0 = 0;
}

// a jump to a while head (lc_ksa, b) must arrive with AX == its slot. Catches
// a jump op the prescan (ivm-walk.c ivm_jump) does not know.
static void ksa_check(word bc_target) {
  uchar k;
  for (k = 0; k < lc_nksa; k++)
    if (lc_ksa[k].bc == bc_target && lc_ksa[k].b &&
        !(acc_at == ACC_AX && acc_s == lc_ksa[k].slot))
      printf("%%JIT: jump to bc %u without AX == slot %u\n",
             (unsigned)bc_target, lc_ksa[k].slot);
}

// next op (at bc) does not read ACC and is not a label: ACC is dead, so
// a KEEP op needn't save it (ivm-walk.c contract: "- SLOT/NEW/UNDEF")
static int acc_unread(const uchar *c, word bc) {
  uchar op = c[bc];
  return op < 32 || strchr("09Q12345678@r'l([KLCUIJ;in", op);
}
static int acc_dead(const uchar *c, word bc, word len) {
  return bc < len && !is_label(bc) && acc_unread(c, bc);
}
static int is_fn_entry(word bc);
// label whose first op doesn't read ACC (not a function entry: that one
// stores AX): jumps to it and code falling into it needn't load ACC
static int acc_free_label(const uchar *c, word bc) {
  return !is_fn_entry(bc) && bc != lc_main_bc && acc_unread(c, bc);
}

// lo / hi byte of a pending ACC (const or slot) into A: LDA #k / LDA slot
static void emit_lda_pend(uchar hi) {
  if (acc_hk) em2(LDAN, (uchar)(hi ? acc_k >> 8 : acc_k));
  else        emop2(LDA, hi ? slothi(acc_s) : slotlo(acc_s));
}

// mem[s] usable as a zero-page pointer: (zp),Y straight on the slot

static int is_fn_entry(word bc) {
  word k;
  if (!lc_vec_bc) return 0;
  for (k = 0; k < lc_ncall; k++)
    if (lc_vec_bc[k] == bc) return 1;
  return 0;
}

// Layout at out_org:
//   NOPs until odd address
//   JMP + vec[0..n)   ; JMP operand is vec[0] (main), filled at main entry
//   code...
// No forward calls / recursion: callee is compiled first, so vec[v] is already set.
// Call = JSR abs from vec[v]. No call fixup list.
// Frames: frame_base[v] known at transpile; entry stores AX to fp+FR_ACC.
#define FR_ACC 3

static uchar lc_retry;        // a short forward branch didn't fit

static word ivm_jit1(const uchar *code, word code_len,
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
  lc_nloop = 0;
  lc_nksa = 0;
  lc_err = 0;
  acc_at = ACC_AX; acc_hk = 0; acc_s = 0xff; acc_x0 = 0; acc_z = 0;
  lc_out_org = out_org;
  lc_ncall = ncall;
  lc_fbase = frame_base;
  lc_vec_bc = vec_bc;
  lc_cur_frame = frame_base ? frame_base[0] : 0;

  for (i = 0; i < code_len; i++)
    map[i] = 0xffff;

  // labels: every jump target, loop body starts (n jumps there), function
  // entries and main's trampoline. 'i' / 'n' rel are not used by the JIT.
  lc_lbl = (uchar *)calloc((code_len >> 3) + 1, 1);
  if (lc_lbl) {
    word t, n;
    #define LBL(x) if ((x) < code_len) lc_lbl[(x) >> 3] |= (uchar)(1 << ((x) & 7))
    for (i = 0; i < code_len; i += n) {
      n = ivm_oplen(code + i);
      if (!n) n = 1;
      if (code[i] == 'i') {        // rotated: body start; while: exit
        t = i_exit_b(code, i, code_len);
        if (t != 0xffff) LBL(t); else LBL(i + 4);
        continue;
      }
      if (code[i] == 'n') continue;
      t = ivm_jump(code + i, i);
      if (t != 0xffff) LBL(t);
      // 'B' to a slot load: candidate "AX == slot" label
      if (code[i] == 'B' && t < code_len && lc_nksa < MAX_LOOPS * 4 &&
          (code[t] < 32 || code[t] == 'L') && !is_fn_entry(t) &&
          t != lc_main_bc) {
        lc_ksa[lc_nksa].bc = t;
        lc_ksa[lc_nksa].slot = code[t] < 32 ? code[t] : code[t + 1];
        lc_ksa[lc_nksa].b = 1;
        lc_nksa++;
      }
    }
    for (i = 0; vec_bc && i < ncall; i++) LBL(vec_bc[i]);
    LBL(lc_main_bc);
    #undef LBL
    // ... but not if any other jump goes there
    for (i = 0; i < code_len; i += n) {
      n = ivm_oplen(code + i);
      if (!n) n = 1;
      t = code[i] == 'i' ? (i_exit_b(code, i, code_len) != 0xffff ?
          i_exit_b(code, i, code_len) : (word)(i + 4)) :
          code[i] == 'n' || code[i] == 'B'
          ? 0xffff : ivm_jump(code + i, i);
      if (t != 0xffff) {
        uchar k;
        for (k = 0; k < lc_nksa; k++)
          if (lc_ksa[k].bc == t) lc_ksa[k].bc = 0xffff;
      }
    }
  }

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

  #ifdef JIT_TRACE
  ostart = lc_at;
  #endif

  while (p < end) {
    bc_pc = (word)(p - code);

    // a label here: ACC is not what the previous op left on every path.
    // A pending load is emitted BEFORE map[bc_pc]: jumps here skip it.
    if (is_label(bc_pc)) {
      // lc_ksa: only worth it if the fall-in has AX == slot already
      // ('i': its JMP said so), else drop it: the 'B's come later
      for (i = 0; i < lc_nksa && lc_ksa[i].bc != bc_pc; i++) ;
      if (i < lc_nksa && !(acc_at == ACC_AX && acc_s == lc_ksa[i].slot))
        lc_ksa[i].bc = 0xffff, i = lc_nksa;
      if (i < lc_nksa) acc_ax_slot(lc_ksa[i].slot);
      else if (acc_free_label(code, bc_pc)) acc_lost();
      else acc_label();
    }

    // Catch function entries (?)
    if (bc_pc <  lc_mapn)    lc_map[bc_pc] = lc_at;
    if (bc_pc == lc_main_bc) lc_main_at = lc_at;

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
          acc_ax_slot((uchar)(lc_cur_frame + FR_ACC));
          break;
        }
      }
    }

    // NEXT op
    op = *p++;
    lc_op = op; lc_bc = bc_pc; lc_acc_said = 0;
    lc_z = acc_z && !is_label(bc_pc) && acc_at == ACC_AX; acc_z = 0;

    #ifdef JIT_TRACE
      printf("------- %02x '%c' %02x %02x %02x %02x ------\n",
        op, isprint(op) ? op : ' ', p[0], p[1], p[2], p[3]);
    #endif

    // 0..31: fast load mem[op] (lazily; nothing if AX already holds it)
    if (op < 32) { acc_ps(op); goto report; }

    // hibit: vector call ($C0+) or zjp
    if (op & 0x80) {

      if (op < 0xC0) {

        // zjp 0x80+rel: jump forward rel if ACC == 0 (ACC kept)
        //   CMP #0; BNE skip; CPX #0; BEQ dest; skip:
        word dest = (word)((p - code) + (op & 0x3f));
        word fx;
        acc_need();                      // in, KEEP
        if (lc_z) emjump(BEQ, dest);                        // Z is ACC == 0
        else if (acc_x0) { em2(CMPN, 0); emjump(BEQ, dest); }   // X == 0
        else {
          em2(CMPN, 0); fx = emrel(BNE);
          em2(CPXN, 0); emjump(BEQ, dest);
          empatch(fx);
        }

      } else {

        // One byte vector call.
        // Only vectors that exist in the call table are real calls.
        uchar v = (uchar)(op - 0xC0);
        acc_need();                      // arg in AX -> result in AX
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
        acc_ax();

      }
      goto report;
    }

    // PAR: one operand byte, advances p
    #define PAR (*p++)

    switch (op) {
    // control: q / X return ACC, Q returns 0, ; the return value cell
    case 'q': case 'X': acc_need(); em(RTS); acc_ax(); break;
    case 'Q': acc_drop(); emit_load0(); em(RTS); acc_ax(); break;
    case ';':
      acc_drop();
      emit_load_slot((uchar)(lc_cur_frame + 1));   // FR_VAL
      em(RTS);
      acc_ax();
      break;

    // constants: loaded only if an op needs them in AX
    case '0': acc_pk(0); break;
    case '9': acc_pk(0xffff); break;

    // load/store
    case 'L': acc_ps(PAR); break;
    case 'S': {                                    // in, KEEP (+SLOT)
      uchar s = PAR;
      acc_need();
      if (acc_s != s) { emit_store_slot(s); acc_s = s; }  // else: already
      break;
    }
    case 'Z': {                                    // KEEP: AX untouched
      uchar s = PAR;
      acc_write(s);
      em2(LDYN, 0); emop2(STY, slotlo(s)); emop2(STY, slothi(s));
      break;
    }

    // frame args: fixed addresses from frame_base (no runtime fp)
    // '8' = first arg / ACC home at fp+FR_ACC
    // '1'..'7' = fp+FR_ACC+(op-'0')
    case '8': acc_ps((uchar)(lc_cur_frame + FR_ACC)); break;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7':
      acc_ps((uchar)(lc_cur_frame + FR_ACC + (op - '0')));
      break;

    // arith / logic: in, NEW
    case '+': case '-': case '&': case '|': case '^': {
      uchar s = PAR;
      uchar ins = op == '+' ? ADC : op == '-' ? SBC : op == '&' ? AND_ :
                  op == '|' ? ORA : EOR;
      if (acc_at == ACC_PEND && acc_hk && is_const(s)) {
        // const OP const: fold
        word a = acc_k, b = const_val(s);
        acc_pk((word)(op == '+' ? a + b : op == '-' ? a - b :
                      op == '&' ? (a & b) : op == '|' ? (a | b) : (a ^ b)));
        break;
      }
      if (op == '+' && acc_s == s &&             // ACC == mem[s] (AX or
          (acc_at == ACC_AX || (acc_at == ACC_PEND && !acc_hk))) {  // pend)
        emit_acc_double();                         // ACC == mem[s]: i + i
        break;
      }
      if (op == '+') em(CLC); else if (op == '-') em(SEC);
      if (acc_at == ACC_PEND) {
        // load each byte straight from where ACC is: no LDX / TXA
        emit_lda_pend(0); emit_alub(ins, s, 0); em(TAY);
        emit_lda_pend(1); emit_alub(ins, s, 1); em(TAX); em(TYA);
        acc_ax();
        break;
      }
      acc_need();
      emit_lohiop(ins, s);
      acc_ax();
      break;
    }

    // compares → boolean ACC (0 or 1), X = 0.  UNSIGNED.
    // '=': CMP lo, CPX hi; 14-16 B (+1 per non-zp operand)
    case '=': {
      uchar s = PAR;
      word fx1, fx2, fx3;
      acc_need();
      emit_alub(CMP, s, 0); fx1 = emrel(BNE);      // lo differs → ne
      emit_alub(CPX, s, 1); fx2 = emrel(BNE);      // hi differs → ne
      em2(LDAN, 1);         fx3 = emrel(BNE);      // Z=0 after LDA #1: always
      empatch(fx1); empatch(fx2);                  // ne:
      em2(LDAN, 0);
      empatch(fx3);
      em2(LDXN, 0);
      acc_ax_byte(0);
      break;
    }
    // '<': ACC < mem  <=>  borrow in ACC - mem  <=>  !C
    case '<': {
      uchar s = PAR;
      acc_need();
      emit_alub(CMP, s, 0); em(TXA);               // C from lo compare
      emit_alub(SBC, s, 1);                        // C = ACC >= mem
      em2(LDAN, 0); em(TAX); em(ROLA); em2(EORN, 1);   // A = !C, X = 0
      acc_ax_byte(1);
      break;
    }
    // '>': ACC > mem  <=>  mem - ACC borrows  <=>  !C of (mem + ~ACC + 1)
    case '>': {
      uchar s = PAR;
      acc_need();
      em(SEC); em2(EORN, 0xff); emit_alub(ADC, s, 0);
      em(TXA); em2(EORN, 0xff); emit_alub(ADC, s, 1);
      em2(LDAN, 0); em(TAX); em(ROLA); em2(EORN, 1);   // A = !C, X = 0
      acc_ax_byte(1);
      break;
    }

    // * / % — helpers as in ivm-vm.asm (ACC op PAR); TODO: PARAX into zp_par
    case '*': case '/': case '%': {
      uchar s = PAR;
      if (op == '*' && is_const(s) && const_val(s) == 2) {   // i * 2
        emit_acc_double();
        break;
      }
      if (op == '*' && acc_at == ACC_PEND && acc_hk && acc_k == 2 &&
          !is_const(s)) {                                     // 2 * i
        acc_ps(s);
        emit_acc_double();
        break;
      }
      acc_need();
      emop2(STA, ACC_ADR);
      emop2(STX, (word)(ACC_ADR + 1));
      emit_load_slot(s);
      emop2(STA, PAR_ADR);
      emop2(STX, (word)(PAR_ADR + 1));
      if (op == '*') ema(JSR, REF(opmul));
      else if (op == '/') ema(JSR, REF(opdiv));
      else ema(JSR, REF(opmod));
      emop2(LDA, ACC_ADR);
      emop2(LDX, (word)(ACC_ADR + 1));
      acc_ax();
      break;
    }

    // I: INC lo; BNE skip; INC hi; reload ACC
    case 'I': {                                    // -, SLOT
      uchar s = PAR;
      acc_drop();
      emit_inc(s);
      acc_ps(s);
      break;
    }
    // J: LDA lo; BNE skip; DEC hi; DEC lo; reload ACC
    case 'J': {                                    // -, SLOT
      uchar s = PAR;
      word fx;
      acc_drop();
      emop2(LDA, slotlo(s));
      fx = emrel(BNE);
      emop2(DEC_, slothi(s));
      empatch(fx);
      emop2(DEC_, slotlo(s));
      acc_ps(s);
      break;
    }

    // I/O — same symbols as ivm-vm.asm. putchar keeps A X Y.
    case 'N': case 'O': {                          // KEEP
      uchar c = (uchar)(op == 'N' ? 10 : PAR);
      uchar save = acc_at == ACC_AX && !acc_dead(code, (word)(p - code), code_len);
      if (save) em(TAY);                           // pending: nothing to save
      em2(LDAN, c); ema(JSR, REF(opputchar));
      if (save) em(TYA); else if (acc_at == ACC_AX) acc_lost();
      if (!lc_acc_said) acc_keep();
      break;
    }
    case 'E': acc_need(); ema(JSR, REF(opputchar)); break;  // ACC as char
    case '.': case 'P':                            // in, KEEP (AX = ivm_acc)
      acc_need();
      emop2(STA, ACC_ADR);
      emop2(STX, (word)(ACC_ADR + 1));
      ema(JSR, op == '.' ? REF(opputnum) : REF(opputs));
      break;

    // heap
    case 'M': {                                    // in (bytes), SLOT
      uchar s = PAR;
      acc_need();
      acc_write(s);
      ema(JSR, REF(opmalloc)); emit_store_slot(s);
      acc_ax_slot(s);
      break;
    }
    case 'F': {                                    // -, UNDEF
      uchar s = PAR;
      acc_drop();
      emit_load_slot(s); ema(JSR, REF(opfree));
      em2(LDYN, 0); emop2(STY, slotlo(s)); emop2(STY, slothi(s));
      break;
    }

    // byte slot
    case 'l': {                                    // NEW = lo(mem[s])
      uchar s = PAR;
      acc_drop();
      em2(LDXN, 0); emop2(LDA, slotlo(s));
      acc_ax_byte(1);
      break;
    }
    case 's': {                                    // in, KEEP: AX untouched
      uchar s = PAR;
      acc_write(s);
      if (acc_at == ACC_PEND) emit_lda_pend(0), acc_keep();  // stays pending
      else acc_need();
      emop2(STA, slotlo(s)); em2(LDYN, 0); emop2(STY, slothi(s));
      break;
    }

    // pointer ops: (zp),Y via emit_ptr(). A pending ACC is stored from where
    // it is and stays pending (mem[] slots are not reachable by pointers),
    // only an ACC in AX is saved around the pointer copy.
    case 'r': {                                    // NEW = *(uchar*)mem[s]
      uchar s = PAR, zp;
      acc_drop();
      zp = emit_ptr(s);
      em2(LDYN, 0); em2(LDXN, 0); em2(LDAIY, zp);
      acc_ax_byte(1);
      break;
    }
    // ': ACC = *(uchar*)ptr; ptr++   (post-increment byte read)
    case '\'': {                                   // NEW
      uchar s = PAR, zp;
      acc_drop();
      zp = emit_ptr(s);
      em2(LDYN, 0); em2(LDAIY, zp); em2(LDXN, 0);
      emit_inc(s);
      acc_ax_byte(0);
      break;
    }
    // w: *(uchar*)ptr = ACC     ,: the same, then ptr++
    case 'w': case ',': {                          // in, KEEP
      uchar s = PAR, zp, pend;
      if (op == ',') acc_write(s);
      pend = acc_at == ACC_PEND;
      if (!pend) acc_need();
      if (!pend && slotlo(s) == 0xff) em(PHA);
      zp = emit_ptr(s);
      if (!pend && slotlo(s) == 0xff) em(PLA);
      if (pend) emit_lda_pend(0);
      em2(LDYN, 0); em2(STAIY, zp);
      if (op == ',') emit_inc(s);
      acc_keep();
      break;
    }

    // @: ACC = *(word*)mem[s]
    case '@': {                                    // NEW
      uchar s = PAR, zp;
      acc_drop();
      zp = emit_ptr(s);
      em2(LDYN, 1); em2(LDAIY, zp); em(TAX);
      em(DEY);      em2(LDAIY, zp);
      acc_ax();
      break;
    }
    // !: *(word*)mem[s] = ACC
    case '!': {                                    // in, KEEP
      uchar s = PAR, zp;
      if (acc_at == ACC_PEND) {
        zp = emit_ptr(s);
        em2(LDYN, 1); emit_lda_pend(1); em2(STAIY, zp);
        em(DEY);      emit_lda_pend(0); em2(STAIY, zp);
        acc_keep();
        break;
      }
      acc_need();
      em(PHA);
      zp = emit_ptr(s);
      em(TXA); em2(LDYN, 1); em2(STAIY, zp);
      em(PLA); em(DEY);      em2(STAIY, zp);
      break;
    }

    // (: ACC = ((word*)base)[idx]
    case '(': {                                    // NEW
      uchar base = PAR, idx = PAR;
      acc_drop();
      emit_elem_ptr(base, idx);            // tmp1/tmp2 = &base[idx]
      // load word: hi first, then lo, no stack traffic
      em2(LDYN, 1); em2(LDAIY, tmp1); em(TAX);
      em(DEY);      em2(LDAIY, tmp1);
      acc_ax();
      break;
    }
    // ): ((word*)base)[idx] = ACC
    case ')': {                                    // in, KEEP
      uchar base = PAR, idx = PAR;
      if (acc_at == ACC_PEND) {
        // ACC still where it is (const / slot): store its bytes from there,
        // it stays pending
        emit_elem_ptr(base, idx);          // tmp1/tmp2 = &base[idx]
        em2(LDYN, 1); emit_lda_pend(1); em2(STAIY, tmp1);
        em(DEY);      emit_lda_pend(0); em2(STAIY, tmp1);
        acc_keep();
      } else {
        acc_need();
        em(PHA); em(TXA); em(PHA);         // save ACC: lo, then hi
        emit_elem_ptr(base, idx);          // tmp1/tmp2 = &base[idx]
        // pop hi → store, pop lo → store; AX ends up = original ACC
        em(PLA); em2(LDYN, 1); em2(STAIY, tmp1); em(TAX);
        em(PLA); em(DEY);      em2(STAIY, tmp1);
      }
      break;
    }

    // [ bb ii : ACC = ((uchar*)base)[idx]
    case '[': {                                    // NEW
      uchar base = PAR, idx = PAR;
      if (acc_at == ACC_AX && acc_s == idx) emit_belem_ptr_ax(base);  // in AX
      else                                  emit_belem_ptr(base, idx);
      acc_drop();
      em2(LDXN, 0); em2(LDAIY, tmp1);      // LDA last: Z == (ACC == 0)
      acc_ax_byte(1);
      break;
    }
    // ] bb ii : ((uchar*)base)[idx] = ACC
    case ']': {                                    // in, KEEP
      uchar base = PAR, idx = PAR;
      if (acc_at == ACC_PEND) {            // store from where ACC is
        emit_belem_ptr(base, idx);
        emit_lda_pend(0); em2(STAIY, tmp1);
        acc_keep();
      } else {
        acc_need();
        em(PHA);                           // X untouched by the helper
        emit_belem_ptr(base, idx);
        em(PLA); em2(STAIY, tmp1);
      }
      break;
    }
      
    // ?: ACC = !ACC
    case '?':                                      // in, NEW
      acc_need();
      if (!acc_x0) { em2(STXZ, tmp1); em2(ORAZ, tmp1); }   // A = lo | hi
      em2(CMPN, 1);                                         // C = ACC != 0
      em2(LDAN, 0); em(TAX); em(ROLA); em2(EORN, 1);        // A = !C, X = 0
      acc_ax_byte(1);
      break;
    // ~: ACC = ~ACC
    case '~':                                      // in, NEW
      if (acc_at == ACC_PEND && acc_hk) { acc_pk((word)~acc_k); break; }
      acc_need();
      em2(EORN, 0xff); em(TAY); em(TXA); em2(EORN, 0xff); em(TAX); em(TYA);
      acc_ax();
      break;

    // G: if ((int16_t)ACC > 0) pc += rel8     (in, KEEP: AX not touched)
    case 'G': {
      word rel = PAR;
      word dest = (word)((p - code) + rel);
      word fn, fgo, fz;
      acc_need();
      if (acc_x0) {                       // X == 0: ACC > 0 <=> A != 0
        if (!lc_z) em2(CMPN, 0);
        emjump(BNE, dest);
        break;
      }
      em2(CPXN, 0x80); fn = emrel(BCS);   // negative -> no jump
      em(TAY);         fgo = emrel(BNE);  // lo != 0: > 0 (Y is scratch)
      em2(CPXN, 0);    fz = emrel(BEQ);   // == 0 -> no jump
      empatch(fgo);
      emjump(JMP, dest);
      empatch(fn); empatch(fz);
      break;
    }
    // B: unconditional back
    case 'B': {
      word rel = PAR;
      word dest = (word)((p - code) - rel);
      uchar k;
      if (!acc_free_label(code, dest)) acc_flush();  // label expects AX
      acc_keep();
      // back to a loop body start that expects AX = its loop var
      for (k = 0; k < lc_nksa; k++)
        if (lc_ksa[k].bc == dest) {
          if (!(acc_at == ACC_AX && acc_s == lc_ksa[k].slot))
            emit_load_slot(lc_ksa[k].slot);
          acc_ax_slot(lc_ksa[k].slot);
          break;
        }
      emjump(JMP, dest);
      acc_ax();
      break;
    }

    // " inline string — helper prints until 0 (keeps A X)
    case '"':
      ema(JSR, REF(opprintstr));
      while (p < end && *p) { em(*p); p++; }
      em(0);
      if (p < end) p++;
      acc_keep();
      break;

    case ' ': acc_keep(); break; // spacer
    case 127: acc_keep(); break; // DEL: peephole filler (squeezed out by ivm-gen)

    // C: copy mem[s] -> mem[d], result in ACC. Two slot operands.
    case 'C': {                                    // -, SLOT (d)
      uchar d = PAR;
      uchar s = PAR;
      if (!(acc_at == ACC_AX && acc_s == s)) {
        acc_drop();
        emit_load_slot(s);
      }
      emit_store_slot(d);
      acc_ax_slot(d);
      if (is_const(s)) { acc_hk = 1; acc_k = const_val(s); }
      break;
    }

    // i: FOR slot lim skip -- rotated loop: the test is only in 'n'
    //   i:    JMP test          (operand patched by the matching 'n')
    //   body: ...               (AX = mem[slot] here, from the test)
    //   n:    INC slot; test: LDA/LDX slot; CPX/CMP lim; Bcc body
    //   exit:                   (falls out of the test: skip is not used)
    case 'i': {
      // does the body start by reading the loop var? (a load of it, or
      // '[' with it as index): then the test leaves it in AX
      const uchar *b = p + 3;
      uchar slot = p[0], ax;
      word ex = i_exit_b(code, bc_pc, code_len);
      if (ex != 0xffff) {          // while (v <= lim): plain test, the 'B'
        uchar lim = p[1];          // at the end jumps back here. -, SLOT
        p += 3;
        if (!(acc_at == ACC_AX && acc_s == slot)) {
          acc_drop();
          emit_load_slot(slot);
        }
        emit_loop_exit(slot, lim, ex);
        acc_ax_slot(slot);
        break;
      }
      ax = (b < end) && (b[0] == slot || (b[0] == 'L' && b[1] == slot) ||
                         (b[0] == '[' && b[2] == slot));
      if (ax) {                    // body start: AX = loop var (loop top)
        if (lc_nksa < MAX_LOOPS * 4) {
          lc_ksa[lc_nksa].bc = (word)(bc_pc + 4);
          lc_ksa[lc_nksa].slot = slot;
          lc_ksa[lc_nksa].b = 0;
          lc_nksa++;
        }
      }
      p += 3;                      // slot lim skip: 'n' reads slot, lim
      if (ax) acc_ax_slot(slot);   // unreachable after the JMP: no fall-in
      else acc_lost();             // load at the body start. -, UNDEF
      em(JMP);
      if (lc_nloop < MAX_LOOPS) {
        lc_loop[lc_nloop].ibc = bc_pc;
        lc_loop[lc_nloop].at  = lc_at;
        lc_loop[lc_nloop].ax  = ax;
        lc_nloop++;
      } else
        printf("%%JIT: loops nested too deep\n");
      em(0); em(0);
      break;
    }

    // n: NEXT back
    // increment the loop slot (from the matching i), then the loop test
    // ('i' jumps here too): back to the body while <= lim, else fall out.
    case 'n': {
      uchar back = PAR;
      // the i is `back` bytes before this operand
      // i layout: slot, lim, skip — we need those
      const uchar *ip = p - back;       // operand_pos - back + 1
      word ibc;
      uchar k, ax = 0;
      acc_lost();                  // -, UNDEF
      if (ip < code || *ip != 'i') {
        // can't find matching i; just consume
        break;
      }
      ibc = (word)(ip - code);
      // increment slot
      emit_inc(ip[1]);
      // test: patch the i's JMP to here (newest open loop is ours)
      for (k = lc_nloop; k-- > 0; ) {
        if (lc_loop[k].ibc != ibc) continue;
        if (lc_loop[k].at + 1 < lc_max) {
          word abs = (word)(lc_out_org + lc_at);
          lc_out[lc_loop[k].at]     = (uchar)(abs & 0xff);
          lc_out[lc_loop[k].at + 1] = (uchar)(abs >> 8);
        }
        ax = lc_loop[k].ax;
        lc_nloop = k;
        break;
      }
      // body = bc pc after the i (backward: short branch when it fits)
      emit_loop_test(ip[1], ip[2], (word)(ibc + 4), ax);
      if (ax) acc_ax_slot(ip[1]);  // loop done: AX = the loop var
      break;
    }

    // U: mem[a] += mem[b]; ACC = result. Two slot operands.
    case 'U': {                                    // -, SLOT (a)
      uchar a = PAR;
      uchar b = PAR;
      // byte by byte in memory, ACC = mem[a] left pending: no juggling
      em(CLC);
      if (acc_at == ACC_AX && acc_s == a) {        // lo, hi already in A, X
        emit_alub(ADC, b, 0); emop2(STA, slotlo(a));
        em(TXA);
      } else {
        acc_drop();
        emop2(LDA, slotlo(a)); emit_alub(ADC, b, 0); emop2(STA, slotlo(a));
        emop2(LDA, slothi(a));
      }
      emit_alub(ADC, b, 1); emop2(STA, slothi(a));
      acc_ps(a);
      break;
    }

    // j: down-loop — 2 operands (not yet implemented)
    case 'j': {
      uchar slot = PAR;
      uchar skip = PAR;
      printf("%%Warning: unimpl op 'j' (slot %u skip %u) — skipping\n",
        slot, skip);
      acc_lost();
      break;
    }

    // m: memset words. ACC = word count, ptr slot, val slot (lo byte = fill).
    //   bytes = 2*ACC: Y = lo, X = hi (= full pages). Remainder on the LAST
    //   page first, then full pages downwards. BPL: max 32K bytes.
    case 'm': {
      uchar ptr = PAR;
      uchar val = PAR;
      word fx;
      // in (count), KEEP: the code below uses A X Y. A pending count stays
      // pending (its source is unchanged), else it is lost.
      uchar pend = (acc_at == ACC_PEND), phk = acc_hk, ps = acc_s;
      word pk = acc_k;
      acc_need();

// 27 Bytes TODO: move out to subroutine, nothing is "special"

      em(TAY);

      // tmp1/tmp2 = ptr + X*256  (last page)
      emop2(LDA, slotlo(ptr)); em2(STAZ, tmp1);
      em(TXA); em(CLC);
      emit_alub(ADC, ptr, 1);  em2(STAZ, tmp2);

      // fill byte
      emop2(LDA, slotlo(val));

      // init
      em2(CPYN, 0);
      fx = emrel(BEQ);                    // no remainder → page

      // loop:
      em(DEY);                            // 1
      em2(STAIY, tmp1);                   // 2
      em2(BNE, (uchar)-5);                // 2 → loop
      empatch(fx);

      // page:
      em2(DECZ, tmp2);                    // 2
      em(DEX);                            // 1
      em2(BPL, (uchar)-10);               // 2 → loop (Y==0: 256 bytes)

      if (pend) { acc_at = ACC_PEND; acc_hk = phk; acc_k = pk; acc_s = ps; }
      else acc_lost();
      break;
    }

    default:
      printf("%%ERROR: unimpl op 0x%02X '%c' at bc %u — continuing\n",
        op, (op >= 32 && op < 127) ? op : '.',
        (unsigned)(p - code - 1));
      acc_lost();
      break;

    }

    #undef PAR

  report:
    if (!lc_acc_said)
      printf("%%JIT: '%c' at bc %u: ACC use not declared\n", op, (unsigned)bc_pc);

  #ifdef JIT_TRACE
    disasm((char *)(lc_out + ostart), (char *)(lc_out + lc_at), 0);
    ostart= lc_at;
  #endif

  }

  free(lc_lbl);
  lc_lbl = NULL;

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

    if (lc_fix[i].sh) {                     // short: rel8 from after it
      int off = (int)mapped - (int)(lc_fix[i].at + 1);
      if (off > 127) {                        // estimate too low: long next
        word b = lc_fix[i].bc_from;
        if (lc_long) lc_long[b >> 3] |= (uchar)(1 << (b & 7));
        lc_retry = 1;
      }
      lc_out[lc_fix[i].at] = (uchar)off;
      continue;
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

// ivm_jit1 again until every short forward branch fits
word ivm_jit(const uchar *code, word code_len,
  uchar *out, word out_max,
  word *map, word map_n,
  word mem_base,
  word out_org,
  const word *vec_bc, const uchar *frame_base, uchar ncall)
{
  word r;
  uchar n = 0;
  lc_long = (uchar *)calloc((code_len >> 3) + 1, 1);
  do {
    lc_retry = 0;
    r = ivm_jit1(code, code_len, out, out_max, map, map_n, mem_base,
                 out_org, vec_bc, frame_base, ncall);
    if (lc_retry) printf("%%JIT: short branch too far, again\n");
  } while (lc_retry && r && ++n < 8);
  free(lc_long);
  lc_long = NULL;
  return lc_retry ? 0 : r;
}

#ifdef HOSTSIM
// ---------------------------------------------------------------------------
// HOSTSIM: run on the host. sim_run_blob() lays an .ivm blob out in the 64K of
// cpu6502.c, JIT-compiles it there and runs it:
//
//   $0000-$00FF  zero page: -z mem[] (top down, below tmp1/tmp2 $FE/$FF)
//   $0200        ivm_acc, ivm_par
//   $0300        mem[] (without -z), then the strings
//   org          JIT code ($1000 default, -a XXXX), then the heap to $F000
//   $FF00        ivm-lib traps (SIM_*), $FFF0 cpu_call sentinel
//
// Lib calls run in C and cost 0 cycles: cycle counts are the JIT code only.
// ---------------------------------------------------------------------------
static word sim_heap, sim_heapend;
static unsigned long sim_traps;

#define SIM_W(a)  (cpu_mem[(word)(a)] | (cpu_mem[(word)((a) + 1)] << 8))
#define SIM_SETW(a, v) (cpu_mem[(word)(a)] = (uchar)(v), \
                        cpu_mem[(word)((a) + 1)] = (uchar)((v) >> 8))
#define SIM_AX(c, v) ((c)->a = (uchar)(v), (c)->x = (uchar)((v) >> 8))

// JIT_TRACE and jit_compile use disasm() on out[]: copy the bytes to where
// they will run, and list them from there
char *disasm(char *mc, char *end, char indent) {
  word at = (word)(lc_out_org + (mc - (char *)lc_out)), stop;
  char buf[40];
  int i;

  memcpy(cpu_mem + at, mc, (size_t)(end - mc));
  stop = (word)(at + (end - mc));
  while (at < stop) {
    for (i = 0; i < indent + 3; i++) putchar(' ');
    printf("%04X %s\n", at, (cpu_dis(at, buf), buf));
    at = (word)(at + cpu_dis(at, buf));
  }
  return end;
}

static int sim_trap(Cpu6502 *c) {
  word acc = (word)SIM_W(ACC_ADR), par = (word)SIM_W(PAR_ADR), t;
  int ch;

  if (c->pc >= SIM_end) return 0;
  sim_traps++;
  switch (c->pc) {
  case SIM_opputchar: putchar(c->a); break;           // A X Y kept
  case SIM_opgetchar:
    ch = getchar();
    t = (word)(ch == EOF ? 0xffff : ch);
    SIM_AX(c, t);
    break;
  case SIM_opputnum: printf("%u", acc); SIM_AX(c, acc); break;
  case SIM_opputs:
    for (t = acc; t && cpu_mem[t]; t++) putchar(cpu_mem[t]);
    SIM_AX(c, acc);
    break;
  case SIM_opprintstr:
    // the string follows the JSR: print it, return after its 0
    t = cpu_mem[0x101 + c->s];
    t |= (word)(cpu_mem[0x102 + c->s] << 8);
    while (cpu_mem[++t]) putchar(cpu_mem[t]);
    cpu_mem[0x101 + c->s] = (uchar)t;                 // RTS: t + 1
    cpu_mem[0x102 + c->s] = (uchar)(t >> 8);
    break;
  case SIM_opmul: SIM_SETW(ACC_ADR, (word)(acc * par)); break;
  case SIM_opdiv: if (par) SIM_SETW(ACC_ADR, (word)(acc / par)); break;
  case SIM_opmod: if (par) SIM_SETW(ACC_ADR, (word)(acc % par)); break;
  case SIM_opmalloc:                                  // AX = bytes -> ptr
    t = (word)(c->a | (c->x << 8));
    if (sim_heap + (unsigned)t > sim_heapend) t = 0;
    else {
      memset(cpu_mem + sim_heap, 0, t);
      ch = sim_heap;
      sim_heap = (word)((sim_heap + t + 1) & ~1);
      t = (word)ch;
    }
    SIM_AX(c, t);
    break;
  case SIM_opfree: break;
  default: return 0;
  }
  return 1;
}

// JIT-compile and run an un-relocated blob (as load_blob reads it on the
// host). zp: -z (zp_at 0 = top down), org: -a (0 = $1000). Returns AX.
word sim_run_blob(uchar *blob, uchar zp, uchar zp_at, word org) {
  // header: magic[4] len memsz constidx main code call fp con data
  #define HW(i) ((word)(blob[4 + 2 * (i)] | (blob[5 + 2 * (i)] << 8)))
  word len = HW(0), memsz = HW(1), cidx = HW(2), hmain = HW(3);
  word hcode = HW(4), hcall = HW(5), hfp = HW(6), hcon = HW(7), hdata = HW(8);
  #undef HW
  word membytes = (word)(memsz * 2), mem_base, data, code_len, size, i, w;
  word vec_bc[64], *map, *img;
  uchar ncall = (uchar)((hcon - hcall) / 2), seen0 = 0, *out;
  Cpu6502 cpu;
  int r;

  if (ncall > 64) ncall = 64;
  memset(cpu_mem, 0, sizeof(cpu_mem));

  // mem[]
  if (zp) {
    if (!zp_at) zp_at = (uchar)(0xFE - membytes);
    if (membytes > 0xFE || zp_at + membytes > 0xFE) {
      printf("%%SIM: -z: mem[] %u bytes does not fit in zp\n", membytes);
      return 0;
    }
    mem_base = zp_at;
    data = 0x0300;
  } else {
    mem_base = 0x0300;
    data = (word)((mem_base + membytes + 1) & ~1);
  }
  if (!org) org = 0x1000;
  if (data + (len - hdata) > org) {
    printf("%%SIM: mem[]+strings reach $%04X, past code org $%04X\n",
      (unsigned)(data + len - hdata), (unsigned)org);
    return 0;
  }

  // strings, then the mem[] image: consts from the pool; string words are
  // blob offsets (before the 0) -> their address here
  memcpy(cpu_mem + data, blob + hdata, len - hdata);
  img = (word *)calloc(memsz ? memsz : 1, sizeof(word));
  for (i = 0; cidx + i < memsz; i++) {
    w = (word)(blob[hcon + 2 * i] | (blob[hcon + 2 * i + 1] << 8));
    if (!w) seen0 = 1;
    else if (!seen0) w = (word)(data + (w - hdata));
    img[cidx + i] = w;
  }
  for (i = 0; i < memsz; i++) SIM_SETW(mem_base + 2 * i, img[i]);

  // JIT
  for (i = 0; i < ncall; i++)
    vec_bc[i] = (word)((blob[hcall + 2 * i] | (blob[hcall + 2 * i + 1] << 8)) - hcode);
  code_len = (word)((hdata > hcode ? hdata : len) - hcode);
  map = (word *)calloc(code_len ? code_len : 1, sizeof(word));
  out = (uchar *)malloc(8192);
  lc_main_bc = (word)(hmain - hcode);
  lc_main_at = 0;
  lc_constidx = (uchar)cidx;
  lc_constmem = img;
  size = ivm_jit(blob + hcode, code_len, out, 8192, map, code_len,
                 mem_base, org, vec_bc, blob + hfp, ncall);
  if (!size || org + size > 0xF000) {
    printf("%%SIM: JIT failed (%u bytes)\n", (unsigned)size);
    free(img); free(map); free(out);
    return 0;
  }
  if (lc_main_at) {                       // startup JMP -> _start of the blob
    out[lc_jmp_at]     = (uchar)(org + lc_main_at);
    out[lc_jmp_at + 1] = (uchar)((org + lc_main_at) >> 8);
  }
  memcpy(cpu_mem + org, out, size);

  printf("--- SIM: mem[] $%04X (%u B), strings $%04X, code $%04X-$%04X"
         " (%u B), heap $%04X ---\n", (unsigned)mem_base, (unsigned)membytes,
         (unsigned)data, (unsigned)org, (unsigned)(org + size - 1),
         (unsigned)size, (unsigned)((org + size + 1) & ~1));

  // run
  sim_heap = (word)((org + size + 1) & ~1);
  sim_heapend = 0xF000;
  sim_traps = 0;
  cpu_trap = sim_trap;
  cpu_trap_lo = SIM_opputnum;
  memset(&cpu, 0, sizeof(cpu));
  cpu.s = 0xff;
  cpu.p = 0x24;
  r = cpu_call(&cpu, org, 0);
  if (r < 0) {
    char buf[40];
    cpu_dis(cpu.pc, buf);
    printf("\n%%SIM: undocumented opcode at $%04X: %s\n", (unsigned)cpu.pc, buf);
  }
  printf("\n--- SIM: %lu cycles, %lu lib calls, AX = %u ---\n",
    cpu.cycles, sim_traps, (unsigned)(cpu.a | (cpu.x << 8)));

  free(img); free(map); free(out);
  return (word)(cpu.a | (cpu.x << 8));
}
#endif // HOSTSIM

#endif
