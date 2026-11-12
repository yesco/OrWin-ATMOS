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

// Code size on 6502 (compile with minimal on ccc65)
// - darr.c compiled to about 869 bytes.

// A hybrid growable array is implemented that
// guarantees permanent element addresses while
// employing only a single fixed-size block type.
//
// Each block carries a short next-chain whose
// maximum length is a small constant (here 8).
// When a chain would overflow, a recursively
// defined super-directory, itself composed of
// identical blocks, is used. The very first
// block is never relocated or replaced. There
// is no initial super-block or directory
// until it needs to spill.
//
// By bounding the next-chain and making the
// directory recursive and homogeneous, the
// structure obtains logarithmic depth, constant
// block size, and permanent pointers while
// remaining suitable for tightly constrained
// environments. No previously published structure
// appears to cover this exact combination.
//
// The design sits between two classical
// families. An unrolled linked list (chunked
// linked list) also uses uniform blocks and
// preserves addresses, yet random access costs
// linear time in the number of blocks. A
// segment array (segmented list, Xar,
// levelwise-allocated pile) offers O(1)
// indexing and stable pointers, but its
// segments grow exponentially and therefore
// violate the single-block-type constraint.
// Ordinary chunked vectors keep uniform chunks
// yet store their directory in a conventional
// dynamic array that may be reallocated.


// EDITING INSTRUCTIONS:
// - do NOT change indentation, formatting and comments; keep the style
// - only USE // comments
// - do not change lines not directly related to the change at hand
// - do NOT remove TODO and other comments, they are docs!

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// for figuring out compile size
//#define MINIMAL
#ifndef MINIMAL
  #include <stdio.h>
  #include <assert.h>
#endif

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

static void dfree_rec(Darr *d, uchar flags) {
  Darr *next;
  void **vp;
  size_t k;

  if (!d) return;

  if (d->super) dfree_rec(d->super, flags + 1);

  // free next list
  while (d) {

    // free directories upwards, walking down
    if (flags) {
      vp = (void **)d->bytes;
      for (k = 0; k < d->bn; k++) {
        // if at leaf level and flag set free items
        if (flags & 0x7f) dfree_rec((Darr *)vp[k], flags - 1);
        else              free(vp[k]);
      }
    }

    // free this chunk
    next = d->next;
    free(d);
    d = next;
  }
}

void dfree(Darr *d)       { dfree_rec(d, 0); }
void dfree_owned(Darr *d) { dfree_rec(d, 128); }


void* dptr(Darr *d, size_t i) {
  size_t bi, local, g, b;
  Darr *super, **sp;

  if (d == NULL) return NULL;

  bi = i / d->bn;           // which logical block
  local = i % d->bn;        // element inside that block

  // overflow: use super index (groups of DARR_CHAIN blocks)
  if (bi >= DARR_CHAIN) {
    g = bi / DARR_CHAIN;     // which group
    b = bi % DARR_CHAIN;     // block inside the group

    super = d->super;
    if (!super) super = d->super = darr(sizeof(Darr*));
    // get node recusively, could do by loop, and depth counter
    sp = dptr(super, g-1);
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

#ifndef MAIN

int main(void) {
#ifndef MINIMAL
  size_t i;
//  size_t z= 10;
  size_t z= 1;
  Darr *d = darr(z);

  if (!d) return 1;

  #ifndef MINIMIAL
  puts("go");
  #endif
  
  // quick check that the same index always returns the same pointer
  for (i = 0; i < 2000; i++) {
    uchar *p1 = dptr(d, i);
    uchar *p2 = dptr(d, i);
    uchar *p3 = dptr(d, i+1);

    #ifndef MINIMAL
    assert(p1 == p2);
    
#ifdef OSCAR64
    printf("%5u: %04x\n", i, (void *)dptr(d, i));
#else
    printf("%5zu: %p\n", i, (void *)dptr(d, i));
#endif
    if (p3 - p1 != (ptrdiff_t)z) printf("-- new block -- d= %ld\n", (long)(p3-p1));
    #endif
  }
  #ifndef MINIMAL
  puts("ok");
  #endif

// TODO: verify that we can find exactly same data we put in!


  dfree(d);
#endif
  return 0;
}

#endif
