// ivm-btable.c - for run_ivm in ivm.c
// 
// Dispatch by binary search, plain gotos.
//
// #included in run_ivm() with -DUSE_BTABLE.
// 
// Works on any compiler (no labels as values, no switch):
//   77 op ranges, 6-7 compares for any op.
//
// GENERATED from ivm-jtable.c:
// 
//   python3 ivm-tree.py ivm-jtable.c > ivm-btable.c
// 
// (op < 32 is handled before this)

  if (op & 0x80) goto op_hibit;

  if (op < 'M') {
    if (op < ':') {
      if (op < '*') {
        if (op < '%') {
          if (op < '"') {
            if (op < '!') goto op_sp;
            goto op_bang;
          }
          if (op < '#') goto op_str;
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
    if (op < 'C') {
      if (op < '>') {
        if (op < '<') {
          if (op < ';') goto op_unk;
          goto op_semi;
        }
        if (op < '=') goto op_lt;
        goto op_eq;
      }
      if (op < '@') {
        if (op < '?') goto op_gt;
        goto op_not;
      }
      if (op < 'A') goto op_at;
      if (op < 'B') goto op_unk;
      goto op_B;
    }
    if (op < 'H') {
      if (op < 'E') {
        if (op < 'D') goto op_C;
        goto op_unk;
      }
      if (op < 'F') goto op_E;
      if (op < 'G') goto op_F;
      goto op_G;
    }
    if (op < 'J') {
      if (op < 'I') goto op_unk;
      goto op_I;
    }
    if (op < 'K') goto op_J;
    if (op < 'L') goto op_K;
    goto op_L;
  }
  if (op < 'i') {
    if (op < 'V') {
      if (op < 'Q') {
        if (op < 'O') {
          if (op < 'N') goto op_M;
          goto op_N;
        }
        if (op < 'P') goto op_O;
        goto op_P;
      }
      if (op < 'S') {
        if (op < 'R') goto op_Q;
        goto op_unk;
      }
      if (op < 'T') goto op_S;
      if (op < 'U') goto op_unk;
      goto op_U;
    }
    if (op < '[') {
      if (op < 'X') {
        if (op < 'W') goto op_unk;
        goto op_W;
      }
      if (op < 'Y') goto op_X;
      if (op < 'Z') goto op_unk;
      goto op_Z;
    }
    if (op < ']') {
      if (op < 0x5C) goto op_bidxl;
      goto op_unk;
    }
    if (op < '^') goto op_bidxs;
    if (op < '_') goto op_xor;
    goto op_unk;
  }
  if (op < 't') {
    if (op < 'n') {
      if (op < 'k') {
        if (op < 'j') goto op_i;
        goto op_j;
      }
      if (op < 'l') goto op_k;
      if (op < 'm') goto op_l;
      goto op_m;
    }
    if (op < 'q') {
      if (op < 'o') goto op_n;
      goto op_unk;
    }
    if (op < 'r') goto op_q;
    if (op < 's') goto op_r;
    goto op_s;
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
