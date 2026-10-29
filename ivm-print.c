// Shared IVM bytecode pretty-printer
// Single place for L80 vs L <80> — used by tpi dump and parse GEN/ACCEPT.

#include "ivm-print.h"
#include <ctype.h>
#include <string.h>

// Single-byte ops (no following operand byte)
static int is_solo(uchar c) {
  if (islower((int)c)) return 1;
  return strchr("#$(),.0123456789;<=>?ADEKNPQR~X", (int)c) != 0;
}

// Print one instruction at *pi; advances *pi.
static void print_one(FILE *fp, const uchar *code, unsigned len, unsigned *pi) {
  uchar c;
  unsigned i = *pi;

  if (i >= len) return;
  c = code[i];

  // embedded string: "....\0
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

  // high bit: const ref 0x80+n or vector 0xC0+n
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

  // control / non-print
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
    if ((c == 'U' || c == '(' || c == ')') && i + 2 < len) {
      fprintf(fp, "%c%02x%02x ", c, code[i + 1], code[i + 2]);
      *pi = i + 3;
      return;
    }
    if (is_solo(c)) {
      fprintf(fp, "%c ", c);
      *pi = i + 1;
      return;
    }
    if (i + 1 < len) {
      fprintf(fp, "%c%02x ", c, code[i + 1]);
      *pi = i + 2;
      return;
    }
    fprintf(fp, "%c ", c);
    *pi = i + 1;
    return;
  }

  fprintf(fp, "$%02x ", (unsigned)c);
  *pi = i + 1;
}

unsigned ivm_print_code_range(FILE *fp, const uchar *code, unsigned start,
                              unsigned end, unsigned wrap) {
  unsigned i = start;
  unsigned n = 0;

  if (!code || end < start) return 0;

  while (i < end) {
    unsigned before = i;
    uchar ch = code[i];
    print_one(fp, code, end, &i);
    if (i <= before) i = before + 1;
    if (wrap && (ch == 'N' || ch == ';' || ch == 'X' || ch == 'Q' || ++n >= wrap)) {
      fputc('\n', fp);
      n = 0;
    }
  }
  return i - start;
}

unsigned ivm_print_code(FILE *fp, const uchar *code, unsigned len) {
  return ivm_print_code_range(fp, code, 0, len, 10);
}
