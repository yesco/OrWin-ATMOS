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
#ifdef VERBOSE
  #define JIT_TRACE 1   // per-op trace + disassembly; comment out for quiet
#endif


/*

INSTRUCTIONS: Keeps this section but keep updated for status

### Status

| Feature                  | Status |
|--------------------------|--------|
| Calls (vector table)     | **Done** — JMP + word table at start, JSR abs |
| Call frame               | **Done** — fixed addresses from frame_base, AX stored at fp+FR_ACC on entry |
| Compares `<` `=` `>`     | **Done** — boolean in ACC, UNSIGNED (needs testing) |
| Loops `t` / `n` / `B` | **Present** — unsigned. `t` closed by a `B` back to it (`while (v <= lim)`, i_exit_b): rotated too, the `B` has the test (loop_close), AX = v in the body and after. Stride loop (is_stride: body ends `U v s; B`): `U` leaves Y = lo, A = hi, the `B` tests those (`cmp/cpy`), body start has v in Y/A (lc_yas: `[` `]` index), `t` sets Y/A then JMPs to the test. Closed by `n`, rotated: `t` = JMP to the test in `n` (patched there, lc_loop[]), one compare per loop. If the body starts by reading the loop var, the test leaves it in AX and that load / `[` index is free; a `B` back to such a body start loads it first (lc_ksa[]) |
| `G`                      | **Done** — keeps AX (CPX #$80 / TAY / CPX #0), as the contract says |
| `U`                      | Present |
| `+` `-` `&` `\|` `^`      | **Done** — const hi byte 0: `adc #n; bcc; inx` (`sbc`/`dex`, `and #n; ldx #0`, `ora`/`eor` lo only). Followed by `S d`: each byte straight into d, leaves Y = lo, A = hi (lc_yah): no TAY/TXA/TAX/TYA |
| `i` `j` `H`              | **Done** — char ++ --: INC / DEC lo only (a char var's hi byte is always 0); int* ++: += 2 |
| `u`                      | **Done** — like `t` but `<`: `for (i = a; i < n; i++)` without the n-1 temp (and right for n == 0) |
| Array `(` `)`            | Present (needs testing) |
| / * %                    | JSR helpers in **ivm-lib.asm**: ACC in $FE/$FF (tmp1/tmp2), the other operand in AX, result in AX; a const goes in AX (`*`: the multiplier, fewer rounds) |
| `A` array swap           | **Done** — `t = a[i]; a[i] = a[j]; a[j] = t;` (int*): AS ($FA/$FB) = &a[j], AI = &a[i] (cache), 2 x 3 (zp),Y moves; AX = old a[i] for the `S t` |
| Element cache            | **Done** — `( ) [ ]` use their own zp pointer arrayindexing ($FC/$FD). AI = &base[idx] is kept while base / idx are not written (no label, call, loop op between); `a[j+1]` after `L j + c S t` reuses it with Y + 2c (word arrays) |
| ZP optimisation          | **Done, one place** — emop2() + zpmap[] (see below) |
| Branch emit              | **Done** — emrel/empatch (local), emjump (bytecode target); forward short by estimate (JIT_BC_SCALE), retry if too far |
| ACC state                | **Done** — lc_axs: slot physically in AX while ACC is pending (`[` `]` index). acc_x0 (X = 0) / acc_z (Z = ACC==0, next op): zjp `beq`, G `bne`. acc_at AX/PEND/LOST + acc_s (slot AX equals) + acc_k (const). Every op declares its ivm-walk.c contract: acc_need (in: loads a pending const/slot, error if LOST), acc_keep, acc_drop/acc_lost, acc_ax, acc_ax_slot, acc_pk/acc_ps (lazy const/slot), acc_write. Undeclared op → `ACC use not declared`. Labels (prescan, lc_lbl) flush. Pending users: `+ - & \| ^` load from the source (no TXA), const folding (also `~`), `)` `]` store from the source. acc_dead(): KEEP ops (N O F) don't save a dead ACC. -z pointer ops `r w ' , @ !` use (slot),Y directly. Contract breaks fixed: Z N O G F s w , ! ; m |
| Runtime library          | **Done (untested)** — ivm-lib.asm: op* helpers, putchar/getchar trampolines, bump heap, opmemset (self-modified STA abs,Y; `m` with count > 256 or not const, const 1..256 is inline). Generated code calls ONLY these (+ zp), never cc65 lib |
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
extern ivmvar ivm_acc;                // jit_puts
#endif



// TODO: emit as native asm to USR using tmp1 as first arg, ACC as second
//   result in ACC: * / % outputnum

#ifndef HOSTSIM
extern void opputnum(void); // print AX; AX kept. there is like a 30 B variant!
extern void opmul(void);    // $FE/$FF OP AX -> AX
extern void opdiv(void);
extern void opmod(void);

// ivm-lib.asm: generated code must only call these (they are inside the
// image saved by -o). putchar/getchar trampolines hide the target.
extern void opputchar(void);  // A = char; A X Y preserved
extern void opgetchar(void);  // AX = char, $FFFF = EOF
extern void opputs(void);     // print string at AX; AX kept
extern void opprintstr(void); // JSR + inline string + 0; AX preserved
extern void opmalloc(void);   // AX = bytes -> AX = zeroed ptr (bump)
extern void opfree(void);     // AX = ptr (no-op)
extern void opmemset(void);   // A = fill, X = pages, Y = rest; see 'm'
extern uchar opmemset_adr[];  // its STA $xxxx,Y operand (self-modified)
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

  // ivm-lib routines: trap addresses (cpu_trap runs them in C)
  enum {
    SIM_opputnum = 0xFF00, SIM_opmul, SIM_opdiv, SIM_opmod,
    SIM_opputchar, SIM_opgetchar, SIM_opputs, SIM_opprintstr,
    SIM_opmalloc, SIM_opfree, SIM_optimed, SIM_end
  };
  // opmemset is real 6502 code (its loop costs cycles): sim_memset[] at
  // $FE00, below the traps. Same bytes as _opmemset in ivm-lib.asm.
  #define SIM_opmemset     0xFE00
  #define SIM_opmemset_adr 0xFE06
  #define REF(f) ((word)SIM_##f)
  char *disasm(char *mc, char *end, char indent);   // HOSTSIM: cpu6502.c
#else
  #define REF(f) ((word)(unsigned long)(void *)(f))
#endif

// zero-page scratch word (lo at tmp1, hi at tmp2) — single named base, not magic 0xfe elsewhere
static uchar tmp1 = 0xfe;
static uchar tmp2 = 0xff;
// zero-page array element pointer ( ) [ ]: only these use it, so it
// survives lib calls and shifts (the element cache, lc_ai_*)
static uchar arrayindexing = 0xfc;
#define AI   arrayindexing
// second element pointer: 'A' (array swap) a[j]
static uchar arrayswap = 0xfa;
#define AS   arrayswap
#define ZPTOP 0xfa          // -z: mem[] ends below this

// element cache: AI == &base[idx] (kind 'w': word ops ( ), Y = 2*off;
// 'b': byte ops [ ], ptr = (idx lo, base hi + idx hi), Y = base lo).
// Dropped at labels, calls, loop ops and writes to base or idx.
// lc_rel: slot t == mem[src] + c ("L j + c S t": a[j+1] is a[j], Y + 2c)
static uchar lc_ai_kind, lc_ai_base, lc_ai_idx;
static uchar lc_rel_t = 0xff, lc_rel_src, lc_rel_c;
static uchar lc_plus_src = 0xff, lc_plus_c;   // ACC == mem[src] + c (this op)

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

// byte slots: hi byte always 0. A slot is a byte slot unless some op
// writes a word to it (S I J U C M H ' , t), or it is a frame header
// (call: ret, prev, ACC). Only 's' 'i' 'j' 'Z' 'F' write them. Prescan.
static uchar lc_wword[32];
static void wword(uchar s) { lc_wword[s >> 3] |= (uchar)(1 << (s & 7)); }
static int is_byte(uchar s) {
  return !is_const(s) && !(lc_wword[s >> 3] & (1 << (s & 7)));
}
// mem[s] < 256 for sure
static int hi0(uchar s) {
  return is_const(s) ? const_val(s) < 256 : is_byte(s);
}


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
  case LDA:  return LDAN;
  case LDX:  return LDXN;
  case LDY:  return LDYN;
  }
  return 0;
}

// ACC = ACC OP mem[s]  (ADC/SBC/AND/ORA/EOR abs).
// CLC/SEC emitted by caller when needed (+/-). Reuse for logical ops too.
static void emit_alub(uchar ins, uchar s, uchar hi);

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
// after a compare: [? ...] then zjp or G, no label between, ACC read by
// neither path? -> the branch target, *skip = bytes to eat, *brc = the
// branch on C = !cond (BCS: jump if !cond, BCC: if cond); else 0xffff
static int acc_unread(const uchar *c, word bc);
static int is_label(word bc);
static word fuse_branch(const uchar *c, word nb, word len, uchar *skip,
                        uchar *brc) {
  word i = nb, t;
  uchar k = 0, jz;
  while (i < len && c[i] == '?' && k < 3) {
    if (is_label(i)) return 0xffff;
    i++; k++;
  }
  if (i >= len || is_label(i)) return 0xffff;
  if ((uchar)(c[i] - 0x80) < 64) { jz = 1; *skip = (uchar)(i - nb + 1); }
  else if (c[i] == 'G' && i + 1 < len) { jz = 0; *skip = (uchar)(i - nb + 2); }
  else return 0xffff;
  t = ivm_jump(c + i, i);
  if (t >= len || !acc_unread(c, (word)(nb + *skip)) || !acc_unread(c, t))
    return 0xffff;
  // value = cond if k even; zjp jumps if value == 0, G if value != 0
  *brc = (uchar)((jz ^ (k & 1)) ? BCS : BCC);
  return t;
}

// flags of a - b (C = a >= b): A only, consts as immediates
static void emit_sub_flags(uchar a, uchar b) {
  if (is_const(a)) em2(LDAN, (uchar)const_val(a)); else emop2(LDA, slotlo(a));
  emit_alub(CMP, b, 0);
  if (is_const(a)) em2(LDAN, (uchar)(const_val(a) >> 8));
  else emop2(LDA, slothi(a));
  emit_alub(SBC, b, 1);
}
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

// for-loop test (in 'n' only, 't' jumps to it): branch back to bytecode
// pc body while mem[slot] <= lim, else fall through (loop done). UNSIGNED.
// ax = 0: lim - slot (10 cycles), AX not touched.
// ax = 1: the body starts by reading the loop var: AX = mem[slot] first,
//   then CPX/CMP, which only read A and X (13-14 cycles, the body's load
//   is free):  CPX hi; BNE t; CMP lo; t:   C = AX >= lim, Z = AX == lim
//   Const lim: i <= lim  <=>  i < lim+1 (folded here): emit_below_const.
// 't' closed by a 'B' back to it (while (v <= lim)): its exit, else 0xffff.
// Rotated like the for loop ('n'), the 'B' has the test (no increment).
#define IS_LOOP(o) ((o) == 't' || (o) == 'u')     // u: < (exclusive)
static word i_exit_b(const uchar *c, word i, word len) {
  word t = ivm_jump(c + i, i);
  if (t >= 2 && t <= len && c[t - 2] == 'B' && (word)(t - c[t - 1]) == i)
    return t;
  return 0xffff;
}

// t / u at i a loop head? Closed by an 'n' or a 'B' back to it. Else it
// is an if (l < r) / while with a mark before it: compare and branch
static int is_loop_head(const uchar *c, word i, word len) {
  word ex = ivm_jump(c + i, i);
  if (ex < 2 || ex > len) return 0;
  if (c[ex - 2] == 'n' && (word)(ex - c[ex - 1]) == i) return 1;
  return i_exit_b(c, i, len) == ex;
}

// stride loop: 't' at ibc closed by "U v s; B" (v its loop var, the 'B' no
// jump target): 'U' leaves Y = lo, A = hi, the 'B' tests those
static int is_label(word bc);
static int is_stride(const uchar *c, word ibc, word len) {
  word ex = i_exit_b(c, ibc, len);
  return ex != 0xffff && ex >= ibc + 9 && c[ex - 5] == 'U' &&
         c[ex - 4] == c[ibc + 1] && !is_label((word)(ex - 2));
}

// reg hi / reg lo < v (const): to body (backward). Common case first:
//   v lo == 0:  CPhi #h; BCC body                      (5 c, 4 B)
//   v hi == 0:  CPhi #0; BNE exit; CPlo #l; BCC body   (hi below: never)
//   near body:  CPhi #h; BCC body; BNE exit; CPlo #l; BCC body  (5 c, 10 B)
//   far body:   CPhi #h; BCC go; BNE exit; CPlo #l; BCS exit; go: JMP body
static void emit_below_const(uchar cphi, uchar cplo, word v, word body) {
  word t = body < lc_mapn ? lc_map[body] : 0xffff, fx, fgo, fx2;
  em2(cphi, (uchar)(v >> 8));
  if (!(uchar)v) { emjump(BCC, body); return; }
  if (!(v >> 8)) {
    fx = emrel(BNE);
    em2(cplo, (uchar)v);
    emjump(BCC, body);
    empatch(fx);
    return;
  }
  if (t != 0xffff && (int)t - (int)(lc_at + 8) >= -128) {   // both short
    emjump(BCC, body);
    fx = emrel(BNE);
    em2(cplo, (uchar)v);
    emjump(BCC, body);
    empatch(fx);
    return;
  }
  fgo = emrel(BCC);
  fx = emrel(BNE);
  em2(cplo, (uchar)v);
  fx2 = emrel(BCS);
  empatch(fgo);
  emjump(JMP, body);
  empatch(fx); empatch(fx2);
}

// Y = lo, A = hi of the loop var: to body while <= lim (as emit_loop_test)
// excl ('u'): while < lim
static void emit_loop_test_ya(uchar lim, word body, uchar excl) {
  word fx;
  if (is_const(lim)) {
    word v = const_val(lim);
    if (excl) { if (v) emit_below_const(CMPN, CPYN, v, body); return; }
    if (v == 0xffff) { emjump(JMP, body); return; }   // always true
    emit_below_const(CMPN, CPYN, (word)(v + 1), body);
  } else {
    emit_alub(CMP, lim, 1); fx = emrel(BNE);
    emit_alub(CPY, lim, 0); empatch(fx);
    if (!excl) emjump(BEQ, body);
    emjump(BCC, body);
  }
}

static void emit_loop_test(uchar slot, uchar lim, word body, uchar ax,
                           uchar excl) {
  word fx;

  if (!ax) {
    if (excl) { emit_lim_minus_slot(slot, lim); emjump(BCC, body); }
    else      { emit_lim_minus_slot(lim, slot); emjump(BCS, body); }
    return;
  }
  emit_load_slot(slot);
  if (is_const(lim)) {
    word v = const_val(lim);
    if (excl) { if (v) emit_below_const(CPXN, CMPN, v, body); return; }
    if (v == 0xffff) { emjump(JMP, body); return; }   // always true
    emit_below_const(CPXN, CMPN, (word)(v + 1), body);
  } else {
    emop2(CPX, slothi(lim)); fx = emrel(BNE);
    emop2(CMP, slotlo(lim)); empatch(fx);
    if (!excl) emjump(BEQ, body);                     // == (Z only if hi ==)
    emjump(BCC, body);                                // <
  }
}

// open loops: 't' emits "JMP test" with an empty operand, its 'n' patches
// it (loops nest, so this is a stack). ibc = bytecode pc of the 't'.
#define MAX_LOOPS 8
static struct { word ibc, at; uchar ax; } lc_loop[MAX_LOOPS];
static uchar lc_nloop;

// patch the JMP of the open loop at 't' ibc to here (its test); its ax
static uchar loop_close(word ibc) {
  uchar k;
  for (k = lc_nloop; k-- > 0; ) {
    if (lc_loop[k].ibc != ibc) continue;
    if (lc_loop[k].at + 1 < lc_max) {
      word abs = (word)(lc_out_org + lc_at);
      lc_out[lc_loop[k].at]     = (uchar)(abs & 0xff);
      lc_out[lc_loop[k].at + 1] = (uchar)(abs >> 8);
    }
    lc_nloop = k;
    return lc_loop[k].ax;
  }
  return 0;
}

// "AX == mem[slot]" at a label. lc_ksa[]: such labels; every jump there
// makes it true, so the load at the label is free:
// - loop body start: only entered from the 'n' test, which leaves the loop
//   var in AX ('t' adds these)
// - a label starting with a slot load, reached only by fall-through and
//   'B' (a while head), if the fall-in has AX == slot: each 'B' loads it
//   unless AX already has it (prescan adds, the label keeps or drops them)
static struct { word bc; uchar slot, b; } lc_ksa[MAX_LOOPS * 4];  // b: by 'B'
static uchar lc_nksa;


// Byte element pointer like emit_belem_ptr, index in Y (lo) and A (hi)
static void emit_belem_ptr_ya(uchar base) {
  em2(STYZ, AI);
  em(CLC);
  emit_alub(ADC, base, 1);
  em2(STAZ, AI + 1);
  if (is_const(base)) em2(LDYN, const_val(base));
  else                emop2(LDY, slotlo(base));
}

// Byte element pointer like emit_belem_ptr, index already in AX
static void emit_belem_ptr_ax(uchar base) {
  em2(STAZ, AI);
  em(TXA);
  em(CLC);
  emit_alub(ADC, base, 1);
  em2(STAZ, AI + 1);
  if (is_const(base)) em2(LDYN, const_val(base));
  else                emop2(LDY, slotlo(base));
}

// Word indexing address calculation
//
// AI = mem[base] + 2*mem[idx]   (clobbers A, X, Y)
// TODO: this is very expensive, we should pass in the size
//   if we know it, also, have a byte operator. If size < 256 bytes
//   then we can simplify to simple LDA (base),Y !
static void emit_elem_ptr_zp(uchar zp, uchar base, uchar idx);
static void emit_elem_ptr(uchar base, uchar idx) {
  emit_elem_ptr_zp(AI, base, idx);
}
// the same into any zp pointer
static void emit_elem_ptr_zp(uchar zp, uchar base, uchar idx) {
  if (is_const(idx)) {
    // constant index folds into one add
    word v = (word)(const_val(idx) << 1);
    emit_load_slot(base);
    em(CLC);
    em2(ADCN, (uchar)(v & 0xff)); em2(STAZ, zp);
    em(TXA);
    em2(ADCN, (uchar)(v >> 8));   em2(STAZ, zp + 1);
    return;
  }
  // idx*2 (lo kept in Y, hi in zp + 1), then + base
  emop2(LDA, slotlo(idx)); em(ASLA); em(TAY);
  emop2(LDA, slothi(idx)); em(ROLA); em2(STAZ, zp + 1);
  em(CLC); em(TYA);
  emit_alub(ADC, base, 0); em2(STAZ, zp);
  em2(LDAZ, zp + 1);
  emit_alub(ADC, base, 1); em2(STAZ, zp + 1);
}

// Byte indexing address calculation
//
// (AI),Y == &((uchar*)mem[base])[mem[idx]]   (clobbers A, Y; X kept)
// ptr = (idx_lo, base_hi+idx_hi), Y = base_lo: the indexed mode does the
// lo add + carry for free.
static void emit_belem_ptr(uchar base, uchar idx) {
  if (is_const(idx)) em2(LDAN, const_val(idx));
  else               emop2(LDA, slotlo(idx));
  em2(STAZ, AI);
  if (is_const(idx)) em2(LDAN, const_val(idx) >> 8);
  else               emop2(LDA, slothi(idx));
  em(CLC);
  emit_alub(ADC, base, 1);
  em2(STAZ, AI + 1);
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
//                 (lc_z: taken over at the start of each op). 2: X is NOT
//                 set (byte in A only): next op is a zjp after which ACC
//                 is dead, it uses BEQ and then drops ACC
static uchar acc_x0, acc_z, lc_z;
// lc_axs: the slot physically in AX at this op's start, also when ACC is
// pending elsewhere (a const after 't': AX still = the loop var). Valid
// while no code is emitted after it was taken (lc_axs_at), 0xff unknown.
static uchar lc_axs;
static word  lc_axs_at;
// stride loop (while ... { body; U v s; B }): the test is on Y = lo, A = hi
// of v, as left by the 'U'. lc_yas: the slot in Y/A at this op's start
// (the body start, lc_ya_bc), valid like lc_axs while no code is emitted.
static uchar lc_yas, lc_ya_next;
static word  lc_yas_at, lc_ya_bc;
// Y = lo, A = hi of slot lc_yah ("op v; S d" leaves them), valid while no
// code is emitted (lc_yah_at == lc_at) and the op is not a label
static uchar lc_yah = 0xff;
static word  lc_yah_at = 0xffff;

// ---- element cache (AI) -------------------------------------------------
static void ai_drop(void) { lc_ai_kind = 0; lc_rel_t = 0xff; }
// AI == &base[idx] already? -> its Y offset (0 or, via lc_rel, 2*c), else
// 0xff
static uchar ai_hit(uchar kind, uchar base, uchar idx) {
  if (lc_ai_kind != kind || lc_ai_base != base) return 0xff;
  if (lc_ai_idx == idx) return 0;
  if (kind == 'w' && idx == lc_rel_t && lc_rel_src == lc_ai_idx)
    return (uchar)(lc_rel_c << 1);
  return 0xff;
}
// AI = &((word*)base)[idx] (or a cached one): its Y offset
static uchar ai_word(uchar base, uchar idx) {
  uchar y = ai_hit('w', base, idx);
  if (y != 0xff) return y;
  emit_elem_ptr(base, idx);
  if (!is_const(idx)) { lc_ai_kind = 'w'; lc_ai_base = base; lc_ai_idx = idx; }
  else lc_ai_kind = 0;
  return 0;
}
// (AI),Y = &((uchar*)base)[idx]: the pointer (or cached), then Y
static void ai_byte(uchar base, uchar idx) {
  if (ai_hit('b', base, idx) == 0xff) {             // the helpers set Y
    if (lc_axs == idx) emit_belem_ptr_ax(base);       // index in AX
    else if (lc_yas == idx) emit_belem_ptr_ya(base);  // in Y/A (stride)
    else emit_belem_ptr(base, idx);
    if (!is_const(idx)) { lc_ai_kind = 'b'; lc_ai_base = base; lc_ai_idx = idx; }
    else lc_ai_kind = 0;
    return;
  }
  if (is_const(base)) em2(LDYN, const_val(base));
  else                emop2(LDY, slotlo(base));
}

static word  acc_k;
static uchar lc_acc_said;   // acc_* calls by the current op
static uchar lc_op;         // current op (messages)
static uchar *lc_lbl;       // label bitmap, one bit per bytecode pc

static int is_label(word bc);
static int yah_is(uchar s) {
  return lc_yah == s && lc_yah_at == lc_at && !is_label(lc_bc);
}
static int is_label(word bc) {
  return lc_lbl && (lc_lbl[bc >> 3] & (1 << (bc & 7)));
}

// emit a pending load (no message if lost)
static void acc_flush(void) {
  if (acc_at == ACC_PEND) {
    if (!acc_hk && yah_is(acc_s)) { em(TAX); em(TYA); }    // Y/A: 4c, 2 B
    else if (acc_hk) emit_load_const(acc_k); else emit_load_slot(acc_s);
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
static void emit_acc_double(void);
static void emit_lda_pend(uchar hi);
// ACC <<= n / >>= n, n const. NEW. >= 8: a byte move (from where ACC is,
// if pending), then the rest in A; 7: a byte move and one bit back;
// < 7: hi via tmp2: ASL A; ROL tmp2
static void emit_shift(uchar right, uchar n) {
  uchar k;
  if (acc_at == ACC_PEND && acc_hk) {              // const: fold
    acc_pk(n > 15 ? 0 : right ? (word)(acc_k >> n) : (word)(acc_k << n));
    return;
  }
  if (!n) { acc_need(); return; }
  if (n > 15) { acc_drop(); emit_load0(); acc_ax(); acc_x0 = 1; return; }
  if (n == 1 && !right) { emit_acc_double(); return; }
  if (n >= 8) {
    if (acc_at == ACC_PEND) emit_lda_pend(right ? 1 : 0);
    else { acc_need(); if (right) em(TXA); }
    for (k = 8; k < n; k++) em(right ? LSRA : ASLA);
    if (right) { em2(LDXN, 0); acc_ax(); acc_x0 = 1; }
    else { em(TAX); em2(LDAN, 0); acc_ax(); }
    return;
  }
  if (n == 7) {                                   // a byte move + 1 bit
    word fx;
    if (!right) {        // hi = hi0:lo>>1, lo = lo0 << 7
      if (acc_at == ACC_PEND) { emit_lda_pend(1); em(LSRA); emit_lda_pend(0); }
      else { acc_need(); em(TAY); em(TXA); em(LSRA); em(TYA); }
      em(RORA); em(TAX); em2(LDAN, 0); em(RORA);
      acc_ax();
      return;
    }
    // lo = hi << 1 | lo7, hi = hi7
    if (acc_at == ACC_PEND) { emit_lda_pend(0); em(ASLA); emit_lda_pend(1); }
    else { acc_need(); em(ASLA); em(TXA); }
    em(ROLA); em2(LDXN, 0);
    fx = emrel(BCC); em(INX); empatch(fx);
    acc_ax();
    return;
  }
  acc_need();
  em2(STXZ, tmp2);
  for (k = 0; k < n; k++) {
    if (right) { em2(LSRZ, tmp2); em(RORA); }
    else       { em(ASLA); em2(ROLZ, tmp2); }
  }
  em2(LDXZ, tmp2);
  acc_ax();
}
// n if v == 1 << n, else 0xff
static uchar log2w(word v) {
  uchar n;
  for (n = 0; n < 16; n++) if (v == (word)(1u << n)) return n;
  return 0xff;
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
  return op < 32 || strchr("09Q12345678@r'l([KLCUIJijH;tun", op);
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
static const uchar *lc_code;  // the bytecode

// A taken branch to another page costs +1 cycle: a loop whose closing
// branch crosses a page gets its body moved to one page on the next run,
// with NOPs after the 't' JMP (never executed: costs bytes, no cycles).
#define JIT_ALIGN_LOOPS 0     // 1: pad innermost loops (sieve: avg -0.1%, +0..37 B)
static struct { word ibc, len; } lc_al[8];  // loops to align, body length
static uchar lc_nal;

// 't' after its JMP: pad if the body (len, from a previous run) would
// straddle a page
static void loop_pad(word ibc) {
  uchar k;
  for (k = 0; k < lc_nal; k++)
    if (lc_al[k].ibc == ibc) {
      word a = (word)((lc_out_org + lc_at) & 0xff);
      if (a + lc_al[k].len > 0x100)
        for (a = (word)(0x100 - a); a; a--) em(NOP);
      return;
    }
}

// after the closing test of the loop at ibc (innermost loops only: they
// run most, and are short): does its short branch back to the body cross
// a page? note it and run again
static void loop_align_check(word ibc) {
  word body = lc_map[ibc + 4], len = (word)(lc_at - body), b;
  uchar k;
  if (!JIT_ALIGN_LOOPS || body == 0xffff || len >= 128) return;
  for (b = (word)(ibc + 4); b < lc_bc; b += ivm_oplen(lc_code + b))
    if ((IS_LOOP(lc_code[b]) && is_loop_head(lc_code, b, lc_mapn)) ||
        !ivm_oplen(lc_code + b)) return;                    // not innermost
  if (lc_at >= 3 && lc_out[lc_at - 3] == JMP) return;   // long form
  if (((lc_out_org + body) ^ (lc_out_org + lc_at)) >> 8 == 0) return;
  for (k = 0; k < lc_nal && lc_al[k].ibc != ibc; k++) ;
  if (k == lc_nal) {
    if (lc_nal == 8) return;
    lc_nal++;
  } else if (lc_al[k].len == len) return;   // padded already: can't help
  lc_al[k].ibc = ibc; lc_al[k].len = len;
  lc_retry = 1;
}

// ---- rules: ops as data ---------------------------------------------
//
// Like a peephole code generator: an op, a guard (when this rule may be
// used: the first rule of the op whose guard holds wins), what it does
// with ACC before and after, and a template of 6502 bytes.
//
// The template is 6502 code where the addressing mode of each opcode says
// what follows it, and the opcodes a 6502 does not have (columns 2, 3, F)
// are the actions:
//
//   abs op  spec   (columns C D E) a slot operand: emop2(op, its address),
//                  zp form if it fits, #imm if a constant (and op has it).
//                  spec 1 / 2 = lo of slot operand 1 / 2, +16 = hi
//   R_ALU   spec   ($0F) the same with the ALU op of the op (+ ADC, - SBC,
//                  & AND, | ORA, ^ EOR)
//   (zp),Y op      on the pointer of R_PTR / R_ELW / R_ELB: no operand
//   branch op      forward branch (emrel): R_PATCH lands it
//   JSR n          library routine n (rule_ref)
//   #imm / zp op n emitted as they are, n included; others one byte
//                  (not BRK RTI RTS: not in templates)
//
//   R_PEND0 / 1    emit_lda_pend: LDA lo / hi of the pending ACC
//   R_PTR          zp pointer = mem[slot 1] (emit_ptr)
//   R_ELW / R_ELB  AI = &((word / uchar *)slot 1)[slot 2], y its offset
//   R_YK0 / 1      LDY #y / #y + 1
//   R_INC1         mem[slot 1]++ (emit_inc)
//
// Guard (all set bits must hold): G_PEND ACC pending, G_AX not pending,
// G_X0 / G_NX0 X == 0 known / not, G_ZP1 / G_NZP1 slot 1 in zp / not,
// the operand classes below. Op: the op, or for the + - & | ^ code (it
// tries two groups, '|' also for '^'): op | $80 group 1, op & $1F group
// 2, 1 any op of group 2.
// ACC before (any of): R_NEED load into AX, R_DROP not needed, R_WRITE it
// may be slot 1 (acc_write, before the guards: all rules of the op).
// After: R_SLOT = mem[slot 1] (pending), R_AXSLOT in AX and = mem[slot 1],
// R_LOST, R_AX, R_AXB1 / R_AXB0 a byte in AX (X = 0, Z of it valid / not),
// else kept.
#define G_PEND  0x0001
#define G_AX    0x0002
#define G_X0    0x0004
#define G_NX0   0x0008
#define G_ZP1   0x0010
#define G_NZP1  0x0020
// operand classes, like a type signature (%d %D %v ... of old): slot 1 is
#define G_K1    0x0040            // a constant
#define G_NK1   0x0080            // not a constant (a variable)
#define G_B1    0x0100            // < 256: const < 256 or a byte slot
#define G_HFF1  0x0200            // a constant with hi byte $FF
#define G_ACCB  0x0400            // ACC < 256: byte pending / AX with X = 0
#define G_LIVE  0x0800            // ACC not lost
#define G_AIW   0x1000            // &slot1[slot2] (word) in AI already
#define G_AIB   0x2000            // the same, uchar
#define R_NEED  0x01
#define R_DROP  0x02
#define R_WRITE 0x04
#define R_SLOT  0x10
#define R_LOST  0x20
#define R_AX    0x30
#define R_AXB1  0x40
#define R_AXB0  0x50
#define R_AXSLOT 0x60
// opcodes as strings: templates are concatenated string literals
#define _ADCN   "\x69"
#define _BCC    "\x90"
#define _BCS    "\xB0"
#define _BNE    "\xD0"
#define _CLC    "\x18"
#define _CMPN   "\xC9"
#define _DEC    "\xCE"
#define _DEX    "\xCA"
#define _DEY    "\x88"
#define _EORN   "\x49"
#define _INC    "\xEE"
#define _INX    "\xE8"
#define _INY    "\xC8"
#define _JSR    "\x20"
#define _LDA    "\xAD"
#define _LDAIY  "\xB1"
#define _LDAN   "\xA9"
#define _LDX    "\xAE"
#define _LDXN   "\xA2"
#define _LDYN   "\xA0"
#define _ORAZ   "\x05"
#define _PHA    "\x48"
#define _PLA    "\x68"
#define _ROLA   "\x2A"
#define _STA    "\x8D"
#define _STAIY  "\x91"
#define _STX    "\x8E"
#define _STXZ   "\x86"
#define _STY    "\x8C"
#define _TAX    "\xAA"
#define _TAY    "\xA8"
#define _TXA    "\x8A"
#define _TYA    "\x98"
#define _TMP1   "\xFE"                    // zp tmp1 (an operand)
// slot operands, actions
#define RLO1    "\x01"                    // slot operand 1: lo / hi
#define RHI1    "\x11"
#define R_ALU   "\x0F"                    // ALU op of the op, + slot spec
#define R_PEND0 "\x02"                    // LDA lo / hi of the pending ACC
#define R_PEND1 "\x12"
#define R_PTR   "\x22"                    // zp pointer = mem[slot 1]
#define R_INC1  "\x32"                    // mem[slot 1]++
#define R_PATCH "\x42"                    // the forward branch lands here
#define R_ELW   "\x52"                    // AI = &slot1[slot2], word
#define R_ELB   "\x62"                    // ... uchar
#define R_YK0   "\x72"                    // LDY #y
#define R_YK1   "\x82"                    // LDY #y + 1

#define RULE(op, g, acc, t) { op, acc, g, sizeof(t) - 1, t }
#define G1(op)  ((op) | 0x80)
#define G2(op)  ((op) & 0x1F)
static const struct {
  uchar op, acc; word g; uchar len; const char *t;
} jit_rules[] = {
  // ---- + - & | ^ (C before: fold, i + i, commutative swap; between the
  // groups: "op v; S d"). Group 1: operand < 256, or ACC < 256
  RULE(G1('+'), G_B1 | G_LIVE, R_NEED | R_AX,
    R_ALU RLO1 _BCC _INX R_PATCH),
  RULE(G1('-'), G_B1 | G_LIVE, R_NEED | R_AX,
    R_ALU RLO1 _BCS _DEX R_PATCH),
  RULE(G1('&'), G_B1 | G_LIVE, R_NEED | R_AX, R_ALU RLO1 _LDXN "\0"),
  RULE(G1('|'), G_B1 | G_LIVE, R_NEED | R_AX, R_ALU RLO1),
  RULE(G1('+'), G_ACCB | G_PEND, R_AX,
    R_PEND0 R_ALU RLO1 _LDX RHI1 _BCC _INX R_PATCH),
  RULE(G1('+'), G_ACCB | G_AX, R_AX,
    R_ALU RLO1 _LDX RHI1 _BCC _INX R_PATCH),
  RULE(G1('&'), G_ACCB | G_PEND, R_AX, R_PEND0 R_ALU RLO1 _LDXN "\0"),
  RULE(G1('&'), G_ACCB | G_AX, R_AX, R_ALU RLO1),
  RULE(G1('|'), G_ACCB | G_PEND, R_AX, R_PEND0 R_ALU RLO1 _LDX RHI1),
  RULE(G1('|'), G_ACCB | G_AX, R_AX, R_ALU RLO1 _LDX RHI1),
  // group 2: ACC pending in a slot: each byte from there; and $FFxx; any
  RULE(1, G_PEND | G_NK1, R_AX,
    R_PEND0 R_ALU RLO1 _TAY R_PEND1 R_ALU RHI1 _TAX _TYA),
  RULE(G2('&'), G_HFF1, R_NEED | R_AX, R_ALU RLO1),
  RULE(1, 0, R_NEED | R_AX, R_ALU RLO1 _TAY _TXA R_ALU RHI1 _TAX _TYA),

  // I: INC lo; BNE +; INC hi
  RULE('I', 0, R_DROP | R_SLOT, _INC RLO1 _BNE _INC RHI1 R_PATCH),
  // J: LDA lo; BNE +; DEC hi; + DEC lo
  RULE('J', 0, R_DROP | R_SLOT,
    _LDA RLO1 _BNE _DEC RHI1 R_PATCH _DEC RLO1),
  // i j: char ++ --: the hi byte of a char var is always 0
  RULE('i', 0, R_DROP | R_SLOT, _INC RLO1),
  RULE('j', 0, R_DROP | R_SLOT, _DEC RLO1),
  // H: int* ++ (word += 2): LDA; CLC; ADC #2; STA; BCC +; INC hi
  RULE('H', 0, R_DROP | R_SLOT,
    _LDA RLO1 _CLC _ADCN "\x02" _STA RLO1 _BCC _INC RHI1 R_PATCH),
  // Z: LDY #0; STY lo; STY hi (AX untouched)
  RULE('Z', 0, R_WRITE, _LDYN "\0" _STY RLO1 _STY RHI1),
  // F: free(mem[s]); mem[s] = 0
  RULE('F', 0, R_DROP | R_LOST,
    _LDA RLO1 _LDX RHI1 _JSR "\x03" _LDYN "\0" _STY RLO1 _STY RHI1),
  // M: mem[s] = malloc(ACC)
  RULE('M', 0, R_NEED | R_WRITE | R_AXSLOT,
    _JSR "\x04" _STA RLO1 _STX RHI1),
  // E . P: putchar / print number / string (ACC kept)
  RULE('E', 0, R_NEED, _JSR "\0"),
  RULE('.', 0, R_NEED, _JSR "\x01"),
  RULE('P', 0, R_NEED, _JSR "\x02"),
  // l: ACC = lo(mem[s]): LDX #0; LDA lo
  RULE('l', 0, R_DROP | R_AXB1, _LDXN "\0" _LDA RLO1),
  // s: mem[s] = lo(ACC), hi 0. Pending: from where it is (stays pending)
  RULE('s', G_PEND, R_WRITE, R_PEND0 _STA RLO1 _LDYN "\0" _STY RHI1),
  RULE('s', 0, R_WRITE | R_NEED, _STA RLO1 _LDYN "\0" _STY RHI1),
  // r: ACC = *(uchar*)mem[s]
  RULE('r', 0, R_DROP | R_AXB1, R_PTR _LDYN "\0" _LDXN "\0" _LDAIY),
  // ': ACC = *(uchar*)ptr; ptr++
  RULE('\'', 0, R_DROP | R_AXB0, R_PTR _LDYN "\0" _LDAIY _LDXN "\0" R_INC1),
  // w ,: *(uchar*)ptr = ACC (, then ptr++). AX saved if the pointer is
  // copied (not in zp)
  RULE('w', G_PEND, 0,      R_PTR R_PEND0 _LDYN "\0" _STAIY),
  RULE('w', G_ZP1,  R_NEED, R_PTR _LDYN "\0" _STAIY),
  RULE('w', 0,      R_NEED, _PHA R_PTR _PLA _LDYN "\0" _STAIY),
  RULE(',', G_PEND, R_WRITE, R_PTR R_PEND0 _LDYN "\0" _STAIY R_INC1),
  RULE(',', G_ZP1,  R_WRITE | R_NEED, R_PTR _LDYN "\0" _STAIY R_INC1),
  RULE(',', 0,      R_WRITE | R_NEED,
    _PHA R_PTR _PLA _LDYN "\0" _STAIY R_INC1),
  // @: ACC = *(word*)mem[s]: hi first, then lo
  RULE('@', 0, R_DROP | R_AX,
    R_PTR _LDYN "\x01" _LDAIY _TAX _DEY _LDAIY),
  // !: *(word*)mem[s] = ACC
  RULE('!', G_PEND, 0,
    R_PTR _LDYN "\x01" R_PEND1 _STAIY _DEY R_PEND0 _STAIY),
  RULE('!', 0, R_NEED,
    _PHA R_PTR _TXA _LDYN "\x01" _STAIY _PLA _DEY _STAIY),
  // ( bb ii: ACC = ((word*)base)[idx]: hi first, then lo
  RULE('(', 0, R_DROP | R_AX, R_ELW R_YK1 _LDAIY _TAX _DEY _LDAIY),
  // ) bb ii: ((word*)base)[idx] = ACC. Pending: from where it is; AI
  // there already: AX kept as is; else: AX saved over the AI setup
  RULE(')', G_PEND, 0,
    R_ELW R_YK1 R_PEND1 _STAIY _DEY R_PEND0 _STAIY),
  RULE(')', G_AIW, R_NEED,
    R_ELW R_YK0 _STAIY _PHA _TXA _INY _STAIY _PLA),
  RULE(')', 0, R_NEED,
    _PHA _TXA _PHA R_ELW _PLA R_YK1 _STAIY _TAX _PLA _DEY _STAIY),
  // ] bb ii: ((uchar*)base)[idx] = ACC (the same three)
  RULE(']', G_PEND, 0,     R_ELB R_PEND0 _STAIY),
  RULE(']', G_AIB, 0,      R_ELB _STAIY),
  RULE(']', 0,     R_NEED, _PHA R_ELB _PLA _STAIY),
  // ~: ACC = ~ACC (a pending const: folded, in C)
  RULE('~', 0, R_NEED | R_AX,
    _EORN "\xFF" _TAY _TXA _EORN "\xFF" _TAX _TYA),
  // ?: ACC = !ACC  (A = lo | hi; CMP #1: C = ACC != 0; A = !C, X = 0)
  RULE('?', G_X0 | G_AX, R_NEED | R_AXB1,
    _CMPN "\x01" _LDAN "\0" _TAX _ROLA _EORN "\x01"),
  RULE('?', 0, R_NEED | R_AXB1, _STXZ _TMP1 _ORAZ _TMP1
    _CMPN "\x01" _LDAN "\0" _TAX _ROLA _EORN "\x01"),
  { 0, 0, 0, 0, 0 }
};

static word rule_ref(uchar n) {
  return n == 0 ? REF(opputchar) : n == 1 ? REF(opputnum) :
         n == 2 ? REF(opputs) : n == 3 ? REF(opfree) : REF(opmalloc);
}

static int rule_guard(word g, const uchar *p) {
  uchar s = p[0];
  if ((g & G_PEND) && acc_at != ACC_PEND) return 0;
  if ((g & G_AX) && acc_at == ACC_PEND) return 0;
  if ((g & G_X0) && !acc_x0) return 0;
  if ((g & G_NX0) && acc_x0) return 0;
  if ((g & G_ZP1) && slotlo(s) >= 0xff) return 0;
  if ((g & G_NZP1) && slotlo(s) < 0xff) return 0;
  if ((g & G_K1) && !is_const(s)) return 0;
  if ((g & G_NK1) && is_const(s)) return 0;
  if ((g & G_B1) && !hi0(s)) return 0;
  if ((g & G_HFF1) && !(is_const(s) && (const_val(s) >> 8) == 0xff)) return 0;
  if ((g & G_LIVE) && acc_at == ACC_LOST) return 0;
  if ((g & G_ACCB) && !((acc_at == ACC_PEND &&
                         (acc_hk ? acc_k < 256 : is_byte(acc_s))) ||
                        (acc_at == ACC_AX && acc_x0))) return 0;
  if ((g & G_AIW) && ai_hit('w', s, p[1]) == 0xff) return 0;
  if ((g & G_AIB) && ai_hit('b', s, p[1]) == 0xff) return 0;
  return 1;
}

// emit the first rule of rop (see above) whose guard holds; op: the real
// op (R_ALU). p: its slot operands. 0: no rule
#define RMATCH(r) ((r) == rop || ((r) == 1 && rop < 0x20))
static int jit_rule_k(uchar rop, uchar op, const uchar *p) {
  const uchar *t, *e;
  uchar k, b, c, a, y = 0, zp = 0;
  word fx[2];
  uchar nfx = 0;

  // a write of slot 1 first (it may flush ACC): then the guards see the
  // ACC as the template will
  for (k = 0; (b = jit_rules[k].op) != 0 && !RMATCH(b); k++) ;
  if (!b) return 0;
  if (jit_rules[k].acc & R_WRITE) acc_write(p[0]);
  for (; (b = jit_rules[k].op) != 0; k++)
    if (RMATCH(b) && rule_guard(jit_rules[k].g, p)) break;
  if (!b) return 0;
  a = jit_rules[k].acc;
  if (a & R_NEED) acc_need();
  else if (a & R_DROP) acc_drop();

  t = (const uchar *)jit_rules[k].t;
  for (e = t + jit_rules[k].len; t < e; ) {
    b = *t++;
    c = b & 15;
    if (c >= 12) {                          // abs (R_ALU): slot operand
      c = *t++;
      if (b == 0x0F)
        b = op == '+' ? ADC : op == '-' ? SBC : op == '&' ? AND_ :
            op == '|' ? ORA : EOR;
      a = p[(c & 15) - 1];
      if (is_const(a) && immop(b))
        em2(immop(b), (uchar)((c & 16) ? const_val(a) >> 8 : const_val(a)));
      else emop2(b, (c & 16) ? slothi(a) : slotlo(a));
    } else if (c == 3 || (c == 2 && b != LDXN)) {           // actions
      switch (b) {
      case 0x02: case 0x12: emit_lda_pend(b >> 4); break;
      case 0x22: zp = emit_ptr(p[0]); break;
      case 0x32: emit_inc(p[0]); break;
      case 0x42: empatch(fx[--nfx & 1]); break;
      case 0x52: y = ai_word(p[0], p[1]); zp = AI; break;
      case 0x62: ai_byte(p[0], p[1]); zp = AI; break;
      default:   em2(LDYN, (uchar)(y + (b >> 7)));    // R_YK0 / 1
      }
    } else if ((b & 0x10) && c == 0) fx[nfx++ & 1] = emrel(b);
    else if ((b & 0x10) && c == 1) em2(b, zp);        // (zp),Y
    else if (b == 0x20) ema(JSR, rule_ref(*t++));
    else if (c <= 2 || (c >= 4 && c <= 6) || c == 9) em2(b, *t++);
    else em(b);
  }
  switch (jit_rules[k].acc & 0x70) {
  case R_SLOT:   acc_ps(p[0]); break;
  case R_LOST:   acc_lost(); break;
  case R_AX:     acc_ax(); break;
  case R_AXB1:   acc_ax_byte(1); break;
  case R_AXB0:   acc_ax_byte(0); break;
  case R_AXSLOT: acc_ax_slot(p[0]); break;
  default:       acc_keep();
  }
  return 1;
}
static int jit_rule(uchar op, const uchar *p) { return jit_rule_k(op, op, p); }

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

  lc_out = out; lc_at = 0; lc_max = out_max; lc_code = code;
  lc_map = map; lc_mapn = map_n; lc_mem = mem_base;
  lc_nfix = 0;
  lc_nloop = 0;
  lc_nksa = 0;
  lc_err = 0;
  acc_at = ACC_AX; acc_hk = 0; acc_s = 0xff; acc_x0 = 0; acc_z = 0;
  lc_axs = 0xff; lc_axs_at = 0xffff;
  lc_yas = 0xff; lc_yas_at = 0xffff; lc_ya_bc = 0xffff;
  ai_drop(); lc_plus_src = 0xff;
  lc_out_org = out_org;
  lc_ncall = ncall;
  lc_fbase = frame_base;
  lc_vec_bc = vec_bc;
  lc_cur_frame = frame_base ? frame_base[0] : 0;

  for (i = 0; i < code_len; i++)
    map[i] = 0xffff;

  // labels: every jump target, loop body starts (n jumps there), function
  // entries and main's trampoline. 't' / 'n' rel are not used by the JIT.
  memset(lc_wword, 0, sizeof lc_wword);
  if (frame_base)
    for (i = 0; i < ncall; i++) {                  // call frame headers
      uchar f = frame_base[i], k;
      for (k = 0; k <= FR_ACC; k++) wword((uchar)(f + k));
    }
  lc_lbl = (uchar *)calloc((code_len >> 3) + 1, 1);
  if (lc_lbl) {
    word t, n;
    #define LBL(x) if ((x) < code_len) lc_lbl[(x) >> 3] |= (uchar)(1 << ((x) & 7))
    for (i = 0; i < code_len; i += n) {
      n = ivm_oplen(code + i);
      if (!n) n = 1;
      if (strchr("SIJUCMH',tu", code[i]) && i + 1 < code_len)
        wword(code[i + 1]);                        // word write (byte slots)
      if (IS_LOOP(code[i]) && is_loop_head(code, i, code_len)) {
        LBL(i + 4);                                // body start
        continue;
      }
      if (code[i] == 'n') continue;
      t = ivm_jump(code + i, i);
      if (code[i] == 'B' && t < code_len && IS_LOOP(code[t]) &&
          i_exit_b(code, t, code_len) == i + 2)
        t = 0xffff;                // closes a rotated while: no jump to 't'
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
      t = IS_LOOP(code[i]) && is_loop_head(code, i, code_len) ? (word)(i + 4)
          : code[i] == 'n' || code[i] == 'B'
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
      // ('t': its JMP said so), else drop it: the 'B's come later
      for (i = 0; i < lc_nksa && lc_ksa[i].bc != bc_pc; i++) ;
      if (i < lc_nksa && !(acc_at == ACC_AX && acc_s == lc_ksa[i].slot))
        lc_ksa[i].bc = 0xffff, i = lc_nksa;
      if (i < lc_nksa) acc_ax_slot(lc_ksa[i].slot);
      else if (bc_pc == lc_ya_bc) acc_lost();     // stride body: Y/A = var
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
    // element cache: drop at joins, calls, loop ops, unknown ops; a write
    // to base or idx drops it, to t or src the a[j+c] relation
    if (is_label(bc_pc) || op >= 0xC0 || strchr("nBkxyzqX;Q", op) ||
        (IS_LOOP(op) && is_loop_head(code, bc_pc, code_len)))
      ai_drop();
    else if (op >= 32 && strchr("SIJUCMH',sijZF", op)) {
      uchar w = *p;
      if (lc_ai_kind && (w == lc_ai_base || w == lc_ai_idx)) lc_ai_kind = 0;
      if (w == lc_rel_t || w == lc_rel_src) lc_rel_t = 0xff;
      if (op == 'S' && lc_plus_src != 0xff && lc_plus_src != w) {
        lc_rel_t = w; lc_rel_src = lc_plus_src; lc_rel_c = lc_plus_c;
      } else if (op == 'C' && p[1] == lc_rel_t && w != lc_rel_src) {
        lc_rel_t = w;              // C t2 t: t2 == src + c too
      }
    }
    if (op != 'S') lc_plus_src = 0xff;
    if (acc_at == ACC_AX) { lc_axs = acc_s; lc_axs_at = lc_at; }
    else if (lc_at != lc_axs_at || is_label(bc_pc)) lc_axs = 0xff;
    if (bc_pc == lc_ya_bc) { lc_yas = lc_ya_next; lc_yas_at = lc_at; }
    else if (lc_at != lc_yas_at) lc_yas = 0xff;
    lc_z = (!is_label(bc_pc) && acc_at == ACC_AX) ? acc_z : 0; acc_z = 0;

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
        if (lc_z) {                                         // Z is ACC == 0
          emjump(BEQ, dest);
          if (lc_z == 2) { acc_lost(); goto report; }       // X never set
        }
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
      uchar s = PAR, sa[1];
      uchar ins = op == '+' ? ADC : op == '-' ? SBC : op == '&' ? AND_ :
                  op == '|' ? ORA : EOR;
      lc_plus_src = (op == '+' && acc_at == ACC_PEND && !acc_hk &&
                     is_const(s) && const_val(s) < 128) ? acc_s : 0xff;
      lc_plus_c = (uchar)(is_const(s) ? const_val(s) : 0);
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
      if (acc_at == ACC_PEND && !acc_hk && lc_axs == s && op != '-') {
        uchar x = acc_s;           // commutative: AX already holds mem[s],
        acc_ax_slot(s);            // so mem[s] OP mem[x] (k = i + prime
        s = x;                     // right after S prime): swap, go on
      }
      sa[0] = s;
      if (jit_rule_k(G1(op == '^' ? '|' : op), op, sa)) break;   // < 256
      if (p[0] == 'S' && !is_label((word)(bc_pc + 2)) &&
          (acc_at == ACC_AX || acc_at == ACC_PEND)) {
        // op v; S d: each byte straight into d, no TAX / TYA. Leaves
        // Y = lo, A = hi (lc_yah), ACC = mem[d]
        uchar d = p[1];
        if (acc_at == ACC_PEND) emit_lda_pend(0);
        emit_alub(ins, s, 0); emop2(STA, slotlo(d)); em(TAY);
        if (acc_at == ACC_PEND) emit_lda_pend(1); else em(TXA);
        emit_alub(ins, s, 1); emop2(STA, slothi(d));
        p += 2;
        acc_drop();                // AX no longer = old ACC
        acc_ps(d);
        lc_yah = d; lc_yah_at = lc_at;
        break;
      }
      jit_rule_k(G2(op == '^' ? '|' : op), op, sa);              // the rest
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
    case '<': case '>': {
      uchar s = PAR, skip, brc;
      word t = fuse_branch(code, (word)(p - code), code_len, &skip, &brc);
      if (t != 0xffff) {
        // compare + [? ...] + zjp / G: just the branch, no 0/1. Flags
        // C = !cond: '<' a - s, '>' s - a (a > s <=> s < a)
        if (acc_at == ACC_PEND && !acc_hk) {
          if (op == '<') emit_sub_flags(acc_s, s); else emit_sub_flags(s, acc_s);
        } else if (op == '<') {
          acc_need();
          emit_alub(CMP, s, 0); em(TXA); emit_alub(SBC, s, 1);
        } else {
          acc_need();
          em(SEC); em2(EORN, 0xff); emit_alub(ADC, s, 0);
          em(TXA); em2(EORN, 0xff); emit_alub(ADC, s, 1);
        }
        p += skip;
        emjump(brc, t);
        acc_lost();
        break;
      }
      if (op == '>') goto op_gt;
      acc_need();
      emit_alub(CMP, s, 0); em(TXA);               // C from lo compare
      emit_alub(SBC, s, 1);                        // C = ACC >= mem
      em2(LDAN, 0); em(TAX); em(ROLA); em2(EORN, 1);   // A = !C, X = 0
      acc_ax_byte(1);
      break;
    }
    // '>': ACC > mem  <=>  mem - ACC borrows  <=>  !C of (mem + ~ACC + 1)
    op_gt: {
      uchar s = p[-1];
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
      uchar n = is_const(s) ? log2w(const_val(s)) : 0xff;
      if (n != 0xff) {                            // power of 2: no JSR
        if (op != '%') { emit_shift(op == '/', n); break; }
        if (acc_at == ACC_PEND && acc_hk) {       // const % const
          acc_pk((word)(acc_k & (const_val(s) - 1)));
          break;
        }
        if (n == 0) { acc_drop(); acc_pk(0); break; }
        if (n < 8) {                              // lo & mask, X = 0
          if (acc_at == ACC_PEND) emit_lda_pend(0); else acc_need();
          if (n) em2(ANDN, (uchar)((1 << n) - 1));
          em2(LDXN, 0);
          acc_ax(); acc_x0 = 1;
        } else if (n == 8) {                      // lo, X = 0
          if (acc_at == ACC_PEND) emit_lda_pend(0); else acc_need();
          em2(LDXN, 0);
          acc_ax(); acc_x0 = 1;
        } else {                                  // hi & mask
          uchar m = (uchar)((1 << (n - 8)) - 1);
          if (acc_at == ACC_PEND) {
            emit_lda_pend(1); em2(ANDN, m); em(TAX); emit_lda_pend(0);
          } else {
            acc_need();
            em(TAY); em(TXA); em2(ANDN, m); em(TAX); em(TYA);
          }
          acc_ax();
        }
        break;
      }
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
      // ivm-lib: first in tmp1/tmp2 ($FE/$FF), second in AX, result AX
      if (op == '*' && acc_at == ACC_PEND && acc_hk && !is_const(s)) {
        // const * x: x first, the const as the multiplier (AX): fewer rounds
        emop2(LDA, slotlo(s)); em2(STAZ, tmp1);
        emop2(LDA, slothi(s)); em2(STAZ, tmp2);
        emit_load_const(acc_k);
      } else {
        if (acc_at == ACC_PEND) {          // straight from where it is
          emit_lda_pend(0); em2(STAZ, tmp1);
          emit_lda_pend(1); em2(STAZ, tmp2);
        } else {
          acc_need();
          em2(STAZ, tmp1); em2(STXZ, tmp2);
        }
        emit_load_slot(s);
      }
      if (op == '*') ema(JSR, REF(opmul));
      else if (op == '/') ema(JSR, REF(opdiv));
      else ema(JSR, REF(opmod));
      acc_ax();
      break;
    }

    // I: INC lo; BNE skip; INC hi; reload ACC

    // i j: char ++ --: the hi byte of a char var is always 0 (all its
    //   stores are 's', 'i', 'j'), so only the lo byte changes

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


    // byte slot
    // A bb ii jj: swap ((word*)base)[i] and [j]; ACC = the old [i]
    //   AS = &base[j], AI = &base[i] (cached: stays valid after)
    case 'A': {                                    // -, NEW
      uchar base = PAR, i = PAR, j = PAR;
      uchar yi = ai_hit('w', base, i), yj = ai_hit('w', base, j);
      acc_drop();
      if (yi != 0xff && yj != 0xff) {
        // both off AI (a[j], a[j+1]: the cache): Y offsets, no AS
        em2(LDYN, (uchar)(yi + 1)); em2(LDAIY, AI); em(TAX);   // hi
        em2(LDYN, (uchar)(yj + 1)); em2(LDAIY, AI);
        em2(LDYN, (uchar)(yi + 1)); em2(STAIY, AI);
        em2(LDYN, (uchar)(yj + 1)); em(TXA); em2(STAIY, AI);
        em2(LDYN, yi); em2(LDAIY, AI); em(PHA);                // lo
        em2(LDYN, yj); em2(LDAIY, AI);
        em2(LDYN, yi); em2(STAIY, AI);
        em2(LDYN, yj); em(PLA); em2(STAIY, AI);
        acc_ax();                                  // A X = old [i]
        break;
      }
      emit_elem_ptr_zp(AS, base, j);
      if (ai_hit('w', base, i) != 0) {
        emit_elem_ptr(base, i);
        if (!is_const(i)) { lc_ai_kind = 'w'; lc_ai_base = base; lc_ai_idx = i; }
        else lc_ai_kind = 0;
      }
      em2(LDYN, 1); em2(LDAIY, AI); em(TAX);       // hi
      em2(LDAIY, AS); em2(STAIY, AI); em(TXA); em2(STAIY, AS);
      em(DEY);      em2(LDAIY, AI); em(PHA);       // lo
      em2(LDAIY, AS); em2(STAIY, AI); em(PLA); em2(STAIY, AS);
      acc_ax();                                    // A X = old [i]
      break;
    }

    // [ bb ii : ACC = ((uchar*)base)[idx]
    case '[': {                                    // NEW
      uchar base = PAR, idx = PAR;
      ai_byte(base, idx);
      acc_drop();
      {                                    // next: zjp, ACC dead after it?
        word nb = (word)(p - code), t;
        if (nb + 1 < code_len && (uchar)(code[nb] - 0x80) < 64 &&
            !is_label(nb) && acc_unread(code, (word)(nb + 1)) &&
            (t = ivm_jump(code + nb, nb)) < code_len && acc_unread(code, t)) {
          em2(LDAIY, AI);                  // only A: Z == (byte == 0)
          acc_ax(); acc_z = 2;
          break;
        }
      }
      em2(LDXN, 0); em2(LDAIY, AI);        // LDA last: Z == (ACC == 0)
      acc_ax_byte(1);
      break;
    }
    // ~: ACC = ~ACC
    case '~':                                      // in, NEW
      if (acc_at == ACC_PEND && acc_hk) { acc_pk((word)~acc_k); break; }
      jit_rule(op, p);
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
      if (IS_LOOP(code[dest]) && i_exit_b(code, dest, code_len) == p - code &&
          is_stride(code, dest, code_len)) {
        acc_lost();                // stride: Y/A from the 'U' just before
        loop_close(dest);
        emit_loop_test_ya(code[dest + 2], (word)(dest + 4), code[dest] == 'u');
        loop_align_check(dest);
        break;                     // fell out: Y/A = var, ACC undefined
      }
      if (IS_LOOP(code[dest]) && i_exit_b(code, dest, code_len) == p - code) {
        acc_lost();                // closes a while: the test is here
        loop_close(dest);
        emit_loop_test(code[dest + 1], code[dest + 2], (word)(dest + 4), 1,
                       code[dest] == 'u');
        loop_align_check(dest);
        acc_ax_slot(code[dest + 1]);   // fell out: AX = the var
        break;
      }
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

    // # name 0: #TIMED. Host sim: JSR to a trap that prints the cycles
    // since the last one (KEEP: A X Y kept), else nothing at all
    case '#':
      #ifdef HOSTSIM
        ema(JSR, REF(optimed));
        while (p < end && *p) { em(*p); p++; }
        em(0);
      #else
        while (p < end && *p) p++;
      #endif
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

    // t: FOR slot lim skip -- rotated loop: the test is only in 'n'
    //   t:    JMP test          (operand patched by the matching 'n')
    //   body: ...               (AX = mem[slot] here, from the test)
    //   n:    INC slot; test: LDA/LDX slot; CPX/CMP lim; Bcc body
    //   exit:                   (falls out of the test: skip is not used)
    case 't': case 'u': {
      // does the body start by reading the loop var? (a load of it, or
      // '[' with it as index): then the test leaves it in AX
      const uchar *b = p + 3;
      uchar slot = p[0], ax;
      if (!is_loop_head(code, bc_pc, code_len)) {
        // if (l < r) / (l <= r): to the body, else out. u: out if l >= r
        // (l - r: C), t: out if r < l (r - l: !C). -, UNDEF
        uchar l = p[0], r = p[1];
        word ex = ivm_jump(code + bc_pc, bc_pc);
        p += 3;
        acc_drop();
        if (op == 'u') { emit_sub_flags(l, r); emjump(BCS, ex); }
        else           { emit_sub_flags(r, l); emjump(BCC, ex); }
        acc_lost();
        break;
      }
      if (is_stride(code, bc_pc, code_len)) {      // test on Y/A: set them
        uchar lim = p[1];
        (void)lim;
        p += 3;
        if (yah_is(slot)) {
          // already: Y = lo, A = hi ("+ v; S slot")
        } else if (lc_axs == slot || (acc_at == ACC_AX && acc_s == slot)) {
          em(TAY); em(TXA);
        } else {
          emop2(LDY, slotlo(slot)); emop2(LDA, slothi(slot));
        }
        acc_lost();
        lc_ya_next = slot; lc_ya_bc = (word)(bc_pc + 4);
        em(JMP);
        if (lc_nloop < MAX_LOOPS) {
          lc_loop[lc_nloop].ibc = bc_pc;
          lc_loop[lc_nloop].at  = lc_at;
          lc_loop[lc_nloop].ax  = 0;
          lc_nloop++;
        } else
          printf("%%JIT: loops nested too deep\n");
        em(0); em(0);
        loop_pad(bc_pc);
        break;
      }
      // while (closed by 'B'): no increment, the test always loads it
      ax = i_exit_b(code, bc_pc, code_len) != 0xffff ||
           ((b < end) && (b[0] == slot || (b[0] == 'L' && b[1] == slot) ||
                          (b[0] == '[' && b[2] == slot)));
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
      loop_pad(bc_pc);
      break;
    }

    // n: NEXT back
    // increment the loop slot (from the matching t), then the loop test
    // ('t' jumps here too): back to the body while <= lim, else fall out.
    case 'n': {
      uchar back = PAR;
      // the t is `back` bytes before this operand
      // t layout: slot, lim, skip — we need those
      const uchar *ip = p - back;       // operand_pos - back + 1
      word ibc;
      uchar ax;
      acc_lost();                  // -, UNDEF
      if (ip < code || !IS_LOOP(*ip)) {
        // can't find matching t; just consume
        break;
      }
      ibc = (word)(ip - code);
      // increment slot
      emit_inc(ip[1]);
      ax = loop_close(ibc);        // the t's JMP lands on the test
      // body = bc pc after the t (backward: short branch when it fits)
      emit_loop_test(ip[1], ip[2], (word)(ibc + 4), ax, *ip == 'u');
      loop_align_check(ibc);
      if (ax) acc_ax_slot(ip[1]);  // loop done: AX = the loop var
      break;
    }

    // U: mem[a] += mem[b]; ACC = result. Two slot operands.
    case 'U': {                                    // -, SLOT (a)
      uchar a = PAR;
      uchar b = PAR;
      // stride loop: next is its closing 'B': leave Y = lo, A = hi
      if (p[0] == 'B') {
        word d = (word)((p + 2 - code) - p[1]);
        if (IS_LOOP(code[d]) && code[d + 1] == a &&
            i_exit_b(code, d, code_len) == (word)(p + 2 - code) &&
            is_stride(code, d, code_len)) {
          acc_drop();
          em(CLC);
          emop2(LDA, slotlo(a)); emit_alub(ADC, b, 0); emop2(STA, slotlo(a));
          em(TAY);
          emop2(LDA, slothi(a)); emit_alub(ADC, b, 1); emop2(STA, slothi(a));
          acc_lost();              // -, SLOT: ACC unused, the 'B' follows
          break;
        }
      }
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

    // { }: ACC <<= / >>= mem[s]. Const: emit_shift; else a loop on Y
    case '{': case '}': {                          // in, NEW
      uchar s = PAR;
      word top, fx;
      if (is_const(s)) {
        emit_shift(op == '}', (uchar)(const_val(s) > 15 ? 16 : const_val(s)));
        break;
      }
      acc_need();
      em2(STXZ, tmp2);
      emop2(LDY, slotlo(s));
      fx = emrel(BEQ);
      top = lc_at;
      if (op == '}') { em2(LSRZ, tmp2); em(RORA); }
      else           { em(ASLA); em2(ROLZ, tmp2); }
      em(DEY);
      em2(BNE, (uchar)(top - (lc_at + 2)));
      empatch(fx);
      em2(LDXZ, tmp2);
      acc_ax();
      break;
    }

    // m: memset bytes. ACC = count, ptr slot, val slot (lo byte = fill).
    //   const 1..256: inline  LDY #n; LDA #v; l: DEY; STA (zp),Y; BNE l
    //   else: X = pages, Y = rest, the routine's STA $xxxx,Y operand =
    //   ptr + X*256 (last page), A = fill; JSR _opmemset (ivm-lib.asm):
    //   rest of the last page first, then full pages down. Max 32K (BPL).
    case 'm': {
      uchar ptr = PAR;
      uchar val = PAR;
      // in (count), KEEP: the code below uses A X Y. A pending count is
      // loaded straight into X (hi) and Y (lo, last) and stays pending; a
      // count in AX: TAY, then it is lost.
      uchar pend = (acc_at == ACC_PEND);

      if (pend && acc_hk && acc_k <= 256) {         // small: all inline
        uchar zp;
        if (acc_k) {
          zp = emit_ptr(ptr);
          em2(LDYN, (uchar)acc_k);                  // 256: 0, DEY wraps
          if (is_const(val)) em2(LDAN, (uchar)const_val(val));
          else               emop2(LDA, slotlo(val));
          em(DEY); em2(STAIY, zp); em2(BNE, (uchar)-5);
        }
        acc_keep();
        break;
      }

      if (pend) {
        if (acc_hk) em2(LDXN, (uchar)(acc_k >> 8));
        else        emop2(LDX, slothi(acc_s));
      } else {
        acc_need();
        em(TAY);
      }

      // the routine's STA operand = ptr + X*256  (last page)
      if (is_const(ptr)) em2(LDAN, (uchar)const_val(ptr));
      else               emop2(LDA, slotlo(ptr));
      ema(STA, REF(opmemset_adr));
      em(TXA); em(CLC);
      emit_alub(ADC, ptr, 1);
      ema(STA, (word)(REF(opmemset_adr) + 1));

      // fill byte, rest
      if (is_const(val)) em2(LDAN, (uchar)const_val(val));
      else               emop2(LDA, slotlo(val));
      if (pend) {
        if (acc_hk) em2(LDYN, (uchar)acc_k);
        else        emop2(LDY, slotlo(acc_s));
      }
      ema(JSR, REF(opmemset));

      if (pend) acc_keep();               // its source is unchanged
      else acc_lost();
      break;
    }

    default:
      if (jit_rule(op, p)) { p += ivm_oplen(p - 1) - 1; break; }
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
  lc_nal = 0;
  do {
    lc_retry = 0;
    r = ivm_jit1(code, code_len, out, out_max, map, map_n, mem_base,
                 out_org, vec_bc, frame_base, ncall);
    if (lc_retry) printf("%%JIT: again (short branch too far / loop align)\n");
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
//   $0300        mem[] (without -z), then the strings
//   org          JIT code ($1000 default, -a XXXX), then the heap to $F000
//   $FF00        ivm-lib traps (SIM_*), $FFF0 cpu_call sentinel
//
// Lib calls run in C and cost 0 cycles: cycle counts are the JIT code only.
// ---------------------------------------------------------------------------
static word sim_heap, sim_heapend;
static unsigned long sim_traps;
static unsigned long sim_t_cyc, sim_t_xp;   // at the last #TIMED
static word sim_t_pc;                       // code after the last #TIMED

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
  word ax = (word)(c->a | (c->x << 8)), jt = (word)SIM_W(0xFE), t;
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
  case SIM_opputnum: printf("%u", ax); break;              // AX kept
  case SIM_opputs:
    for (t = ax; t && cpu_mem[t]; t++) putchar(cpu_mem[t]);
    break;
  case SIM_opprintstr:
    // the string follows the JSR: print it, return after its 0
    t = cpu_mem[0x101 + c->s];
    t |= (word)(cpu_mem[0x102 + c->s] << 8);
    while (cpu_mem[++t]) putchar(cpu_mem[t]);
    cpu_mem[0x101 + c->s] = (uchar)t;                 // RTS: t + 1
    cpu_mem[0x102 + c->s] = (uchar)(t >> 8);
    break;
  // $FE/$FF OP AX -> AX; /0: AX = $FE/$FF (as the VM: ACC unchanged)
  case SIM_opmul: SIM_AX(c, (word)(jt * ax)); break;
  case SIM_opdiv: SIM_AX(c, ax ? (word)(jt / ax) : jt); break;
  case SIM_opmod: SIM_AX(c, ax ? (word)(jt % ax) : jt); break;
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
  case SIM_optimed: {                                 // JSR + name + 0
    // since the last #TIMED (or the start): cycles without this JSR's 6,
    // code bytes from the end of the last one to this JSR
    //   =======      4711c    32B - name   (+ page-crossing cycles if any)
    // no name (bare #TIMED): print nothing, just start counting here
    unsigned long now = c->cycles - 6;
    t = cpu_mem[0x101 + c->s];
    t |= (word)(cpu_mem[0x102 + c->s] << 8);
    if (cpu_mem[t + 1]) {
      printf("\n\n======= %9luc %5uB - ", now - sim_t_cyc,
        (unsigned)(word)(t - 2 - sim_t_pc));
      while (cpu_mem[++t]) putchar(cpu_mem[t]);
      if (c->xpage - sim_t_xp) printf("  +%luc page", c->xpage - sim_t_xp);
      putchar('\n');
    } else t++;
    sim_t_pc = (word)(t + 1);
    cpu_mem[0x101 + c->s] = (uchar)t;                 // RTS: t + 1
    cpu_mem[0x102 + c->s] = (uchar)(t >> 8);
    sim_t_cyc = c->cycles;
    sim_t_xp = c->xpage;
    break;
  }
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
    if (!zp_at) zp_at = (uchar)(ZPTOP - membytes);
    if (membytes > ZPTOP || zp_at + membytes > ZPTOP) {
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
  {                                       // _opmemset, as in ivm-lib.asm
    static const uchar sim_memset[] = {
      0xC0, 0x00,        //       CPY #0
      0xF0, 0x06,        //       BEQ page
      0x88,              // loop: DEY
      0x99, 0xFF, 0xFF,  //       STA $FFFF,Y   (SIM_opmemset_adr)
      0xD0, 0xFA,        //       BNE loop
      0xCE, 0x07, 0xFE,  // page: DEC SIM_opmemset_adr+1
      0xCA,              //       DEX
      0x10, 0xF4,        //       BPL loop
      0x60               //       RTS
    };
    memcpy(cpu_mem + SIM_opmemset, sim_memset, sizeof(sim_memset));
  }

  printf("--- SIM: mem[] $%04X (%u B), strings $%04X, code $%04X-$%04X"
         " (%u B), heap $%04X ---\n", (unsigned)mem_base, (unsigned)membytes,
         (unsigned)data, (unsigned)org, (unsigned)(org + size - 1),
         (unsigned)size, (unsigned)((org + size + 0xFF) & ~0xFF));

  // run
  sim_heap = (word)((org + size + 0xFF) & ~0xFF);   // page: data alignment
                                          // must not change with code size
  sim_heapend = 0xF000;
  sim_traps = 0;
  sim_t_cyc = sim_t_xp = 0;
  sim_t_pc = org;
  cpu_trap = sim_trap;
  cpu_trap_lo = SIM_opputnum;
  memset(&cpu, 0, sizeof(cpu));
  memset(cpu_xpage_at, 0, sizeof(cpu_xpage_at));
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
  if (cpu.xpage) {                     // layout: branches across a page
    unsigned long n = 0; word at = 0; long k;
    for (k = 0; k < 65536L; k++)
      if (cpu_xpage_at[k] > n) { n = cpu_xpage_at[k]; at = (word)k; }
    printf("--- SIM: page-crossing branches cost %lu (most: $%04X"
      " %lu times) ---\n", cpu.xpage, (unsigned)at, n);
  }

  free(img); free(map); free(out);
  return (word)(cpu.a | (cpu.x << 8));
}
#endif // HOSTSIM

#endif
