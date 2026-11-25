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
| Loops `t` / `n` / `B` | **Present** — unsigned. `t` closed by a `B` back to it (`while (v <= lim)`, i_exit_b): rotated too, the `B` has the test (loop_close), AX = v in the body and after. Stride loop (is_stride: body ends `U v s; B`): `U` leaves Y = lo, A = hi, the `B` tests those (`cmp/cpy`), body start has v in Y/A (lc_yas: `[` `]` index), `t` sets Y/A then JMPs to the test. Closed by `n`, rotated: `t` = JMP to the test in `n` (patched there, lc_loop[]), one compare per loop. The test always leaves the loop var in AX (cheaper: high byte first), so the body's load of it / `[` index is free; a `B` back to the body start loads it first (lc_ksa[]) |
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
// strings / memory (V $ x y): arguments in ivmlib_args (a0 a1 a2), AX out
extern uchar ivmlib_args[];
extern word ivmlib_libv[];    // x / y id 16 on: the routines' addresses
extern void opmove(void), opstrlen(void), opstrcmp(void), opstrncmp(void),
  opmemcmp(void), opstrchr(void), opmemchr(void), opstrcpy(void),
  opstrncpy(void);
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
    SIM_opmalloc, SIM_opfree, SIM_optimed,
    SIM_opmove, SIM_opstrlen, SIM_opstrcmp, SIM_opstrncmp, SIM_opmemcmp,
    SIM_opstrchr, SIM_opmemchr, SIM_opstrcpy, SIM_opstrncpy, SIM_end
  };
  #define SIM_ivmlib_args  0xFDF0        // their arguments (a0 a1 a2)
  // opmemset is real 6502 code (its loop costs cycles): sim_memset[] at
  // $FE00, below the traps. Same bytes as _opmemset in ivm-lib.asm.
  #define SIM_opmemset     0xFE00
  #define SIM_opmemset_adr 0xFE06
  // writing an image (ivm-image.c, -o): the lib's real addresses, from
  // ivm-lib-TARGET.bin; else the traps
  static uchar *hl_lib;
  static word hl_sym(const char *name);
  static word hl_vec(const char *name, uchar i);   // word i of a lib table
  #define REF(f) (hl_lib ? hl_sym("_" #f) : (word)SIM_##f)
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
#define _AIZ  "\xFC"                     // AI, AI + 1 in templates
#define _AIZ1 "\xFD"
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
static const uchar *lc_code;  // the bytecode
static word lc_len;           // its length
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
  LcFix *f;

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

  // a fixup: patched at the end
  if (lc_nfix >= 256) { lc_err = 1; return; }
  f = &lc_fix[lc_nfix++];
  f->bc_target = bc_target;
  f->bc_from = lc_bc;

  // short: the offset patched
  if (bop != JMP && !IS_LONG(lc_bc) &&
      (long)(bc_target - lc_bc) * JIT_BC_SCALE <= 127) {
    em2(bop, 0);
    f->at = (word)(lc_at - 1);
    f->sh = 1;

  // long: the opposite cond skips a JMP, its address patched
  } else {
    if (bop != JMP) em2(rev_branch(bop), 3);
    em(JMP);
    at = lc_at;
    em(0); em(0);
    f->at = at;
    f->sh = 0;
  }
}


// consts are not in an image's mem[] (from lc_kcut on: their values are
// immediates); a read of one from memory sets lc_kread: lay out again with them
static uchar lc_kcut = 0xff, lc_kread;

static word slotlo(uchar s) {
  if (s >= lc_kcut) lc_kread = 1;
  return (word)(lc_mem + ((word)s << 1));
}
static word slothi(uchar s) { return (word)(slotlo(s) + 1); }

static uchar lc_constidx;     // first constant slot (255 = none)
static word *lc_constmem;     // mem array with constant values filled

static int is_const(uchar s) {
  return lc_constidx != 255 && s >= lc_constidx && lc_constmem;
}

static word const_val(uchar s) { return lc_constmem[s]; }

// byte slots: hi byte always 0. A slot is a byte slot unless some op
// writes a word to it (S I J U C M H ' , t), or it is a frame header
// (call: ret, prev, ACC). Only 's' 'i' 'j' 'Z' 'Y' 'F' write them. Prescan.
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

// ---- templates: 6502 code as data (see "rules" below) -------------
//
// emit_tpl interprets one; TPL(t, slots) emits a template t (a string
// literal) on the slot operands slots, TPLOP the same for an ALU op.
static void emit_tpl(const uchar *t, uchar len, const uchar *p, uchar op);
// template inputs from C: Y offsets (R_YK R_YJ), the zp pointer at the
// start (R_ZAI / R_ZAS change it), the bytecode target (R_BRD)
static uchar lc_ty0, lc_ty1, lc_tzp = 0xfc;
static word  lc_tdest;
#define TPL(t, p)       emit_tpl((const uchar *)(t), sizeof(t) - 1, p, 0)
#define TPLOP(t, p, op) emit_tpl((const uchar *)(t), sizeof(t) - 1, p, op)

// opcodes as strings: templates are concatenated string literals
#define _ADC    "\x6D"
#define _ADCN   "\x69"
#define _ASLA   "\x0A"
#define _BEQ    "\xF0"
#define _CPX    "\xEC"
#define _CPY    "\xCC"
#define _CMP    "\xCD"
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
#define _LDAAY  "\xB9"
#define _LDXAY  "\xBE"
#define _LDAN   "\xA9"
#define _LDAZ   "\xA5"
#define _CPXN   "\xE0"
#define _LDXZ   "\xA6"
#define _LDY    "\xAC"
#define _LSRZ   "\x46"
#define _ROLZ   "\x26"
#define _LSRA   "\x4A"
#define _RORA   "\x6A"
#define _RTS    "\x60"
#define _SBC    "\xED"
#define _SEC    "\x38"
#define _LDX    "\xAE"
#define _LDXN   "\xA2"
#define _LDYN   "\xA0"
#define _ORAZ   "\x05"
#define _PHA    "\x48"
#define _PLA    "\x68"
#define _ROLA   "\x2A"
#define _STA    "\x8D"
#define _STAIY  "\x91"
#define _STAAY  "\x99"
#define _STAZ   "\x85"
#define _STX    "\x8E"
#define _STXZ   "\x86"
#define _STY    "\x8C"
#define _STYZ   "\x84"
#define _TAX    "\xAA"
#define _TAY    "\xA8"
#define _TXA    "\x8A"
#define _JMP    "\x4C"
#define _TYA    "\x98"
#define _TMP1   "\xFE"                    // zp tmp1 / tmp2 (operands)
#define _TMP2   "\xFF"
// slot specs (after an abs op or R_ALU): which slot, lo / hi
#define RLO1    "\x01"                    // slot operand 1: lo / hi
#define RHI1    "\x11"
#define RLO2    "\x02"                    // slot operand 2
#define RHI2    "\x12"
#define RLOV    "\x03"                    // return value cell (FR_VAL)
#define RHIV    "\x13"
#define RS_HI   0x10                      // spec: hi byte
#define RS_VAL  3                         // spec: the return value cell
#define RLOA    "\x04"                    // the pending ACC (const / slot)
#define RHIA    "\x14"
#define RS_ACC  4
#define RLOM    "\x05"                    // opmemset_adr: the routine's STA
#define RHIM    "\x15"                    // operand (+ 1)
#define RS_MSA  5
#define RKA1    "\x06"                    // (abs,Y) address: slot 1's const
#define RKK     "\x07"                    // abs address: slot 1 + slot 2,
#define RS_KK   7                         // both consts (a[k])
#define RKW     "\x08"                    // ... slot 1 + 2 * slot 2 (word
#define RKWH    "\x18"                    // a[k], a static struct's s.f)
#define RS_KW   8
#define RKA3    "\x09"                    // slot 1 + 1's const as the address
#define RS_KA3  9                         // (abs,Y too: a striped hi)
#define RK32    "\x0A"                    // that + slot 2, consts (the hi
#define RS_K32  10                        // byte of a striped a[k])

// library routines (JSR n, rule_ref)
#define LIB_PUTCHAR 0
#define LIB_PUTNUM  1
#define LIB_PUTS    2
#define LIB_FREE    3
#define LIB_MALLOC  4
#define LIB_MUL     5
#define LIB_DIV     6
#define LIB_MOD     7
#define LIB_PRINTSTR 8
#define LIB_MEMSET  10
#define LIB_GETCHAR 11
#define LIB_TIMED   9
#define LIB_MULDIV  0xFF                  // MUL / DIV / MOD by the op
#define L_PUTCHAR   "\x00"
#define L_PUTNUM    "\x01"
#define L_PUTS      "\x02"
#define L_FREE      "\x03"
#define L_MALLOC    "\x04"
#define L_MUL       "\x05"
#define L_PRINTSTR  "\x08"
#define L_MEMSET    "\x0A"
#define L_GETCHAR   "\x0B"
#define L_TIMED     "\x09"
#define L_MULDIV    "\xFF"

// actions: the opcodes a 6502 does not have (columns 2, 3, F; not LDX #)
#define A_ALU    0x0F                     // ALU op of the op, + slot spec
#define A_PEND0  0x02                     // LDA lo / hi of the pending ACC
#define A_PEND1  0x12
#define A_PTR    0x22                     // zp pointer = mem[slot 1]
#define A_INC1   0x32                     // mem[slot 1]++
#define A_PATCH  0x42                     // the forward branch lands here
#define A_ELW    0x52                     // AI = &slot1[slot2], word
#define A_ELB    0x62                     // ... uchar
#define A_YK0    0x72                     // LDY #lc_ty0 / #lc_ty0 + 1
#define A_YK1    0x82
#define A_TOP    0x92                     // loop top ...
#define A_BACK   0xB2                     // ... branch back to it: + brop
#define A_STR    0xC2                     // the inline string, 0 too
#define A_PATCHU 0xD2                     // A_PATCH the one under the top
#define A_LD2    0xE2                     // AX = mem[slot 2] (emit_load_slot)
#define A_LDO    0xF2                     // LDA #operand byte 1
#define A_NOW    0x03                     // acc_need() here
#define A_LD1    0x13                     // AX = mem[slot 1]
#define A_REP    0x23                     // next len bytes, lc_rn times
#define A_SUB8   0x33                     // lc_rn -= 8
#define A_LDYRN  0x43                     // LDY #lc_rn
#define A_YJ0    0x53                     // LDY #lc_ty1 / #lc_ty1 + 1
#define A_YJ1    0x63
#define A_ZAI    0x73                     // (zp),Y / zp ops: on AI / AS
#define A_ZAS    0x83
#define A_BRD    0x93                     // emjump(op, lc_tdest): + op
#define A_FOLD   0xA3                     // ACC = const ACC op const slot 1
#define A_CARRY  0xB3                     // CLC for +, SEC for -
#define A_ELBK   0xC3                     // Y = idx lo, A = idx hi + base hi
                                          // -> the next abs,Y's hi (STA)
// ... the same as template strings
#define R_ALU    "\x0F"
#define R_PEND0  "\x02"
#define R_PEND1  "\x12"
#define R_PTR    "\x22"
#define R_INC1   "\x32"
#define R_PATCH  "\x42"
#define R_ELW    "\x52"
#define R_ELB    "\x62"
#define R_YK0    "\x72"
#define R_YK1    "\x82"
#define R_TOP    "\x92"
#define R_BACK   "\xB2"
#define R_STR    "\xC2"
#define R_PATCHU "\xD2"
#define R_LD2    "\xE2"
#define R_LDO    "\xF2"
#define R_NOW    "\x03"
#define R_LD1    "\x13"
#define R_REP    "\x23"
#define R_SUB8   "\x33"
#define R_LDYRN  "\x43"
#define R_YJ0    "\x53"
#define R_YJ1    "\x63"
#define R_ZAI    "\x73"
#define R_ZAS    "\x83"
#define R_BRD    "\x93"
#define R_FOLD   "\xA3"
#define R_CARRY  "\xB3"
#define R_ELBK   "\xC3"
#define _TZP     "\x00"                   // zp op on the pointer (zp), + 1
#define _TZP1    "\x01"

static void emit_load_slot(uchar s) {
  if (is_const(s)) { emit_load_const(const_val(s)); return; }
  emop2(LDA, slotlo(s));
  emop2(LDX, slothi(s));
}
static void emit_store_slot(uchar s) { TPL(_STA RLO1 _STX RHI1, &s); }
static void emit_load0(void) { em2(LDAN, 0); em(TAX); }

// abs opcode → immediate form, 0 if none. Column D: imm = abs - 4
// (not STA); LDY CPY CPX (AC CC EC): - 12; LDX: LDXN
static uchar immop(uchar ins) {
  switch (ins & 15) {
  case 0xD: return (uchar)(ins == STA ? 0 : ins - 4);
  case 0xC: return (uchar)(ins >= LDY ? ins - 12 : 0);
  case 0xE: return (uchar)(ins == LDX ? LDXN : 0);
  }
  return 0;
}

// ACC = ACC OP mem[s]  (ADC/SBC/AND/ORA/EOR abs).
// CLC/SEC emitted by caller when needed (+/-). Reuse for logical ops too.
static void emit_alub(uchar ins, uchar s, uchar hi);

// mem[s]++ (word)
static void emit_inc(uchar s) { TPL(_INC RLO1 _BNE _INC RHI1 R_PATCH, &s); }

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
  if (is_const(s) && immop(ins)) {
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
static int acc_unread(word bc);
static int is_label(word bc);
static word fuse_branch(word nb, uchar *skip, uchar *brc) {
  const uchar *c = lc_code;
  word i = nb, t;
  uchar k = 0, jz;
  while (i < lc_len && c[i] == '?' && k < 3) {
    if (is_label(i)) return 0xffff;
    i++; k++;
  }
  if (i >= lc_len || is_label(i)) return 0xffff;
  if ((uchar)(c[i] - 0x80) < 64) { jz = 1; *skip = (uchar)(i - nb + 1); }
  else if (c[i] == 'G' && i + 1 < lc_len) { jz = 0; *skip = (uchar)(i - nb + 2); }
  else return 0xffff;
  t = ivm_jump(c + i, i);
  if (t >= lc_len || !acc_unread((word)(nb + *skip)) || !acc_unread(t))
    return 0xffff;
  // value = cond if k even; zjp jumps if value == 0, G if value != 0
  *brc = (uchar)((jz ^ (k & 1)) ? BCS : BCC);
  return t;
}

// flags of a - b (C = a >= b): A only, consts as immediates
static void emit_sub_flags(uchar a, uchar b) {
  uchar sa[2];
  sa[0] = a; sa[1] = b;
  TPL(_LDA RLO1 _CMP RLO2 _LDA RHI1 _SBC RHI2, sa);
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
static word i_exit_b(word i) {
  const uchar *c = lc_code;
  word t = ivm_jump(c + i, i);
  if (t >= 2 && t <= lc_len && c[t - 2] == 'B' && (word)(t - c[t - 1]) == i)
    return t;
  return 0xffff;
}

// t / u at i a loop head? Closed by an 'n' or a 'B' back to it. Else it
// is an if (l < r) / while with a mark before it: compare and branch
static int is_loop_head(word i) {
  const uchar *c = lc_code;
  word ex = ivm_jump(c + i, i);
  if (ex < 2 || ex > lc_len) return 0;
  if (c[ex - 2] == 'n' && (word)(ex - c[ex - 1]) == i) return 1;
  return i_exit_b(i) == ex;
}

// stride loop: 't' at ibc closed by "U v s; B" (v its loop var, the 'B' no
// jump target): 'U' leaves Y = lo, A = hi, the 'B' tests those
static int is_label(word bc);
static int is_stride(word ibc) {
  const uchar *c = lc_code;
  word ex = i_exit_b(ibc);
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
  if (is_const(lim)) {
    word v = const_val(lim);
    if (excl) { if (v) emit_below_const(CMPN, CPYN, v, body); return; }
    if (v == 0xffff) { emjump(JMP, body); return; }   // always true
    emit_below_const(CMPN, CPYN, (word)(v + 1), body);
  } else {
    TPL(_CMP RHI1 _BNE _CPY RLO1 R_PATCH, &lim);
    if (!excl) emjump(BEQ, body);
    emjump(BCC, body);
  }
}

static void emit_loop_test(uchar slot, uchar lim, word body, uchar ax,
                           uchar excl) {

  if (!ax) {
    if (excl) { emit_sub_flags(slot, lim); emjump(BCC, body); }
    else      { emit_sub_flags(lim, slot); emjump(BCS, body); }
    return;
  }
  emit_load_slot(slot);
  if (is_const(lim)) {
    word v = const_val(lim);
    if (excl) { if (v) emit_below_const(CPXN, CMPN, v, body); return; }
    if (v == 0xffff) { emjump(JMP, body); return; }   // always true
    emit_below_const(CPXN, CMPN, (word)(v + 1), body);
  } else {
    TPL(_CPX RHI1 _BNE _CMP RLO1 R_PATCH, &lim);
    if (!excl) emjump(BEQ, body);                     // == (Z only if hi ==)
    emjump(BCC, body);                                // <
  }
}

// open loops: 't' emits "JMP test" with an empty operand, its 'n' patches
// it (loops nest, so this is a stack). ibc = bytecode pc of the 't'.
#define MAX_LOOPS 8
typedef struct { word ibc, at; uchar ax; } LcLoop;
static LcLoop lc_loop[MAX_LOOPS];
static uchar lc_nloop;

// patch the JMP of the open loop at 't' ibc to here (its test); its ax
static uchar loop_close(word ibc) {
  uchar k;
  LcLoop *l;
  for (k = lc_nloop; k-- > 0; ) {
    l = &lc_loop[k];
    if (l->ibc != ibc) continue;

    // its JMP: to here
    if (l->at + 1 < lc_max) {
      word abs = (word)(lc_out_org + lc_at);
      lc_out[l->at]     = (uchar)(abs & 0xff);
      lc_out[l->at + 1] = (uchar)(abs >> 8);
    }
    lc_nloop = k;
    return l->ax;
  }
  return 0;
}

// open the loop at 't' ibc: its JMP's address is at lc_at
static void loop_open(word ibc, uchar ax) {
  LcLoop *l;
  if (lc_nloop == MAX_LOOPS) { printf("%%JIT: loops nested too deep\n"); return; }
  l = &lc_loop[lc_nloop++];
  l->ibc = ibc; l->at = lc_at; l->ax = ax;
}

// "AX == mem[slot]" at a label. lc_ksa[]: such labels; every jump there
// makes it true, so the load at the label is free:
// - loop body start: only entered from the 'n' test, which leaves the loop
//   var in AX ('t' adds these)
// - a label starting with a slot load, reached only by fall-through and
//   'B' (a while head), if the fall-in has AX == slot: each 'B' loads it
//   unless AX already has it (prescan adds, the label keeps or drops them)
typedef struct { word bc; uchar slot, b; } LcKsa;          // b: by 'B'
static LcKsa lc_ksa[MAX_LOOPS * 4];
static uchar lc_nksa;

// a new "AX == mem[slot]" label (the caller checks there is room)
static void ksa_add(word bc, uchar slot, uchar b) {
  LcKsa *q = &lc_ksa[lc_nksa++];
  q->bc = bc; q->slot = slot; q->b = b;
}
// the one at bc, 0 if none
static LcKsa *ksa_at(word bc) {
  uchar k;
  for (k = 0; k < lc_nksa; k++) if (lc_ksa[k].bc == bc) return &lc_ksa[k];
  return 0;
}


// Byte element pointer like emit_belem_ptr, index in Y (lo) and A (hi)
//   T_BTAIL: AI hi = base hi + index hi, Y = base lo (slot 1: base)
#define T_BTAIL _CLC _ADC RHI1 _STAZ _AIZ1 _LDY RLO1
static void emit_belem_ptr_ya(uchar base) { TPL(_STYZ _AIZ T_BTAIL, &base); }

// Byte element pointer like emit_belem_ptr, index already in AX
static void emit_belem_ptr_ax(uchar base) {
  TPL(_STAZ _AIZ _TXA T_BTAIL, &base);
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
  uchar sa[2];
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
  sa[0] = base; sa[1] = idx;
  lc_tzp = zp;
  TPL(_LDA RLO2 _ASLA _TAY _LDA RHI2 _ROLA _STAZ _TZP1
      _CLC _TYA _ADC RLO1 _STAZ _TZP _LDAZ _TZP1 _ADC RHI1 _STAZ _TZP1, sa);
  lc_tzp = AI;
}

// Byte indexing address calculation
//
// (AI),Y == &((uchar*)mem[base])[mem[idx]]   (clobbers A, Y; X kept)
// ptr = (idx_lo, base_hi+idx_hi), Y = base_lo: the indexed mode does the
// lo add + carry for free.
static void emit_belem_ptr(uchar base, uchar idx) {
  uchar sa[2];
  sa[0] = base; sa[1] = idx;
  TPL(_LDA RLO2 _STAZ _AIZ _LDA RHI2 T_BTAIL, sa);
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
// Stores through pointers ( ! , w ) ] m ) keep what is known about ACC:
// they store ACC itself. With & of a variable (` in the code: lc_alias)
// they may hit mem[]: the element cache goes (its idx), m A forget ACC.
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
static uchar lc_alias;      // ` in the code: & of a variable (prescan)
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
  return lc_lbl != 0 && (lc_lbl[bc >> 3] & (1 << (bc & 7)));
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
static int jit_rule_k(uchar rop, uchar op, const uchar *p);
static uchar lc_rn;            // the rules' shift count (0xff: a variable)
static uchar lc_rskip;         // bytecode bytes a rule ate after its op
static word  lc_libtop;        // the highest lib routine a JSR went to
// main's global inits done at JIT time (init_fold): into lc_initmem,
// the mem[] the program starts with (the -o image, the simulator's); 0: not
static word *lc_initmem;
static uchar lc_ifold, lc_ifk; // folding; ACC known: lc_ifv
static uchar lc_main_vec;      // main's vector if it folds, else 0xff
static word  lc_ifv;
// ACC <<= n / >>= n, n const. NEW. Folded, by 0, by 16+, << 1 here,
// else the { } rules, by lc_rn = n
static void emit_shift(uchar right, uchar n) {
  uchar sa[1];

  // const: fold
  if (acc_at == ACC_PEND && acc_hk) {
    acc_pk(n > 15 ? 0 : right ? (word)(acc_k >> n) : (word)(acc_k << n));
    return;
  }

  // by 0, by 16+: 0, << 1: ACC + ACC
  if (!n) { acc_need(); return; }
  if (n > 15) { acc_drop(); emit_load0(); acc_ax(); acc_x0 = 1; return; }
  if (n == 1 && !right) { emit_acc_double(); return; }

  // the rules
  lc_rn = n; sa[0] = 0;
  jit_rule_k(right ? '}' : '{', right ? '}' : '{', sa);
}
// n if v == 1 << n, else 0xff
static uchar log2w(word v) {
  uchar n;
  for (n = 0; n < 16; n++) if (v == (word)(1u << n)) return n;
  return 0xff;
}

// ACC *= 2 (i+i, i*2, 2*i): from the pending slot, or in AX. NEW
static void emit_acc_double(void) {
  if (acc_at == ACC_PEND && !acc_hk)
    TPL(_LDA RLO1 _ASLA _TAY _LDA RHI1 _ROLA _TAX _TYA, &acc_s);
  else {
    acc_need();
    TPL(_ASLA _TAY _TXA _ROLA _TAX _TYA, 0);
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
  LcKsa *q = ksa_at(bc_target);
  if (q && q->b && !(acc_at == ACC_AX && acc_s == q->slot))
    printf("%%JIT: jump to bc %u without AX == slot %u\n",
           (unsigned)bc_target, q->slot);
}

// next op (at bc) does not read ACC and is not a label: ACC is dead, so
// a KEEP op needn't save it (ivm-walk.c contract: "- SLOT/NEW/UNDEF")
static int acc_unread(word bc) {
  uchar op = lc_code[bc];
  return op < 32 || strchr("09Q12345678@r'l([aKLCUIJijH;tuvnV$", op);
}
static int acc_dead(word bc) {
  return bc < lc_len && !is_label(bc) && acc_unread(bc);
}
static int is_fn_entry(word bc);
// label whose first op doesn't read ACC (not a function entry: that one
// stores AX): jumps to it and code falling into it needn't load ACC
static int acc_free_label(word bc) {
  return !is_fn_entry(bc) && bc != lc_main_bc && acc_unread(bc);
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

// A taken branch to another page costs +1 cycle: a loop whose closing
// branch crosses a page gets its body moved to one page on the next run,
// with NOPs after the 't' JMP (never executed: costs bytes, no cycles).
#define JIT_ALIGN_LOOPS 0     // 1: pad innermost loops (sieve: avg -0.1%, +0..37 B)
typedef struct { word ibc, len; } LcAl;
static LcAl lc_al[8];                       // loops to align, body length
static uchar lc_nal;
// the one for the loop at ibc, 0 if none
static LcAl *al_at(word ibc) {
  uchar k;
  for (k = 0; k < lc_nal; k++) if (lc_al[k].ibc == ibc) return &lc_al[k];
  return 0;
}

// 't' after its JMP: pad if the body (len, from a previous run) would
// straddle a page
static void loop_pad(word ibc) {
  LcAl *q = al_at(ibc);
  word a = (word)((lc_out_org + lc_at) & 0xff);
  if (q && a + q->len > 0x100)
    for (a = (word)(0x100 - a); a; a--) em(NOP);
}

// after the closing test of the loop at ibc (innermost loops only: they
// run most, and are short): does its short branch back to the body cross
// a page? note it and run again
static void loop_align_check(word ibc) {
  word body = lc_map[ibc + 4], len = (word)(lc_at - body), b;
  LcAl *q;
  if (!JIT_ALIGN_LOOPS || body == 0xffff || len >= 128) return;
  for (b = (word)(ibc + 4); b < lc_bc; b += ivm_oplen(lc_code + b))
    if ((IS_LOOP(lc_code[b]) && is_loop_head(b)) ||
        !ivm_oplen(lc_code + b)) return;                    // not innermost
  if (lc_at >= 3 && lc_out[lc_at - 3] == JMP) return;   // long form
  if (((lc_out_org + body) ^ (lc_out_org + lc_at)) >> 8 == 0) return;
  // note it (padded already: can't help)
  if ((q = al_at(ibc)) == 0) {
    if (lc_nal == 8) return;
    q = &lc_al[lc_nal++];
  } else if (q->len == len) return;
  q->ibc = ibc; q->len = len;
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
//                  spec 1 / 2 = lo of slot operand 1 / 2, 3 = of the
//                  return value cell (FR_VAL), 4 = of the pending ACC,
//                  5 = opmemset_adr (absolute), +16 = hi
//   R_ALU   spec   ($0F) the same with the ALU op of the op (+ ADC, - SBC,
//                  & AND, | ORA, ^ EOR)
//   (zp),Y op      on the pointer (R_PTR R_ELW R_ELB R_ZAI R_ZAS): no
//                  operand
//   branch op      forward branch (emrel): R_PATCH lands it (a stack),
//                  R_PATCHU the one under it
//   JSR n          library routine n (rule_ref; $FF: * / % by the op)
//   #imm / zp op n emitted as they are, n included; others one byte
//
//   R_PEND0 / 1    emit_lda_pend: LDA lo / hi of the pending ACC
//   R_PTR          zp pointer = mem[slot 1] (emit_ptr)
//   R_ELW / R_ELB  AI = &((word / uchar *)slot 1)[slot 2], y its offset
//   R_YK0 / 1      LDY #lc_ty0 / #lc_ty0 + 1 (R_ELW sets it)
//   R_YJ0 / 1      LDY #lc_ty1 / #lc_ty1 + 1
//   R_ZAI / R_ZAS  the pointer: AI / AS (at the start: lc_tzp)
//   zp op _TZP     _TZP / _TZP1 as its operand: the pointer / + 1
//   R_BRD op       emjump(op, lc_tdest): to a bytecode target
//   R_FOLD         ACC = the pending const ACC op the const slot 1
//   R_CARRY        CLC for +, SEC for - (nothing for & | ^)
//   R_ELBK         static array (slot 1 a const), any index (slot 2): Y =
//                  index lo, then A = index hi + base hi into the hi byte
//                  of the next abs,Y (self-modifying): base + index with
//                  no pointer
//   abs,Y op spec  RKA1 / RKA3: slot 1's / 1 + 1's const as the address
//                  (+ an R_ELBK's hi)
//   abs op RKW     RKWH: slot 1's const + 2 * slot 2's (a word a[k])
//   R_INC1         mem[slot 1]++ (emit_inc)
//   R_NOW          acc_need() here (a pending const / slot into AX)
//   R_LD1 / R_LD2  AX = mem[slot 1 / 2] (const: LDA #lo, TAX if hi == lo)
//   R_STR          the inline string operand, its 0 too
//   R_LDO          LDA #operand 1 (a byte, not a slot)
//   R_TOP          a loop starts here, R_BACK brop: branch back to it
//   R_REP len      the next len bytes, lc_rn times (the shift count)
//   R_SUB8         lc_rn -= 8
//   R_LDYRN        LDY #lc_rn
//
// Guard: flags (all must hold) and a class (below). Op: the op, or the
// keys BINOP, MULDIV below.
// ACC before (any of): R_NEED load into AX, R_DROP not needed, R_WRITE it
// may be slot 1 (acc_write, before the guards: all rules of the op).
// After: R_SLOT = mem[slot 1] (pending), R_AXSLOT in AX and = mem[slot 1],
// R_LOST, R_AX, R_AXBZ / R_AXB a byte in AX (X = 0, Z of it valid / not),
// R_AXCOPY in AX and = mem[slot 1] = mem[slot 2], R_SETS = mem[slot 1]
// too, R_AXZ in A only (X junk) with Z of it, R_FUSED "op v; S d": mem[d]
// pending, the S eaten (lc_rskip), else kept. R_AGAIN: then
// match the key again, in the new ACC state (a rule that only moves ACC
// to where another rule wants it).
// flags (all set ones must hold): the state of ACC, X, the next op
#define G_ACC_PEND 0x0001         // ACC pending
#define G_ACC_AX   0x0002         // ACC not pending (in AX, or lost)
#define G_ACC_LIVE 0x0004         // ACC not lost
#define G_ACC_K    0x0008         // ACC a pending const
#define G_ACC_BYTE 0x0010         // ACC < 256: byte pending / AX with X = 0
#define G_ACC_IS1  0x0020         // ACC == mem[slot 1] (+ G_ACC_AX: in AX)
#define G_ACC_IS2  0x0040         // ACC == mem[slot 2]
#define G_X_0      0x0080         // X == 0 known
#define G_NEXT_S   0x0100         // next op "S d", not a label
#define G_ZJP_DEAD 0x0200         // next: zjp, ACC not read after it
// class (one, the top 6 bits): like a type signature (%d %D %v ... of
// old), of slot 1 (2), the shift count lc_rn, ACC, what comes after
#define GC(n)      ((word)(n) << 10)
#define G_S1_ZP    GC(1)          // slot 1 in zp
#define G_S1_VAR   GC(2)          // slot 1 not a const (a variable)
#define G_S1_K     GC(3)          // slot 1 a const
#define G_S1_BYTE  GC(4)          // slot 1 < 256: const < 256 / byte slot
#define G_S1_HIFF  GC(5)          // slot 1 a const with hi byte $FF
#define G_N_1TO4   GC(6)          // count 1..4
#define G_N_7      GC(7)          // count 7
#define G_N_8UP    GC(8)          // count 8..15
#define G_N_LT8    GC(9)          // count 0..7 (in a byte)
#define G_N_K      GC(10)         // count a const
#define G_ACC_K0   GC(11)         // ACC the pending const 0
#define G_ACC_K256 GC(12)         // ACC a pending const 1..256
#define G_DEAD     GC(13)         // ACC is not read after this op
#define G_K1_B2    GC(14)         // slot 1 a const (a static array), slot
                                  // 2 < 256 (a byte index)
#define G_K1_K2    GC(17)         // slot 1 and 2 consts (a[k])
#define G_AI_WORD  GC(15)         // &slot1[slot2] (word) in AI already
#define G_AI_BYTE  GC(16)         // the same, uchar
#define G_ZP_K2    GC(18)         // slot 1 in zp, slot 2 a const < 128
#define G_SIM      GC(19)         // HOSTSIM, running in cpu6502.c (no image)
                                  // (p->f: (zp),Y with Y = 2k)
#define R_NEED  0x01
#define R_DROP  0x02
#define R_WRITE 0x04
#define R_AGAIN 0x08
#define R_SLOT  0x10
#define R_LOST  0x20
#define R_AX    0x30
#define R_AXBZ  0x40
#define R_AXB   0x50
#define R_AXSLOT 0x60
#define R_AXCOPY 0x70
#define R_SETS  0x80
#define R_AXZ   0x90
#define R_FUSED 0xA0
// rule keys: the op ($20..$7F), BINOP, MULDIV: not bytecode ops ($80..
// are zjp / calls, but those never get here: only the switch's default
// and the C below look rules up)
#define RULE(op, g, acc, t) { op, acc, g, sizeof(t) - 1, t }
#define BINOP   1                         // any of + - & | (^: as |)
#define MULDIV  0x81                      // * / % (not by a power of 2)
typedef struct {
  uchar op, acc; word g; uchar len; const char *t;
} JitRule;
static const JitRule jit_rules[] = {
  // ---- + - & | ^ (C before: the a[j+c] relation, the commutative
  // swap). R_CARRY: CLC for +, SEC for -. In order: const OP const,
  // i + i, operand < 256 or ACC < 256, "op v; S d", ACC pending in a
  // slot, & $FFxx, any
  RULE(BINOP, G_ACC_K | G_S1_K, 0, R_FOLD),
  RULE('+', G_ACC_PEND | G_ACC_IS1, R_AX,
    _LDA RLO1 _ASLA _TAY _LDA RHI1 _ROLA _TAX _TYA),
  RULE('+', G_ACC_AX | G_ACC_IS1, R_NEED | R_AX,
    _ASLA _TAY _TXA _ROLA _TAX _TYA),
  RULE('+', G_S1_BYTE | G_ACC_LIVE, R_NEED | R_AX,
    R_CARRY R_ALU RLO1 _BCC _INX R_PATCH),
  RULE('-', G_S1_BYTE | G_ACC_LIVE, R_NEED | R_AX,
    R_CARRY R_ALU RLO1 _BCS _DEX R_PATCH),
  RULE('&', G_S1_BYTE | G_ACC_LIVE, R_NEED | R_AX, R_ALU RLO1 _LDXN "\0"),
  RULE('|', G_S1_BYTE | G_ACC_LIVE, R_NEED | R_AX, R_ALU RLO1),
  RULE('+', G_ACC_BYTE | G_ACC_PEND, R_AX,
    R_CARRY R_PEND0 R_ALU RLO1 _LDX RHI1 _BCC _INX R_PATCH),
  RULE('+', G_ACC_BYTE | G_ACC_AX, R_AX,
    R_CARRY R_ALU RLO1 _LDX RHI1 _BCC _INX R_PATCH),
  RULE('&', G_ACC_BYTE | G_ACC_PEND, R_AX,
    R_PEND0 R_ALU RLO1 _LDXN "\0"),
  RULE('&', G_ACC_BYTE | G_ACC_AX, R_AX, R_ALU RLO1),
  RULE('|', G_ACC_BYTE | G_ACC_PEND, R_AX, R_PEND0 R_ALU RLO1 _LDX RHI1),
  RULE('|', G_ACC_BYTE | G_ACC_AX, R_AX, R_ALU RLO1 _LDX RHI1),
  // op v; S d: each byte straight into d, no TAX / TYA. Leaves Y = lo,
  // A = hi (lc_yah), ACC = mem[d] pending, the S eaten
  RULE(BINOP, G_NEXT_S | G_ACC_PEND, R_FUSED, R_CARRY
    R_PEND0 R_ALU RLO1 _STA RLO2 _TAY R_PEND1 R_ALU RHI1 _STA RHI2),
  RULE(BINOP, G_NEXT_S | G_ACC_AX | G_ACC_LIVE, R_FUSED, R_CARRY
    R_ALU RLO1 _STA RLO2 _TAY _TXA R_ALU RHI1 _STA RHI2),
  RULE(BINOP, G_ACC_PEND | G_S1_VAR, R_AX, R_CARRY
    R_PEND0 R_ALU RLO1 _TAY R_PEND1 R_ALU RHI1 _TAX _TYA),
  RULE('&', G_S1_HIFF, R_NEED | R_AX, R_ALU RLO1),
  RULE(BINOP, 0, R_NEED | R_AX,
    R_CARRY R_ALU RLO1 _TAY _TXA R_ALU RHI1 _TAX _TYA),

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
  RULE('Y', 0, R_WRITE, _LDYN "\1" _STY RLO1 _DEY _STY RHI1),
  // F: free(mem[s]); mem[s] = 0
  RULE('F', 0, R_DROP | R_LOST,
    _LDA RLO1 _LDX RHI1 _JSR L_FREE _LDYN "\0" _STY RLO1 _STY RHI1),
  // M: mem[s] = malloc(ACC)
  RULE('M', 0, R_NEED | R_WRITE | R_AXSLOT,
    _JSR L_MALLOC _STA RLO1 _STX RHI1),
  // E . P: putchar / print number / string (ACC kept)
  RULE('E', 0, R_NEED, _JSR L_PUTCHAR),
  RULE('.', 0, R_NEED, _JSR L_PUTNUM),
  RULE('P', 0, R_NEED, _JSR L_PUTS),
  // K: ACC = getchar() ($FFFF: EOF)
  RULE('K', 0, R_DROP | R_AX, _JSR L_GETCHAR),
  // l: ACC = lo(mem[s]): LDX #0; LDA lo
  RULE('l', 0, R_DROP | R_AXBZ, _LDXN "\0" _LDA RLO1),
  // s: mem[s] = lo(ACC), hi 0. Pending: from where it is (stays pending)
  RULE('s', G_ACC_PEND, R_WRITE, R_PEND0 _STA RLO1 _LDYN "\0" _STY RHI1),
  RULE('s', 0, R_WRITE | R_NEED, _STA RLO1 _LDYN "\0" _STY RHI1),
  // r: ACC = *(uchar*)mem[s]
  RULE('r', 0, R_DROP | R_AXBZ, R_PTR _LDYN "\0" _LDXN "\0" _LDAIY),
  // ': ACC = *(uchar*)ptr; ptr++
  RULE('\'', 0, R_DROP | R_AXB, R_PTR _LDYN "\0" _LDAIY _LDXN "\0" R_INC1),
  // w ,: *(uchar*)ptr = ACC (, then ptr++). AX saved if the pointer is
  // copied (not in zp)
  RULE('w', G_ACC_PEND, 0,      R_PTR R_PEND0 _LDYN "\0" _STAIY),
  RULE('w', G_S1_ZP,  R_NEED, R_PTR _LDYN "\0" _STAIY),
  RULE('w', 0,      R_NEED, _PHA R_PTR _PLA _LDYN "\0" _STAIY),
  RULE(',', G_ACC_PEND, R_WRITE, R_PTR R_PEND0 _LDYN "\0" _STAIY R_INC1),
  RULE(',', G_S1_ZP,  R_WRITE | R_NEED, R_PTR _LDYN "\0" _STAIY R_INC1),
  RULE(',', 0,      R_WRITE | R_NEED,
    _PHA R_PTR _PLA _LDYN "\0" _STAIY R_INC1),
  // @: ACC = *(word*)mem[s]: hi first, then lo
  RULE('@', 0, R_DROP | R_AX,
    R_PTR _LDYN "\x01" _LDAIY _TAX _DEY _LDAIY),
  // !: *(word*)mem[s] = ACC
  RULE('!', G_ACC_PEND, 0,
    R_PTR _LDYN "\x01" R_PEND1 _STAIY _DEY R_PEND0 _STAIY),
  RULE('!', 0, R_NEED,
    _PHA R_PTR _TXA _LDYN "\x01" _STAIY _PLA _DEY _STAIY),
  // ( bb ii: ACC = ((word*)base)[idx]: hi first, then lo. A static
  // array / struct at a const index: absolute
  RULE('(', G_K1_K2, R_DROP | R_AX, _LDA RKW _LDX RKWH),
  RULE('(', 0, R_DROP | R_AX, R_ELW R_YK1 _LDAIY _TAX _DEY _LDAIY),
  // ) bb ii: ((word*)base)[idx] = ACC. A static one at a const index:
  // absolute. Pending: from where it is; AI there already, or the
  // pointer in zp at a const index: AX kept as is; else: AX saved over
  // the AI setup
  RULE(')', G_ACC_PEND | G_K1_K2, 0,      R_PEND0 _STA RKW R_PEND1 _STA RKWH),
  RULE(')', G_K1_K2,              R_NEED, _STA RKW _STX RKWH),
  RULE(')', G_ACC_PEND, 0,
    R_ELW R_YK1 R_PEND1 _STAIY _DEY R_PEND0 _STAIY),
  RULE(')', G_AI_WORD, R_NEED,
    R_ELW R_YK0 _STAIY _PHA _TXA _INY _STAIY _PLA),
  RULE(')', G_ZP_K2, R_NEED,
    R_ELW R_YK0 _STAIY _PHA _TXA _INY _STAIY _PLA),
  RULE(')', 0, R_NEED,
    _PHA _TXA _PHA R_ELW _PLA R_YK1 _STAIY _TAX _PLA _DEY _STAIY),
  // ] bb ii: ((uchar*)base)[idx] = ACC. A static array (base a const):
  // a const index: STA base+k; a byte index: STA base,Y (Y = idx); else
  // self-modifying (R_ELBK)
  RULE(']', G_ACC_PEND | G_K1_K2, 0,      R_PEND0 _STA RKK),
  RULE(']', G_K1_K2,              R_NEED, _STA RKK),
  RULE(']', G_ACC_PEND | G_K1_B2, 0,      _LDY RLO2 R_PEND0 _STAAY RKA1),
  RULE(']', G_K1_B2,              R_NEED, _LDY RLO2 _STAAY RKA1),
  RULE(']', G_ACC_PEND | G_S1_K,  0,      R_ELBK R_PEND0 _STAAY RKA1),
  RULE(']', G_S1_K,               R_NEED, _PHA R_ELBK _PLA _STAAY RKA1),
  // else via AI: pending: from where it is; AI there already; AX saved
  RULE(']', G_ACC_PEND, 0,     R_ELB R_PEND0 _STAIY),
  RULE(']', G_AI_BYTE, 0,      R_ELB _STAIY),
  RULE(']', 0,     R_NEED, _PHA R_ELB _PLA _STAIY),
  // a ll ii: ACC = striped word: lo[i] | hi[i] << 8 (an array of structs'
  // field, lo: const ll, hi: const ll + 1, i < N <= 254: Y = its lo).
  // b: stored
  RULE('a', G_K1_K2, R_DROP | R_AX, _LDA RKK _LDX RK32),
  RULE('a', 0,       R_DROP | R_AX, _LDY RLO2 _LDAAY RKA1 _LDXAY RKA3),
  RULE('b', G_ACC_PEND | G_K1_K2, 0,      R_PEND0 _STA RKK R_PEND1 _STA RK32),
  RULE('b', G_K1_K2,              R_NEED, _STA RKK _STX RK32),
  RULE('b', G_ACC_PEND, 0,
    _LDY RLO2 R_PEND0 _STAAY RKA1 R_PEND1 _STAAY RKA3),
  RULE('b', 0,          R_NEED,
    _LDY RLO2 _STAAY RKA1 _PHA _TXA _STAAY RKA3 _PLA),
  // [ bb ii: ACC = ((uchar*)base)[idx]; a zjp next and ACC dead after
  // it: only A (Z), else LDA last: Z == (ACC == 0). A static array: LDA
  // base,Y (as for ])
  RULE('[', G_ZJP_DEAD | G_K1_K2, R_DROP | R_AXZ,  _LDA RKK),
  RULE('[', G_K1_K2,              R_DROP | R_AXBZ, _LDXN "\0" _LDA RKK),
  RULE('[', G_ZJP_DEAD | G_K1_B2, R_DROP | R_AXZ,  _LDY RLO2 _LDAAY RKA1),
  RULE('[', G_K1_B2,              R_DROP | R_AXBZ,
    _LDY RLO2 _LDXN "\0" _LDAAY RKA1),
  RULE('[', G_ZJP_DEAD | G_S1_K,  R_DROP | R_AXZ,  R_ELBK _LDAAY RKA1),
  RULE('[', G_S1_K,               R_DROP | R_AXBZ, R_ELBK _LDXN "\0" _LDAAY RKA1),
  RULE('[', G_ZJP_DEAD, R_DROP | R_AXZ,  R_ELB _LDAIY),
  RULE('[', 0,     R_DROP | R_AXBZ, R_ELB _LDXN "\0" _LDAIY),
  // N / O c: putchar('\n' / c), A X Y kept: AX saved in Y if read later
  RULE('N', G_ACC_AX | G_ACC_LIVE | G_DEAD, R_LOST, _LDAN "\x0A" _JSR L_PUTCHAR),
  RULE('N', G_ACC_AX | G_ACC_LIVE, 0, _TAY _LDAN "\x0A" _JSR L_PUTCHAR _TYA),
  RULE('N', 0, 0, _LDAN "\x0A" _JSR L_PUTCHAR),
  RULE('O', G_ACC_AX | G_ACC_LIVE | G_DEAD, R_LOST, R_LDO _JSR L_PUTCHAR),
  RULE('O', G_ACC_AX | G_ACC_LIVE, 0, _TAY R_LDO _JSR L_PUTCHAR _TYA),
  RULE('O', 0, 0, R_LDO _JSR L_PUTCHAR),
  // * / % (C before: by a power of 2, by 2): ivm-lib, the first in
  // tmp1 / tmp2, the second in AX, result AX. const * x: x first, the
  // const as the multiplier (AX): fewer rounds
  RULE('*', G_ACC_K | G_S1_VAR, R_AX, _LDA RLO1 _STAZ _TMP1
    _LDA RHI1 _STAZ _TMP2 R_NOW _JSR L_MUL),
  RULE(MULDIV, G_ACC_PEND, R_AX, R_PEND0 _STAZ _TMP1 R_PEND1 _STAZ _TMP2
    R_LD1 _JSR L_MULDIV),
  RULE(MULDIV, 0, R_NEED | R_AX, _STAZ _TMP1 _STXZ _TMP2
    R_LD1 _JSR L_MULDIV),
  // " text 0: print it (JSR + the string inline; A X kept)
  RULE('"', 0, 0, _JSR L_PRINTSTR R_STR),
  // # name 0: #TIMED. Host sim: JSR to a trap that prints the cycles
  // since the last one (A X Y kept), else nothing at all
#ifdef HOSTSIM
  RULE('#', G_SIM, 0, _JSR L_TIMED R_STR),
#endif
  RULE('#', 0, 0, ""),
  // m ptr val: memset(mem[ptr], val, ACC). A pending count stays pending.
  // 1..256: inline (LDY #n: 256 is 0, DEY wraps); else opmemset: its STA
  // operand = ptr + X * 256 (the last page), Y = count lo, X = count hi
  #define T_MSET _LDA RLO1 _STA RLOM _TXA _CLC _ADC RHI1 _STA RHIM _LDA RLO2
  RULE('m', G_ACC_K0, 0, ""),
  RULE('m', G_ACC_K256, 0,
    R_PTR _LDY RLOA _LDA RLO2 R_TOP _DEY _STAIY R_BACK _BNE),
  RULE('m', G_ACC_PEND, 0, _LDX RHIA T_MSET _LDY RLOA _JSR L_MEMSET),
  RULE('m', 0, R_NEED | R_LOST, _TAY T_MSET _JSR L_MEMSET),
  // ~: ACC = ~ACC (a pending const: folded, in C)
  RULE('~', 0, R_NEED | R_AX,
    _EORN "\xFF" _TAY _TXA _EORN "\xFF" _TAX _TYA),
  // q X: return ACC, Q: return 0, ;: return the return value cell
  RULE('q', 0, R_NEED | R_AX, _RTS),
  RULE('X', 0, R_NEED | R_AX, _RTS),
  RULE('Q', 0, R_DROP | R_AX, _LDAN "\0" _TAX _RTS),
  RULE(';', 0, R_DROP | R_AX, _LDA RLOV _LDX RHIV _RTS),
  // S: mem[s] = ACC (in AX after); not if it is mem[s] already
  RULE('S', G_ACC_IS1, R_NEED, ""),
  RULE('S', 0, R_NEED | R_SETS, _STA RLO1 _STX RHI1),
  // C d s: mem[d] = mem[s], ACC = it
  RULE('C', G_ACC_AX | G_ACC_IS2, R_AXCOPY, _STA RLO1 _STX RHI1),
  RULE('C', 0, R_DROP | R_AXCOPY, R_LD2 _STA RLO1 _STX RHI1),
  // U a b: mem[a] += mem[b] byte by byte, ACC = mem[a] left pending
  // (C before: the stride loop form)
  RULE('U', G_ACC_AX | G_ACC_IS1, R_SLOT,
    _CLC _ADC RLO2 _STA RLO1 _TXA _ADC RHI2 _STA RHI1),
  RULE('U', 0, R_DROP | R_SLOT,
    _CLC _LDA RLO1 _ADC RLO2 _STA RLO1 _LDA RHI1 _ADC RHI2 _STA RHI1),
  // =: ACC = ACC == mem[s]: CMP lo, CPX hi; X = 0
  RULE('=', 0, R_NEED | R_AXB, _CMP RLO1 _BNE _CPX RHI1 _BNE
    _LDAN "\x01" _BNE R_PATCHU R_PATCHU _LDAN "\0" R_PATCH _LDXN "\0"),
  // { }: ACC <<= / >>= n. Const n (lc_rn, C before: fold, 0, 16+, << 1)
  // 8..15: a byte move, the rest in A; 7: a byte move, one bit back;
  // 1..4: ASL A; ROL tmp2 (X in tmp2); 5 6: that as a loop on Y
  RULE('{', G_ACC_PEND | G_N_8UP, R_AX,
    R_PEND0 R_SUB8 R_REP "\x01" _ASLA _TAX _LDAN "\0"),
  RULE('{', G_N_8UP, R_NEED | R_AX,
    R_SUB8 R_REP "\x01" _ASLA _TAX _LDAN "\0"),
  RULE('{', G_ACC_PEND | G_N_7, R_AX,
    R_PEND1 _LSRA R_PEND0 _RORA _TAX _LDAN "\0" _RORA),
  RULE('{', G_N_7, R_NEED | R_AX,
    _TAY _TXA _LSRA _TYA _RORA _TAX _LDAN "\0" _RORA),
  RULE('{', G_N_1TO4, R_NEED | R_AX,
    _STXZ _TMP2 R_REP "\x03" _ASLA _ROLZ _TMP2 _LDXZ _TMP2),
  RULE('{', G_N_K, R_NEED | R_AX, _STXZ _TMP2 R_LDYRN R_TOP
    _ASLA _ROLZ _TMP2 _DEY R_BACK _BNE _LDXZ _TMP2),
  // >>: a byte ACC: 8..15: 0, 7: the top bit, else A only (a pending
  // one: in AX first);
  // 8..15: the hi byte (to AX first), then as a byte
  RULE('}', G_ACC_BYTE | G_N_8UP, R_DROP | R_AXB, _LDAN "\0" _TAX),
  RULE('}', G_ACC_PEND | G_ACC_BYTE | G_N_LT8, R_AXB | R_AGAIN, R_PEND0 _LDXN "\0"),
  RULE('}', G_ACC_BYTE | G_N_7, R_AXBZ, _ASLA _LDAN "\0" _ROLA),
  RULE('}', G_ACC_BYTE | G_N_LT8, R_AXB, R_REP "\x01" _LSRA),
  RULE('}', G_ACC_PEND | G_N_8UP, R_AXB | R_AGAIN, R_PEND1 _LDXN "\0" R_SUB8),
  RULE('}', G_N_8UP, R_NEED | R_AXB | R_AGAIN, _TXA _LDXN "\0" R_SUB8),
  RULE('}', G_ACC_PEND | G_N_7, R_AX,
    R_PEND0 _ASLA R_PEND1 _ROLA _LDXN "\0" _BCC _INX R_PATCH),
  RULE('}', G_N_7, R_NEED | R_AX,
    _ASLA _TXA _ROLA _LDXN "\0" _BCC _INX R_PATCH),
  RULE('}', G_N_1TO4, R_NEED | R_AX,
    _STXZ _TMP2 R_REP "\x03" _LSRZ _TMP2 _RORA _LDXZ _TMP2),
  RULE('}', G_N_K, R_NEED | R_AX, _STXZ _TMP2 R_LDYRN R_TOP
    _LSRZ _TMP2 _RORA _DEY R_BACK _BNE _LDXZ _TMP2),
  // a variable n: a loop on Y (n may be 0)
  RULE('{', 0, R_NEED | R_AX, _STXZ _TMP2 _LDY RLO1 _BEQ R_TOP
    _ASLA _ROLZ _TMP2 _DEY R_BACK _BNE R_PATCH _LDXZ _TMP2),
  RULE('}', 0, R_NEED | R_AX, _STXZ _TMP2 _LDY RLO1 _BEQ R_TOP
    _LSRZ _TMP2 _RORA _DEY R_BACK _BNE R_PATCH _LDXZ _TMP2),
  // ?: ACC = !ACC  (A = lo | hi; CMP #1: C = ACC != 0; A = !C, X = 0)
  RULE('?', G_X_0 | G_ACC_AX, R_NEED | R_AXBZ,
    _CMPN "\x01" _LDAN "\0" _TAX _ROLA _EORN "\x01"),
  RULE('?', 0, R_NEED | R_AXBZ, _STXZ _TMP1 _ORAZ _TMP1
    _CMPN "\x01" _LDAN "\0" _TAX _ROLA _EORN "\x01"),
  { 0, 0, 0, 0, 0 }
};

// library routine n (LIB_...): its address
static word rule_ref(uchar n) {
  switch (n) {
  case LIB_PUTCHAR: return REF(opputchar);
  case LIB_PUTNUM:  return REF(opputnum);
  case LIB_PUTS:    return REF(opputs);
  case LIB_FREE:    return REF(opfree);
  case LIB_MALLOC:  return REF(opmalloc);
  case LIB_MUL:     return REF(opmul);
  case LIB_DIV:     return REF(opdiv);
  case LIB_MOD:     return REF(opmod);
  case LIB_MEMSET:  return REF(opmemset);
  case LIB_GETCHAR: return REF(opgetchar);
  #ifdef HOSTSIM
  case LIB_TIMED:   return REF(optimed);
  #endif
  }
  return REF(opprintstr);
}

static int rule_guard(word g, const uchar *p) {
  uchar s = p[0];
  word nb = 0, t;                                // the next op's bc
  if ((g & (G_NEXT_S | G_ZJP_DEAD)) || g >= G_DEAD)  // (classes from G_DEAD)
    nb = (word)(lc_bc + ivm_oplen(lc_code + lc_bc));

  // flags
  if ((g & G_ACC_PEND) && acc_at != ACC_PEND) return 0;
  if ((g & G_ACC_AX) && acc_at == ACC_PEND) return 0;
  if ((g & G_ACC_LIVE) && acc_at == ACC_LOST) return 0;
  if ((g & G_ACC_K) && !(acc_at == ACC_PEND && acc_hk)) return 0;
  if ((g & G_ACC_BYTE) && !((acc_at == ACC_PEND &&
                             (acc_hk ? acc_k < 256 : is_byte(acc_s))) ||
                            (acc_at == ACC_AX && acc_x0))) return 0;
  if ((g & G_ACC_IS1) && acc_s != s) return 0;
  if ((g & G_ACC_IS2) && acc_s != p[1]) return 0;
  if ((g & G_X_0) && !acc_x0) return 0;
  if ((g & G_NEXT_S) && !(lc_code[nb] == 'S' && !is_label(nb))) return 0;
  if ((g & G_ZJP_DEAD) &&
      !(nb + 1 < lc_len && (uchar)(lc_code[nb] - 0x80) < 64 &&
        !is_label(nb) && acc_unread((word)(nb + 1)) &&
        (t = ivm_jump(lc_code + nb, nb)) < lc_len && acc_unread(t)))
    return 0;

  // class
  switch (g >> 10) {
  case G_S1_ZP >> 10:    return slotlo(s) < 0xff;
  case G_S1_VAR >> 10:   return !is_const(s);
  case G_S1_K >> 10:     return is_const(s);
  case G_S1_BYTE >> 10:  return hi0(s);
  case G_S1_HIFF >> 10:  return is_const(s) && (const_val(s) >> 8) == 0xff;
  case G_N_1TO4 >> 10:   return (uchar)(lc_rn - 1) < 4;
  case G_N_7 >> 10:      return lc_rn == 7;
  case G_N_8UP >> 10:    return (uchar)(lc_rn - 8) < 8;
  case G_N_LT8 >> 10:    return lc_rn < 8;
  case G_N_K >> 10:      return lc_rn != 0xff;
  case G_ACC_K0 >> 10:   return acc_at == ACC_PEND && acc_hk && !acc_k;
  case G_ACC_K256 >> 10: return acc_at == ACC_PEND && acc_hk && acc_k - 1 < 256;
  case G_DEAD >> 10:     return acc_dead(nb);
  case G_K1_B2 >> 10:   return is_const(s) && hi0(p[1]);
  case G_K1_K2 >> 10:   return is_const(s) && is_const(p[1]);
  case G_ZP_K2 >> 10:    return is_const(p[1]) && const_val(p[1]) < 128 &&
                                !is_const(s) && slotlo(s) < 0xff;
  case G_AI_WORD >> 10:  return ai_hit('w', s, p[1]) != 0xff;
#ifdef HOSTSIM
  case G_SIM >> 10:      return !hl_lib;
#endif
  case G_AI_BYTE >> 10:  return ai_hit('b', s, p[1]) != 0xff;
  }
  return 1;
}

// emit the first rule of rop (see above) whose guard holds; op: the real
// op (R_ALU). p: its slot operands. 0: no rule
#define RMATCH(r) ((r) == rop || ((r) == BINOP && (rop == '+' || \
                   rop == '-' || rop == '&' || rop == '|')))
static int jit_rule_k(uchar rop, uchar op, const uchar *p) {
  const JitRule *r;                  // a pointer: no index * 7 per use
  uchar b, a;

  // first rule of the key; a write of slot 1 first (it may flush ACC):
  // then the guards see the ACC as the template will
  for (r = jit_rules; (b = r->op) != 0 && !RMATCH(b); r++) ;
  if (!b) return 0;
  if (r->acc & R_WRITE) acc_write(p[0]);

  // the first rule whose guard holds (R_AGAIN: from the first again)
again:
  for (r = jit_rules; (b = r->op) != 0; r++)
    if (RMATCH(b) && rule_guard(r->g, p)) break;
  if (!b) return 0;

  // ACC before
  a = r->acc;
  if (a & R_NEED) acc_need();
  else if (a & R_DROP) acc_drop();

  // the template
  emit_tpl((const uchar *)r->t, r->len, p, op);

  // ACC after
  switch (r->acc & 0xF0) {
  case R_SLOT:   acc_ps(p[0]); break;
  case R_LOST:   acc_lost(); break;
  case R_AX:     acc_ax(); break;
  case R_AXBZ:   acc_ax_byte(1); break;
  case R_AXB:   acc_ax_byte(0); break;
  case R_AXSLOT: acc_ax_slot(p[0]); break;
  case R_AXCOPY:                             // C d s: = mem[s] too
    acc_ax_slot(p[0]);
    if (is_const(p[1])) { acc_hk = 1; acc_k = const_val(p[1]); }
    break;
  case R_SETS:   acc_keep(); acc_s = p[0]; break;
  case R_AXZ:    acc_ax(); acc_z = 2; break;
  case R_FUSED:                              // op v; S d: d pending, Y A
    acc_drop(); acc_ps(p[1]);
    lc_yah = p[1]; lc_yah_at = lc_at;
    lc_rskip = 2;                            // the S eaten
    break;
  default:       acc_keep();
  }

  // ACC moved for another rule: match again
  if (r->acc & R_AGAIN) goto again;
  return 1;
}
// emit template t (len bytes) on the slot operands p; op: for A_ALU,
// LIB_MULDIV
static void emit_tpl(const uchar *t, uchar len, const uchar *p, uchar op) {
  const uchar *e, *rs = 0, *re = 0;
  uchar b, c, a, zp = lc_tzp, rn = 0;
  word fx[4], top = 0, smfix = 0, a16;
  uchar nfx = 0;

  // by the opcode's column (lo nibble c)
  for (e = t + len; t < e; ) {
    b = *t++;
    c = b & 15;

    // abs op / A_ALU (columns C D E F): + slot spec
    if (c >= 12) {
      c = *t++;
      if (b == A_ALU)
        b = op == '+' ? ADC : op == '-' ? SBC : op == '&' ? AND_ :
            op == '|' ? ORA : EOR;
      a = c & 15;
      c &= RS_HI;

      // the pending ACC: a const as #imm, else its slot
      if (a == RS_ACC && acc_hk)
        em2(immop(b), (uchar)(c ? acc_k >> 8 : acc_k));

      // opmemset_adr: absolute
      else if (a == RS_MSA) ema(b, (word)(REF(opmemset_adr) + (c ? 1 : 0)));

      // a static array's element at a const index: absolute
      else if (a == RS_KK) ema(b, (word)(const_val(p[0]) + const_val(p[1])));
      else if (a == RS_KW)
        ema(b, (word)(const_val(p[0]) + (const_val(p[1]) << 1) + (c ? 1 : 0)));
      else if (a == RS_KA3) ema(b, const_val(p[0] + 1));
      else if (a == RS_K32) ema(b, (word)(const_val(p[0] + 1) + const_val(p[1])));

      // a slot
      else {
        a = a == RS_VAL ? (uchar)(lc_cur_frame + 1) :
            a == RS_ACC ? acc_s : p[a - 1];
        emit_alub(b, a, c);
      }

    // actions (columns 2, 3; not LDX #)
    } else if (c == 3 || (c == 2 && b != LDXN)) {
      switch (b) {
      case A_PEND0: case A_PEND1: emit_lda_pend(b >> 4); break;
      case A_PTR:   zp = emit_ptr(p[0]); break;
      case A_INC1:  emit_inc(p[0]); break;
      case A_PATCH: empatch(fx[--nfx & 3]); break;
      case A_PATCHU:                         // the one under the top
        nfx--;
        top = fx[(nfx - 1) & 3]; fx[(nfx - 1) & 3] = fx[nfx & 3];
        empatch(top);
        break;
      // a const index < 128 (p->f): the pointer itself, Y = 2k, no add
      case A_ELW:
        if (is_const(p[1]) && const_val(p[1]) < 128 && !is_const(p[0])) {
          zp = emit_ptr(p[0]);
          lc_ty0 = (uchar)(const_val(p[1]) << 1);
        } else {
          lc_ty0 = ai_word(p[0], p[1]);
          zp = AI;
        }
        break;
      case A_ELB:   ai_byte(p[0], p[1]); zp = AI; break;
      case A_YK0:   em2(LDYN, lc_ty0); break;
      case A_YK1:   em2(LDYN, (uchar)(lc_ty0 + 1)); break;
      case A_YJ0:   em2(LDYN, lc_ty1); break;
      case A_YJ1:   em2(LDYN, (uchar)(lc_ty1 + 1)); break;
      case A_ZAI:   zp = AI; break;
      case A_ZAS:   zp = AS; break;
      case A_BRD:   emjump(*t++, lc_tdest); break;
      case A_CARRY:
        if (op == '+') em(CLC); else if (op == '-') em(SEC);
        break;
      case A_ELBK:                           // Y = idx lo, A = idx hi
        if (lc_axs == p[1]) TPL(_TAY _TXA, 0);
        else if (lc_yas != p[1]) TPL(_LDY RLO2 _LDA RHI2, p);
        TPL(_CLC _ADC RHI1, p);              // + base hi: the abs,Y's hi
        em(STA); smfix = lc_at; em(0); em(0);
        break;
      case A_FOLD: {
        word k = const_val(p[0]);
        acc_pk((word)(op == '+' ? acc_k + k : op == '-' ? acc_k - k :
                      op == '&' ? (acc_k & k) : op == '|' ? (acc_k | k) :
                      (acc_k ^ k)));
        break;
      }
      case A_TOP:   top = lc_at; break;
      case A_BACK:  b = *t++; em2(b, (uchar)(top - (lc_at + 2))); break;
      case A_LD1:   emit_load_slot(p[0]); break;
      case A_LD2:   emit_load_slot(p[1]); break;
      case A_LDO:   em2(LDAN, p[0]); break;
      case A_NOW:   acc_need(); break;
      case A_STR:   c = 0; do em(p[c]); while (p[c++]); break;
      case A_REP:                            // next c bytes, lc_rn times
        c = *t++;
        if (lc_rn) { rs = t; re = t + c; rn = lc_rn; }
        else t += c;
        break;
      case A_SUB8:  lc_rn -= 8; break;
      case A_LDYRN: em2(LDYN, lc_rn); break;
      }

    // branch: forward, A_PATCH lands it
    } else if ((b & 0x10) && c == 0) fx[nfx++ & 3] = emrel(b);

    // abs,Y (column 9, odd rows) + RKA1 / RKA3: slot 1's / 1 + 1's const
    // as the address; an R_ELBK before stores into its hi
    else if (c == 9 && (b & 0x10)) {
      ema(b, const_val(p[0] + (*t++ == RS_KA3)));
      if (smfix) {
        lc_out[smfix]     = (uchar)(lc_out_org + lc_at - 1);
        lc_out[smfix + 1] = (uchar)((lc_out_org + lc_at - 1) >> 8);
        smfix = 0;
      }
    }

    // (zp),Y: on the pointer of A_PTR / A_ELW / A_ELB
    else if ((b & 0x10) && c == 1) em2(b, zp);

    // JSR library routine
    else if (b == JSR) {
      c = *t++;
      if (c == LIB_MULDIV)
        c = op == '*' ? LIB_MUL : op == '/' ? LIB_DIV : LIB_MOD;
      a16 = rule_ref(c);
      if (a16 > lc_libtop) lc_libtop = a16;    // the lib the image needs
      ema(JSR, a16);
    }

    // BRK RTI RTS: one byte
    else if (c == 0 && !(b & 0x80)) em(b);

    // zp op on _TZP / _TZP1: the pointer
    else if (c >= 4 && c <= 6 && !(b & 0x10) && *t < 2) em2(b, zp + *t++);

    // #imm, zp, (zp,X): + its operand byte
    else if (c <= 2 || (c >= 4 && c <= 6) || c == 9) em2(b, *t++);

    // implied: one byte
    else em(b);

    // end of an R_REP body: again
    if (t == re && --rn) t = rs;
  }
}
static int jit_rule(uchar op, const uchar *p) { return jit_rule_k(op, op, p); }

// main's vector, if main runs once: _start's call (the first call op from
// lc_main_bc) is the only call of it, and no jump goes to its start. 0xff:
// not so
static uchar main_once(void) {
  word pc = lc_main_bc, k;
  uchar v = 0xff, n = 0, op;

  // _start calls it
  while (pc < lc_len && lc_code[pc] < 0xC0) {
    k = ivm_oplen(lc_code + pc);
    pc += k ? k : 1;
  }
  if (pc >= lc_len) return 0xff;
  v = (uchar)(lc_code[pc] - 0xC0);
  if (!lc_vec_bc || v >= lc_ncall) return 0xff;

  // only there; no jump back to its start
  for (pc = 0; pc < lc_len; pc += k ? k : 1) {
    op = lc_code[pc];
    if (op == (uchar)(0xC0 + v)) n++;
    if (ivm_jump(lc_code + pc, pc) == lc_vec_bc[v]) return 0xff;
    k = ivm_oplen(lc_code + pc);
  }
  return n == 1 ? v : 0xff;
}

// One op of main's global inits, done now: C L S Z l s on mem[] and ACC.
// 0: not one of them (or ACC unknown for an S): the inits end here
static uchar init_fold(const uchar *p) {
  uchar op = p[0], d = p[1], k = lc_constidx;

  // a load: ACC = mem[s] (a const, or what the inits put there)
  if (op < 32 || op == 'L' || op == 'l' || op == 'C') {
    uchar s = op < 32 ? op : op == 'C' ? p[2] : d;
    lc_ifv = s >= k ? lc_constmem[s] : lc_initmem[s];
    if (op == 'l') lc_ifv &= 0xff;
    lc_ifk = 1;
    if (op != 'C') return 1;
  } else if (op == 'Z' || op == 'Y') {
    if (d >= k) return 0;
    lc_initmem[d] = op == 'Y';
    return 1;
  } else if ((op != 'S' && op != 's') || !lc_ifk) return 0;

  // a store: mem[d] = ACC (s: its lo, hi 0)
  if (d >= k) return 0;
  lc_initmem[d] = op == 's' ? (word)(lc_ifv & 0xff) : lc_ifv;
  return 1;
}

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

  lc_out = out; lc_at = 0; lc_max = out_max; lc_code = code; lc_len = code_len;
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
  lc_libtop = 0;
  lc_kread = 0;
  lc_ifold = 0;
  lc_ncall = ncall;
  lc_fbase = frame_base;
  lc_vec_bc = vec_bc;
  lc_cur_frame = frame_base ? frame_base[0] : 0;
  lc_main_vec = lc_initmem ? main_once() : 0xff;

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
  lc_alias = 0;
  lc_lbl = (uchar *)calloc((code_len >> 3) + 1, 1);
  if (lc_lbl) {
    word t, n;
    #define LBL(x) if ((x) < code_len) lc_lbl[(x) >> 3] |= (uchar)(1 << ((x) & 7))
    for (i = 0; i < code_len; i += n) {
      n = ivm_oplen(code + i);
      if (!n) n = 1;
      if (code[i] == '`') lc_alias = 1;            // & of a variable
      if (strchr("SIJUCMH',tu", code[i]) && i + 1 < code_len)
        wword(code[i + 1]);                        // word write (byte slots)
      if (IS_LOOP(code[i]) && is_loop_head(i)) {
        LBL(i + 4);                                // body start
        continue;
      }
      if (code[i] == 'n') continue;
      t = ivm_jump(code + i, i);
      if (code[i] == 'B' && t < code_len && IS_LOOP(code[t]) &&
          i_exit_b(t) == i + 2)
        t = 0xffff;                // closes a rotated while: no jump to 't'
      if (t != 0xffff) LBL(t);
      // 'B' to a slot load: candidate "AX == slot" label
      if (code[i] == 'B' && t < code_len && lc_nksa < MAX_LOOPS * 4 &&
          (code[t] < 32 || code[t] == 'L') && !is_fn_entry(t) &&
          t != lc_main_bc) {
        ksa_add(t, code[t] < 32 ? code[t] : code[t + 1], 1);
      }
    }
    for (i = 0; vec_bc && i < ncall; i++) LBL(vec_bc[i]);
    LBL(lc_main_bc);
    #undef LBL
    // ... but not if any other jump goes there
    for (i = 0; i < code_len; i += n) {
      n = ivm_oplen(code + i);
      if (!n) n = 1;
      t = IS_LOOP(code[i]) && is_loop_head(i) ? (word)(i + 4)
          : code[i] == 'n' || code[i] == 'B'
          ? 0xffff : ivm_jump(code + i, i);
      if (t != 0xffff) {
        LcKsa *q;
        while ((q = ksa_at(t)) != 0) q->bc = 0xffff;
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
      LcKsa *q = ksa_at(bc_pc);
      if (q && !(acc_at == ACC_AX && acc_s == q->slot)) q->bc = 0xffff, q = 0;
      if (q) acc_ax_slot(q->slot);
      else if (bc_pc == lc_ya_bc) acc_lost();     // stride body: Y/A = var
      else if (acc_free_label(bc_pc)) acc_lost();
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

          // main starts with the global inits: fold them (main_once)
          if (i == lc_main_vec) { lc_ifold = 1; lc_ifk = 0; }
          break;
        }
      }
    }

    // main's global inits: done now, into mem[]: no code. Until another
    // op or a label: then ACC is the pending const they left
    if (lc_ifold) {
      if ((bc_pc == lc_vec_bc[lc_main_vec] || !is_label(bc_pc)) &&
          init_fold(p)) {
        p += ivm_oplen(p);
        continue;
      }
      lc_ifold = 0;
      if (lc_ifk) acc_pk(lc_ifv);
    }

    // NEXT op
    op = *p++;
    lc_op = op; lc_bc = bc_pc; lc_acc_said = 0;

    // element cache: drop it at joins, calls ($C0+), jumps (n B k),
    // sys calls (x y z), returns (q X ; Q), loop heads
    if (is_label(bc_pc) || op >= 0xC0 || strchr("nBkxyzqX;Q", op) ||
        (IS_LOOP(op) && is_loop_head(bc_pc)))
      ai_drop();

    // a write of slot w: drops the cache if w is its base or idx, the
    // a[j+c] relation if w is t or src
    // & of a variable: a store through a pointer may write any slot
    else if (lc_alias && op >= 32 && strchr("!,w)]mAVxy", op)) {
      ai_drop();
      if (op == 'm' || op == 'A') acc_s = 0xff;
    }
    else if (op >= 32 && strchr("SIJUCMH',sijZYF", op)) {
      uchar w = *p;
      if (lc_ai_kind && (w == lc_ai_base || w == lc_ai_idx)) lc_ai_kind = 0;
      if (w == lc_rel_t || w == lc_rel_src) lc_rel_t = 0xff;

      // new relation: S t after "+ c" (t = src + c), C t2 t (t2 too)
      if (op == 'S' && lc_plus_src != 0xff && lc_plus_src != w) {
        lc_rel_t = w; lc_rel_src = lc_plus_src; lc_rel_c = lc_plus_c;
      } else if (op == 'C' && p[1] == lc_rel_t && w != lc_rel_src) {
        lc_rel_t = w;
      }
    }
    if (op != 'S') lc_plus_src = 0xff;          // "+ c" only right before S

    // AX holds slot lc_axs (valid while no code is emitted, no label)
    if (acc_at == ACC_AX) { lc_axs = acc_s; lc_axs_at = lc_at; }
    else if (lc_at != lc_axs_at || is_label(bc_pc)) lc_axs = 0xff;

    // Y / A hold slot lc_yas (stride loop body)
    if (bc_pc == lc_ya_bc) { lc_yas = lc_ya_next; lc_yas_at = lc_at; }
    else if (lc_at != lc_yas_at) lc_yas = 0xff;

    // Z flag of the last op's ACC: for this op (not after a label)
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
        acc_need();                      // in, KEEP
        if (lc_z) {                                         // Z is ACC == 0
          emjump(BEQ, dest);
          if (lc_z == 2) { acc_lost(); goto report; }       // X never set
        }
        else if (acc_x0) { em2(CMPN, 0); emjump(BEQ, dest); }   // X == 0
        else {
          lc_tdest = dest;
          TPL(_CMPN "\0" _BNE _CPXN "\0" R_BRD _BEQ R_PATCH, 0);
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

    // constants: loaded only if an op needs them in AX
    case '0': acc_pk(0); break;
    case '9': acc_pk(0xffff); break;

    // load/store
    case 'L': acc_ps(PAR); break;
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
      uchar s = PAR, sa[2];

      // the a[j+c] relation: "+ c" on a pending slot
      lc_plus_src = (op == '+' && acc_at == ACC_PEND && !acc_hk &&
                     is_const(s) && const_val(s) < 128) ? acc_s : 0xff;
      lc_plus_c = (uchar)(is_const(s) ? const_val(s) : 0);

      // commutative, AX already holds mem[s] (k = i + prime right after
      // S prime): mem[s] OP mem[x]
      if (acc_at == ACC_PEND && !acc_hk && lc_axs == s && acc_s != s &&
          op != '-') {
        uchar x = acc_s;
        acc_ax_slot(s);
        s = x;
      }

      // the rules ('^' as '|': R_ALU uses the op)
      sa[0] = s; sa[1] = p[1];
      lc_rskip = 0;
      jit_rule_k(op == '^' ? '|' : op, op, sa);
      p += lc_rskip;
      break;
    }

    // compares → boolean ACC (0 or 1), X = 0.  UNSIGNED.
    //   T_CMPS:  C = ACC >= mem (CMP lo; TXA; SBC hi)
    //   T_RSUBS: C = mem >= ACC (mem + ~ACC + 1)
    //   T_NOTC:  A = !C, X = 0
    #define T_CMPS  _CMP RLO1 _TXA _SBC RHI1
    #define T_RSUBS _SEC _EORN "\xFF" _ADC RLO1 _TXA _EORN "\xFF" _ADC RHI1
    #define T_NOTC  _LDAN "\0" _TAX _ROLA _EORN "\x01"
    // '<': ACC < mem  <=>  borrow in ACC - mem  <=>  !C
    case '<': case '>': {
      uchar s = PAR, skip, brc;
      word t = fuse_branch((word)(p - code), &skip, &brc);
      if (t != 0xffff) {
        // compare + [? ...] + zjp / G: just the branch, no 0/1. Flags
        // C = !cond: '<' a - s, '>' s - a (a > s <=> s < a)
        if (acc_at == ACC_PEND && !acc_hk) {
          if (op == '<') emit_sub_flags(acc_s, s); else emit_sub_flags(s, acc_s);
        } else if (op == '<') {
          acc_need();
          TPL(T_CMPS, &s);
        } else {
          acc_need();
          TPL(T_RSUBS, &s);
        }
        p += skip;
        emjump(brc, t);
        acc_lost();
        break;
      }
      if (op == '>') goto op_gt;
      acc_need();
      TPL(T_CMPS T_NOTC, &s);
      acc_ax_byte(1);
      break;
    }
    // '>': ACC > mem  <=>  mem - ACC borrows  <=>  !C of (mem + ~ACC + 1)
    op_gt: {
      uchar s = p[-1];
      acc_need();
      TPL(T_RSUBS T_NOTC, &s);
      acc_ax_byte(1);
      break;
    }

    // * / % — helpers as in ivm-vm.asm (ACC op PAR); TODO: PARAX into zp_par
    case '*': case '/': case '%': {
      uchar s = PAR, sa[1];
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
      sa[0] = s;                           // ivm-lib: JSR
      if (!jit_rule_k(op, op, sa)) jit_rule_k(MULDIV, op, sa);
      break;
    }

    // I: INC lo; BNE skip; INC hi; reload ACC

    // i j: char ++ --: the hi byte of a char var is always 0 (all its
    //   stores are 's', 'i', 'j'), so only the lo byte changes


    // byte slot
    // A bb ii jj: swap ((word*)base)[i] and [j]; ACC = the old [i]
    //   AS = &base[j], AI = &base[i] (cached: stays valid after)
    case 'A': {                                    // -, NEW
      uchar base = PAR, i = PAR, j = PAR;
      uchar yi = ai_hit('w', base, i), yj = ai_hit('w', base, j);
      acc_drop();
      if (yi != 0xff && yj != 0xff) {
        // both off AI (a[j], a[j+1]: the cache): Y offsets, no AS. hi
        // via X, then lo via the stack
        lc_ty0 = yi; lc_ty1 = yj;
        TPL(R_YK1 _LDAIY _TAX R_YJ1 _LDAIY R_YK1 _STAIY R_YJ1 _TXA _STAIY
            R_YK0 _LDAIY _PHA R_YJ0 _LDAIY R_YK0 _STAIY R_YJ0 _PLA _STAIY, 0);
        acc_ax();                                  // A X = old [i]
        break;
      }
      emit_elem_ptr_zp(AS, base, j);
      if (ai_hit('w', base, i) != 0) {
        emit_elem_ptr(base, i);
        if (!is_const(i)) { lc_ai_kind = 'w'; lc_ai_base = base; lc_ai_idx = i; }
        else lc_ai_kind = 0;
      }
      // swap: hi via X, then lo via the stack
      TPL(_LDYN "\x01" _LDAIY _TAX R_ZAS _LDAIY R_ZAI _STAIY _TXA R_ZAS _STAIY
          _DEY R_ZAI _LDAIY _PHA R_ZAS _LDAIY R_ZAI _STAIY _PLA R_ZAS _STAIY,
          0);
      acc_ax();                                    // A X = old [i]
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
      acc_need();
      if (acc_x0) {                       // X == 0: ACC > 0 <=> A != 0
        if (!lc_z) em2(CMPN, 0);
        emjump(BNE, dest);
        break;
      }
      // negative, == 0: no jump; lo != 0: > 0 (Y is scratch)
      lc_tdest = dest;
      TPL(_CPXN "\x80" _BCS _TAY _BNE _CPXN "\0" _BEQ
          R_PATCHU R_BRD _JMP R_PATCH R_PATCH, 0);
      break;
    }
    // B: unconditional back
    case 'B': {
      word rel = PAR;
      word dest = (word)((p - code) - rel);
      LcKsa *q;
      if (IS_LOOP(code[dest]) && i_exit_b(dest) == p - code &&
          is_stride(dest)) {
        acc_lost();                // stride: Y/A from the 'U' just before
        loop_close(dest);
        emit_loop_test_ya(code[dest + 2], (word)(dest + 4), code[dest] == 'u');
        loop_align_check(dest);
        break;                     // fell out: Y/A = var, ACC undefined
      }
      if (IS_LOOP(code[dest]) && i_exit_b(dest) == p - code) {
        acc_lost();                // closes a while: the test is here
        loop_close(dest);
        emit_loop_test(code[dest + 1], code[dest + 2], (word)(dest + 4), 1,
                       code[dest] == 'u');
        loop_align_check(dest);
        acc_ax_slot(code[dest + 1]);   // fell out: AX = the var
        break;
      }
      if (!acc_free_label(dest)) acc_flush();  // label expects AX
      acc_keep();
      // back to a loop body start that expects AX = its loop var
      if ((q = ksa_at(dest)) != 0) {
        if (!(acc_at == ACC_AX && acc_s == q->slot)) emit_load_slot(q->slot);
        acc_ax_slot(q->slot);
      }
      emjump(JMP, dest);
      acc_ax();
      break;
    }

    case ' ': acc_keep(); break; // spacer
    case 127: acc_keep(); break; // DEL: peephole filler (squeezed out by ivm-gen)

    // t: FOR slot lim skip -- rotated loop: the test is only in 'n'
    //   t:    JMP test          (operand patched by the matching 'n')
    //   body: ...               (AX = mem[slot] here, from the test)
    //   n:    INC slot; test: LDA/LDX slot; CPX/CMP lim; Bcc body
    //   exit:                   (falls out of the test: skip is not used)
    case 't': case 'u': {
      uchar slot = p[0], ax;
      if (!is_loop_head(bc_pc)) {
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
      if (is_stride(bc_pc)) {      // test on Y/A: set them
        uchar lim = p[1];
        (void)lim;
        p += 3;
        if (yah_is(slot)) {
          // already: Y = lo, A = hi ("+ v; S slot")
        } else if (lc_axs == slot || (acc_at == ACC_AX && acc_s == slot)) {
          TPL(_TAY _TXA, 0);
        } else {
          TPL(_LDY RLO1 _LDA RHI1, &slot);
        }
        acc_lost();
        lc_ya_next = slot; lc_ya_bc = (word)(bc_pc + 4);
        em(JMP);
        loop_open(bc_pc, 0);
        em(0); em(0);
        loop_pad(bc_pc);
        break;
      }
      // The test always leaves the loop var in AX.
      // - the body may use it (also s = s + i: + is commutative)
      // - it's cheaper anyway: the high byte first, mostly done there
      //
      // Before, only when the body started with it (test-loop-*.c).
      ax = 1;
      if (ax) {                    // body start: AX = loop var (loop top)
        if (lc_nksa < MAX_LOOPS * 4) ksa_add((word)(bc_pc + 4), slot, 0);
      }
      p += 3;                      // slot lim skip: 'n' reads slot, lim
      if (ax) acc_ax_slot(slot);   // unreachable after the JMP: no fall-in
      else acc_lost();             // load at the body start. -, UNDEF
      em(JMP);
      loop_open(bc_pc, ax);
      em(0); em(0);
      loop_pad(bc_pc);
      break;
    }

    // v a #k skip: if (a == k), k a byte: past the body if not. -, UNDEF
    //   LDA a; CMP #k; BNE ex; LDA a+1; BNE ex   (k 0: no CMP)
    case 'v': {
      word ex = ivm_jump(code + bc_pc, bc_pc);
      acc_drop();
      TPL(_LDA RLO1, p);
      if (p[1]) em2(CMPN, p[1]);
      emjump(BNE, ex);
      TPL(_LDA RHI1, p);
      emjump(BNE, ex);
      p += 3;
      acc_lost();
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
    // `: ACC = &mem[s], a const here (zp with -z)        -, NEW
    case '`': {
      uchar s = PAR;
      acc_pk(slotlo(s));
      break;
    }

    // V dd ss zz
    // $ v 
    // x id a
    // y id a b
    //
    // Arguments are put into ivmlib_args.
    // (ACC, x y's last one: first, AX is used after)
    // JSR the routine (ivm-lib.asm cut 6)
    // result: in AX
    case 'V': case '$': case 'x': case 'y': {
      word args = REF(ivmlib_args), r = 0;
      uchar xy = op == 'x' || op == 'y', k;
      uchar ns = op == 'V' ? 3 : op == 'y' ? 2 : 1;      // slot operands
      const uchar *sl = xy ? p + 1 : p;
      if (xy) {
        // the routine: from ivm-lib's vector (the simulator: its traps,
        // in the same order)
        uchar i = (uchar)(p[0] - 16);
        if (i < IVM_NLIB) {
        #ifdef HOSTSIM
          r = hl_lib ? hl_vec("_ivmlib_libv", i) : (word)(SIM_opstrcmp + i);
        #else
          r = ivmlib_libv[i];
        #endif
        } else {
          printf("%%JIT: no library function %u\n", p[0]);
          lc_err = 1;
        }
        acc_need();
        ema(STA, (word)(args + 2 * ns));
        ema(STX, (word)(args + 2 * ns + 1));
      } else {
        acc_drop();
        r = op == 'V' ? REF(opmove) : REF(opstrlen);
      }
      for (k = 0; k < ns; k++) {
        emit_load_slot(sl[k]);
        ema(STA, (word)(args + 2 * k));
        ema(STX, (word)(args + 2 * k + 1));
      }
      if (r > lc_libtop) lc_libtop = r;          // the lib the image needs
      ema(JSR, r);
      acc_ax();
      p += ivm_oplen(p - 1) - 1;
      break;
    }

    // U dd aa     ACC = dd += aa;
    case 'U': {                                    // -, SLOT (a)
      uchar a = PAR;
      p++;                                         // b: slot operand 2
      // stride loop: next is its closing 'B': leave Y = lo, A = hi
      if (p[0] == 'B') {
        word d = (word)((p + 2 - code) - p[1]);
        if (IS_LOOP(code[d]) && code[d + 1] == a &&
            i_exit_b(d) == (word)(p + 2 - code) &&
            is_stride(d)) {
          acc_drop();
          TPL(_CLC _LDA RLO1 _ADC RLO2 _STA RLO1 _TAY
              _LDA RHI1 _ADC RHI2 _STA RHI1, p - 2);
          acc_lost();              // -, SLOT: ACC unused, the 'B' follows
          break;
        }
      }
      jit_rule(op, p - 2);                         // byte by byte
      break;
    }

    // { }: ACC <<= / >>= mem[s].
    // Const: emit_shift; else a loop on Y
    case '{': case '}': {                          // in, NEW
      uchar s = PAR;
      if (is_const(s)) {
        emit_shift(op == '}', (uchar)(const_val(s) > 15 ? 16 : const_val(s)));
        break;
      }
      lc_rn = 0xff;
      jit_rule(op, p - 1);                         // a loop on Y
      break;
    }

    // m: memset bytes. ACC = count, ptr slot, val slot (lo byte = fill).
    //   const 1..256: inline  LDY #n; LDA #v; l: DEY; STA (zp),Y; BNE l
    //   else: X = pages, Y = rest, the routine's STA $xxxx,Y operand =
    //   ptr + X*256 (last page), A = fill; JSR _opmemset (ivm-lib.asm):
    //   rest of the last page first, then full pages down. Max 32K (BPL).
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

  if (lc_at > lc_max) {                 // em() dropped the rest
    printf("%%JIT: code buffer full (%u of %u bytes)\n", (unsigned)lc_at,
      (unsigned)lc_max);
    return 0;
  }
  if (lc_err) {
    printf("%%JIT: short branch out of range\n");
    return 0;
  }

  // patch forward jumps (G/i and any unmapped target). Calls need no fixups.
  for (i = 0; i < lc_nfix; i++) {
    LcFix *f = &lc_fix[i];
    word dest = f->bc_target;
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

    if (f->sh) {                            // short: rel8 from after it
      int off = (int)mapped - (int)(f->at + 1);
      if (off > 127) {                        // estimate too low: long next
        word b = f->bc_from;
        if (lc_long) lc_long[b >> 3] |= (uchar)(1 << (b & 7));
        lc_retry = 1;
      }
      lc_out[f->at] = (uchar)off;
      continue;
    }

    // write absolute JMP target (long form)
    if (f->at + 1 < lc_max) {
      word abs = (word)(lc_out_org + mapped);
      lc_out[f->at]     = (uchar)(abs & 0xff);
      lc_out[f->at + 1] = (uchar)(abs >> 8);
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

#ifdef MEMMAP
// ---- memory map (-DMEMMAP) ---------------------------------------------
//
// The areas in address order and what's in them: slots (names, frames,
// consts), the library routines, strings, static arrays, the functions,
// the heap. Names from NAME.sym (parse -DSYMS) next to the .ivm, if there.
// The caller notes each area (mm_area), then mm_print.

const char *mm_ivm;                    // the .ivm file (the caller sets it)

enum { MM_SLOTS, MM_LIB, MM_IMG, MM_DATA, MM_BSS, MM_CODE, MM_HEAP, MM_N };
static const char *const mm_what[MM_N] = {
  "SLOTS mem[]", "ivm-lib", "MEM[] image", "DATA strings, init arrays",
  "BSS static arrays", "CODE", "HEAP"
};
static word  mm_at[MM_N], mm_len[MM_N];
static char *mm_slot[256];             // .sym: slot -> name
static uchar mm_arr[256];              // .sym: an array (its const slot)
static char *mm_fn[64];                // .sym: vector -> function name

static void mm_area(uchar k, word at, word n) { mm_at[k] = at; mm_len[k] = n; }

// NAME.sym: "  ss  M  name ..." (A: an array), "aaaa  Vvv  name ..."
static char mm_path[128], mm_line[128], mm_name[64];   // not on the C stack
static void mm_syms(void) {
  char *path = mm_path, *line = mm_line, *name = mm_name, *dot = 0, *q;
  unsigned a, v;
  char tag;
  FILE *f;

  if (!mm_ivm || strlen(mm_ivm) > 120) return;
  strcpy(path, mm_ivm);
  for (q = path; *q; q++) if (*q == '.') dot = q;
  strcpy(dot ? dot : q, ".sym");
  if (!(f = fopen(path, "r"))) return;
  while (fgets(line, sizeof mm_line, f)) {
    if (line[0] == ' ' && sscanf(line, " %x %c %63s", &a, &tag, name) == 3 &&
        (tag == 'M' || tag == 'A') && a < 256 && !mm_slot[a]) {
      mm_slot[a] = strdup(name);
      mm_arr[a] = tag == 'A';
    } else if (sscanf(line, "%x V%x %63s", &a, &v, name) == 3 && v < 64)
      mm_fn[v] = strdup(name);
  }
  fclose(f);
}

// slot i: its name, a frame cell, or a const
static void mm_slotline(word at, uchar i, uchar memsz) {
  uchar v, fb, best = 0xff, k;
  static const char *const hd[4] = { "ret", "value", "prev fp", "last param" };

  printf("  $%04X  %02x  ", at, i);
  if (mm_slot[i]) printf("%s%s", mm_slot[i], mm_arr[i] ? "  (array)" : "");
  if (is_const(i)) {
    printf("%sconst $%04X", mm_slot[i] ? "  " : "", const_val(i));
    if (mm_len[MM_BSS] && const_val(i) == mm_at[MM_BSS] + mm_len[MM_BSS])
      printf("  (bss end)");
    printf("\n");
    return;
  }

  // the frame it's in: the highest base <= i
  for (v = 0; lc_fbase && v < lc_ncall; v++) {
    fb = lc_fbase[v];
    if (fb <= i && (best == 0xff || fb > lc_fbase[best])) best = v;
  }
  if (best != 0xff && !mm_slot[i]) {
    k = (uchar)(i - lc_fbase[best]);
    if (mm_fn[best]) printf("%s", mm_fn[best]); else printf("V%02x", best);
    if (k < 4) printf(" %s", hd[k]); else printf(" +%u", k);
  }
  printf("\n");
  (void)memsz;
}

// a list of named addresses, then printed by address, each with its size
// (to the next one; the last: to end)
#define MM_MAXL 80
static word  mm_la[MM_MAXL];
static const char *mm_ln[MM_MAXL];
static uchar mm_nl;
static char  mm_vn[64][4];             // "V00" for unnamed functions

static void mm_add(word a, const char *n) {
  if (mm_nl < MM_MAXL) { mm_la[mm_nl] = a; mm_ln[mm_nl++] = n; }
}

static void mm_list(word end) {
  uchar k, m;
  word a;
  const char *n;

  // sort by address (insertion)
  for (k = 1; k < mm_nl; k++)
    for (m = k; m && mm_la[m - 1] > mm_la[m]; m--) {
      a = mm_la[m]; mm_la[m] = mm_la[m - 1]; mm_la[m - 1] = a;
      n = mm_ln[m]; mm_ln[m] = mm_ln[m - 1]; mm_ln[m - 1] = n;
    }
  for (k = 0; k < mm_nl; k++)
    printf("  $%04X  %-24s %5u bytes\n", mm_la[k], mm_ln[k],
           (word)((k + 1 < mm_nl ? mm_la[k + 1] : end) - mm_la[k]));
  mm_nl = 0;
}

static uchar mm_memsz;
static void mm_print(uchar n) {
  static const char *const lib[] = { "putchar", "putnum", "puts", "free",
    "malloc", "mul", "div", "mod", "printstr", "timed", "memset" };
  uchar k, j, done = 0, memsz;
  word i, w, end;

  mm_memsz = n;                        // (kept off the C stack)
  mm_syms();
  memsz = mm_memsz;
  printf("\n\n======== MEMMAP %s ========\n", mm_ivm ? mm_ivm : (const char *)"");
  while (done != (1 << MM_N) - 1) {

    // the next area by address
    for (j = 0xff, k = 0; k < MM_N; k++)
      if (!(done & (1 << k)) && (j == 0xff || mm_at[k] < mm_at[j])) j = k;
    done |= (uchar)(1 << j);
    if (!mm_len[j]) continue;
    printf("$%04X --- %s (%u bytes) ---\n", mm_at[j], mm_what[j], mm_len[j]);
    end = (word)(mm_at[j] + mm_len[j]);

    switch (j) {

    // each slot: its name, a frame cell or a const
    case MM_SLOTS:
      for (i = 0; i < mm_len[j] / 2; i++)       // (consts: not in it)
        mm_slotline((word)(mm_at[j] + 2 * i), (uchar)i, memsz);
      break;

    // the routines (the startup before them)
    case MM_LIB:
      #ifndef HOSTSIM
      mm_add(mm_at[j], "startup");
      #endif
      for (k = 0; k <= LIB_MEMSET; k++) {
        #ifndef HOSTSIM
        if (k == LIB_TIMED) continue;
        #endif
        if (rule_ref(k) < end) mm_add(rule_ref(k), lib[k]);   // (not cut)
      }
      mm_list(end);
      break;

    // the arrays in it (their const slots point here)
    case MM_DATA: case MM_BSS:
      for (i = 0; i < memsz; i++)
        if (mm_arr[i] && is_const((uchar)i) &&
            (w = const_val((uchar)i)) >= mm_at[j] && w - mm_at[j] < mm_len[j])
          mm_add(w, mm_slot[i]);
      mm_list(end);
      break;

    // the functions (their entries), _start
    case MM_CODE:
      mm_add(mm_at[j], "JMP _start, vectors");
      for (k = 0; lc_vec_bc && k < lc_ncall && k < 64; k++) {
        if (!mm_fn[k]) sprintf(mm_vn[k], "V%02x", k);
        mm_add((word)(lc_out_org + lc_map[lc_vec_bc[k]]),
               mm_fn[k] ? mm_fn[k] : mm_vn[k]);
      }
      if (lc_main_at)
        mm_add((word)(lc_out_org + lc_main_at), "_start (calls main)");
      mm_list(end);
      break;
    }
  }
  printf("\n");
}
#endif // MEMMAP

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
  // strings / memory, their arguments at SIM_ivmlib_args (the JIT put
  // them there): memmove strlen, the x / y ones (ivm.c ivm_lib) on cpu_mem
  case SIM_opmove:
    t = (word)SIM_W(SIM_ivmlib_args);
    memmove(cpu_mem + t, cpu_mem + (word)SIM_W(SIM_ivmlib_args + 2),
            (word)SIM_W(SIM_ivmlib_args + 4));
    SIM_AX(c, t);
    break;
  case SIM_opstrlen:
    t = (word)strlen((char *)cpu_mem + (word)SIM_W(SIM_ivmlib_args));
    SIM_AX(c, t);
    break;
  case SIM_opstrcmp: case SIM_opstrncmp: case SIM_opmemcmp: case SIM_opstrchr:
  case SIM_opmemchr: case SIM_opstrcpy: case SIM_opstrncpy:
    t = ivm_lib(cpu_mem, (uchar)(16 + c->pc - SIM_opstrcmp),
                (word)SIM_W(SIM_ivmlib_args), (word)SIM_W(SIM_ivmlib_args + 2),
                (word)SIM_W(SIM_ivmlib_args + 4));
    SIM_AX(c, t);
    break;
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
  // mem[] without the consts (immediates; lc_kcut checks)
  word membytes = (word)((cidx < memsz ? cidx : memsz) * 2);
  word mem_base, data, code_len, size, i, w;
  word bss_n = 0, bss;
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

  // static arrays (bss): reloc words past the blob's end; the largest is
  // the end marker. A page aligned, zeroed bss right below $F000 (the code
  // size isn't known yet)
  for (i = 0; cidx + i < memsz; i++) {
    w = (word)(blob[hcon + 2 * i] | (blob[hcon + 2 * i + 1] << 8));
    if (!w) break;
    if (w >= len && (word)(w - len) > bss_n) bss_n = (word)(w - len);
  }
  bss = (word)((0xF000 - bss_n) & 0xFF00);

  // strings, then the mem[] image: consts from the pool; string words are
  // blob offsets (before the 0) -> their address here
  memcpy(cpu_mem + data, blob + hdata, len - hdata);
  img = (word *)calloc(memsz ? memsz : 1, sizeof(word));
  for (i = 0; cidx + i < memsz; i++) {
    w = (word)(blob[hcon + 2 * i] | (blob[hcon + 2 * i + 1] << 8));
    if (!w) seen0 = 1;
    else if (!seen0)
      w = w >= len ? (word)(bss + (w - len)) : (word)(data + (w - hdata));
    img[cidx + i] = w;
  }
  // JIT (folds _start's global inits into img)
  for (i = 0; i < ncall; i++)
    vec_bc[i] = (word)((blob[hcall + 2 * i] | (blob[hcall + 2 * i + 1] << 8)) - hcode);
  code_len = (word)((hdata > hcode ? hdata : len) - hcode);
  map = (word *)calloc(code_len ? code_len : 1, sizeof(word));
  out = (uchar *)malloc(8192);
  lc_main_bc = (word)(hmain - hcode);
  lc_main_at = 0;
  lc_constidx = (uchar)cidx;
  lc_constmem = img;
  lc_initmem = img;
  lc_kcut = (uchar)(cidx < memsz ? cidx : 0xff);
  size = ivm_jit(blob + hcode, code_len, out, 8192, map, code_len,
                 mem_base, org, vec_bc, blob + hfp, ncall);
  for (i = 0; i < membytes / 2; i++) SIM_SETW(mem_base + 2 * i, img[i]);
  if (lc_kread) {
    printf("%%SIM: JIT code reads a const from mem[]\n");
    size = 0;
  }
  if (!size || org + size > bss) {
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

#ifdef MEMMAP
  mm_area(MM_SLOTS, mem_base, membytes);
  mm_area(MM_DATA, data, (word)(len - hdata));
  mm_area(MM_BSS, bss, bss_n);
  mm_area(MM_CODE, org, size);
  mm_area(MM_HEAP, (word)((org + size + 0xFF) & ~0xFF),
          (word)(bss - ((org + size + 0xFF) & ~0xFF)));
  mm_area(MM_LIB, SIM_opmemset, (word)(SIM_opputnum + 0x10 - SIM_opmemset));
                                          // C traps (memset is 6502)
  mm_print((uchar)memsz);
#endif
  if (bss_n)
    printf("--- SIM: static arrays $%04X-$%04X ---\n", (unsigned)bss,
           (unsigned)(bss + bss_n - 1));
  printf("--- SIM: mem[] $%04X (%u B), strings $%04X, code $%04X-$%04X"
         " (%u B), heap $%04X ---\n", (unsigned)mem_base, (unsigned)membytes,
         (unsigned)data, (unsigned)org, (unsigned)(org + size - 1),
         (unsigned)size, (unsigned)((org + size + 0xFF) & ~0xFF));

  // run
  sim_heap = (word)((org + size + 0xFF) & ~0xFF);   // page: data alignment
                                          // must not change with code size
  sim_heapend = bss;                      // static arrays above
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
