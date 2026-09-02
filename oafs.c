// OAFS - Object Ascending File System
//
// (C) 2026 Jonas S Karlsson jsk@yesco.org
//
// A "SmallTable" implementation
//
// Features:
// - "simple"
// - ordered by string keys <= 80 chars
// - page oriented index
// - inline small data (<= 80 chars)
// - file prefix recursive meta forwarding index entries! (=> log n!)
// - delete thombstone allows "versioning"
// - design for idempotency
// - "safe" (maybe optionally log-based)
// - optinally transactional for several entries "appends" (log file at end/beginning)

// It's designed to execute as part of OrWIN ATMOS windowing
// multi-concurrent actor system. OrWIN Actor, lol

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

FILE* aof= 0;

int qputsn(char* s, int len, FILE* f) {
  int n= 0; char c;

  if (!s)  return fputs("(NULL)", f);
  if (!*s) return fputs("\"\"", f);
  
  n += fputs("\'", f);
 next:
  --len;
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
    if (c==0 && len < 0) goto done;
    if (c<32 || c>126)
      n+= fprintf(f, "\\x%02x", c);
    else
      n+= fprintf(f, "%c", c);
    if (len>0 && len) goto next;
  }
 done:
  n+= fputs("'", f);
  return n;
}

void nl() { putchar('\n'); }


#ifdef __ATMOS__

  #TODO: Add ORIC ATMOS disk read asm code

#else

// Simulate the filesystem in a file

// TODO: read "any size"? (smaller bigger)
char* readsector(char* buff, unsigned int n) {
  // we allocate one byte more, lol, to terminate any "strings"
  char* b= buff? buff: calloc(257, 1);
  size_t rd= b? fread(b, 256, 1, aof): 0;
  printf("RD %zu: ", rd); qputsn(b, 256, stdout); nl();
  if (!rd && !buff) { free(b); b= 0; }
  return b;
}

char* writesector(char* buff, unsigned int n) {
  size_t wr= buff? fwrite(buff, 256, 1, aof): 0;
  printf("WR %zu: " , wr); qputsn(buff, 256, stdout); nl();
  return wr? buff: 0;
}

#endif


// ENTRY:
//   <keycollen> <prefixlen+1> <coloffset> keycolrest...
//   <timestamp>
//   <type> <datalen> <data> 0
//
// TODO: <keycollen> ???

// keycollen: is total expanded key column length expanded, incl 0 0
// prefixlen: how many bytes to take from previous keycol
// coloffset: which bytes in expanded is columnoffset
// keycolrest: and add these bytes to it to form the current
// 0: added to make it "printable" (if only text data)
// NOTE: keycol reconstituted is: KEY 0 COL
// NOTE2: COL is always ending with 0:
// COL: 0       : no column name, 0 length
//      -128    : explicit column name string of length n including 0
//      128-255 : column number

// timestamp: monotonically increasing number (32 bits, or runlen encoded)
//
// col(len):
//   - len < 128: is length of column name string (max 64?)
//   - 'len (>128): is column number
// column: len bytes
// 0: terminated with a redundant zero
//
// offset: serialized long
//
// type: 1 byte of type
//   0 - forwarding key search to sector
//   1 - inline data
// 128 - data in sector
//
// (* 42 2 256 19) = 4MB max?
//
// 1) datalen:
// 

/*

Maximum Native Disk Geometry

The most advanced Oric DOS, Sedoric (the standard choice for the
Microdisc and compatible controllers), defines its largest native disk
structure using the following maximum geometric values:

Tracks (Cylinders): Up to 80 tracks (numbered 0 to 79).

Sides (Heads): Up to 2 sides (double-sided).

Sectors per Track: Exactly 17 sectors (under optimal Sedoric MFM formatting, though the standard Microdisc used
16).

Bytes per Sector: Exactly 256 bytes (inherent to the Oric file
system and Western Digital floppy controllers).

Maximum Disk Calculation:

`80 tracks * 2 sides * 17 sectors * 256 = 696,320 bytes (approx. 680 KB unformatted 544 KB formatted data space)

2. Physical Drive

*Compatibility:*

While the original official Oric Microdisc drive system shipped with a
single-sided 3-inch mechanism restricted to 40 tracks, 16 sectors, and
160 KB, the underlying disk code and alternative interfaces (like the
Cumana or Jasmin) natively support 3.5-inch or 5.25-inch
double-density PC floppy mechanisms modified to step to 80 tracks.

Interesting storage systems:

- https://github.com/keirf/flashfloppy

3. Disk Code Limitations (.DSK Header)

The software container blueprint for Oric Atmos disk images dictates
these hard limits. In the standard Oric .DSK file structure, the
geometry data is embedded directly in the 256-byte header:

```
02 00 00 00 => Max 2 Sides
50 00 00 00 => Max 80 Tracks ($50 hex = 80 decimal)
11 00 00 00 => Max 17 Sectors ($11 hex = 17 decimal)


JSK: (* 2 80 17) = 2720 sectors

- read sector in track: 12.5 ms
- wait for sector: 0 - 200ms (one full rotation in worst case)
- head move track 0-39: 39 * 6 + 12 = 246 ms (tracks * 6 + 12 ms)
- switch side: 0 ms!
- switch side SEDORIC: Track Skewing: 12.5 ms!





Gemini generated info:

To understand the physical access times for the Oric Atmos (using the
official Oric Microdisc or compatible floppy systems), we have to look
at the mechanical constraints of the drives from that era and the
Western Digital WD1793 Floppy Disk Controller (FDC). [1] Floppy drives
for the Oric rotate at a constant speed of 300 RPM (5 revolutions per
second). This means a single full rotation takes exactly 200
milliseconds. [2]

------------------------------

## 1. Read 1 Sector on the Current Track: ~12.5 ms to 212.5 ms

When the read head is already on the correct track, the access time is
a combination of Rotational Latency (waiting for the sector to spin
under the head) and Transfer Time (reading the data): [3]

* Best Case (0 ms latency):

  The sector is passing under the head exactly as the read command is
  issued. You only pay the transfer time for 1 sector. Because there
  are 16 sectors per track on a standard Microdisc, reading 1 sector
  takes $200\text{ ms} / 16 = \mathbf{12.5\text{ ms}}$. [4]

* Worst Case (200 ms latency):

  The sector just passed the head. You must wait a full rotation (200
  ms) plus the transfer time (12.5 ms) = 212.5 ms.

* Average Case:

  Half a rotation (100 ms) plus transfer time (12.5 ms) = 112.5 ms.


## 2. Moving One Track Away (1 Track Seek): ~18.5 ms to 218.5 ms

To move the head, the FDC sends a step pulse to the drive's stepper
motor. The WD1793 controller has selectable Step Rates. On the Oric,
the step rate is usually configured to 6 ms per track.

* Mechanical Step Time: 6 ms

* Head Settling Time: Mechanical drives require an extra 12 ms to 15
  ms at the end of a movement for the head to stop vibrating before it
  can read data.

* Total Moving Delay: ~18 ms to 21 ms. [5, 6] 

Once the head settles, you are hit with the same rotational latency as
above.

* Total Time: 18 ms (step/settle) + Rotational Latency (0 to 200 ms) + 12.5 ms (transfer).
* Average Time: ~130.5 ms.

## 3. Moving Two Tracks Away: ~24.5 ms to 224.5 ms

The step rate applies to every track crossed, but the head settling
time only happens once at the very end of the movement. [6]

* Formula: Tracks * 6 ms + 12  ms (settle)
* Calculation: (2 × 6 ms) + 12 ms = 24 ms mechanical delay.
* Average Time: 24 ms} + 100 ms (avg. rotation)} + 12.5 ms (transfer) = 136.5 ms.
* 

## 4. Moving N Tracks Away (The "Seek" Formula)

For any arbitrary track distance N, you can calculate the mechanical
delay using the standard formula: $$\text{Mechanical Delay} = (N
\times 6\text{ ms}) + 12\text{ ms}$$

* Max Seek (Track 0 to 39 on a 40-track drive): (39 × 6) + 12 = 246 ms
  just for the head to move.

* Overall Average Access Time (including rotation/transfer): Add
  roughly 112.5 ms to your mechanical delay.
 

## 5. Switching Sides to Read the Same Track: ~12.5 ms (Instantaneous)

Switching sides is completely electronic, not mechanical. The FDC
simply changes the voltage on the "Side Select" pin of the floppy
drive ribbon cable.

* Head Switch Time: 0 ms (instantaneous).

* The Catch (Sector Interleaving/Skew):

  Because switching sides takes micro seconds, the disk keeps
  spinning. If Side 0 Sector 1 is physically directly "above" Side 1
  Sector 1, by the time the FDC switches sides, Sector 1 will have
  already spun past the head. You would have to wait a painful 187.5
  ms for it to rotate all the way back around. [7]

* To solve this, advanced Oric DOS formats like Sedoric use Track
  Skewing. They offset the sector numbers on the second side (or
  adjacent tracks) by a few sectors so that when a side switch
  happens, the next logical sector is perfectly positioned to be read
  immediately (12.5 ms). [8]



------------------------------
If you are optimizing a loader or writing raw assembly code for the WD1793, I can give you the exact register commands to change the step rates or explain how to structure your file sectors to minimize rotational delay. What are you building?

[1] [https://www.atari-forum.com](https://www.atari-forum.com/viewtopic.php?t=37919&start=75)
[2] [https://www.os2museum.com](http://www.os2museum.com/wp/floppy-capacity-math/)
[3] [https://medium.com](https://medium.com/@sazalkanti/hard-disk-drive-1e6a75d2ffda)
[4] [https://www.computinghistory.org.uk](https://www.computinghistory.org.uk/det/31261/Oric-Microdisc-Drive/)
[5] [https://www.cp.eng.chula.ac.th](https://www.cp.eng.chula.ac.th/~piak/teaching/ca/disk.htm)
[6] [https://en.wikipedia.org](https://en.wikipedia.org/wiki/Hard_disk_drive_performance_characteristics)
[7] [https://stackoverflow.com](https://stackoverflow.com/questions/41767414/how-is-average-seek-time-calculated)
[8] [https://github.com](https://github.com/keirf/flashfloppy/issues/144)


*/



#define MAX_KEYS (256 / 11) // 24


// simplest hack
// TODO: make dynamic/growing?
typedef struct OAFSpage {
  char     n;
  char     maxklen;
  char     totklen;
  char     totdlen;

  // ordered keys
  char     klen[MAX_KEYS];
  char*    keys[MAX_KEYS];

  uint32_t ts  [MAX_KEYS];
  char     type[MAX_KEYS];

  char     dlen[MAX_KEYS];
  char*    data[MAX_KEYS];
} OAFSpage;

// For now only one page, lol
OAFSpage FSpage;


OAFSpage* FSinsert
(char klen, char* key,
 uint32_t ts, char type,
 char dlen, char* data) {

  // TODO: handle overflow
  assert(FSpage.n < MAX_KEYS);
    
  // simple insert sort
  // not even worth doing binary search?
  char i= 0, len, l= klen;
  while(i < FSpage.n) {
    len= FSpage.klen[i];
    if (len < klen) l= len;
    if (memcmp(key, FSpage.keys[i], l) <= 0) break;
    ++i;
  }
  // insert at i location
  // TODO: irritating, use array of struct?
  int z= FSpage.n-i;
  memmove(FSpage.klen + i + 1, FSpage.klen + i, sizeof(FSpage.klen[i])*z);
  memmove(FSpage.keys + i + 1, FSpage.keys + i, sizeof(FSpage.keys[i])*z);
  memmove(FSpage.ts   + i + 1, FSpage.ts   + i, sizeof(FSpage.ts  [i])*z);
  memmove(FSpage.type + i + 1, FSpage.type + i, sizeof(FSpage.type[i])*z);
  memmove(FSpage.dlen + i + 1, FSpage.dlen + i, sizeof(FSpage.dlen[i])*z);
  memmove(FSpage.data + i + 1, FSpage.data + i, sizeof(FSpage.data[i])*z);
  
  FSpage.klen[i]= klen;
  FSpage.keys[i]= key;
  FSpage.ts  [i]= ts;
  FSpage.type[i]= type;
  FSpage.dlen[i]= dlen;
  FSpage.data[i]= data;

  if (FSpage.maxklen < klen) FSpage.maxklen= klen;
  FSpage.totklen+= klen;
  FSpage.totdlen+= dlen;
  ++FSpage.n;

  return &FSpage;
}

void printPage() {
  char i;
  // overestimate; gives some slack!
  int z= FSpage.totdlen + FSpage.totklen + FSpage.n * (256 / MAX_KEYS) + 4;
  printf("==== OAFS PAGE: n: %2d maxklen: %2d totklen: %3d totdlen: %3d est: %3d\n",
	 FSpage.n, FSpage.maxklen, FSpage.totklen, FSpage.totdlen, z);
  for(i=0; i<FSpage.n; ++i) {
    printf("%2d:", FSpage.klen[i]);
    { int l= qputsn(FSpage.keys[i], FSpage.klen[i], stdout); while (l++ < 20) putchar(' '); }
    printf("  %5x %02x  %2d:", FSpage.ts  [i], FSpage.type[i], FSpage.dlen[i] );
    { int l= qputsn(FSpage.data[i], FSpage.dlen[i], stdout); while (l++ < 20) putchar(' '); }
    nl();
  }
}
  
void insertlines(char* name) {
  FILE* f= fopen(name, "r");
  char* s= 0; size_t len= 0;

  assert(f);
  while(getline(&s, &len, f) > 0) {
    char len= strlen(s); uint32_t ts= -1;
    char type= 0;
    FSinsert(len, strdup(s),  ts, type,  len, strdup(s));
  }
  free(s);
}

int main(int argc, char** argv) {
  assert(argc);
  aof= fopen(argv[1], "rw+");
  assert(aof);

  char* xs= readsector(0, 0);

  printPage();
  
  insertlines("numbers.txt");

  printPage();

  fclose(aof);
  return 0;
}
