// JIT — load-time 6502 emitter
//
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
// #define JIT 1


/*

INSTRUCTIONS: Keeps this section but keep updated for status

### Call vectors — done

Per your INSTRUCTION, `ivm_jit` now takes the resolved vector table:

```c
word ivm_jit(..., const word *callvec, uchar ncall);

// $C0+v → JSR callvec[v]
// if callvec[v]==0, site is recorded for later patch

void ivm_jit_resolve_vectors(callvec, ncall, vec_bc, map, out_org);
void ivm_jit_patch_calls(out, out_max, callvec, ncall);
```

Typical flow:

1. Compile with `callvec=NULL` (JSRs recorded)
2. `resolve_vectors`: `callvec[i] = out_org + map[vec_bc[i]]`
3. `patch_calls` fills JSR absolute addresses

---

### What sieve still needs

| Need | Status |
|------|--------|
| **Calls**                | **emit+resolve** (above) |
| **Call frame**           | **Missing** — plain `JSR`/`RTS` is not enough; VM call pushes frame / sets `fp` |
| **Compares** `<` `=` `>` | **Missing** (`BRK`) — sieve is full of `i < LIMIT` |
| **Loops** `i` / `n` (or only `G`/`B`) | **Partial** — `G`/`B` only; no `i`/`n`/`U` |
| **Array** `(` `)`        | Present (needs testing) |
| / *                      | JSR helpers; still need **PARAX → zp_par** |

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

#ifdef JIT

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
#define SBCZ  0xE5
#define SBC   0xED
#define INC_  0xEE
#define DEC_  0xCE
#define DEX   0xCA
#define INX   0xE8
#define NOP   0xEA
#define ASL   0x0A
#define ROL   0x2A
#define PHA   0x48
#define PLA   0x68

extern ivmvar ivm_acc;

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

extern ivmvar ivm_zalloc(ivmvar n);
extern void   ivm_free(ivmvar p);

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

// jump vector table at start of code (word per vector)
static word  lc_vecmark;      // offset in out[] of vector table
static word  lc_out_org;      // absolute base of out[]
static uchar lc_ncall;
static uchar lc_cur_frame;    // frame_base of function being compiled
static const uchar *lc_fbase; // frame_base[v]
static const word  *lc_vec_bc;// bytecode entry pc per vector


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

// Reverse a branch opcode (flip the condition bit).
static uchar rev_branch(uchar bop) { return (uchar)(bop ^ 0x20); }

// Forward: long form (reversed branch skips a JMP) — distance unknown yet.
// Backward: short branch if the mapped distance fits in -128..127.
static void emit_rel(uchar bop, word bc_target) {
  word at;

  // backward and already mapped?
  if (bc_target < lc_mapn && lc_map[bc_target] != 0xffff) {
    int rel = (int)lc_map[bc_target] - (int)(lc_at + 2);
    if (rel >= -128 && rel <= 127) {
      em2(bop, (uchar)rel);
      return;
    }
  }

  // long form
  em2(rev_branch(bop), 3);
  em(JMP);
  at = lc_at;
  em(0); em(0);
  if (lc_nfix < 256) {
    lc_fix[lc_nfix].at = at;
    lc_fix[lc_nfix].bc_target = bc_target;
    lc_nfix++;
  }
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

  if (!code || !out || !map || code_len > map_n) return 0;

  lc_out = out; lc_at = 0; lc_max = out_max;
  lc_map = map; lc_mapn = map_n; lc_mem = mem_base;
  lc_nfix = 0;
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
            word start = pc > 4 ? pc - 4 : 0;
            word stop  = pc + 5 < code_len ? pc + 5 : code_len;
            for (k = (int)start; k < (int)stop; k++)
              printf(" %02X%s", code[k], k == (int)pc ? "*" : "");
          }
          printf("\n");
        }

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

    // compares → boolean ACC (0 or 1). '=' trivial; '<'/'>' via subtract + sign/ne
    case '=': {
      uchar s = PAR;
      em(SEC); emit_lohiop(SBC, s);           // ACC -= mem[s]
      em2(STXZ, tmp1); em2(ORAZ, tmp1);       // Z if equal
      em2(BEQ, 6);
      em2(LDAN, 0); em2(LDXN, 0); em2(BEQ, 4);
      em2(LDAN, 1); em2(LDXN, 0);
      break;
    }
    case '<': {
      uchar s = PAR;
      em(SEC); emit_lohiop(SBC, s);           // ACC = ACC - mem[s]
      // signed < : result negative → 1
      em(TXA); em2(BMI, 6);
      em2(LDAN, 0); em2(LDXN, 0); em2(BEQ, 4);
      em2(LDAN, 1); em2(LDXN, 0);
      break;
    }
    case '>': {
      uchar s = PAR;
      // ACC > mem  iff  mem < ACC  iff  !(ACC <= mem) — do mem-ACC via swap:
      // compute mem[s]-ACC: load mem, subtract ACC
      em2(STAZ, tmp1); em2(STXZ, tmp2);       // save ACC
      emit_load_slot(s);
      em(SEC);
      em2(SBCZ, tmp1); em(TAY);
      em(TXA); em2(SBCZ, tmp2); em(TAX);      // AX = mem - ACC
      em(TYA);
      // mem-ACC negative ⇒ mem < ACC ⇒ ACC > mem
      em(TXA); em2(BMI, 6);
      em2(LDAN, 0); em2(LDXN, 0); em2(BEQ, 4);
      em2(LDAN, 1); em2(LDXN, 0);
      break;
    }

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
    case 'N': em2(LDAN, 10); ema(JSR, REF(putchar)); break;
    case 'E': ema(JSR, REF(putchar)); break;  // emit ACC as char
    case '.':
      ema(STA, (word)(unsigned)&ivm_acc);
      ema(STX, (word)((unsigned)&ivm_acc + 1));
      ema(JSR, REF(opputnum));
      break;
    case 'P':
      ema(STA, (word)(unsigned)&ivm_acc);
      ema(STX, (word)((unsigned)&ivm_acc + 1));
      ema(JSR, REF(jit_puts));
      break;
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
      emit_load_slot(slot);
      em(SEC);
      emit_lohiop(SBC, lim);       // ACC = slot - lim
      em(TXA);                     // high byte
      em2(BMI, 2);                 // neg → slot < lim → skip the BNE
      emit_rel(0xD0, dest);        // BNE → slot > lim → skip body
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
      if (ip < code || *ip != 'i') {
        // can't find matching i; just consume
        break;
      }
      slot = ip[1];
      lim  = ip[2];
      loop_top = (word)((ip + 4) - code);  // bc pc of body start
      // increment slot
      ema(INC_, slotlo(slot));
      em2(BNE, 3);
      ema(INC_, slothi(slot));
      // compare with lim
      emit_load_slot(slot);
      em(SEC);
      emit_lohiop(SBC, lim);       // A = low, X = high
      em2(STAZ, 0xfe);             // save low
      em(TXA);
      em2(ORAZ, 0xfe);             // A = high|low; Z if both zero
      // C still valid from SBC: C=0 means slot < lim
      em2(BCC, 4);                 // < → loop
      em2(BEQ, 2);                 // == → loop
      em2(BPL, 3);                 // > → skip CLC(1)+BCC(2)
      em(CLC);
      emit_rel(BCC, loop_top);
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
  }

  // patch relative branches only (G/B). Calls need no fixups.
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
