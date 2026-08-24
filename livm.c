// LiVM - A simplistic Limitied Virtual Machine
//
// (C) 2026 Jonas S Karlsson

// A virtual machine with managed storage.
// There can only by 256 values!
// Max 128 different numeric integer,
// And 128 different managed strings.

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>

typedef uint16_t word;

typedef struct ByteStore {
  // probably do striped!
  word  num[64];
  char* str[64];
  char  idx[0];
} ByteStore;

ByteStore bs;

char gcbs() {
  // TODO: implment!
  return 0;
}

void InitBS(char n) {
  bzero(&bs, sizeof(ByteStore) + 2*n);
  bs.str[0]= (char*)&bs.num; // LOL, 2 zeroes!
  bs.str[63]= (char*)-1; // EOS
}


// Numeric values 0-127, -1 are encoded as one byte value
// any others 
char Num(word v) {
  if (v<128 || v==(word)-1) return v;
  else {
    char i= 0;
    while(++i <= 63) {
      //printf("--%d %d %d\n", i, v, bs.num[i]);
      if (bs.num[i]<=128 || bs.num[i]==v) {
	bs.num[i]= v; return i | 128;
      }
    }
    // no free SLOT!
    printf("%% %d\n", v);
    if (!gcbs()) { perror("BS: Nums exhausted"); return 0; }
    return Num(v);
  }
}

#define STRBASE (128+64)

word num(unsigned char i) {
  // If given a str, returns string address!
#ifdef __CC65__
  return i<128? i: bs.num[i^128];
#else
  return i<128? i: i==255? -1: bs.num[i^128];
#endif  
}

char Str(char* s) {
// String values: NULL is encoded a number 0,
// empty string as str[0], other by idx.
  if (!s) return 0;
  if (!*s) return STRBASE;
  else {
    char i= 0;
    while(++i <= 63) {
      //printf("--%d %p >%s<\n", i, bs.str[i], bs.str[i]);
      // TODO: find same string? - nah!
      if (!bs.str[i]) { bs.str[i]= s; return i | STRBASE; }
    }
    // no free SLOT!
    if (!gcbs()) { perror("BS: Strs exhausted"); return 0; }
    return Str(s);
  }
}

char* str(char i) {
  return i<STRBASE? 0: bs.str[i & 63];
}

int main() {
  char i;
  long v, vv;
  char *s, *ss, line[80];

  for(v=-1; v<1024; ++v) {
    i= Num(v); vv= num(i);
    printf("Num(%5lu) => %3d : %5lu -- %s\n", v, i, vv, v==vv? "OK": "FAIL");
  }

  while(fgets(line, sizeof(line), stdin)) {
    i= Str(line); ss= str(i);
    printf("Str(\"%s\") => %3d : \"%s\" -- %s\n", line, i, ss, !strcmp(line,ss)? "OK": "FAIL");
  }
}
