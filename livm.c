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
#include <errno.h>
#include <assert.h>

typedef uint16_t word;

typedef union ByteProgram {
  char bytes[];
  word words[];
} ByteProgram;
  
// 64: sh6lor - prefix loader
// 32: read global
//  8: read local
//  8: write local
//  8: read local--
//  8: EXTRA!

// 64: jmp +/- 32
// 32: jsr[]
// 32: instructions

#ifdef LIVM

void livm(ByteProgram* bp) {
  static char c, f, i, n, *fp, *sp, pre, t;
  word tos, tmp;

  // s points to index of current tos
  #define PUSH (t= bs->idx[--s]= Num(tos), tos)
  // TODO: zero out popped value?
  #define POP (tos= num(t= bs->idx[++s]))

  // TODO: these are in ByteStorage
  i= bs->i-1; n= 0; fp= bp->bytes + bp->words[f= bs->f];

  // TODO: and for string? (t is the BYTE)
 pop:
  POP;

 next:
  pre= 0;

 nextlit:

  // divide into 32 instr groups
  switch((c= fp[++i])>>6) {

  case 0: case 1: // SH6LOR
    if (!pre) { PUSH; tos= 0; }
    tos= (tos<<6) | c; ++pre;
    goto nextlit;

  case 2: // read global
    PUSH; tos= bp->idx[c & 31];
    break;

  case 3: // local
    switch((c>>3) & 3) {
    case 0: PUSH; tos= num(t= bp->idx[bp->iframe + c&7]); goto next; // RLOCAL
    case 1: t= bp->idx[bp->iframe + c&7]= Num(tosO); goto pop; // WLOCAL

      // TODO: redundant and expensive in code
    case 2: PUSH; tos= num(t= bp->idx[bp->iframe] + c&7)+1; goto next;

    // String operations
    case 3: {
      switch(c & 7) {
      case 0: break; // *++ (nextP) (STRING -> TRING S)
      case 1: break; // STRLEN
      case 2: break; // STRCHR
      case 3: break; // RINDEX
      case 4: break; // STRSTR / SEARCH
      case 5: break; // CONCAT (how many)
      case 6: break; // /STTRING
      case 7: break; // COMPARE <=>
	// ~TRAILING
	// SUBSTITUTE
	// REPLACES
	// UNESCAPE ESCAPE

	// -- IMMUTABLE STRINGS
	// ATOI ITOA STR= STR<
	// S-LEN CONCAT
	// S-SUB (str offset u - substr)
	// S-TRIM
	// S-SPLIT (str count - str ... strn) ??
	// S-FREE ref count free?
      }
      // cleanup
      goto next;
    }

  case 4: case 5: // JMP
    i+= c - 32; goto next;

  case 6: // JSR[]
    RPUSH(f); RPUSH(i); RPUSH(bp->iframe); PUSH;
    f= c & 31; i= 255; bp->iframe= s;
    goto next;

  case 7: // instruction
    switch(c & 31) {
    case 0:  tos^= 0x8000; break;        // SIGN

    #ifdef FORTH
    // TODO: FORTH - only needed for forth... not codegen
    // (how many bytes)
    case 1:  PUSH; break;                // DUP
    case 2:  POP; break;                 // DROP
    case 3:  { word x;
	tmp= tos; POP; x= tos;
	tos= tmp; PUSH; x= tos; break; } // SWAP
    case 4:  break; // OVER
    case 5:  break; // NIP
    case 6:  break; // TUCK
    case 7:  break; // PICK

    case 8:  break; // R>
    case 9:  break; // >R
    case 10: break; // RDROP

    #endif // FORTH

    case 11: break; // STR>NUM atoi
      // NUM>STR: itoa
    case 12: break; // WORD from input (delim -> word)
    case 13: break; // ACCEPT (addr u1 - u2)
    case 14: break; // KEY!!!
      
    case 15: // RETURN/EXIT
      bp->iframe= RPOP(); i= RPOP(); f= RPOP();
      // TODO: expensive, lol, maybe add offset when loading!
      fp= bp->bytes + bp->words[f= bs->f];
      goto POP;

    // Arith
    // TODO: string safe
    case 16: tmp= tos; POP; tos+= tmp; goto next; // ADD
    case 17: tmp= tos; POP; tos-= tmp; goto next; // SUB
    case 18: tmp= tos; POP; tos*= tmp; goto next; // MUL
    case 19: tmp= tos; POP; tos/= tmp; goto next; // DIV
    case 20: tmp= tos; POP; tos&= tmp; goto next; // AND
    case 21: tmp= tos; POP; tos|= tmp; goto next; // OR
    case 22: tmp= tos; POP; tos^= tmp; goto next; // XOR

    case 23:                tos<<= 1;  goto next; // SHL1
    case 24:                tos>>= 1;  goto next; // SHR1
    case 25:                tos= ~tos  goto next; // NEGATE
    case 26:                tos= -tos  goto next; // MINUS
      
    // PRINT: EMIT & TYPE!
    case 27: if (t<STRINGPOS) putchar(tos); else putz(tos); goto pop;
    case 28: wputi(t);      goto pop;  // . = PRINTNUM
    case 29: wputc(' ');    goto next; // SPC
    case 30: nl();          goto next; // NL
      
    case 31: PUSH; tos= -1; goto next; // TRUE
    }

  }

  goto next;

 end:
  // put back tos
  PUSH;
}
  
#endif // LIVM
  

typedef struct ByteStore {
  // probably do striped!
  word  num[64];
  char* str[64];
  char  nix;
  char  idx[0];
} ByteStore;


// TODO: 6502 should be set/allocate to full page
ByteStore *bs;

ByteStore* NewBS(char n) {
  ByteStore* bs= calloc(sizeof(ByteStore) + n*sizeof(word), 1);
  if (!bs) return bs;
  bs->str[0]= (char*)&bs->num; // LOL, 2 zeroes!
  bs->str[63]= (char*)-1; // EOS
  return bs;
}


char gcbs() {
  // TODO: implment!
  return 0;
}


// Numeric values 0-127, -1 are encoded as one byte value
// any others 
char Num(word v) {
  if (v<128 || v==(word)-1) return v;
  else {
    char i= 0;
    while(++i <= 63) {
      if (bs->num[i]<=128 || bs->num[i]==v) {
	bs->num[i]= v; return i | 128;
      }
    }
    // no free SLOT!
    errno= EXFULL;
    if (!gcbs()) { perror("BS: Nums exhausted"); return 0; }
    return Num(v);
  }
}

#define STRBASE (128+64)

word num(unsigned char i) {
  // If given a str, returns string address!
#ifdef __CC65__
  return i<128? i: bs->num[i^128];
#else
  return i<128? i: i==255? -1: bs->num[i^128];
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
      // TODO: find same string? - nah!
      if (!bs->str[i]) { bs->str[i]= s; return i | STRBASE; }
    }
    // no free SLOT!
    errno= EXFULL;
    if (!gcbs()) { perror("BS: Strs exhausted"); return 0; }
    return Str(s);
  }
}

char* str(char i) {
  return i<STRBASE? 0: bs->str[i & 63];
}




int main() {
  char i;
  long v, vv;
  char *s, *ss, line[80];

  bs= NewBS(64);
  
  for(v=-1; v<1024; ++v) {
    i= Num(v); vv= num(i);
    printf("Num(%5lu) => %3d : %5lu -- %s\n",
	   v, i, vv, v==vv? "OK": "FAIL");
  }

  do {
    // last line will get null
    s= fgets(line, sizeof(line), stdin);
#if 0
    printf("%s", s);
#else
    i= Str(s); ss= str(i);
    printf("Str(\"%s\") => %3d : \"%s\" -- %s\n",
	   s?s:"(NULL)", i, ss?ss:"(NULL)", (s && ss && !strcmp(s,ss)) || s==ss? "OK": "FAIL");
#endif
  } while(s);
}
