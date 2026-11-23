// ivm-jtable.c - run_ivm() dispatch by label table: goto *vt[op]
// #included in run_ivm() with -DUSE_LABELS (gcc, clang; cc65 is unreliable).
// This table is also the source for ivm-btable.c (ivm-tree.py).
  {
    static void *vt[128];
    static uchar inited;
    if (!inited) {
      // TODO: just one big static array assign? (or doesn't work cc65?)
      // build once in a local table, then memcpy → static (no per-op ifdefs)
      void *t[128];
      uchar k;

      for (k = 0; k < 128; k++) t[k] = &&op_unk;

      t['q'] = &&op_q;   t['Q'] = &&op_Q;   t['X'] = &&op_X;   t[';'] = &&op_semi;

      t['8'] = &&op_8;
      t['1'] = &&op_17;   t['2'] = &&op_17;  t['3'] = &&op_17;
      t['4'] = &&op_17;   t['5'] = &&op_17;  t['6'] = &&op_17; t['7'] = &&op_17;

      t['+'] = &&op_add;  t['-'] = &&op_sub; t['*'] = &&op_mul;
      t['/'] = &&op_div;  t['%'] = &&op_mod;
      t['&'] = &&op_and;  t['|'] = &&op_or;  t['^'] = &&op_xor;
      t['{'] = &&op_shl;  t['}'] = &&op_shr;
//      t['{'] = &&op_shlm; t['}'] = &&op_shrm;

      t['L'] = &&op_L;    t['S'] = &&op_S;   t['Z'] = &&op_Z;
      t['C'] = &&op_C;
      t['l'] = &&op_l;    t['s'] = &&op_s;

      t['I'] = &&op_I;    t['J'] = &&op_J;   t['U'] = &&op_U;
      t['i'] = &&op_bi;   t['j'] = &&op_bj;  t['H'] = &&op_H;
      t['A'] = &&op_A;

      t['M'] = &&op_M;    t['F'] = &&op_F;
      t['m'] = &&op_m;
      t['@'] = &&op_at;   t['!'] = &&op_bang;
      t['r'] = &&op_r;    t['w'] = &&op_w;
      t['\'']= &&op_ri;   t[','] = &&op_wi;
      t['('] = &&op_idxl; t[')'] = &&op_idxs;
      t['['] = &&op_bidxl;t[']'] = &&op_bidxs;
      t['a'] = &&op_sidxl;t['b'] = &&op_sidxs;

      t['t'] = &&op_t;    t['n'] = &&op_n;
      t['u'] = &&op_u;    t['k'] = &&op_k;

      t['<'] = &&op_lt;   t['='] = &&op_eq;  t['>'] = &&op_gt;
      t['?'] = &&op_not;  t['~'] = &&op_bnot;

      t[' '] = &&op_sp;    t[127] = &&op_DEL; // DEL: NOP too

      t['0'] = &&op_0;    t['9'] = &&op_9;

      t['K'] = &&op_K;    t['E'] = &&op_E;   t['N'] = &&op_N;
      t['P'] = &&op_P;    t['O'] = &&op_O;   t['W'] = &&op_W;
      t['.'] = &&op_dot;  t['"'] = &&op_str;   t['#'] = &&op_time;

      t['G'] = &&op_G;    t['B'] = &&op_B;

      t['x'] = &&op_x;    t['y'] = &&op_y;   t['z'] = &&op_z;

      memcpy(vt, t, sizeof(vt));
      inited = 1;
    }

    if (op & 0x80) goto op_hibit;
    goto *vt[op];
  }
