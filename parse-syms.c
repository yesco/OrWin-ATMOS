// parse-syms.c - the .sym file of parse.c (-DSYMS): variables, functions,
// call graph, jump patches. #included by parse.c (needs its statics:
// sym_r/sym_w, change_ext, last_func_vec).

static uchar frame_sz[MAX_VECTORS];

// size of frame v: distance to the next higher frame base, else the size
// noted at alloc time (frame_sz), at least the 4 header cells
static uchar frame_size(uchar v) {
  uchar j, ob, sz = 0, fb = frame_base[v];

  for (j = 0; j < nvector; j++) {
    ob = frame_base[j];
    if (ob > fb && (!sz || (uchar)(ob - fb) < sz)) sz = (uchar)(ob - fb);
  }
  if (!sz) sz = frame_sz[v] ? frame_sz[v] : 4;
  return sz;
}

// "c" "w*", an array "c[100]"
static void sym_type_str(Symbol *s, char *buf) {
  buf[0] = (char)(s->vtype ? s->vtype : 'w');
  buf[1] = 0;
  if (s->nelem) sprintf(buf + 1, "[%u]", (unsigned)s->nelem);
  else if (s->is_ptr) { buf[1] = '*'; buf[2] = 0; }
}

// a local / param (gone at its scope's end, kept in sym_dead): slot,
// name, type, its function
static void sym_local(Symbol *s) {
  char tbuf[12];
  uchar v, best = 0xff;
  if (s->type != SYM_VAR) return;
  sym_type_str(s, tbuf);
  fprintf(sym_fp, "  %02x  %c  %-20s  %s  %-5s", s->address,
          s->nelem ? 'A' : 'M', s->name, s->pidx ? "param    " : "local    ",
          tbuf);

  // its function: the frame with the highest base <= its slot
  for (v = 0; v < nvector; v++)
    if (frame_base[v] <= s->address &&
        (best == 0xff || frame_base[v] > frame_base[best])) best = v;
  if (best != 0xff && !s->nelem) fprintf(sym_fp, "  V%02x", best);
  fputc('\n', sym_fp);
}

// stream one patch line when resolved — no arrays
static void sym_patch(char kind, word at, word to, int rel, uchar vec) {
  if (!sym_fp) return;
  fprintf(sym_fp, "  %c   %04x    %04x    %+d", kind, at, to, rel);
  if (vec != 0xff) fprintf(sym_fp, "     V%02x", vec);
  fputc('\n', sym_fp);
}

// open NAME.sym for writing
static void sym_open(const char *path) {
  char out[128];

  // TODO: these rewrites happen in several places
  // A: done: change_ext() is now shared with the .ivm name in main()
  change_ext(out, path, ".sym");

  memset(sym_r, 0, sizeof(sym_r));
  memset(sym_w, 0, sizeof(sym_w));
  memset(frame_sz, 0, sizeof(frame_sz));

  if (sym_fp) fclose(sym_fp);            // (a compile again)
  sym_fp = fopen(out, "w");
  if (sym_fp)
    fprintf(sym_fp, "# %s\n# num  tag  name  scope  type  refs\n", out);
}

static void sym_write_all(void) {
  word i, j, pc;
  uchar v, fb, sz, op;
  char tbuf[12];
  Symbol *s;
  // reuse the static note_* arrays: 512 bytes less on the cc65 C stack
  uchar *r = sym_r, *wcnt = sym_w;
  uchar ccall[MAX_VECTORS];
  word cmask[MAX_VECTORS];

  if (!sym_fp) return;

  memset(sym_r, 0, sizeof(sym_r));
  memset(sym_w, 0, sizeof(sym_w));
  memset(ccall, 0, sizeof(ccall));
  memset(cmask, 0, sizeof(cmask));

  // Final bytecode only — emit-time note_* is inflated by backtracking
  // Parses bytecode to figure out number of read and write references
  // (that can potentiall by replaced by one byte ops)
  for (pc = 0; pc < (word)bin_ptr; ) {

    op = bin[pc];
    j = ivm_oplen(bin + pc);
    pc += j ? j : 1;

    // only count L S !!! (thats the byte savers!) - <32 is a one-byte L
    if (op < 32) {
      if (r[op] < 255) r[op]++;
      continue;
    }
    if (op == 'L' && r[bin[pc - 1]] < 255) r[bin[pc - 1]]++;
    if (op == 'S' && wcnt[bin[pc - 1]] < 255) wcnt[bin[pc - 1]]++;

    if (op >= 0xC0) {
      uchar callee = (uchar)(op - 0xC0);
      uchar caller = 0xff;
      word at = (word)(pc - 1);

      if (callee < MAX_VECTORS && ccall[callee] < 255)
        ccall[callee]++;

      for (i = 0; i < nvector; i++) {
        word a = vectors[i], b = (word)bin_ptr;
        for (j = 0; j < nvector; j++)
          if (vectors[j] > a && vectors[j] < b) b = vectors[j];
        if (at >= a && at < b) { caller = (uchar)i; break; }
      }

      if (caller != 0xff && callee < 16)
        cmask[caller] |= (word)(1u << callee);
    }
  }

  // one line per variable: slot, name, global/local/param, type, how
  // often it's loaded (L) and stored (S) in the final code, and which
  // function's frame it's in.
  for (i = 0; i < nsym; i++) {

    s = &syms[i];
    if (s->type != SYM_VAR) continue;
    sym_type_str(s, tbuf);
    fprintf(sym_fp, "  %02x  %c  %-20s  ", s->address, s->nelem ? 'A' : 'M',
            s->name);
    if (s->pidx) fprintf(sym_fp, "param     ");
    else if (s->scope == 0) fprintf(sym_fp, "global    ");
    else fprintf(sym_fp, "local     ");
    fprintf(sym_fp, "%-5s  L=%u S=%u",
            tbuf, (unsigned)r[s->address], (unsigned)wcnt[s->address]);

    // find the function whose frame holds this variable (a param or
    // local): the frame is [frame_base[v], +frame_size(v)). Prints Vnn.
    for (v = 0; v < nvector; v++) {
      fb = frame_base[v];
      if (s->address >= fb && s->address < (uchar)(fb + frame_size(v))) {
        fprintf(sym_fp, "  V%02x", v);
        break;
      }
    }

    fputc('\n', sym_fp);
  }

  // the locals and params
  for (i = 0; i < nsym_dead; i++) sym_local(&sym_dead[i]);

  // one line per function: entry address, vector, frame base and
  // size, how often it's called, and its parameter list.
  for (i = 0; i < nsym; i++) {
    s = &syms[i];
    if (s->type != SYM_FUNC) continue;
    v = s->address;
    fb = frame_base[v];
    sz = frame_size(v);

    fprintf(sym_fp, "%04x  V%02x  %-20s  fp=%02x  sz=%02x  c=%u",
            (unsigned)vectors[v], v, s->name, fb, sz, (unsigned)ccall[v]);

    // print the parameters in order (pidx 1..n): param j is at base+4+j,
    // the last one at base+3 (params_end). Only while still in scope.
    for (j = 0; j < frame_nparams[v]; j++) {
      word k;
      uchar at = (j + 1 == frame_nparams[v]) ? fb + 3 : fb + 4 + j;

      for (k = 0; k < nsym; k++) {
        if (syms[k].type != SYM_VAR || syms[k].pidx != j + 1 ||
            syms[k].address != at)
          continue;
        sym_type_str(&syms[k], tbuf);
        fprintf(sym_fp, "%s%s %s", j ? "," : "  ", tbuf, syms[k].name);
        break;
      }
    }
    fputc('\n', sym_fp);
  }

  // TODO: document what this loop does
  // A: call graph: for each function, the functions it calls
  for (v = 0; v < nvector; v++) {
    fprintf(sym_fp, "V%02x:", v);

    // TODO: document what this loop does
    // A: bit j of cmask[v] = V(v) calls V(j), from the first loop
    //   (only 16 bits: callees >= 16 aren't shown)
    for (j = 0; j < nvector && j < 16; j++)
      if (cmask[v] & (word)(1u << j))
        fprintf(sym_fp, " V%02x", j);
    fputc('\n', sym_fp);
  }

  fclose(sym_fp);
  sym_fp = 0;
}
