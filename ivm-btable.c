// ivm-btable.c - for run_ivm in ivm.c
// 
// Dispatch by binary search, plain gotos.
//
// #included in run_ivm() with -DUSE_BTABLE.
// 
// Works on any compiler (no labels as values, no switch):
//   80 op ranges, 6-7 compares for any op.
//
// GENERATED from ivm-jtable.c:
// 
//   python3 ivm-tree.py ivm-jtable.c > ivm-btable.c
// 

  if (op & 0x80) goto op_hibit;

  if (op < 'N') {
    if (op < ':') {
      if (op < '*') {
        if (op < '%') {
          if (op < '"') {
            if (op < '!') goto op_sp;
            goto op_bang;
          }
          if (op < '#') goto op_str;
          if (op < '$') goto op_time;
          goto op_unk;
        }
        if (op < '\'') {
          if (op < '&') goto op_mod;
          goto op_and;
        }
        if (op < '(') goto op_ri;
        if (op < ')') goto op_idxl;
        goto op_idxs;
      }
      if (op < '/') {
        if (op < ',') {
          if (op < '+') goto op_mul;
          goto op_add;
        }
        if (op < '-') goto op_wi;
        if (op < '.') goto op_sub;
        goto op_dot;
      }
      if (op < '1') {
        if (op < '0') goto op_div;
        goto op_0;
      }
      if (op < '8') goto op_17;
      if (op < '9') goto op_8;
      goto op_9;
    }
    if (op < 'D') {
      if (op < '?') {
        if (op < '<') {
          if (op < ';') goto op_unk;
          goto op_semi;
        }
        if (op < '=') goto op_lt;
        if (op < '>') goto op_eq;
        goto op_gt;
      }
      if (op < 'A') {
        if (op < '@') goto op_not;
        goto op_at;
      }
      if (op < 'B') goto op_A;
      if (op < 'C') goto op_B;
      goto op_C;
    }
    if (op < 'I') {
      if (op < 'F') {
        if (op < 'E') goto op_unk;
        goto op_E;
      }
      if (op < 'G') goto op_F;
      if (op < 'H') goto op_G;
      goto op_H;
    }
    if (op < 'K') {
      if (op < 'J') goto op_I;
      goto op_J;
    }
    if (op < 'L') goto op_K;
    if (op < 'M') goto op_L;
    goto op_M;
  }
  if (op < 'k') {
    if (op < 'X') {
      if (op < 'S') {
        if (op < 'P') {
          if (op < 'O') goto op_N;
          goto op_O;
        }
        if (op < 'Q') goto op_P;
        if (op < 'R') goto op_Q;
        goto op_unk;
      }
      if (op < 'U') {
        if (op < 'T') goto op_S;
        goto op_unk;
      }
      if (op < 'V') goto op_U;
      if (op < 'W') goto op_unk;
      goto op_W;
    }
    if (op < ']') {
      if (op < 'Z') {
        if (op < 'Y') goto op_X;
        goto op_unk;
      }
      if (op < '[') goto op_Z;
      if (op < 0x5C) goto op_bidxl;
      goto op_unk;
    }
    if (op < '_') {
      if (op < '^') goto op_bidxs;
      goto op_xor;
    }
    if (op < 'i') goto op_unk;
    if (op < 'j') goto op_bi;
    goto op_bj;
  }
  if (op < 'v') {
    if (op < 'q') {
      if (op < 'm') {
        if (op < 'l') goto op_k;
        goto op_l;
      }
      if (op < 'n') goto op_m;
      if (op < 'o') goto op_n;
      goto op_unk;
    }
    if (op < 's') {
      if (op < 'r') goto op_q;
      goto op_r;
    }
    if (op < 't') goto op_s;
    if (op < 'u') goto op_t;
    goto op_u;
  }
  if (op < '{') {
    if (op < 'x') {
      if (op < 'w') goto op_unk;
      goto op_w;
    }
    if (op < 'y') goto op_x;
    if (op < 'z') goto op_y;
    goto op_z;
  }
  if (op < '}') {
    if (op < '|') goto op_shl;
    goto op_or;
  }
  if (op < '~') goto op_shr;
  if (op < 0x7F) goto op_bnot;
  goto op_DEL;
