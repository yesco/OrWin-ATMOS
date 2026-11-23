// ivm-btable.c - for run_ivm in ivm.c
// 
// Dispatch by binary search, plain gotos.
//
// #included in run_ivm() with -DUSE_BTABLE.
// 
// Works on any compiler (no labels as values, no switch):
//   83 op ranges, 6-7 compares for any op.
//
// GENERATED from ivm-jtable.c:
// 
//   python3 ivm-tree.py ivm-jtable.c > ivm-btable.c
// 

  if (op & 0x80) goto op_hibit;

  if (op < 'O') {
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
    if (op < 'L') {
      if (op < 'J') goto op_I;
      if (op < 'K') goto op_J;
      goto op_K;
    }
    if (op < 'M') goto op_L;
    if (op < 'N') goto op_M;
    goto op_N;
  }
  if (op < 'j') {
    if (op < 'Y') {
      if (op < 'T') {
        if (op < 'Q') {
          if (op < 'P') goto op_O;
          goto op_P;
        }
        if (op < 'R') goto op_Q;
        if (op < 'S') goto op_unk;
        goto op_S;
      }
      if (op < 'V') {
        if (op < 'U') goto op_unk;
        goto op_U;
      }
      if (op < 'W') goto op_unk;
      if (op < 'X') goto op_W;
      goto op_X;
    }
    if (op < '^') {
      if (op < '[') {
        if (op < 'Z') goto op_unk;
        goto op_Z;
      }
      if (op < 0x5C) goto op_bidxl;
      if (op < ']') goto op_unk;
      goto op_bidxs;
    }
    if (op < 'b') {
      if (op < '_') goto op_xor;
      if (op < 'a') goto op_unk;
      goto op_sidxl;
    }
    if (op < 'c') goto op_sidxs;
    if (op < 'i') goto op_unk;
    goto op_bi;
  }
  if (op < 'u') {
    if (op < 'o') {
      if (op < 'l') {
        if (op < 'k') goto op_bj;
        goto op_k;
      }
      if (op < 'm') goto op_l;
      if (op < 'n') goto op_m;
      goto op_n;
    }
    if (op < 'r') {
      if (op < 'q') goto op_unk;
      goto op_q;
    }
    if (op < 's') goto op_r;
    if (op < 't') goto op_s;
    goto op_t;
  }
  if (op < 'z') {
    if (op < 'w') {
      if (op < 'v') goto op_u;
      goto op_unk;
    }
    if (op < 'x') goto op_w;
    if (op < 'y') goto op_x;
    goto op_y;
  }
  if (op < '}') {
    if (op < '{') goto op_z;
    if (op < '|') goto op_shl;
    goto op_or;
  }
  if (op < '~') goto op_shr;
  if (op < 0x7F) goto op_bnot;
  goto op_DEL;
