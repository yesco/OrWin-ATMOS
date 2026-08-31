// OAFS - OrWIN ATMOS File System
//
// (C) 2026 Jonas S Karlsson jsk@yesco.org
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

// Varoious encodings:

// working I think

char* LEB128(char* s, long v) {
  while(v >= 128) {
    *s++= (v & 127) | 128;
    v>>= 7;
  }
  *s++= v;
  return s;
}

char* unLEB128(char* s, long *v) {
  char c;
  if ((c= *s) < 128) {
    *v|= c;
    return s+1;
  } else {
    *v|= c & 127;
    *v<<= 7;
    return unLEB128(s+1, v);
  }
}


// reverse LEB128 encoding (big endian)
// working?
char* BEL128(char* s, unsigned long v) {
  if (v >= 128) {
    *s= 128;
    s= BEL128(s, v >> 7);
  }

  *s^= v;
  return s+1;
}

char* unBEL128(char* s, long *v) {
  static char c;
  *v= 0;
  while((c= *s++) >= 128) {
    *v|= c & 127;
    *v<<= 7;
  }
  *v|= c;
  return s;
}



/*

This view maps out exactly how many bytes each short-named method
consumes for specific value ranges, making it easy to spot where your
OAQ architecture outperforms the others.


##  Key Observation

* OAQ vs. The Field (16,384 to 30,719):

  Look at this range. MIDI, LEB128, and standard Prefix Varint spill
  over into a 3-byte penalty here. Your OAQ variant successfully holds
  the line at 2 bytes, maximizing data density for common 16-bit integer
  boundaries without costing you a single bitwise shift on the 6502.

* The Negative Advantage:

  While every other protocol treats small negative numbers like heavy
  32/64-bit integers (forcing maximum byte length inflation to capture
  sign bits), OAQ dumps them natively into a tight, 2-byte escape
  sequence.


---16 BITS "INT"

=============================================================
Integer Range       OAQ   MIDI-VLQ   LEB128   PrefVar   BTC
=============================================================
  -xxx ...   -257    3 !     5         5         5       5
  -256 ...     -1    2 !     5         5         5       5
     0 ...    127    1       1         1         1       1
   128 ...    252    2       2         2         2       1 !
   253 ... 16,383    2       2         2         2       3 -
16,384 ... 30,719    2 !     3         3         3       3
30,720 ... 32,767    3       3         3         3       3



---32 BITS "LONG"

================================================================
Integer Range       OAQ   MIDI-VLQ   LEB128   PrefVar   BTC
================================================================
  -xxx ...   -257e   5       5         5         5       5
  -256 ...     -1   (2) !    5         5         5       5
     0 ...    127    1       1         1         1       1
   128 ...    252    2       2         2         2       1 !
   253 ... 16,383    2       2         2         2       3 -
16,384 ... 30,719    2 !     3         3         3       3
30,720 ... 32,767    3       3         3         3       3
30,720 ... 65,535    3       3         3         3       3
   64K ...     2M    4 -     3         3         3       5 --
    2M ...  16.7M    4       4         4         4       5 -
 16.7M ... 268.4M    5 -     4         4         4       5 -
268.4M ...   4G      5       5         5         5       5 
=================================================================


=======================================================================================
Encoding      Byte  Data  Bit      Raw Value             Target Architecture /
Format        Size  Bits  Waste    Range Covered         Primary Design Goal
=======================================================================================
OAQ           1     7     1 bit    0 to 127              MOS 6502 (Optimized)
              2     15    1 bit    128 to 30,719         Maximizes 2-byte bandwidth 
              2     8     8 bits   -1 to -256 (Signed)   while bypassing all runtime
              4     24    8 bits   30,720 to 16,777,215  bit-shifting routines via 
              5     32    8 bits   Up to 4,294,967,295   compile-time static masking.
---------------------------------------------------------------------------------------
MIDI VLQ      1     7     1 bit    0 to 127              8-bit MIDI Hardware
(Standard)    2     14    2 bits   128 to 16,383         Simple stream framing. Needs 
              3     21    3 bits   16,384 to 2,097,151   expensive loop bit-shifting
              4     28    4 bits   Up to 268,435,455     on every single byte.
---------------------------------------------------------------------------------------
LEB128        1     7     1 bit    0 to 127              Modern DWARF / WASM
(Protobuf)    2     14    2 bits   128 to 16,383         Optimized for Little-Endian 
              3     21    3 bits   16,384 to 2,097,151   32/64-bit CPUs. Penalizes 
              4     28    4 bits   Up to 268,435,455     8-bit chips with loop shifts.
---------------------------------------------------------------------------------------
Prefix Varint 1     7     1 bit    0 to 127              Modern Networking
(Standard)    2     14    2 bits   128 to 16,383         Groups control bits into the 
              3     21    3 bits   16,384 to 2,097,151   first byte, but forces runtime
              4     28    4 bits   Up to 268,435,455     shifts to pack unaligned holes.
---------------------------------------------------------------------------------------
Bitcoin       1     8     0 bits   0 to 252              Blockchain Storage
CompactSize   3     16    8 bits   253 to 65,535         Simple opcode design. Completely
              5     32    8 bits   65,536 to 4,294,967,295 penalizes numbers 128-252 
                                                         by forcing a 3-byte expansion.
=======================================================================================

Key Structural TakeawaysThe 2-Byte Sweet Spot: Both standard MIDI VLQ
and LEB128 choke out at 16,383 inside 2 bytes because they strip a
mandatory control bit out of every single byte. OAQ leaves trailing
bytes completely unaligned and raw, pushing your 2-byte positive
ceiling up to 30,719 (~14.91 bits of usable precision).The Negative
Escape Benefit: Standard formats require massive 5-byte sign-extension
chains to store a simple negative number like -1. OAQ recognizes the
value of immediate native evaluation and drops -1 to -256 down into a
tight, 2-byte payload sequence (0xFF 0xFF for -1).Hardware Execution
cost: While Protobuf (LEB128) and Prefix Varint require a loop tracker
and bit-masking logic to piece unaligned data chunks back together
across bytes, OAQ maps its control markers to instant CPU boundary
flags (BPL, CMP #$F8), letting the 6502 copy trailing data bytes
directly to zero-page memory registers via high-speed, unrolled
indexing.How would you like to build out the next phase of the OAQ
format? We can look into building an automated script to handle bulk
testing matrixes, or write a compiler toolchain script to parse asset
blocks into native OAQ binaries.

## Efficiency

To estimate efficiency on the 6502 versus a modern ARM chip, we must
look at CPU clock cycles and hardware constraints:

* MOS 6502: No barrel shifter.

  Shifting a byte 7 times takes roughly 14–18 cycles inside a
  loop. Bit masking (AND) is cheap (2 cycles).

* ARM: Has a hardware barrel shifter built directly into the ALU.

  An ARM instruction can shift, mask, and add in a single clock cycle.

The tables below map decoding efficiency as Estimated Clock
Cycles. Low numbers are fast; high numbers are slow.


### 🚀 MOS 6502 Decoding Efficiency (Clock Cycles)

Lower cycles = Higher efficiency

==============================================================
Integer Range          OAQ   MIDI-VLQ   LEB128   PrefVar  BTC
==============================================================
0 to 127                 7       13       13      11       11
128 to 252              19       45       48      24       11
253 to 16,383           19       45       48      24       25
16,384 to 30,719        19       70       75      36       25
30,720 to 65,535        27       70       75      36       25
65,536 to 2,097,151     27       70       75      36       40
2,097,152 to 16.7M      27       95      102      48       40
16.7M to 268.4M         35       95      102      48       40
268.4M to 4,294,967,295 35      120      129      60       40
-1 to -256              18      120      129      60       40
==============================================================


### ⚡ ARM Decoding Efficiency (Clock Cycles)

ARM handles barrel shifts in the ALU natively, meaning execution is
bound entirely by branches and memory fetches.

===============================================================
Integer Range          OAQ   MIDI-VLQ   LEB128   PrefVar   BTC
===============================================================
0 to 127                2       2         2         2       2
128 to 252              3       3         3         2       2
253 to 16,383           3       3         3         2       3
16,384 to 30,719        3       4         4         2       3
30,720 to 65,535        4       4         4         2       3
65,536 to 2,097,151     4       4         4         2       4
2,097,152 to 16.7M      4       5         5         2       4
16.7M to 268.4M         5       5         5         2       4
268.4M to 4,294,967,295 5       6         6         2       4
-1 to -256              3       6         6         2       4
==========================================================+++++


## Structural Engineering Analysis

* The 6502 Core Victory (MIDI/LEB128 vs OAQ):

  Standard formats like MIDI-VLQ and LEB128 require loop-driven bit
  shifting over every incoming byte. On large 32-bit integers, this
  skyrockets to over 100 cycles on a 6502. Your OAQ variant caps out
  at a blazing 35 cycles max because it eliminates dynamic loops
  entirely and copies unaligned bytes directly to zero-page registers
  via static indexing.

* The ARM Contrast:

  On ARM, standard PrefixVar is the performance leader. ARM can read
  your prefix, use a single cycle "Count Leading Zeros" (CLZ)
  instruction, and mask/shift the values in a flash.
  
* Why OAQ is a 6502 is an excellent fit:

  OAQ deliberately shifts the encoding optimization away from what
  makes modern 32/64-bit chips happy (tight byte packing) and anchors
  it entirely to 6502 status flag branching (BPL) and immediate memory
  reads.


## LEGEND

Here is the reference legend explaining each short name, how they
function structurally, and where they are used.  ## Method Reference
Legend

* OAQ (Offset-Aligned Quantity)

  A custom, 6502-hardware optimized hybrid encoding. It preserves a
  1-byte fast-path for values 0 to 127. For multi-byte positive
  sequences, it pairs raw trailing bytes with the first byte's
  remaining 7 bits without requiring shifting loops. It also reserves
  unique top-of-byte markers (0xF8 to 0xFE) to indicate bytes coming.

  The prefix byte 0xFF flash-sign-extend short negative ranges like
  -256 to -1 (or unsigned word 0xFFxx-0xFFFF) directly into
  memory. [1, 2]

* MIDI-VLQ (Variable-Length Quantity)

  The traditional, standard big-endian variable-width integer
  scheme. It maps a sequence using the high bit of every single byte
  (bit 7) as a continuation marker (1 = more data coming, 0 = terminal
  byte). While compact, it forces processors to execute dynamic
  runtime bit-shifts to stitch the fragmented 7-bit packets back
  together.

  Read more on the [Wikipedia Variable-Length Integer](https://en.wikipedia.org/wiki/Variable-length_integer) page. [3, 4, 5, 6] 

* LEB128 (Little-Endian Base 128)

  Conceptually identical to MIDI-VLQ, but maps numbers in a
  little-endian layout. It reserves bit 7 of every byte as a
  continuation flag. It serves as the native integer compaction layer
  for modern systems because 32/64-bit ALUs handle its shifts in a
  single clock cycle, though it is highly inefficient on loop-starved
  8-bit chips.

  View documentation via [Wikipedia LEB128 Specification](https://en.wikipedia.org/wiki/LEB128). [3, 4, 5, 6, 7, 8] 

* PrefVar (Prefix Varint)

  A stream optimization that moves all continuation tags into a unary
  pattern inside the first byte (10xxxxxx, 110xxxxx). This allows
  modern CPUs to use specialized instructions (like Count Leading
  Zeros) to find the size instantly. However, it requires significant
  shifting to align the uneven data payloads left in the first byte's
  remaining slots.

  See implementation details on the Google Third Party Libtextclassifier Git Repository. [1, 9, 10, 11, 12] 

* Btc (Bitcoin CompactSize)

  A pure byte-aligned escape-opcode protocol. If the first byte is
  below 253 (0xFD), it is interpreted directly as a 1-byte value. If
  it hits 0xF7 through 0xFF, it acts as an absolute type marker
  telling the deserializer to read the next 2, 4, or 8 trailing bytes
  natively. It eliminates all bit-shifting but heavily penalizes
  values between 128 and 252 by forcing a 3-byte expansion.

  Read the consensus rules at the [Bitcoin Developer Transaction  Reference](https://developer.bitcoin.org/reference/transactions.html). [2,13, 14]


```
[1] [https://news.ycombinator.com](https://news.ycombinator.com/item?id=40426666)
[2] [https://www.buybitcoinsmart.com](https://www.buybitcoinsmart.com/glossary/compactsize)
[3] [https://arxiv.org](https://arxiv.org/html/2403.06898v4)
[4] [https://en.wikipedia.org](https://en.wikipedia.org/wiki/LEB128)
[5] [https://en.wikipedia.org](https://en.wikipedia.org/wiki/Variable-length_integer)
[6] [https://lobste.rs](https://lobste.rs/s/8b5qh4/leb128_vlq_for_variable_length_numbers)
[7] [https://www.dcode.fr](https://www.dcode.fr/leb128-encoding)
[8] [https://hackernoon.com](https://hackernoon.com/encoding-base128-varints-explained-371j3uz8)
[9] [https://news.ycombinator.com](https://news.ycombinator.com/item?id=11263378)
[10] [https://docs.rs](https://docs.rs/prefix_uvarint)
[11] [https://chromium.googlesource.com](https://chromium.googlesource.com/chromiumos/third_party/libtextclassifier/+/adbbad2e0138453af45cc08cb3d04317ae2b8ba1/utils/base/prefixvarint.h)
[12] [https://john-millikin.com](https://john-millikin.com/vu128-efficient-variable-length-integers)
[13] [https://bitcoin.stackexchange.com](https://bitcoin.stackexchange.com/questions/114584/what-is-the-difference-between-the-compactsize-and-varint-encodings)
[14] [https://developer.bitcoin.org](https://developer.bitcoin.org/reference/transactions.html)
```


*/







// Assumption SMALL_ENDIAN 6502/ARM
// TODO: make it independent
union oaq_type {
  // TODO: rename for bits
  unsigned char c;
  uint16_t      w;
  int16_t       i;
  uint32_t      u;
  int32_t       l;

  char          arr[4];
  struct b {
    char first;
    char second;
    char third;
    char fourth;
  } b;
} oaq_val;

// 1 byte  =>  7   bits precision
// 2 bytes => 14.9 bits precision
// 3 bytes => 16   bits precision
//
//-32768 ... -257 : 3 bytes
//
//  -256 ...   -1 : 2 bytes  \
//     0 ...  127 : 1 byte    > OPTIMAL RANGE!
//   128 ...32719 : 2 bytes  /
//
// 32720 ...32768 : 3 bytes
//
//
char* OAQ(char* s, uint16_t w) {
  if (w < 0x80) { *s= w; return s+1; }
  oaq_val.w= w;
  // TODO: or is it < 0xf80 ???
  if (oaq_val.b.second+1 < 0b01111001) {
    s[0]= oaq_val.b.second | 0x80;
    s[1]= oaq_val.b.first;
    return s+2;
  } else {
    s[0]= 0b1111000 | 0x80;
    // LOL, reverse, direct extract
    s[1]= oaq_val.b.first;
    s[2]= oaq_val.b.second;
    return s+3;
  }
}

char* LOAQ(char* s, uint32_t l) {
  if (l < 0xffff) return OAQ(s, l);
  { char i, n= 1;
    oaq_val.l= l;
    // start encoding first non-zero byte
    for(i=3; i--; )
      if ((s[n]= oaq_val.arr[i]) || n > 1) ++n;

    s[0]= 0b11111000 + n - 3; // -3 I think... lol
    return s + n - 1;
  }
}

char* QAO(char* s, uint16_t *w) {
  char c= *s++;
  if (c < 0x80) { *w= c; return s; }
  // hi-bit set
  if (c+1 < 0b11111001) {
    *w= ((~c? c ^ 0x80: c) << 8) | *s++;
    return s;
  } else {
    *w= *(unsigned int*)s; // LOL
    return s+2;
  }
}

char* QAOL(char* s, uint32_t *l) {
  char c= *s++;
  //  if (c < 0x80) { *w= c; return s; }
  // hi-bit set
  l= 0;
  if (c+1 < 0b11111001) return QAO(s, (uint16_t*)l);
  // generic loop
  { char n= 0;
    oaq_val.l= 0;
    c-= 0b1111000 - 2;
    while(c--)
      oaq_val.arr[n++]= *s++;
    return s;
  }
}

// RUNLENGTH encoding of time:
//   0-127: itself
//   hival (preval << 7) | (hival & 0x7f)
// (terminates before next byte < 128)
//
// OR USES LEB129
// minimal bytes keylen prefixlen 0 collen 0 datalen 0

// extracts: a long from a pointer to char* pointer by moving it forward:
//   char* s= ...
//   long val= JSK128(&s);
//   // s is advanced to the next byte to process
//
long unJSK128(char* *s) {
  static char c; static long v;
  if ((c= *(*s)++) < 128) return c;
  v= c ^ 128;
  while((c= *++*s) >= 128) {
    v= (v<<7) + c ^ 128;
  }
  return v;
}

/*
  // AX= ptr to next byte
  unJSK128:
    sta ptr
    stx ptr+1
    ldy #0
    ldX #0

    lda (ptr),y
    inc ptr
    bne :+
    inc ptr+1
  :
    bpl ret

    and #127

  loop:
    


  ret:
    rts
 */


int qputsn(char* s, int len, FILE* f) {
  int n= 0; char c;

  if (!s) return fputs("(NULL)", f);
  n += fputc('"', f);

 next:
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
    if (c<32 || c>126)
      n+= fprintf(f, "\\x%02x", c);
    else
      n+= fputc(c, f);
    if (len>0 && --len) goto next;
  }

  n+= fputc('"', f);
  return n;
}

void nl() { putchar('\n'); }


int main(int argc, char** argv) {
  // test encoding
  #if 0
  // specific number 3,4,5 in 128 lol
  char s[10];
  memset(s, 0x00, sizeof(s));
  long z= 0;
  z<<=7; z+= 3;
  z<<=7; z+= 4;
  z<<=7; z+= 5;
  char* pe= BEL128(s, z);
  long w;
  char* pd= unBEL128(s, &w);
  printf("%8ld: %ld %ld %ld === %s\n", i, pe-s, pd-s, (long)w, w==i? "OK": "---FAIL---");
  printf("\tEN: "); qputsn(s, pe-s, stdout); nl();
  return 0;
  #endif
    
  // 32 BIT
  // for(long i=-512; i<666666; i+= i<1024 || (i>65200 && i<65536+100)? 1: 1024) {

  // 16 BIT
  for(long i=-512; i<32768; i+= (i<1024 || i>=29696)? 1: 1024) {
    char s[10];
    memset(s, 0xff, sizeof(s));
    //char* pe= LEB128(s, i);
    //char* pe= BEL128(s, i);
    char* pe= OAQ(s, i);
    uint32_t l;
    int16_t w;
    //w= 0; // TODO: remove
    //char* pd= unLEB128(s, &w);
    //char* pd= unBEL128(s, &w);
    char* pd= QAO(s, (uint16_t*)&w);
    printf("%9ld: %ld %ld %9ld === %s", i, pe-s, pd-s, (long)w, w==i? "   OK     ": "---FAIL---");
    printf("\tEN: "); qputsn(s, pe-s, stdout); nl();
  }
  return 0;
}
