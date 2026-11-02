// darr.c - a dynamically growing array of whatever
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

// Requirements:
// - storage efficency on 6502
// - fixed size records or bytes
// - unlimited, unexpected growth in available memory
// - minimal default chunk size 64 bytes data + 2 pointers
// - pointer/location of fixed size recrods are permanent
// - limited next chains: 8 (configurable)
// - it's using a logartihmic spill-pointer

// Code size:
// - xxx 3617 bytes ==== too big!!!



// EDITING INSTRUCTIONS:
// - do NOT change indentation, formatting and comments; keep the style
// - only USE // comments
// - do not change lines not directly related to the change at hand
// - do NOT remove TODO and other comments, they are docs!

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#ifndef UCHAR
  #define UCHAR
  typedef unsigned char uchar;
  typedef uint16_t      word;
#endif

#ifdef __CC65__

  // On limited system
  #define DARR_BYTES     64

#else

  // On larger systems
  #define DARR_BYTES    256

#endif

#define DARR_CHAIN  8   // max length of a next-chain


typedef struct Darr {
  struct Darr *next;
  struct Darr *super;

  // size_t  max;    // highest index
  size_t z;          // bytes per element
  size_t bn;         // elements that fit in one physical block

  uchar bytes[];
} Darr;


// TODO: consider a more advanced variant
//   where you can give size hint and MAX_BLCOKS
//   is set to accomondate it wihtout linear search overhead.
//
Darr *darr(size_t z) {
  Darr* d;
  size_t zz = z ? z : 1;
  size_t bn = DARR_BYTES / zz;

  if (bn < 4) bn = 4; // keep blocks from becoming tiny

  d = calloc(1, sizeof(Darr) + bn * zz);
  if (!d) return NULL;

  d->z   = zz;
  d->bn  = bn;

  return d;
}

void dfree(Darr *d) {
  Darr *next;

  if (!d) return;

  dfree(d->super);

  // free next list
  while (d) {
    next = d->next;
    free(d);
    d = next;
  }
}

void* dptr(Darr *d, size_t i) {
  size_t bi, local;
  Darr *super, **sp;

  if (d == NULL) return NULL;

  bi = i / d->bn;           // which logical block
  local = i % d->bn;        // element inside that block

  // overflow: use super index (groups of DARR_CHAIN blocks)
  if (bi >= DARR_CHAIN) {
    size_t g = bi / DARR_CHAIN;     // which group
    size_t b = bi % DARR_CHAIN;     // block inside the group

    super = d->super;
    if (!super) super = d->super = darr(sizeof(Darr*));
    // get node recusively, could do by loop, and depth counter
    sp = dptr(super, g);
    if (sp == NULL) return NULL;
    if (*sp == NULL) *sp = darr(d->z);
    if (*sp == NULL) return NULL;

    // now walk the chosen group’s next-chain
    d = *sp;
    bi = b;
  }

  // find local block inside the (at most DARR_CHAIN) next-chain
  while (bi > 0) {
    if (!d->next) {
      // grow (one step)
      d->next = darr(d->z);
      if (!d->next) return NULL;
    }
    d = d->next;
    bi--;
  }

  return d->bytes + local * d->z;
}

#if 0
void* dpush(Darr* d, void* p) {
  void* ptr= dptr(d, d->max+1);
  
  return ptr? memcpy(ptr, p, d->z): NULL;
}
#endif

// ------------------------------------------------------------------ 
int main(void) {
  size_t i;
//  size_t z= 10;
  size_t z= 1;
  Darr *d = darr(z);

  if (!d) return 1;

  puts("go");

  // quick check that the same index always returns the same pointer
  for (i = 0; i < 2000; i++) {
    uchar *p1 = dptr(d, i);
    uchar *p2 = dptr(d, i);
    uchar *p3 = dptr(d, i+1);

    assert(p1 == p2);
    
    printf("%5zu: %p\n", i, (void *)dptr(d, i));
    if (p3 - p1 != (ptrdiff_t)z) printf("-- new block -- d= %ld\n", (long)(p3-p1));
  }
  puts("ok");


// TODO: verify that we can find exactly same data we put in!


  dfree(d);

  return 0;
}
