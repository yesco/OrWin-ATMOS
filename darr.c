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


typedef struct Darr {
  struct Darr *next;
  struct Darr *super;

  // size_t  max;    // highest index
  uchar z;           // bytes per element
  uchar bn;         // elements that fit in one physical block

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
  Darr *next, *p;
  
  dfree(d->super);

  // free next list
  p= d;
  do {
    next = p->next;
    free(p);
  } while (p = next);
      
  // Free yourself!
  free(d);
}

void* dptr(Darr *d, size_t i) {
  size_t bi, offset;
  Darr *super, **sp;

  if (d == NULL) return NULL;

  offset = i * d->z;
  bi = offset / DARR_BYTES;

  // overflow: use super index
  if (bi >= 8) {
    super = d->super;
    if (!super) super = d->super = darr(sizeof(Darr*));
    // get node recusively, could do by loop, and depth counter
    sp= dptr(super, bi - 8);
    if (sp == NULL) return NULL;
    if (*sp == NULL) *sp= darr(d->z);
    if (*sp == NULL) return NULL;
    d= *sp;
    offset= offset % DARR_BYTES;
  }

  // find local block
  while(bi && --bi && d->next) d= d->next;

  // grow (one step)
  // TODO: assumes linear access to grow...
  if (!bi) d = d->next = darr(d->z);

  return d->bytes + offset;
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
  size_t z= 10;
//  size_t z= 1;
  Darr *d = darr(z);

  if (!d) return 1;

  puts("go");

  // quick check that the same index always returns the same pointer
  for (i = 0; i < 20; i++) {
    uchar *p1 = dptr(d, i);
    uchar *p2 = dptr(d, i);
    uchar *p3 = dptr(d, i+1);

//    assert(p1 == p2);
    
    printf("%5zu: %p\n", dptr(d, i));
    if (p3 - p1 != z) printf("-- new block -- d= %ld\n", (long)(p3-p1));
  }
  puts("ok");


// TODO: verify that we can find exactly same data we put in!


  dfree(d);

  return 0;
}
