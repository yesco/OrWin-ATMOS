// Shared IVM bytecode pretty-printer (tpi.c + parse.c)
#ifndef IVM_PRINT_H
#define IVM_PRINT_H

#include <stdio.h>

#ifndef uchar
typedef unsigned char uchar;
#endif

// Print code[0..len) to fp. wrap every ~10 ops (and after N/;/X/Q).
unsigned ivm_print_code(FILE *fp, const uchar *code, unsigned len);

// Print code[start..end). wrap=0 for one-line GEN slices.
unsigned ivm_print_code_range(FILE *fp, const uchar *code, unsigned start,
                              unsigned end, unsigned wrap);

#endif
