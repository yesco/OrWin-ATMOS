// ivm-image.c - the host compiler's -o: a program image for a target
// (sim65 .sim, oric .tap), with ivm-lib from ivm-lib-TARGET.bin (./mklib)
//
// The image the 6502 build writes (ivm.c jit_layout, jit_image, jit_save),
// but laid out for the target, not where the compiler happens to run:
//
//   [hdr][ivm-lib: a startup part .. a cut][mem[]][strings][0 gap][JIT code]
//   -z: [hdr][ivm-lib][strings][0 gap][JIT code][mem[] image: zp copy source]
//
// The lib isn't relocatable: its bytes go where mklib linked them, the
// rest right after. Its patched operands and vars are set in a copy of it.
// Included by ivm.c (HOSTSIM, after jit_targets[]).

// the .bin: "IVML" 1 target\0 org.w len.w nsym.w { addr.w name\0 }* bytes
static uchar *hl_file;          // all of it
static word   hl_org, hl_len;   // address of hl_lib[0], its length
static uchar *hl_syms;          // the symbol table in it
static word   hl_nsym;
// hl_lib (ivm-jit.c): its bytes, NULL while running in cpu6502.c

// address of lib symbol name ("_opmul"), 0 if none
static word hl_sym(const char *name) {
  const uchar *p = hl_syms;
  word i;

  for (i = 0; i < hl_nsym; i++) {
    if (!strcmp((const char *)p + 2, name)) return (word)(p[0] | p[1] << 8);
    p += 2 + strlen((const char *)p + 2) + 1;
  }
  printf("%%JIT: ivm-lib has no %s\n", name);
  return 0;
}

#define HL(a) (hl_lib + (word)((a) - hl_org))    // the lib byte at address a
static void hl_b(const char *s, uchar v) { *HL(hl_sym(s)) = v; }
static void hl_w(const char *s, word v) {
  uchar *p = HL(hl_sym(s));
  p[0] = (uchar)v; p[1] = (uchar)(v >> 8);
}

// load ivm-lib-TARGET.bin. 0: none
static int hl_load(const char *target) {
  char f[80];
  FILE *fp;
  long n;
  uchar *p, *end;
  word i;

  sprintf(f, "ivm-lib-%.40s.bin", target);
  if (!(fp = fopen(f, "rb"))) { printf("%%JIT: no %s (./mklib)\n", f); return 0; }
  fseek(fp, 0, SEEK_END);
  n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  free(hl_file);
  hl_file = (uchar *)malloc((size_t)n + 1);
  if (!hl_file || fread(hl_file, 1, (size_t)n, fp) != (size_t)n) n = 0;
  fclose(fp);
  end = hl_file + n;
  if (n < 12 || memcmp(hl_file, "IVML", 4) || hl_file[4] != 1) {
    printf("%%JIT: %s: not an ivm-lib (./mklib)\n", f);
    return 0;
  }
  p = hl_file + 5;
  if (strcmp((const char *)p, target)) {
    printf("%%JIT: %s is for %s\n", f, p);
    return 0;
  }
  p += strlen((const char *)p) + 1;
  hl_org  = (word)(p[0] | p[1] << 8);
  hl_len  = (word)(p[2] | p[3] << 8);
  hl_nsym = (word)(p[4] | p[5] << 8);
  hl_syms = p += 6;
  for (i = 0; i < hl_nsym && p < end; i++) p += 2 + strlen((const char *)p + 2) + 1;
  if (p + hl_len > end) { printf("%%JIT: %s: cut short\n", f); return 0; }
  hl_lib = p;
  return 1;
}

// the cut after lib address top (the highest routine called; floor: the
// startup's own call)
static word hl_cut(word top, word floor) {
  static const char *const cut[] = { "_ivmlib_cut0", "_ivmlib_cut1",
    "_ivmlib_cut2", "_ivmlib_cut3", "_ivmlib_cut4", "_ivmlib_end" };
  uchar k;

  if (top < floor) top = floor;
  for (k = 0; k < 5 && top >= hl_sym(cut[k]); k++) ;
  return hl_sym(cut[k]);
}

// JIT-compile an un-relocated blob (as load_blob reads it on the host) and
// write the image for target to outfile. zp: -z (zp_at 0: top down), org:
// -a (0: right after the strings). 0: failed
int host_image(uchar *blob, const char *target, uchar zp, uchar zp_at,
               word org, const char *outfile) {
  // header: magic[4] len memsz constidx main code call fp con data
  #define HW(i) ((word)(blob[4 + 2 * (i)] | (blob[5 + 2 * (i)] << 8)))
  word len = HW(0), memsz = HW(1), cidx = HW(2), hmain = HW(3);
  word hcode = HW(4), hcall = HW(5), hfp = HW(6), hcon = HW(7), hdata = HW(8);
  #undef HW
  const JitTarget *tg = NULL;
  word datalen = hdata ? (word)(len - hdata) : 0;
  word code_len = (word)((hdata > hcode ? hdata : len) - hcode);
  word vec_bc[64], *map = NULL, *img = NULL, i, w, size = 0;
  word bss_n = 0, bss_img, heapend, floor, libbeg, libend, membytes = 0;
  word imgmem, imgdata, mem_base, at = 0, n, imgbytes, imgend, heap, gap;
  uchar ncall = (uchar)((hcon - hcall) / 2), kkeep = 0, kcut, seen0, zpa = 0;
  uchar *out = NULL, mb[512], h[17], hn = 0;
  FILE *f;
  int ok = 0;

  if (!target) target = "sim6502";
  for (i = 0; i < sizeof(jit_targets) / sizeof(jit_targets[0]); i++)
    if (!strcmp(jit_targets[i].name, target)) tg = &jit_targets[i];
  if (!tg) { printf("%%JIT: unknown target '%s'\n", target); return 0; }
  if (!hl_load(target)) return 0;
  if (ncall > 64) ncall = 64;
  if (memsz > 256) { printf("%%JIT: memsz %u\n", (unsigned)memsz); goto done; }

  // static arrays (bss): reloc words past the blob's end, page aligned at
  // the top, below the target's heapend (zeroed by the startup)
  for (i = 0; cidx + i < memsz; i++) {
    w = (word)(blob[hcon + 2 * i] | (blob[hcon + 2 * i + 1] << 8));
    if (!w) break;
    if (w >= len && (word)(w - len) > bss_n) bss_n = (word)(w - len);
  }
  bss_img = (word)((tg->heapend - bss_n) & 0xFF00);
  heapend = bss_n ? bss_img : tg->heapend;
  floor = bss_n ? hl_sym("_opmemset") : 0;

  for (i = 0; i < ncall; i++)
    vec_bc[i] = (word)((blob[hcall + 2 * i] | (blob[hcall + 2 * i + 1] << 8)) - hcode);
  map = (word *)calloc(code_len ? code_len : 1, sizeof(word));
  out = (uchar *)malloc(8192);
  img = (word *)calloc(memsz ? memsz : 1, sizeof(word));
  if (!map || !out || !img) { printf("%%JIT: out of memory\n"); goto done; }

  // lay out, compile; again if the code calls more of the lib than the
  // image had, or reads a const from mem[] (then they are in it)
  libend = hl_cut(0, floor);
  for (;;) {
    kcut = (uchar)(kkeep || cidx >= memsz ? 0xff : cidx);
    membytes = (word)((kcut == 0xff ? memsz : kcut) * 2);
    imgmem = libend;
    imgdata = zp ? imgmem : (word)(imgmem + membytes);
    if (zp) {
      zpa = zp_at ? zp_at : (uchar)(tg->zptop - membytes);
      if (membytes > tg->zptop || zpa + membytes > tg->zptop ||
          zpa < *HL(hl_sym("_ivmlib_zplo"))) {
        printf("%%JIT: -z: mem[] %u bytes at $%02X does not fit in zp\n",
          (unsigned)membytes, zpa);
        goto done;
      }
      mem_base = zpa;
    } else mem_base = imgmem;
    at = (word)(imgdata + datalen);
    if (org) {
      if (org < at) {
        printf("%%JIT: -a %04X overlaps ivm-lib/mem/strings, min %04X\n",
          (unsigned)org, (unsigned)at);
        goto done;
      }
      at = org;
    }

    // mem[]: vars 0, consts from the pool; string words are blob offsets
    // (before the 0), static arrays past its end -> their addresses
    memset(img, 0, (memsz ? memsz : 1) * sizeof(word));
    for (seen0 = 0, i = 0; cidx + i < memsz; i++) {
      w = (word)(blob[hcon + 2 * i] | (blob[hcon + 2 * i + 1] << 8));
      if (!w) seen0 = 1;
      else if (!seen0)
        w = w >= len ? (word)(bss_img + (w - len)) : (word)(imgdata + (w - hdata));
      img[cidx + i] = w;
    }
    lc_main_bc = (word)(hmain - hcode);
    lc_main_at = 0;
    lc_constidx = (uchar)cidx;
    lc_constmem = img;
    lc_initmem = img;              // _start's global inits fold into it
    lc_kcut = kcut;
    size = ivm_jit(blob + hcode, code_len, out, 8192, map, code_len,
                   mem_base, at, vec_bc, blob + hfp, ncall);
    if (!size) { printf("--- JIT: compilation failed ---\n"); goto done; }
    if (lc_kread && !kkeep) { kkeep = 1; continue; }
    w = hl_cut(lc_libtop, floor);
    if (w <= libend) break;
    libend = w;
  }
  if (lc_main_at) {                       // startup JMP -> main
    out[lc_jmp_at]     = (uchar)(at + lc_main_at);
    out[lc_jmp_at + 1] = (uchar)((at + lc_main_at) >> 8);
  }

#ifdef VERBOSE
  printf("--- DISASM ---\n");
  disasm((char *)out, (char *)(out + size), 2);
#endif
  printf("\n--- JIT: emitted %u bytes (BC: %u) of 6502 at org 0x%04X ---\n\n",
    (unsigned)size, (unsigned)code_len, (unsigned)at);

  // the mem[] image, little endian. -z: up to its last non-0 byte (copied
  // to zp at start, the rest of the live mem[] zeroed there); else all
  for (i = 0; i < membytes / 2; i++) {
    mb[2 * i] = (uchar)img[i];
    mb[2 * i + 1] = (uchar)(img[i] >> 8);
  }
  n = membytes;
  if (zp) while (n && !mb[n - 1]) n--;
  else n = 0;
  imgbytes = zp ? n : membytes;
  imgend = (word)(at + size + n);
  heap = (word)((imgend + 0xFF) & ~0xFF);       // page: no (zp),Y crossing
  if (heap >= heapend)
    printf("%%JIT: warning: no heap left ($%04X >= $%04X)\n", heap, heapend);

  // the startup: from the first part it needs; its operands, the vars
  hl_b("_ivmlib_bssp", (uchar)(bss_n >> 8));
  hl_b("_ivmlib_bssr", (uchar)bss_n);
  hl_w("_opmemset_adr", (word)((bss_img + bss_n) & 0xFF00));   // last page
  hl_b("_ivmlib_zpclen", (uchar)(zp ? membytes : 0));
  hl_b("_ivmlib_zpcn", (uchar)n);
  hl_b("_ivmlib_zplen", (uchar)n);
  hl_w("_ivmlib_zpsrc", (word)(at + size - 1));
  hl_b("_ivmlib_zpdst", (uchar)(zpa - 1));
  hl_b("_ivmlib_zpcdst", (uchar)(zpa - 1));
  hl_w("_ivmlib_entry", at);
  hl_w("_ivmlib_heap", heap);
  hl_w("_ivmlib_heapend", heapend);
  libbeg = hl_sym(bss_n ? "_ivmlib_init" : zp && membytes > n ? "_ivmlib_zpclr" :
                  n ? "_ivmlib_zpinit" : "_ivmlib_start");

  if (tg->hdr == 1) {                   // sim65: "sim65" ver cpu sp load reset
    memcpy(h, "sim65", 5);
    h[5]  = 2;
    h[6]  = 0;                          // 6502
    h[7]  = *HL(hl_sym("_ivmlib_spzp"));
    h[8]  = (uchar)libbeg; h[9]  = (uchar)(libbeg >> 8);   // load
    h[10] = (uchar)libbeg; h[11] = (uchar)(libbeg >> 8);   // reset
    hn = 12;
  } else if (tg->hdr == 2) {            // oric tap, machine code, autorun
    h[0] = h[1] = h[2] = 0x16; h[3] = 0x24;
    h[4] = 0; h[5] = 0;
    h[6] = 0x80;                        // machine code
    h[7] = 0xC7;                        // autorun (at start address)
    h[8]  = (uchar)((imgend - 1) >> 8); h[9]  = (uchar)(imgend - 1); // big endian
    h[10] = (uchar)(libbeg >> 8);       h[11] = (uchar)libbeg;
    h[12] = 0;
    h[13] = 'I'; h[14] = 'V'; h[15] = 'M'; h[16] = 0;
    hn = 17;
  }

  if (!(f = fopen(outfile, "wb"))) { printf("cannot write %s\n", outfile); goto done; }
  gap = (word)(at - (imgdata + datalen));
  fwrite(h, 1, hn, f);
  fwrite(HL(libbeg), 1, (word)(libend - libbeg), f);
  if (!zp) fwrite(mb, 1, membytes, f);
  fwrite(blob + hdata, 1, datalen, f);
  while (gap--) fputc(0, f);
  fwrite(out, 1, size, f);
  if (zp) fwrite(mb, 1, imgbytes, f);
  fclose(f);
  printf("--- JIT: wrote %s (%s) $%04X-$%04X, code $%04X, heap $%04X-$%04X ---\n",
    outfile, tg->name, libbeg, imgend - 1, at, heap, heapend);
  ok = 1;

 done:
  free(map); free(out); free(img);
  hl_lib = NULL;                        // cpu6502.c again (its trap REFs)
  return ok;
}
