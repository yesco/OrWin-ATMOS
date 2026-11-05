// LOADCOMPILE — load-time 6502 emitter
// switch(bytecode) → 6502 bytes into out[]. No intermediate ISA.
// ACC: A=lo X=hi (cc65). Y temp.
// JSR helpers share symbols with ivm-vm.asm / C.
//
// #define LOADCOMPILE 1

#ifdef LOADCOMPILE

#include <stdio.h>
#include <string.h>

// ---- 6502 opcodes ----
#define BRK   0x00
#define ORAZ  0x05
#define ORA   0x0D
#define BPL   0x10
#define CLC   0x18
#define JSR   0x20
#define AND_  0x2D
#define BMI   0x30
#define SEC   0x38
#define EOR   0x4D
#define JMP   0x4C
#define RTS   0x60
#define ADC   0x6D
#define STA   0x8D
#define STX   0x8E
#define STY   0x8C
#define STAZ  0x85
#define STXZ  0x86
#define STYZ  0x84
#define DEY   0x88
#define TXA   0x8A
#define TYA   0x98
#define BCC   0x90
#define LDAZ  0xA5
#define LDXZ  0xA6
#define LDYZ  0xA4
#define TAY   0xA8
#define LDAN  0xA9
#define TAX   0xAA
#define LDY   0xAC
#define LDA   0xAD
#define LDX   0xAE
#define LDYN  0xA0
#define LDXN  0xA2
#define BCS   0xB0
#define LDAIY 0xB1
#define STAIY 0x91
#define INY   0xC8
#define DEX   0xCA
#define CMPN  0xC9
#define BNE   0xD0
#define BEQ   0xF0
#define SBC   0xED
#define INC_  0xEE
#define DEC_  0xCE
#define INX   0xE8
#define NOP   0xEA

// same names as ivm-vm.asm imports / C helpers
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

// ---- emitter ----
static uchar *lc_out;
static word   lc_at, lc_max;
static word  *lc_map, lc_mapn, lc_mem;

typedef struct { word at, bc_target; uchar rel; } LcFix;
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
static void emit_load0(void) {
  em2(LDAN, 0);
  em(TAX);
}
static void emit_load_ffff(void) {
  em2(LDAN, 0xff);
  em(TAX);
}

// ACC = ACC OP mem[s]  (ADC/SBC/AND/ORA/EOR). CLC/SEC by caller if needed.
static void emit_lohiop(uchar ins, uchar s) {
  ema(ins, slotlo(s)); em(TAY);
  em(TXA); ema(ins, slothi(s)); em(TAX);
  em(TYA);
}

// zp $FE/$FF as pointer temp
static void emit_ax_to_zpfe(void) {
  em2(STAZ, 0xfe);
  em2(STXZ, 0xff);
}

static void emit_rel(uchar bop, word bc_target) {
  word at;
  em2(bop, 0);
  at = (word)(lc_at - 1);
  if (lc_nfix < 256) {
    lc_fix[lc_nfix].at = at;
    lc_fix[lc_nfix].bc_target = bc_target;
    lc_fix[lc_nfix].rel = 1;
    lc_nfix++;
  }
}

word ivm_loadcompile(const uchar *code, word code_len,
  uchar *out, word out_max,
  word *map, word map_n,
  word mem_base
) {
  word pc;

  if (!code || !out || !map || code_len > map_n) return 0;

  lc_out = out; lc_at = 0; lc_max = out_max;
  lc_map = map; lc_mapn = map_n; lc_mem = mem_base;
  lc_nfix = 0;

  for (pc = 0; pc < code_len; pc++)
    map[pc] = 0xffff;

  for (pc = 0; pc < code_len; ) {
    uchar op;

    if (pc < lc_mapn)
      lc_map[pc] = lc_at;

    op = code[pc];

    // one byte memory ops
    if (op < 32) {
      emit_load_slot(op);
      pc++;
      continue;
    }

    // one byte vector calls
    if (op & 0x80) {
      if (op >= 0xC0)
        ema(JSR, 0); // TODO: vector
      else
        emit_load_slot((uchar)(op & 0x7f));
      pc++;
      continue;
    }

    #define PAR (code[pc + 1])

    switch (op) {

    case 'q': 
      em(RTS);
      pc++;
      break;

    case 'Q':
      emit_load0();
      // fall
    case 'X':
    case ';':
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
      emit_load0();
      emit_store_slot(PAR);
      pc += 2;
      break;

    case '+':
      em(CLC);
      emit_lohiop(ADC, PAR);
      pc += 2;
      break;

    case '-':
      em(SEC);
      emit_lohiop(SBC, PAR);
      pc += 2;
      break;

    case '&':
      emit_lohiop(AND_, PAR);
      pc += 2;
      break;

    case '|':
      emit_lohiop(ORA, PAR);
      pc += 2;
      break;

    case '^':
      emit_lohiop(EOR, PAR);
      pc += 2;
      break;

    // * / % — helpers as in ivm-vm.asm (ACC op PAR)
    case '*':
      // TODO: PARAX into zp_par then
      ema(JSR, REF(opmul));
      pc += 2;
      break;
    case '/':
      ema(JSR, REF(opdiv));
      pc += 2;
      break;
    case '%':
      ema(JSR, REF(opmod));
      pc += 2;
      break;

    // I: INC lo; BNE +3; INC hi; reload ACC
    case 'I': {
      uchar s = PAR;
      ema(INC_, slotlo(s));
      em2(BNE, 3);
      ema(INC_, slothi(s));
      emit_load_slot(s);
      pc += 2;
      break;
    }

    // J: DEC — if lo==0 dec hi first path: LDA lo; BNE +3; DEC hi; DEC lo
    case 'J': {
      uchar s = PAR;
      ema(LDA, slotlo(s));
      em2(BNE, 3);
      ema(DEC_, slothi(s));
      ema(DEC_, slotlo(s));
      emit_load_slot(s);
      pc += 2;
      break;
    }

    case 'N':
      ema(JSR, REF(nl));
      pc++;
      break;

    case '.':
      ema(JSR, REF(opputnum));
      pc++;
      break;

    case 'P':
      // asm op_P: putchar loop on *ACC
      ema(JSR, REF(opputnum)); // if puts is separate, point REF at puts_ax
      pc++;
      break;

    case 'O':
      em2(LDAN, PAR);
      ema(JSR, REF(putchar));
      pc += 2;
      break;

    case 'M': {
      uchar s = PAR;
      // AX = size already in ACC
      ema(JSR, REF(ivm_zalloc));
      emit_store_slot(s);
      pc += 2;
      break;
    }

    case 'F': {
      uchar s = PAR;
      emit_load_slot(s);
      ema(JSR, REF(ivm_free));
      emit_load0();
      emit_store_slot(s);
      pc += 2;
      break;
    }

    // l: ACC = low byte of slot, hi=0
    case 'l': {
      uchar s = PAR;
      ema(LDA, slotlo(s));
      em2(LDXN, 0);
      pc += 2;
      break;
    }

    // s: mem[s] = ACC & 0xff, hi=0
    case 's': {
      uchar s = PAR;
      ema(STA, slotlo(s));
      em2(LDAN, 0);
      ema(STA, slothi(s));
      pc += 2;
      break;
    }

    // r: ACC = *(uchar*)mem[s]
    case 'r': {
      uchar s = PAR;
      emit_load_slot(s);
      emit_ax_to_zpfe();
      em2(LDYN, 0);
      em2(LDAIY, 0xfe);
      em2(LDXN, 0);
      pc += 2;
      break;
    }

    // w: *(uchar*)mem[s] = ACC
    case 'w': {
      uchar s = PAR;
      em(TAY); // save lo char
      emit_load_slot(s);
      emit_ax_to_zpfe();
      em(TYA);
      em2(LDYN, 0);
      em2(STAIY, 0xfe);
      pc += 2;
      break;
    }


    // ' : ACC = *(uchar*)ptr; ptr++  (ptr in slot s)
    case '\'': {
      uchar s = PAR;
      emit_load_slot(s);
      emit_ax_to_zpfe();
      em2(LDYN, 0);
      em2(LDAIY, 0xfe);
      em(PHA);                      // char
      em2(LDAZ, 0xfe);
      em2(0x69, 1);                 // ADC #1
      em2(STAZ, 0xfe);
      em2(LDAZ, 0xff);
      em2(0x69, 0);                 // ADC #0
      em2(STAZ, 0xff);
      em2(LDAZ, 0xfe);
      ema(STA, slotlo(s));
      em2(LDAZ, 0xff);
      ema(STA, slothi(s));
      em(PLA);
      em2(LDXN, 0);
      pc += 2;
      break;
    }

    // , : *(uchar*)ptr = ACC; ptr++
    case ',': {
      uchar s = PAR;
      em(TAY);                      // save char
      emit_load_slot(s);
      emit_ax_to_zpfe();
      em(TYA);
      em2(LDYN, 0);
      em2(STAIY, 0xfe);
      em2(LDAZ, 0xfe);
      em2(0x69, 1);
      em2(STAZ, 0xfe);
      em2(LDAZ, 0xff);
      em2(0x69, 0);
      em2(STAZ, 0xff);
      em2(LDAZ, 0xfe);
      ema(STA, slotlo(s));
      em2(LDAZ, 0xff);
      ema(STA, slothi(s));
      pc += 2;
      break;
    }

    // @ : ACC = *(word *)mem[s]
    case '@': {
      uchar s = PAR;
      emit_load_slot(s);
      emit_ax_to_zpfe();
      em2(LDYN, 0);
      em2(LDAIY, 0xfe);
      em(PHA);
      em(INY);
      em2(LDAIY, 0xfe);
      em(TAX);
      em(PLA);
      pc += 2;
      break;
    }

    case '!': {
      uchar s = PAR;
      em2(STAZ, 0xfc); em2(STXZ, 0xfd); // save ACC
      emit_load_slot(s);
      emit_ax_to_zpfe();
      em2(LDAZ, 0xfc);
      em2(LDYN, 0);
      em2(STAIY, 0xfe);
      em(INY);
      em2(LDAZ, 0xfd);
      em2(STAIY, 0xfe);
      em2(LDAZ, 0xfc); em2(LDXZ, 0xfd); // restore ACC
      pc += 2;
      break;
    }

    // ( base idx : ACC = ((word*)base)[idx]
    case '(': {
      uchar base = PAR;
      uchar idx = code[pc + 2];
      emit_load_slot(idx);
      em(ASL); em(TAY);
      em(TXA); em(ROL); em(TAX);
      em(TYA);
      em2(STAZ, 0xfc); em2(STXZ, 0xfd);
      emit_load_slot(base);
      em(CLC);
      em2(ADCZ, 0xfc); em(TAY);
      em(TXA); em2(ADCZ, 0xfd); em(TAX);
      em(TYA);
      emit_ax_to_zpfe();
      em2(LDYN, 0);
      em2(LDAIY, 0xfe); em(PHA);
      em(INY);
      em2(LDAIY, 0xfe); em(TAX);
      em(PLA);
      pc += 3;
      break;
    }

    case ')': {
      uchar base = PAR;
      uchar idx = code[pc + 2];
      em2(STAZ, 0xfa); em2(STXZ, 0xfb);
      emit_load_slot(idx);
      em(ASL); em(TAY);
      em(TXA); em(ROL); em(TAX);
      em(TYA);
      em2(STAZ, 0xfc); em2(STXZ, 0xfd);
      emit_load_slot(base);
      em(CLC);
      em2(ADCZ, 0xfc); em(TAY);
      em(TXA); em2(ADCZ, 0xfd); em(TAX);
      em(TYA);
      emit_ax_to_zpfe();
      em2(LDAZ, 0xfa);
      em2(LDYN, 0);
      em2(STAIY, 0xfe);
      em(INY);
      em2(LDAZ, 0xfb);
      em2(STAIY, 0xfe);
      pc += 3;
      break;
    }

    case '?':
      em2(STXZ, 0xfe);
      em2(ORAZ, 0xfe);
      em2(BEQ, 6);
      em2(LDAN, 0);
      em2(LDXN, 0);
      em2(BEQ, 4);
      em2(LDAN, 1);
      em2(LDXN, 0);
      pc++;
      break;

    case '~':
      em2(0x49, 0xff); // EOR #$FF
      em(TAY);
      em(TXA);
      em2(0x49, 0xff);
      em(TAX);
      em(TYA);
      pc++;
      break;

    // G: if ((int16_t)ACC > 0) pc += rel
    case 'G': {
      word rel = PAR;
      word dest = (word)(pc + 2 + rel);

      em2(STAZ, 0xfe);
      em(TXA);
      em2(BMI, 7);
      em2(ORAZ, 0xfe);
      em2(BEQ, 3);

      em(CLC);
      emit_rel(BCC, dest);

      em2(LDAZ, 0xfe);
      pc += 2;
      break;
    }

    // B: unconditional back
    case 'B': {
      word rel = PAR;
      word dest = (word)(pc + 2 - rel);
      em(CLC);
      emit_rel(BCC, dest);
      pc += 2;
      break;
    }

    case '"': {
      word s = pc + 1;
      ema(JSR, REF(printinlinedstring));
      while (s < code_len && code[s] != 0) s++;
      if (s < code_len) s++;
      pc = s;
      break;
    }

    case ' ':
      pc++;
      break;

    default:
      em(BRK);
      pc++;
      break;
    }
  }

  for (pc = 0; pc < lc_nfix; pc++) {
    word dest = lc_fix[pc].bc_target;
    word mapped = 0xffff;
    word d;
    int rel;
    if (dest < lc_mapn)
      mapped = lc_map[dest];
    if (mapped == 0xffff) {
      for (d = dest; d < code_len && d < lc_mapn; d++)
        if (lc_map[d] != 0xffff) { mapped = lc_map[d]; break; }
    }
    if (mapped == 0xffff) {
      printf("%%LOADCOMPILE: bad fixup %u\n", (unsigned)dest);
      return 0;
    }
    rel = (int)mapped - (int)(lc_fix[pc].at + 1);
    if (rel < -128 || rel > 127) {
      printf("%%LOADCOMPILE: rel range %d\n", rel);
      return 0;
    }
    if (lc_fix[pc].at < lc_max)
      lc_out[lc_fix[pc].at] = (uchar)rel;
  }

  return lc_at;
}

#endif
