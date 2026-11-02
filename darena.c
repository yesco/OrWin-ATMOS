// darena.c - a dynamically growing arena
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

// Requirements:
// - efficency on 6502
// - every allocation ~80 bytes (or smaller)
// - fixed-size table of starter block pointers
// - unlimited, unexpected growth in available memory
// - next chains
// - fixed blocks[DARENA_BLOCKS] table of starter pointers
// - Each starter heads a singly-linked chain of small DaBlocks
// - div is the number of logical items covered by one top-level entry
// - When the table would overflow we double div and compact the table
//   (the “shrink”)
// - Blocks themselves stay small (DARENA_BLOCK_BYTES payload target)
// - On modern system, just have larger MAX_BLOCKS

// Code size:
// - 3617 bytes ==== too big!!!



// EDITING INSTRUCTIONS:
// - do NOT change indentation, formatting and comments; keep the style
// - only USE // comments
// - do not change lines not directly related to the change at hand
// - do NOT remove TODO and other comments, they are docs!

//#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
//#include <assert.h>

#ifndef UCHAR
  #define UCHAR
  typedef unsigned char uchar;
  typedef uint16_t      word;
#endif

typedef struct DaBlock {
  struct DaBlock *next;
  size_t          offset;   // first logical index this physical block covers
  uchar           bytes[];
} DaBlock;

#ifdef __CC65__

  // On limited system
  #define DARENA_BLOCKS         32
  #define DARENA_BLOCK_BYTES    64

#else

  // On larger systems
  #define DARENA_BLOCKS        256
  #define DARENA_BLOCK_BYTES  1024

#endif


typedef struct Darena {
  size_t max;        // highest index
  size_t z;          // bytes per element
  size_t bn;         // elements that fit in one physical block
  size_t div;        // elements covered by one top-level blocks[] entry
  size_t bz;         // total malloc size of one DaBlock

  DaBlock *current;
  DaBlock *blocks[DARENA_BLOCKS];
} Darena;


// TODO: consider a more advanced variant
//   where you can give size hint and MAX_BLCOKS
//   is set to accomondate it wihtout linear search overhead.
//
Darena *darena(size_t z) {
  size_t zz = z ? z : 1;
  size_t bn = DARENA_BLOCK_BYTES / zz;
  Darena *d = calloc(1, sizeof(Darena));

  if (!d) return NULL;

  if (bn < 4) bn = 4; // keep blocks from becoming tiny

  d->z   = zz;
  d->bn  = bn;
  d->div = bn; // initially one physical block per top-level slot */
  d->bz  = sizeof(DaBlock) + bn * zz;

  return d;
}

void dafree(Darena *d) {
  size_t j;

  for(j = 0; j < DARENA_BLOCKS; j++)
    free(d->blocks[j]);
      
  free(d);
}

// Allocate a fresh physical block that starts at logical index 'start'.
static DaBlock *dablock(Darena *d, size_t start) {
  DaBlock *b = calloc(1, d->bz);

  if (!b) return NULL;

  b->offset = start;
  return b;
}


uchar *daptr(Darena *d, size_t i) {
  size_t bi, local, start;
  DaBlock *b, *prev;

// TODO: a bit messy - diblock(d, i) 
//   would hide this offset
//   couild hide the doubling
//   and searching the blocks

  // same block?
  // z= 1 : (/ 59039922 56089594.0) -  5.3% faster
  // z=10 : (/ 79150591 64422046.0) - 23.9% faster
  // (/ 2000 6) = 333 blocks (/ 333 16) = 20 in list
#if 1
  if (d->current) {
    local = d->current->offset;
    if (local <= i && i < local + d->bn) {
      b= d->current; goto skipit;
    }
  }
#endif

again:
  bi = i / d->div;

  // ---- top-level table full → double coverage and compact ---- 
  if (bi >= DARENA_BLOCKS) {
    size_t j;

    d->div *= 2;

    // keep every even starter; the odd ones are now covered by the
    // doubled span of the preceding entry (they become part of its chain) 
    for (j = 0; j < DARENA_BLOCKS; j += 2) {
      DaBlock *even = d->blocks[j];
      DaBlock *odd  = d->blocks[j+1];

      if (odd) {

        // append the whole odd chain onto the even chain
        if (!even) {

          d->blocks[j] = odd; // nothing there yet

        } else {

          DaBlock *tail = even;
          while (tail->next) tail = tail->next;
          tail->next = odd;

        }
        d->blocks[j+1] = NULL;
      }
    }
    
    // slide the surviving (even) starters down
    for (j = 2; j < DARENA_BLOCKS; j += 2)
      d->blocks[j/2] = d->blocks[j];
    
    memset(d->blocks + DARENA_BLOCKS/2, 0,
      (DARENA_BLOCKS/2) * sizeof(DaBlock *));

    goto again; // recompute bi with the new div
  }

  // ---- obtain / create the starter block for this bi ----
  b = d->blocks[bi];
  if (!b) {

    // lazy allocated first time accessed
    start = bi * d->div;
    b = d->blocks[bi] = dablock(d, start);
    if (!b) return NULL;

  }

 skipit:
  
  // ---- walk the chain until we reach the block that owns 'i' ----
  local = i - b->offset;        // index relative to this starter
  prev  = NULL;

  while (local >= d->bn) {
    prev = b;
    if (!b->next) {

      // append a new physical block
      size_t next_start = b->offset + d->bn;

      b->next = dablock(d, next_start);
      if (!b->next) return NULL;

    }

    b = b->next;
    local = i - b->offset;
  }

  d->current = b;
  if (i > d->max) d->max= i;
  
  return b->bytes + local * d->z;
}

void* dapush(Darena* d, void* p) {
  void* ptr= daptr(d, d->max+1);
  
  return ptr? memcpy(ptr, p, d->z): NULL;
}
  
// ------------------------------------------------------------------ 
int main(void) {
  size_t i;
  size_t z= 10;
//  size_t z= 1;
  Darena *d = darena(z);
  if (!d) return 1;

  // quick check that the same index always returns the same pointer
  for (i = 0; i < 2000; i++) {
    uchar *p1 = daptr(d, i);
    uchar *p2 = daptr(d, i);
    uchar *p3 = daptr(d, i+1);

//    assert(p1 == p2);
    
    //printf("%5zu: %p\n", i, (void *)daptr(d, i));
    //if (p3 - p1 != z) printf("-- new block -- d= %ld\n", p3-p1);
  }
//  puts("ok");



// TODO: verify that we can find exactly same data we put in!


  dafree(d);

  return 0;
}
