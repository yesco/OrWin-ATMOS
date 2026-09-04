// OAFS - Object    Atomic      File System
//        Ordered   Amorphic    
//                  Associative
//                  Attribute
//                  Array
//
// (C) 2026 Jonas S Karlsson jsk@yesco.org
//
// A "SmallTable" implementation optimized for 8-bitters
//
// Features:
// - "simple"
// - ordered by multi-compont composite binary keys <= 80 chars
// - page oriented index
// - inline small data (<= 80 chars)
// - file prefix recursive meta forwarding index entries! (=> log n!)
// - delete thombstone allows "versioning"
// - design for idempotency
// - "safe" (optionally log-based)
// - optionally transactional for several entries "appends" (log file at end/beginning)

// It's designed to execute as part of OrWIN ATMOS windowing
// multi-concurrent actor system. OrWIN Actor, lol

// If the key is plain ASCI: and maybe starts with '/'
// it's just a filesystem path. End with <$00> <page:offset> !
//
// TODO: maybe use a different delimiter, and possibly prefix subpath names
// with 0xFF to that they sort later?
//
// Problem:
//
//    /foo/bar/abba.txt
//    /foo/bar/fie/fum.txt
//    /foo/bar/fie/... 1 million files ...
//    /foo/bar/gurka
//
// So a prefix scan on /foo/bar/ would have to scan all sub dirs
// to get to see all the file names.
//
// Solution:
//
//    replace by 0xFF     _ Last / is repolace by 0x00
//      /        /       /
//    <FF> foo <FF> bar <00> abba.txt         FILE
//    <FF> foo <FF> bar <00> fie              DIR <----! NEW!
//    <FF> foo <FF> bar <00> gurka            FILE 
//    <FF> foo <FF> bar <FF> fie <00> fum.txt FILE
//    <FF> foo <FF> bar <FF> fie <00> ... 1 million files ...
//
//
// When it comes to keyvalues being multiattribute, maybe they
// could be prefixed with its inherent encoding:
//
// <tablename>
//     <$80> "bwLws" <byte> <word> <long:Reverse> <word> <string>
//
// L: indicates long reverse (descending) encoding and sort order
// h: could be a pointer to <len><string> (a bin variant of Hollerith)
// a: could be follows by byte indicate number of bytes
//
// no need delimiting items
//
// since [ <tablename> <$80> "bwLws" ] is invariant it'll prefcode
// away mostly in practice!
//
// [tablename] [$80] <table_meta...>               ; Table Info
// [tablename] [$81] "w" [u_i_d + $80]             ; Col 1: Name ends with high-bit set!
// [tablename] [$82] "L" [t_i_m_e_s_t_a_m_p + $80] ; Col 2: Name ends with high-bit set!
// ...
// [tablename] [$BF] "wL" <data_payload_1>         ; First Data Row
// [tablename] [$BF] "wL" <data_payload_2>         ; Second Data Row

/*

For now actors have their own state, support unix pipes. For a train
of processes w pipes, there is only one active LINE being passed
around - a pointer. The receiver owns the heap allocated LINE. If it
needs to pass it on it needs to take it's own copy.

They could be more structured by having an "train" ENV with named
variables set to native #var for number and $var for strings. That is
not yet implemented.

For now, reading a diskblock can return a sequence of linked
diskblocks without knowing any much more of the data (just a first two
byte marker and 4 bytes next sector), the consumer backtracks by
returning null and the trainrunner backs up to the producer. This
would allow ls to parse the blocks of the index:

`oafs-start 'key' | oafs-next-block | oafs-parse | grep fish | more`


 */


// TODO: read:
//
// - https://wiki.defence-force.org/doku.php?id=oric:hardware:dsk_disk_format
//
// - https://osdk.org/index.php?page=documentation&subpage=floppybuilder
//
// RocksDB user defined timestamps:
// - https://github.com/facebook/rocksdb/wiki/User-defined-Timestamp
//
// RocksDB atomic update functions:
// - https://github.com/facebook/rocksdb/wiki/Merge-Operator
// Transactions
// - https://github.com/facebook/rocksdb/wiki/Transactions

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
//#include <strings.h>
#include <string.h>

#include <assert.h>

#define assertptr(p) assert(p!=NULL)


#ifndef WORD

  typedef uint16_t word;

  #define WORD

#endif // WORD


#ifdef OSCAR64

char* strdup(char* s) {
  char* r;
  if (!s) return 0;
  if (!(r= calloc(strlen(s)+1, 1))) return 0;
  return strcpy(r, s);
}

#endif


FILE* oaf= 0;

int fputqsnw(char* s, int len, FILE* f, int width) {
  int n= 0; char c;

  if (!s)  return fputs("(NULL)", f);
  if (!*s) return fputs("\"\"", f);
  
  n += fputs("\"", f);
 next:
  --len;
  if (width > 0 && width-n <= 3) { n+= fprintf(f, "..."); goto spaces; }
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
  n+= fputs("\"", f);

 spaces:
  while (n++ < width) putchar(' ');

  return n;
}

void fputqsn(char* s, int len, FILE* f) {
  fputqsnw(s, len, f, -1);
}

// TODO: remove lol
#define nl() { putchar('\n'); }

#ifdef __ATMOS__

char* readsector(char* buff, word n) {
  return 0;
  
}

char* writesector(char* buff, word n) {
  return 0;
}

//#TODO: Add ORIC ATMOS disk read asm code

#else

#ifndef SEEK_SET
  #define SEEK_SET 1
#endif

int fseek(FILE* f, long pos, int whence) {
  return -1;
}

// Simulate the filesystem in a file

#ifdef __CC65__

char* readsector(char* buff, word n) {
  char* b= buff? buff: (char*)calloc(256, 1);
  char fn[32];
  char x= sprintf(fn, "OAFS/sec%05u", n);
  FILE* f= fopen(fn, "r");
  size_t rd= b? fread(b, 256, 1, oaf): 0;

  printf("RD %zu: ", rd); fputqsn(b, 256, stdout); nl();

  if (!rd && !buff) { free(b); b= 0; }
  fclose(f);
  return b;
}

char* writesector(char* buff, word n) {
  char fn[32];
  char x= sprintf(fn, "OAFS/sec%05u", n);
  FILE* f= fopen(fn, "r");
  size_t wr= buff? fwrite(buff, 256, 1, oaf): 0;

  printf("WR %zu: " , wr); fputqsn(buff, 256, stdout); nl();

  fclose(f);
  return wr? buff: 0;
}

#else

// TODO: read "any size"? (smaller bigger)
char* readsector(char* buff, word n) {
  char* b= buff? buff: (char*)calloc(256, 1);
  int fs= fseek(oaf, 256*n, SEEK_SET);
  size_t rd= b? fread(b, 256, 1, oaf): 0;
  printf("RD %zu: ", rd); fputqsn(b, 256, stdout); nl();
  if (!rd && !buff) { free(b); b= NULL; }
  return b;
}

char* writesector(char* buff, word n) {
  int fs= fseek(oaf, 256*n, SEEK_SET);
  size_t wr= buff? fwrite(buff, 256, 1, oaf): 0;
  printf("WR %zu: " , wr); fputqsn(buff, 256, stdout); nl();
  return wr? buff: 0;
}

#endif // __CC65__

#endif // __ATMOS__




// LOL
#define MAIN

#include "oaq.c"

#undef MAIN



// sectors: (* 2 80 19) = 3040 max?
//  .words: 2433 pages! (3253 if have word's)
//
// prefix compression => 72% (saved 28)

// reserve first 64KB lol (cc65 gives much code)
word next_sector= 256;


// TODO: make it "near"
word FSnewsector(word cur) {
  return next_sector++;
  (void)cur;
}


// TODO: redundant, just do it!
#define DELETED(p,a)   ((p)[a+1])&0x80)

struct OAFSentry {
  char     skipoff;
  char     prefix;
  char     dataoff;

  // decoded
  char     deleted;
  
  uint32_t ts;

  char     klen;
  char*    key;

  word     dlen;
  char*    data;
} entry;


#ifndef __CC65__

  #define bzero(a, z) memset(a, 0, z)

#endif

// Returns: 0 if fail
char parseentry(char* page, char o) { 
  char klen, *p= page+o;

  bzero(&entry, sizeof(entry));

  // END ?
  if (!(klen= entry.skipoff= *p++)) return 0;

  if ((entry.prefix= *p++) & 0x80) {
    entry.prefix&= 0x7f; entry.deleted= 1;
  }
  
  // get key
  if ((entry.dataoff= *p++))
    klen-= entry.dataoff;

  assert(p-page==3);
  entry.klen= (klen-= 3);
  entry.key= p;

  // optionally: get data
  if (entry.dataoff) {
    p= QAOL(p + entry.dataoff, &entry.ts);
    entry.data= p;
  }

  // next entry offset, 0 if last
  return entry.skipoff;
}


char OAFSparsepage(char* page) {
  // 4 byte header
  char i= 4;

  // TODO: 255 if error? lol
  if (page[0] != '$'+128) return 0;
  if (page[1] != 'I'+128) return 0;
  
  next_sector= page[2] + (page[3]<<8);
  
  while(i) {
    char o= i;
    i= parseentry(page, i);
    printf("  %3u o%02x d%u p%3u d%02x  ",
	   o,
	   entry.skipoff, entry.deleted, entry.prefix,
	   entry.dataoff);
    
    printf("  ts%x kL%3u > %s : tL%3u = %s\n",
	   entry.ts, entry.klen, entry.key,
	   entry.dlen, entry.data);
  }

  return 1;
}

// ENTRY DOCUMENATION
// ==================
//
// Minimal key only entry size: 3 bytes!
//   (possible for "" or shorter prefix key)
//
// Minimal key+data entry size: 4 bytes!
//   (timestamp: if not used and is 0x00 - 1 extra byte)
//   (0 bytes data, minimal, but storing it, lol)
//
// Maximal overhead: 3 + bytes for timestamp (1, 2-5)
//
// KeyLength : limited to like 80 chars
// DataLength: inline <= 42 (lol)
// Timestamp : optional (0 growing), always stored if there is data
//
// Generic size formula:nn
//
//    bytes= 3 + keylen [ + timestamplen + datalen]
//
// --- ENTRY LAYOUT
//
// @skipoffset:
//   <skipoffset> <prefixlen> <dataoffset>
//        1 B        1 B          1 B      
//
//(@key:)
//   ...keysuffix
//
// @dataoffset:
//   <timestamp> <data>
//       OAQ      bytes
//
//
// skipoffset: page index location of next record, 0 indicates END
// prefixlen:  0..80 shared bytes w previous key, hibit == DELETED!
// dataoffset: offset (and end) of key data, 0 means no timestamp/data
//
// timestamp: monotonically increasing number (DESCENDING sort order)
//  OAQ: encoding, if just using overwrite/timstamp==0 reversed encodes
//    as one byte 0xff. Other values at least 2 bytes.
//    the timestamp is never reaching 0, so no conflict with delete marker...
//
// LOGICAL SORT ORDER
// ---- KEY [TIMESTAMP DESCENDING optional] !deleted
//
// KEY: binary bytes
//   prefixlen: how many bytes to take from previous entry key
//   keysuffix: and add these bytes to it to form the current
//
//   NOTE: key reconstituted is: ROW (coloffset-skipoffset) COL
//
// DATA: binary bytes, length= skipoffset-dataoffset
//
// (* 42 2 256 19) = 4MB max?
//
// 1) datalen:
// 

// Max buffered writes?
#define MAX_KEYS 255

//#define MAX_KEYS 16

//#define MAX_KEYS (256 / 3) // 85!


// simplest hack
// TODO: make dynamic/growing?

// TODO: this is more like a buffer...
//   maybe used then to update many pages?
typedef struct OAFSpage {
  char     n;
  char     maxklen;
  char     totklen;
  char     totdlen;

  // ordered keys
  char     klen[MAX_KEYS];
  char*    keys[MAX_KEYS];

  uint32_t ts  [MAX_KEYS];

  char     dlen[MAX_KEYS];
  char*    data[MAX_KEYS];
} OAFSpage;

// For now only one page, lol
OAFSpage FSpage = {0};


OAFSpage* FSinsert
(size_t klen, char* key,
 uint32_t ts, char type,
 size_t dlen, char* data)
{
  int z;
  char i= 0, len, l= klen;

  // TODO: handle overflow
  if (FSpage.n >= MAX_KEYS) return 0;
    
  // simple insert sort
  while(i < FSpage.n) {
    len= FSpage.klen[i];
    if (len < klen) l= len;

    // TODO: need to be MORE advanced
    
    if (memcmp(key, FSpage.keys[i], l) <= 0) break;

    // if (FSpage.ts[i]      <=> ...)
    // if (FSpage.deleted[i] <=> ...)

    ++i;
  }
  // insert at i location
  // TODO: irritating, use array of struct?
  z= FSpage.n-i;
  memmove(FSpage.klen + i + 1, FSpage.klen + i, sizeof(FSpage.klen[i])*z);
  memmove(FSpage.keys + i + 1, FSpage.keys + i, sizeof(FSpage.keys[i])*z);
  memmove(FSpage.ts   + i + 1, FSpage.ts   + i, sizeof(FSpage.ts  [i])*z);
  //memmove(FSpage.type + i + 1, FSpage.type + i, sizeof(FSpage.type[i])*z);
  memmove(FSpage.dlen + i + 1, FSpage.dlen + i, sizeof(FSpage.dlen[i])*z);
  memmove(FSpage.data + i + 1, FSpage.data + i, sizeof(FSpage.data[i])*z);
  
  FSpage.klen[i]= klen;
  FSpage.keys[i]= key;
  FSpage.ts  [i]= ts;
  //FSpage.type[i]= type;
  FSpage.dlen[i]= dlen;
  FSpage.data[i]= data;

  if (FSpage.maxklen < klen) FSpage.maxklen= klen;
  FSpage.totklen+= klen;
  FSpage.totdlen+= dlen;
  ++FSpage.n;

  return &FSpage;
  (void)type;
}

// Returns: index of next item to store (if it didn't fit)
//   or 0 if all ok
char packpage(char* page, word next) {
  char towrite_skipoff = 0, towrite_dataoff= 0, dataoff= 0,
    z= 0, n= 0, plen= 0, *pkey= 0;
  word saved= 0;
  char *p, j;
  
  bzero(page, 256);
  
  // header
  page[z++]= '$'+128;
  page[z++]= 'I'+128;
  page[z++]= next;
  page[z++]= next>>8;

  printf("--- Packer\n");
  for(j=next; j<FSpage.n; ++j) {
    char prefix= 0; // This works, but TODO: compress
    char* key= FSpage.keys[j];
    char klen= FSpage.klen[j];
    char need;

    // TODO: LevelDB allows a (single) empty key! 
    if (!key || !klen) { printf("  %%NO KEY: %u %p %u\n", j, key, klen); continue; }

    // TODO: typedatalen and timestamp serializes to how many bytes?
    //  maybe move abort till later?
    need= 3 + 1 + klen + FSpage.dlen[j];
    
    // max of prev and current key len
    if (klen <= plen) plen= klen;
    //    if (pkey) while(prefix < plen && pkey[prefix]==key[prefix]) ++prefix;
    if (pkey) while(prefix < plen && pkey[prefix]==key[prefix]) ++prefix;
    need-= prefix;

    if (256-z < need) { printf(" =NEED : %3u\n", need); break; }

    towrite_skipoff= z;
    page[z++]= 0; // <skipoff>

    // TODO: delete marker
    page[z++]= prefix; // <prefixlen>

    towrite_dataoff= z;
    page[z++]= 0; // dataoff
    
    memcpy(page + z, FSpage.keys[j]+prefix, FSpage.klen[j] - prefix); z+= FSpage.klen[j] - prefix;

    // --- DATAOFF (or keyend)
    // - timestamp
    dataoff= 0;

    if (FSpage.ts[j] || FSpage.dlen[j] || FSpage.data[j]) {
      //printf("DATA!!![ %u %u %p]", FSpage.ts[j], FSpage.dlen[j], FSpage.data[j]);
      dataoff= z;
      p= OAQ(page + z, ~FSpage.ts[j]); // REVERSE ORDER!

      // - typedatalen 0 if deleted ??? TODO:
      //assert(FSpage.dlen[j] < 42); // LOL, unless we have stream-multipages
      //p= OAQ(page + z, FSpage.dlen[j] + 1); // typedatalen TODO: FSpage.type type

      // - acutal data
      memcpy(p, FSpage.data[j], FSpage.dlen[j]); p+= FSpage.dlen[j];
      z= p - page;
    }

    // Update forward pointers
    page[towrite_skipoff]= z;
    page[towrite_dataoff]= dataoff;

    // store previous
    free(pkey);
    plen= klen; pkey= key;

    ++n;
    
    printf("  %3d: %02x-%02x %3d   p%2u %2d ", j,
	   towrite_skipoff, z,z-towrite_skipoff, prefix, klen);
    { char i= prefix; while(i--) putchar('.'); }
    fputqsn(key+prefix, klen-prefix, stdout);
    printf("\t  @%02x DATA[%u]: ", dataoff, FSpage.dlen[j]);
    fputqsn(FSpage.data[j], FSpage.dlen[j], stdout);
    nl();

    saved+= prefix;

    // cleanup
    FSpage.keys[j]= NULL;
    free(FSpage.data[j]); FSpage.data[j]= NULL;
  }

  // END marker (should already be 0!)
  page[++z]= 0;

  // TODO: 2 byte CRC of the page, add 2 bytes at end to make it 0x0000

  // return inext index to process, or 0 if done
  j= j >= FSpage.n? 0: j;

  // TODO: binary, and add to "super index"
  printf(" =USED : %3u\n =SAVED: %3u\n =COUNT: %3u\n =LAST : \"%s\"\n =INEXT: %3u\n\n",
	 z, saved, n, pkey, j);

  free(pkey); if (j) FSpage.keys[j-1]= NULL;

  return j;
}

void printPage() {
  char i;
  // overestimate; gives some slack!
  int z= FSpage.totdlen + FSpage.totklen + FSpage.n * (256 / MAX_KEYS) + 4;
  printf("==== OAFS PAGE: n: %2d maxklen: %2d totklen: %3d totdlen: %3d est: %3d\n",
	 FSpage.n, FSpage.maxklen, FSpage.totklen, FSpage.totdlen, z);
  for(i=0; i<FSpage.n; ++i) {
    printf("%2d:", FSpage.klen[i]);
    fputqsnw(FSpage.keys[i], FSpage.klen[i], stdout, 20);
    //printf("  %5x %02x  %2d:", FSpage.ts  [i], FSpage.type[i], FSpage.dlen[i] );
    printf("  %5x %2d:", FSpage.ts[i], FSpage.dlen[i] );
    fputqsnw(FSpage.data[i], FSpage.dlen[i], stdout, 20);
    nl();
  }
}
  
#if defined(__CC65__) || defined(OSCAR64)

#define PR(...) (void)0

#if 1

int getline(char **s, size_t *z, FILE* f) {
  size_t pos = 0;
  int c;

  PR("GET: 11111\n");
    
  // Enforce explicit initialization guard
  if (!s || !z || !f) return -1;

  PR("GET: 22222\n");
  if (!*s || *z == 0) {
    PR("GET: 3333\n");
    *z = 80;
    *s = (char*)realloc(*s, *z);
    PR("GET: 444\n");
    if (!*s) return -1;
    PR("GET: 555\n");
  }

  while ((c = fgetc(f)) != EOF) {
    PR("GET: 666\n");
    // Grow buffer if we are running out of space (leaving room for \n and \0)
    if (pos >= *z - 2) {
      PR("GET: 777\n");
      *z += 40;
      char* new_s = (char*)realloc(*s, *z);
      PR("GET: 888\n");
      if (!new_s) return -1;
      PR("GET: 999\n");
      *s = new_s;
    }

    PR("GET: aaa\n");
    (*s)[pos++] = (char)c;

    // Break on newline
    if (c == '\n') {
      PR("GET: bbb\n");
      break;
    }
    PR("GET: ccc\n");
  }

  // Handle End of File conditions cleanly
  PR("GET: ddd\n");
  if (pos == 0 && c == EOF) {
    PR("GET: eee\n");
    return -1;
  }

  (*s)[pos] = '\0'; // Null-terminate
  PR("GET: ffff\n");
  return (int)pos;
}

#else

int getline(char **s, size_t *z, FILE* f) {
  char* r;
  size_t len;
  char* rr;

  // Initialize buffer if it's empty
  if (!*s || !*z) {
    printf("get000\n");
    if (!(*s= realloc(*s, *z= 80))) return -1;
    printf("get111\n");
  }

  r= *s;

  static char xyz[128]= "blueberry";
  printf("get11212\n");
//  while ((rr= fgets(r, *z - (r - *s), f))) {
  while ((rr= fgets(xyz, sizeof(xyz), f))) {
    printf("get2222\n");
    len= strlen(r);
    // continues? (no nl)
    printf("get333\n");
    if (len > 0 && r[len - 1] != '\n') {
      printf("get444\n");
      *z+= 40;
      if (!(*s= realloc(*s, *z))) return -1;
      printf("get555\n");
      r= *s + strlen(*s);
    } else {
      printf("get666\n");
      break;
    }
  }
  printf("get777: rr= >%s<\n", rr);

      
  if (r == *s && feof(f) && strlen(*s) == 0) return -1;

  printf("get888\n");
  return strlen(*s);
}

#endif // CC65 || OSCAR64



// Assummes:
//  "KEY DATA....\n"
//
// NOTE: No space in KEY, and no \n in DATA. lol
void insertlines(char* name) {
  FILE* f= strcmp(name, "-")==0? stdin: fopen(name, "r");
  char type= 0, *s= 0;
  size_t z= 0;
  int len;
  char *data, *ks, *ds;
  word ts;

  //assertptr(f);
  //printf("file=%s\n", name);
  //f= strcmp(name, "-")==0? stdin: fopen(name, "r");
  //printf("file=%s p=%x\n", name, f);
  do {
    // TODO: nono on cc65
    //printf("getline>>>\n");
    len= getline(&s, &z, f);
    //printf("<<<getline\n");
    // truncate ending \n
    if (len >=0 && s[len-1]==10) s[--len]= 0;

    data= strchr(s, ' ');
    if (data) *data++= 0;
    
    // data now points to char after ' ' or '\0'

    ts= 0;
    type= 0;

    // TODO: inserting NULL is same a delete? THINK!
    ks= len>=0? strdup(s): NULL;
    ds= len>=0 && data && *data? strdup(data? data: ""): NULL;

    //printf("%3ld:KEY=%s\t%3ld:DATA=%s\n", ks? strlen(ks): 0, ks, ds? strlen(ds): 0, ds);
    printf("%3d:KEY=%s\t%3d:DATA=%s\n", ks? strlen(ks): 0, ks, ds? strlen(ds): 0, ds);

  retry:
    
    if (len < 0 || !FSinsert(strlen(s), ks, ts, type, ds? strlen(ds): 0, ds)) {
      char* page= calloc(256, 1);
      char inext= 0;

      printf("\n%%Overflow - FLUSH buffer\n");

      while((inext= packpage(page, inext)));

      // TODO: instead of looping till none, shift them up, and refill
      
      FSpage.n= 0;

      // TODO: save "page"
      
      free(page);

      if (len < 0) break;

      // TODO: limit?
      // Need to retry
      goto retry;
    }

  } while(1);

  free(s);
  //  if (f != stdin) fclose(f);
}

#ifndef MAIN


#ifdef OSCAR64

int main(void) {
  int argc = 2;
  static const char* argv[] = { "oafs", "FIL.OAFS", NULL }; 

#else

int main(int argc, char** argv) {

#endif

  char *xs;
  
  // also not on sim
  //dio_read(7, 42, argv);
	   
  printf("1111\n");
  assert(argc);
  printf("11212112\n");
#if 0

  oaf= fopen(argv[1], "r");

  printf("22222\n");
  assertptr(oaf);
  printf("333\n");

  xs= readsector(0, 0);
  printf("4444\n");
  
  printPage();
#endif
  printf("555\n");
  
  printf("argv %s\n", argv[2]);
  insertlines(argc>2? (char*)argv[2]: "-");

  printf("666\n");
  
  //printPage();
  
  fclose(oaf);
  return 0;
}

#endif // !MAIN
