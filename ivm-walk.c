// ivm-walk.c - IVM opcode table: how many operand bytes, which of them are
// mem[] slots, and relative jumps. ONE table for every bytecode walker:
//   ivm-gen.c   build_blob()      slot remap (consts / -O packing)
//   parse.c     sym_write_all()   L / S counts per slot
//   ivm-print.c ivm_print_one()   pretty printer
//
//   #include "ivm-walk.c" where needed (include guarded)
//
// Source of truth is the VM (ivm.c run_ivm): if an opcode changes there,
// change its byte here, and all walkers follow.

#ifndef IVM_WALK_C
#define IVM_WALK_C

// Info byte per opcode ' '..DEL (ivm_optab[op - 32]):
//
//   bit 0..2  slot mask: operand byte 1, 2, 3 is a mem[] slot (shift out)
//   bit 3..4  number of operand bytes 0..3
//   bit 5     REL:  the LAST operand byte is a relative jump
//   bit 6     BACK: ... jumping backward (else forward)
//   bit 7     SPEC: special length (inline string "...\0") / unknown op
//
// Every relative jump: target = (offset of the rel byte) + 1 +- rel
//   G  rel           if ACC > 0 forward
//   B  rel           back
//   t  slot lim rel  FOR (<=): past body when done (forward)
//   u  slot lim rel  FOR (<):  same
//   n  rel           NEXT: back to its 't' / 'u'
//
//   0x80+rel    zjp (rel < 64): jump forward if ACC == 0, see ivm_jump()
//
// Not in the table (ivm_opinfo() gives 0, one byte, no operands):
//   0x00..0x1f  one-byte load: the opcode IS the slot
//   0x80..0xbf  zjp: the rel is in the opcode itself
//   0xc0..0xff  call vector op - 0xc0
//   0x7f DEL    NOP; filler of the peephole, removed by gen_squeeze()

// ACC contract (VM = truth; every impl must honour KEEP, the compiler and
// peephole may rely on it). in: op reads ACC.  out:
//   KEEP  ACC unchanged       SLOT  ACC == mem[1st slot operand] after
//   NEW   new value           UNDEF don't rely on it (impls differ)
//
//   in  out    ops
//   -   KEEP   ' ' DEL B O N W Z # (#TIMED: JIT on the host sim only)
//   in  KEEP   S(+SLOT) ! , w s ) ] m . P E zjp X(returns it)
//   -   SLOT   L <32 C(dst) U(1st) I J i j H
//   in  SLOT   M(acc = mem[s] = alloc(acc))
//   -   NEW    0 9 Q 1..8 @ r ' l ( [ K A (old a[i])
//   in  NEW    + - * / % & | ^ { } < = > ? ~ x y z call(arg -> result)
//   -   UNDEF  t u n (VM: loop var; JIT: clobbered) F
//   ;   NEW    ACC = return value cell, then return
//
// JIT status (ivm-jit.c) vs this contract, 2026-11:
//   all honoured (acc_* in ivm-jit.c)    missing: W K x y z k
//   l: was sign-extended by host VM
#define OPI_SLOTS    0x07
#define OPI_NARGS(i) (((i) >> 3) & 3)
#define OPI_REL      0x20
#define OPI_BACK     0x40
#define OPI_SPEC     0x80

#define _A0  0x00   // no operands
#define _S1  0x09   // slot
#define _S2  0x13   // slot slot
#define _S3  0x1f   // slot slot slot
#define _D1  0x08   // 1 data byte (not a slot)
#define _D2  0x10   // 2 data bytes
#define _D3  0x18   // 3 data bytes
#define _FW  0x28   // rel forward
#define _BK  0x68   // rel back
#define _I3  0x3b   // slot slot rel-forward
#define _J2  0x31   // slot rel-forward
#define _SP  0x80   // special / unknown

static const uchar ivm_optab[96] = {
  // ' '   !    "    #    $    %    &    '
  _A0, _S1, _SP, _SP, _SP, _S1, _S1, _S1,
  //  (    )    *    +    ,    -    .    /
  _S2, _S2, _S1, _S1, _S1, _S1, _A0, _S1,
  //  0    1    2    3    4    5    6    7
  _A0, _A0, _A0, _A0, _A0, _A0, _A0, _A0,
  //  8    9    :    ;    <    =    >    ?
  _A0, _A0, _SP, _A0, _S1, _S1, _S1, _A0,
  //  @    A    B    C    D    E    F    G
  _S1, _S3, _BK, _S2, _SP, _A0, _S1, _FW,
  //  H    I    J    K    L    M    N    O
  _S1, _S1, _S1, _A0, _S1, _S1, _A0, _D1,
  //  P    Q    R    S    T    U    V    W
  _A0, _A0, _SP, _S1, _SP, _S2, _SP, _S1,
  //  X    Y    Z    [   bsl   ]    ^    _
  _A0, _SP, _S1, _S2, _SP, _S2, _S1, _SP,
  //  `    a    b    c    d    e    f    g
  _SP, _SP, _SP, _SP, _SP, _SP, _SP, _SP,
  //  h    i    j    k    l    m    n    o
  _SP, _S1, _S1, _SP, _S1, _S2, _BK, _SP,
  //  p    q    r    s    t    u    v    w
  _SP, _A0, _S1, _S1, _I3, _I3, _SP, _S1,
  //  x    y    z    {    |    }    ~   DEL
  _D1, _D2, _D3, _S1, _S1, _S1, _A0, _A0,
};

// info byte of opcode op (0 outside ' '..DEL)
#define ivm_opinfo(op) \
  ((uchar)((op) - 32) < 96 ? ivm_optab[(uchar)((op) - 32)] : 0)

// bytes used by the op at p, opcode included; 0 = unknown opcode
// TODO: 'k' (switch) when it is emitted: low n back... / 0xff n (c back)...
static word ivm_oplen(const uchar *p) {
  uchar i = ivm_opinfo(*p);
  word n;

  if (!(i & OPI_SPEC)) return 1 + OPI_NARGS(i);
  if (*p != '"' && *p != '#') return 0;

  // inline string: " bytes... 0   (# name... 0: #TIMED)
  for (n = 1; p[n]; n++)
    ;
  return n + 1;
}

// jump target of the op at p (= code + pc), 0xffff if it is not a jump
//   0x80+rel (rel < 64): zjp, forward if ACC == 0 (ACC kept)
static word ivm_jump(const uchar *p, word pc) {
  uchar i = ivm_opinfo(*p), n;

  if ((uchar)(*p - 0x80) < 64) return (word)(pc + 1 + (*p & 0x3f));
  if (!(i & OPI_REL)) return 0xffff;

  n = OPI_NARGS(i);           // the rel byte is the last operand: p[n]
  if (i & OPI_BACK) return (word)(pc + n + 1 - p[n]);
  return (word)(pc + n + 1 + p[n]);
}

// do body for each slot operand of the op at p; q points at that byte
//   IVM_EACH_SLOT(p, q, *q = map[*q]);
#define IVM_EACH_SLOT(p, q, body) do {               \
    uchar _m = ivm_opinfo(*(p)) & OPI_SLOTS;          \
    (q) = (p);                                        \
    while (_m) {                                      \
      ++(q);                                          \
      if (_m & 1) { body; }                           \
      _m >>= 1;                                       \
    }                                                 \
  } while (0)

#endif // IVM_WALK_C
