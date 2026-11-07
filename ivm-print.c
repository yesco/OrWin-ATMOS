// Shared IVM bytecode pretty-printer + blob dump
//   #include "ivm-print.c" from tpi.c / parse.c
//   or:  cc -DIVM_PRINT_MAIN ivm-print.c -o ivm-print
//        ./ivm-print foo.ivm

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

#ifndef UCHAR
#define UCHAR
typedef unsigned char uchar;
typedef uint16_t       word;
#endif

#ifndef IVM_PRINT_C
#define IVM_PRINT_C

// ---- bytecode pretty-printer (existing) ---- 

static void ivm_print_one(FILE *fp, const uchar *code, unsigned len, unsigned *pi) {
  uchar c;
  unsigned i = *pi;

  if (i >= len) return;

  c = code[i];

  if (c == '"') {
    fputc('\n', fp);
    do {
      fputc((int)c, fp);
      i++;
      if (i >= len) break;
      c = code[i];
    } while (c != 0 && i < len);
    if (i < len && code[i] == 0) {
      fputs("\\0\n", fp);
      i++;
    }
    *pi = i;
    return;
  }

  if (c >= 0xC0) {
    fprintf(fp, "(%02x) ", (unsigned)(c - 0xC0));
    *pi = i + 1;
    return;
  }
  if (c >= 0x80) {
    fprintf(fp, "C%02x ", (unsigned)(c & 0x7f));
    *pi = i + 1;
    return;
  }

  if (c < 32) {
    fprintf(fp, "<%02x> ", (unsigned)c);
    *pi = i + 1;
    return;
  }
  if (c == ' ') {
    fputs("_ ", fp);
    *pi = i + 1;
    return;
  }

  if (c > ' ' && c < 127) {

    // 3 args
    if ((c == 'i' || c == 'j') && i + 3 < len) {
      fprintf(fp, "%c%02x%02x%02x ", c, code[i + 1], code[i + 2], code[i + 3]);
      *pi = i + 4;
      return;
    }

    // 2 args
    if ((c == 'C' || c == 'U' || c == '(' || c == ')') && i + 2 < len) {
      fprintf(fp, "%c%02x%02x ", c, code[i + 1], code[i + 2]);
      *pi = i + 3;
      return;
    }

    // 0 arg
    if (strchr("#$(),.0123456789;?ADEKNPQR~X", c) != NULL) {
      fprintf(fp, "%c ", c);
      *pi = i + 1;
      return;
    }

    // assume 1 arg
    if (i + 1 < len) {
      fprintf(fp, "%c%02x ", c, code[i + 1]);
      *pi = i + 2;
      return;
    }

    // fallback at end
    fprintf(fp, "%c ", c);
    *pi = i + 1;
    return;
  }

  fprintf(fp, "$%02x ", (unsigned)c);
  *pi = i + 1;
}

unsigned ivm_print_code_range(
  FILE *fp, const uchar *code, unsigned start,
  unsigned end, unsigned wrap)
{
  unsigned i = start;
  unsigned n = 0;

  if (!code || end < start) return 0;

  while (i < end) {
    unsigned before = i;
    uchar ch = code[i];

    if (wrap && (strchr("ijnm", ch) != NULL)) {
      fputc('\n', fp);
      n = 0;
    }
      
    ivm_print_one(fp, code, end, &i);
    if (i <= before) i = before + 1;

    if (wrap && ((strchr("ijnmN;XQ", ch) != NULL) || ++n >= wrap)) {
      fputc('\n', fp);
      n = 0;
    }
  }
  return i - start;
}

unsigned ivm_print_code(FILE *fp, const uchar *code, unsigned len) {
  return ivm_print_code_range(fp, code, 0, len, 10);
}

// ---- blob dump ----
//
// Layout (matches ivm.c comments / build_blob):
//   BlobHeader
//   call vectors... 0000
//   const words...
//   fp bytes
//   code
// data
// *
// Slot lines (word-aligned regions):
//   1234 V vector12:   0325  - name
//   1240 C constant03: 002a
//   1242 S string00:   0008  "hi"
//
// Bytecode: 8 bytes/line, global offset + internal offset, hexdump -C style.

typedef struct {
  char magic[4];
  word len;
  word memsz;
  word constidx;
  word main;
  word code;
  word call;
  word fp;
  word con;
  word data;
} IvmBlobHdr;

static word ivm_rd16(const uchar *p) {
  return (word)(p[0] | ((word)p[1] << 8));
}

static void ivm_dump_str_preview(
  FILE *fp, const uchar *blob, unsigned len,
  word data_off, word val)
{
  unsigned pos;
  int n;

  /* val is data-relative offset, or already absolute inside [data,len) */
  if (val >= data_off && val < len)
    pos = val;
  else if ((unsigned)val + data_off < len)
    pos = (unsigned)val + data_off;
  else
    return;

  fputs("  \"", fp);
  for (n = 0; n < 40 && pos < len; n++, pos++) {
    uchar c = blob[pos];
    if (c == 0) break;
    if (c >= 32 && c < 127)
      fputc((int)c, fp);
    else
      fprintf(fp, "\\x%02x", (unsigned)c);
  }
  fputc('"', fp);
  if (pos < len && blob[pos] != 0)
    fputs("...", fp);
}

void ivm_dump_blob(FILE *fp, const uchar *blob, unsigned len) {
  IvmBlobHdr h;
  unsigned off;
  unsigned i, nvec, nconst, code_len;
  word w;

  if (!blob || len < sizeof(IvmBlobHdr)) {
    fprintf(fp, "ivm_dump_blob: too short (%u)\n", len);
    return;
  }

  // header fields are little-endian words in the file
  memcpy(&h, blob, sizeof(h));
  h.len      = ivm_rd16(blob + 4);
  h.memsz    = ivm_rd16(blob + 6);
  h.constidx = ivm_rd16(blob + 8);
  h.main     = ivm_rd16(blob + 10);
  h.code     = ivm_rd16(blob + 12);
  h.call     = ivm_rd16(blob + 14);
  h.fp       = ivm_rd16(blob + 16);
  h.con      = ivm_rd16(blob + 18);
  h.data     = ivm_rd16(blob + 20);

    if (blob[0] != 'I' || blob[1] != 'V' || blob[2] != 'M' || blob[3] != 0) {
    fprintf(fp, "ivm_dump_blob: not an IVM blob (magic %02x %02x %02x %02x",
            blob[0], blob[1], blob[2], blob[3]);
    if (blob[0] >= 32 && blob[0] < 127 && blob[1] >= 32 && blob[1] < 127)
      fprintf(fp, " = '%c%c..'", blob[0], blob[1]);
    fprintf(fp, ").\n");
    fprintf(fp, "  Expected binary .ivm from the compiler (e.g. test-nums.ivm),\n");
    fprintf(fp, "  not a source/script file.\n");
    return;
  }

if (h.len == 0 || h.len > len)
    h.len = (word)len;

  fprintf(fp, "--- IVM BLOB dump (%u bytes) ---\n", (unsigned)h.len);
  fprintf(fp, "%04x  H  magic:      %c%c%c%c\n", 0,
    blob[0], blob[1], blob[2], blob[3] ? blob[3] : ' ');
  fprintf(fp, "%04x  H  len:        %04x  (%u)\n", 4, h.len, h.len);
  fprintf(fp, "%04x  H  memsz:      %04x  (%u cells)\n", 6, h.memsz, h.memsz);
  fprintf(fp, "%04x  H  constidx:   %04x\n", 8, h.constidx);
  fprintf(fp, "%04x  H  main:       %04x\n", 10, h.main);
  fprintf(fp, "%04x  H  code:       %04x\n", 12, h.code);
  fprintf(fp, "%04x  H  call:       %04x\n", 14, h.call);
  fprintf(fp, "%04x  H  fp:         %04x\n", 16, h.fp);
  fprintf(fp, "%04x  H  con:        %04x\n", 18, h.con);
  fprintf(fp, "%04x  H  data:       %04x\n", 20, h.data);

  // ---- call vectors (through h.con; no 0000 terminator) ----
  if (h.call < h.len) {
    unsigned vend = h.con && h.con > h.call ? h.con : h.len;
    fprintf(fp, "-- vectors (then string consts / 0000) --\n");
    off = h.call;
    nvec = 0;
    while (off + 1 < vend) {
      w = ivm_rd16(blob + off);
      fprintf(fp, "%04x  V  vector%02u:   %04x", off, nvec, w);
      if (h.code && w >= h.code)
        fprintf(fp, "  code+%u", (unsigned)(w - h.code));
      fputc('\n', fp);
      nvec++;
      off += 2;
    }
  } else {
    off = sizeof(IvmBlobHdr);
    nvec = 0;
  }

  // ---- constants: [string offs...][0000][ints...] (no reloc list) ----
  if (h.con && h.con < h.len) {
    unsigned con_end = h.fp && h.fp > h.con ? h.fp : (h.code && h.code > h.con ? h.code : h.len);
    unsigned max_words = (con_end > h.con) ? (con_end - h.con) / 2 : 0;
    unsigned seen0 = 0;

    if (h.constidx <= 253)
      nconst = (unsigned)(253 - h.constidx + 1);
    else
      nconst = max_words;
    if (nconst > max_words)
      nconst = max_words;

    fprintf(fp, "-- constants (%u words @ %04x) [strings..|0|ints] --\n", nconst, h.con);
    for (i = 0; i < nconst; i++) {
      unsigned slot = (h.constidx <= 253) ? (unsigned)(h.constidx + i) : i;
      off = h.con + i * 2;
      w = ivm_rd16(blob + off);
      if (!seen0) {
        if (w == 0) {
          seen0 = 1;
          fprintf(fp, "%04x  C  zero:        0000  slot=%02x  (reloc end + real 0)\n",
                  off, slot);
        } else {
          fprintf(fp, "%04x  S  string%02u:   %04x  slot=%02x", off, i, w, slot);
          ivm_dump_str_preview(fp, blob, h.len, h.data, w);
          fputc('\n', fp);
        }
      } else {
        fprintf(fp, "%04x  C  constant%02u: %04x  slot=%02x : %5u  $%04x\n",
                off, i, w, slot, (unsigned)w, (unsigned)w);
      }
    }
  }

  // ---- frame bases (bytes) ---- 
  if (h.fp && h.fp < h.len && h.code > h.fp) {
    unsigned fp_end = h.code;

    fprintf(fp, "-- frame bases --\n");
    for (off = h.fp, i = 0; off < fp_end; off++, i++) {
      fprintf(fp, "%04x  F  frame%02u:    %02x\n", off, i, blob[off]);
    }
  }

  // ---- bytecode ---- 
  if (h.code && h.code < h.len) {
    unsigned code_end = h.data && h.data > h.code ? h.data : h.len;
    code_len = code_end - h.code;

    // pad note: prefer even start
    fprintf(fp, "-- bytecode (%u bytes @ %04x) --\n", code_len, h.code);

    for (off = h.code; off < code_end; ) {
      unsigned line = (unsigned)(off - h.code);
      unsigned nb = code_end - off;
      unsigned k;

      if (nb > 8) nb = 8;

      fprintf(fp, "%04x  B  +%04x  ", off, line);

      for (k = 0; k < 8; k++) {
        if (k < nb)
          fprintf(fp, "%02x ", blob[off + k]);
        else
          fputs("   ", fp);
      }

      fputs(" |", fp);

      for (k = 0; k < nb; k++) {
        uchar c = blob[off + k];

        fputc((c >= 32 && c < 127) ? (int)c : '.', fp);
      }

      fputs("|\n", fp);
      off += nb;
    }
  }

  // ---- data (string bytes) ---- a
  if (h.data && h.data < h.len) {
    unsigned data_len = h.len - h.data;

    fprintf(fp, "-- data (%u bytes @ %04x) --\n", data_len, h.data);

    for (off = h.data; off < h.len; ) {
      unsigned line = (unsigned)(off - h.data);
      unsigned nb = h.len - off;
      unsigned k;

      if (nb > 8) nb = 8;
      fprintf(fp, "%04x  D  +%04x  ", off, line);
      for (k = 0; k < 8; k++) {
        if (k < nb)
          fprintf(fp, "%02x ", blob[off + k]);
        else
          fputs("   ", fp);
      }

      fputs(" |", fp);

      for (k = 0; k < nb; k++) {
        uchar c = blob[off + k];

        fputc((c >= 32 && c < 127) ? (int)c : '.', fp);
      }
      fputs("|\n", fp);
      off += nb;
    }
  }

  fprintf(fp, "--- end blob ---\n");

  fprintf(fp, "\n\n--- BYTECODE (%u bytes) ---\n", code_len);
  ivm_print_code(fp, blob + h.code, code_len);
  fputc('\n', fp);

}

#endif // IVM_PRINT_C

#ifndef MAIN

int main(int argc, char **argv) {
  FILE *f;
  uchar *buf;
  long sz;
  const char *path;

  // - get arg filename
  if (argc < 2) {
    fprintf(stderr, "usage: %s file.ivm\n", argv[0]);
    return 1;
  }

  // - open file
  path = argv[1];
  f = fopen(path, "rb");
  if (!f) {
    // try path.ivm
    char alt[512];

    snprintf(alt, sizeof(alt), "%s.ivm", path);
    f = fopen(alt, "rb");
    if (f)
      path = alt;
    else {
      perror(argv[1]);
      return 1;
    }
  }

  // - get size for malloc
  if (fseek(f, 0, SEEK_END) != 0) {
    perror("seek");
    fclose(f);
    return 1;
  }
  sz = ftell(f);
  if (sz < 0 || sz > 8 * 1024 * 1024) {
    fprintf(stderr, "bad size %ld\n", sz);
    fclose(f);
    return 1;
  }

  // - allocate
  rewind(f);
  buf = (uchar *)malloc((size_t)sz);
  if (!buf) {
    fprintf(stderr, "oom\n");
    fclose(f);
    return 1;
  }

  // - read
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
    perror("read");
    free(buf);
    fclose(f);
    return 1;
  }
  fclose(f);

  ivm_dump_blob(stdout, buf, (unsigned)sz);

  free(buf);
  return 0;
}
#endif // MAIN

