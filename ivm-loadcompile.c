// LOADCOMPILE — load-time 6502 emitter
// switch(bytecode) → 6502 bytes into out[]. No intermediate ISA.
//
// ACC: A=lo X=hi (cc65 style). Y is temp.
// Pointer walks with op=*p++; PAR advances. No pc++ / pc+=2.
// JSR helpers share symbols with ivm-vm.asm / C.
//
// Opcode naming: base mnemonic + suffix
//   (none) abs   N imm   Z zp   X abs,X   Y abs,Y
//   IX (zp,X)    IY (zp),Y
//
// #define LOADCOMPILE 1


/*

INSTRUCTIONS: Keeps this section but keep updated for status

### Call vectors — done

Per your INSTRUCTION, `ivm_loadcompile` now takes the resolved vector table:

```c
word ivm_loadcompile(..., const word *callvec, uchar ncall);

// $C0+v → JSR callvec[v]
// if callvec[v]==0, site is recorded for later patch

void ivm_loadcompile_resolve_vectors(callvec, ncall, vec_bc, map, out_org);
void ivm_loadcompile_patch_calls(out, out_max, callvec, ncall);
```

Typical flow:

1. Compile with `callvec=NULL` (JSRs recorded)
2. `resolve_vectors`: `callvec[i] = out_org + map[vec_bc[i]]`
3. `patch_calls` fills JSR absolute addresses

---

### What sieve still needs

| Need | Status |
|------|--------|
| **Calls** `$C0` main→primes | **emit+resolve** (above) |
| **Call frame** (fp, args, return) | **Missing** — plain `JSR`/`RTS` is not enough; VM call pushes frame / sets `fp` |
| **Compares** `<` `=` `>` | **Missing** (`BRK`) — sieve is full of `i < LIMIT` |
| **Loops** `i` / `n` (or only `G`/`B`) | **Partial** — `G`/`B` only; no `i`/`n`/`U` |
| **Array** `(` `)` | Present (needs testing) |
| **\* / /** | JSR helpers; still need **PARAX → zp_par** |

Sieve is dominated by **`for` + compare + index + call to
`primes`**. Without **`<`/`>`** and a **real call/return frame**, it
will not run even with vectors patched.

### Next concrete steps

1. Emit **frame prologue/epilogue** (or shared `JSR call_helper` that
matches `ivm-vm.asm` call) for `$C0+`

2. Implement **`<` `=` `>`** → boolean in ACC (then `G` works)

3. Confirm **`(` `)`** and **`M`/`F`** against soft-heap / 6502 memory
model


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
- 

*/

#ifdef LOADCOMPILE

#include <stdio.h>
#include <string.h>

// INSTRUCTION: Keep all OPS defined on 6502 here
//   Don't randomly remove because not used "today"
//   Example: DEX you removed, why?
#define BRK   0x00
#define ORAZ  0x05
#define ORA   0x0D
#define BPL   0x10
#define CLC   0x18
#define JSR   0x20
#define AND_  0x2D
#define BMI   0x30
#define SEC   0x38
#define EORN  0x49
#define EOR   0x4D
#define JMP   0x4C
#define RTS   0x60
#define ADC   0x6D
#define ADCZ  0x65
#define STA   0x8D
#define STX   0x8E
#define STAZ  0x85
#define STXZ  0x86
#define TXA   0x8A
#define TYA   0x98
#define BCC   0x90
#define LDAZ  0xA5
#define LDXZ  0xA6
#define TAY   0xA8
#define LDAN  0xA9
#define TAX   0xAA
#define LDA   0xAD
#define LDX   0xAE
#define LDYN  0xA0
#define LDXN  0xA2
#define LDAIY 0xB1
#define STAIY 0x91
#define INY   0xC8
#define BNE   0xD0
#define BEQ   0xF0
#define SBC   0xED
#define INC_  0xEE
#define DEC_  0xCE
#define ASL   0x0A
#define ROL   0x2A
#define PHA   0x48
#define PLA   0x68

extern void opputnum(void);
extern void putchar(void);
extern void nl(void);
extern void printinlinedstring(void);
extern void opmul(void);
extern void opdiv(void);
extern void opmod(void);
extern void ivm_zalloc(void);
extern void ivm_free(void);

#define REF(f) ((word)(unsigned long)(void *)(f))

// zero-page scratch word (lo at tmp1, hi at tmp2) — single named base, not magic 0xfe elsewhere
static uchar tmp1 = 0xfe;
static uchar tmp2 = 0xff;

static uchar *lc_out;
static word   lc_at, lc_max;
static word  *lc_map, lc_mapn, lc_mem;

typedef struct { word at, bc_target; } LcFix;

static LcFix lc_fix[256];
static word  lc_nfix;

static void em(uchar b) {
  if (lc_at < lc_max) lc_out[lc_at] = b;
  lc_at++;
}
static void em2(uchar a, uchar b) { em(a); em(b); }
static void ema(uchar op, word addr) {
  em(op);
  em((uchar)(addr & 0xff));
  em((uchar)(addr >> 8));
}

static word slotlo(uchar s) { return (word)(lc_mem + ((word)s << 1)); }
static word slothi(uchar s) { return (word)(slotlo(s) + 1); }

static void emit_load_slot(uchar s) {
  ema(LDA, slotlo(s));
  ema(LDX, slothi(s));
}
static void emit_store_slot(uchar s) {
  ema(STA, slotlo(s));
  ema(STX, slothi(s));
}
static void emit_load0(void) { em2(LDAN, 0); em(TAX); }
static void emit_load_ffff(void) { em2(LDAN, 0xff); em(TAX); }

// ACC = ACC OP mem[s]  (ADC/SBC/AND/ORA/EOR abs).
// CLC/SEC emitted by caller when needed (+/-). Reuse for logical ops too.
static void emit_lohiop(uchar ins, uchar s) {
           ema(ins, slotlo(s)); em(TAY);
  em(TXA); ema(ins, slothi(s)); em(TAX);
  em(TYA);
}

static void emit_ax_to_tmp1(void) {
  em2(STAZ, tmp1);
  em2(STXZ, tmp2);
}

// INSTRUCTION:
//   TODO: rewrite so it takes the relative bytecode value
//     this routine can do all the mapping and translation to
//     actual machine code address, as it's record for each
//     bytecode

static void emit_rel(uchar bop, word bc_target) {
  word at;

  em2(bop, 0);
  at = lc_at - 1;
  if (lc_nfix < 256) {
    lc_fix[lc_nfix].at = at;
    lc_fix[lc_nfix].bc_target = bc_target;
    lc_nfix++;
  }
}

// INSTRUCTION: pass in the vector table of resolved native addresses.
// callvec[i] = absolute 6502 address of compiled body for vector i
// (from blob call table: native = out_org + map[vec_bytecode_offset]).
// If callvec is NULL or entry is 0, JSR site is recorded and can be
// patched later with ivm_loadcompile_patch_calls().
word ivm_loadcompile(const uchar *code, word code_len,
  uchar *out, word out_max,
  word *map, word map_n,
  word mem_base,
  const word *callvec, uchar ncall)
{
  const uchar *p, *end;
  word i, bc_pc;
  uchar op;

  if (!code || !out || !map || code_len > map_n) return 0;

  lc_out = out; lc_at = 0; lc_max = out_max;
  lc_map = map; lc_mapn = map_n; lc_mem = mem_base;
  lc_nfix = 0;
  lc_ncalls = 0;
  lc_callvec = callvec;
  lc_ncall = ncall;

  // reset mapping to "slot not used"
  for (i = 0; i < code_len; i++)
    map[i] = 0xffff;

  p = code;
  end = code + code_len;

  while (p < end) {

    // add current compiled address to mapping
    // for each bytecode start pos
    bc_pc = (word)(p - code);
    if (bc_pc < lc_mapn)
      lc_map[bc_pc] = lc_at;

    // next op
    op = *p++;

    // 0..31: fast load mem[op]
    if (op < 32) { emit_load_slot(op); continue; }

    // hibit: vector call ($C0+) or const-slot load
    if (op & 0x80) {
      if (op >= 0xC0) {
        uchar v = (uchar)(op - 0xC0);
        word dest = 0;
        word at;
        if (lc_callvec && v < lc_ncall)
          dest = lc_callvec[v];
        ema(JSR, dest);
        // operand lo sits at lc_at-2 after ema
        at = (word)(lc_at - 2);
        if (!dest && lc_ncalls < 128) {
          lc_calls[lc_ncalls].at = at;
          lc_calls[lc_ncalls].vec = v;
          lc_ncalls++;
        } else if (dest && at + 1 < lc_max) {
          lc_out[at]     = (uchar)(dest & 0xff);
          lc_out[at + 1] = (uchar)(dest >> 8);
        }
      } else {
        emit_load_slot((uchar)(op & 0x7f));
      }
      continue;
    }

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

    // arith / logic (emit_lohiop)
    case '+': em(CLC); emit_lohiop(ADC, PAR); break;
    case '-': em(SEC); emit_lohiop(SBC, PAR); break;
    case '&':          emit_lohiop(AND_, PAR); break;
    case '|':          emit_lohiop(ORA, PAR); break;
    case '^':          emit_lohiop(EOR, PAR); break;

    // * / % — helpers as in ivm-vm.asm (ACC op PAR); TODO: PARAX into zp_par
    case '*': PAR; ema(JSR, REF(opmul)); break;
    case '/': PAR; ema(JSR, REF(opdiv)); break;
    case '%': PAR; ema(JSR, REF(opmod)); break;

    // I: INC lo; BNE +3; INC hi; reload ACC
    case 'I': {
      uchar s = PAR;
      ema(INC_, slotlo(s)); em2(BNE, 3); ema(INC_, slothi(s));
      emit_load_slot(s);
      break;
    }
    // J: LDA lo; BNE +3; DEC hi; DEC lo; reload ACC
    case 'J': {
      uchar s = PAR;
      ema(LDA, slotlo(s)); em2(BNE, 3); ema(DEC_, slothi(s));
      ema(DEC_, slotlo(s)); emit_load_slot(s);
      break;
    }

    // I/O — same symbols as ivm-vm.asm
    case 'N': ema(JSR, REF(nl)); break;
    case '.': ema(JSR, REF(opputnum)); break;
    case 'P': ema(JSR, REF(opputnum)); break; // asm op_P is puts(*ACC); wire puts_ax if split
    case 'O': em2(LDAN, PAR); ema(JSR, REF(putchar)); break;

    // heap
    case 'M': { uchar s = PAR; ema(JSR, REF(ivm_zalloc)); emit_store_slot(s); break; }
    case 'F': { uchar s = PAR; emit_load_slot(s); ema(JSR, REF(ivm_free)); emit_load0(); emit_store_slot(s); break; }

    // byte slot
    case 'l': { uchar s = PAR; ema(LDA, slotlo(s)); em2(LDXN, 0); break; } // ACC = lo(mem[s])
    case 's': { uchar s = PAR; ema(STA, slotlo(s)); em2(LDAN, 0); ema(STA, slothi(s)); break; }

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

    case '\'': {
      uchar s = PAR;
      emit_load_slot(s); emit_ax_to_tmp1();
      em2(LDYN, 0); em2(LDAIY, tmp1); em(PHA);
      em2(LDAZ, tmp1); em2(0x69, 1); em2(STAZ, tmp1);
      em2(LDAZ, (uchar)(tmp2)); em2(0x69, 0); em2(STAZ, (uchar)(tmp2));
      em2(LDAZ, tmp1); ema(STA, slotlo(s));
      em2(LDAZ, (uchar)(tmp2)); ema(STA, slothi(s));
      em(PLA); em2(LDXN, 0);
      break;
    }
    // ,: *(uchar*)ptr = ACC; ptr++
    case ',': {
      uchar s = PAR;
      em(TAY); emit_load_slot(s); emit_ax_to_tmp1();
      em(TYA); em2(LDYN, 0); em2(STAIY, tmp1);
      em2(LDAZ, tmp1); em2(0x69, 1); em2(STAZ, tmp1);
      em2(LDAZ, (uchar)(tmp2)); em2(0x69, 0); em2(STAZ, (uchar)(tmp2));
      em2(LDAZ, tmp1); ema(STA, slotlo(s));
      em2(LDAZ, (uchar)(tmp2)); ema(STA, slothi(s));
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
      // ptr = mem[base] + 2*mem[idx] into tmp1, then word load
      emit_load_slot(base); emit_ax_to_tmp1();
      emit_load_slot(idx);
      em(ASL); em(TAY); em(TXA); em(ROL); em(TAX); em(TYA);
      em(CLC);
      em2(ADCZ, tmp1); em2(STAZ, tmp1);
      em(TXA); em2(ADCZ, (uchar)(tmp2)); em2(STAZ, (uchar)(tmp2));
      // load word into AX using ptr
      em2(LDYN, 0); em2(LDAIY, tmp1); em(PHA);
      em(INY); em2(LDAIY, tmp1); em(TAX); em(PLA);
      break;
    }
    // ): ((word*)base)[idx] = ACC
    case ')': {
      uchar base = PAR, idx = PAR;
      em(PHA); em(TXA); em(PHA);           // save ACC value
      emit_load_slot(base); emit_ax_to_tmp1();
      emit_load_slot(idx);
      em(ASL); em(TAY); em(TXA); em(ROL); em(TAX); em(TYA);
      em(CLC);
      em2(ADCZ, tmp1); em2(STAZ, tmp1);
      em(TXA); em2(ADCZ, (uchar)(tmp2)); em2(STAZ, (uchar)(tmp2));
      em(PLA); em(TAX); em(PLA);           // ACC value A=lo X=hi? pull hi then lo if pushed lo,hi
      // pushed PHA lo; PHA hi → PLA hi; TAX; PLA lo
      em2(LDYN, 0); em2(STAIY, tmp1);
      em(INY); em(TXA); em2(STAIY, tmp1);
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
      // INSTRUCITON: why the F did you remove op codes names? Don't use random consatnats!
      em2(EORN, 0xff); em(TAY); em(TXA); em2(EORN, 0xff); em(TAX); em(TYA);
      break;

    // G: if ((int16_t)ACC > 0) pc += rel8
    case 'G': {
      word rel = PAR;
      word dest = (word)((p - code) + rel);
      em2(STAZ, tmp1); em(TXA);
      em2(BMI, 7); em2(ORAZ, tmp1); em2(BEQ, 3);
// TODO: does this take the instructions into account
      em(CLC); emit_rel(BCC, dest);
      em2(LDAZ, tmp1);
      break;
    }
    // B: unconditional back  pc -= rel8
    case 'B': {
      word rel = PAR;
      word dest = (word)((p - code) - rel);
// TODO: does this take CLC and BCC into account?
      em(CLC); emit_rel(BCC, dest);
      break;
    }

    // " inline string — helper prints until 0
    case '"':
      ema(JSR, REF(printinlinedstring));
      while (p < end && *p) p++;
      if (p < end) p++;
      break;

    case ' ': break; // spacer

    default: em(BRK); break; // unknown

    }
  }

  // patching jumps
  for (i = 0; i < lc_nfix; i++) {
    word dest = lc_fix[i].bc_target;
    word mapped = 0xffff;
    word d;
    int rel;

    if (dest < lc_mapn) mapped = lc_map[dest];

    // INSTRUCITON: describe semantcis of this
    if (mapped == 0xffff)
      for (d = dest; d < code_len && d < lc_mapn; d++)
        if (lc_map[d] != 0xffff) { mapped = lc_map[d]; break; }

    if (mapped == 0xffff) {
      printf("%%LOADCOMPILE: bad fixup %u\n", (unsigned)dest);
      return 0;
    }

    // calulcate relative jump
    rel = (int)mapped - (int)(lc_fix[i].at + 1);

    if (rel < -128 || rel > 127) {
      printf("%%LOADCOMPILE: rel range %d\n", rel);
      return 0;
    }

    if (lc_fix[i].at < lc_max)
      lc_out[lc_fix[i].at] = (uchar)rel;
  }

  return lc_at;
}

#endif
