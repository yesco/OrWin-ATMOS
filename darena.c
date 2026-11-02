// darena.c - a dynamically growing arena
//
// (C) 2026 Jonas S Karlsson (jsk@yesco.org)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <assert.h>

#ifndef UCHAR
  #define UCHAR
  typedef unsigned char uchar;
  typedef uint16_t      word;
#endif

typedef struct DaBlock {
  struct DaBlock *next;
  size_t offset;
  uchar bytes[];
} DaBlock;

#define DARENA_BLOCKS 32
#define DARENA_BLOCK_BYTES 64
  
typedef struct Darena {
  size_t n;
  size_t z;
  size_t az;
  size_t bz;
  size_t bn;
  size_t div;

  DaBlock* current;
  DaBlock* end;

  DaBlock* blocks[DARENA_BLOCKS];
} Darena;

Darena* darena(size_t z) {
  size_t zz= z? z: 1;
  size_t bn= DARENA_BLOCK_BYTES/(zz);
  Darena* d= calloc(1, sizeof(Darena));

  if (!d) return NULL;

  d->z= zz;
  // TODO: mimimize waste
  if (bn<=1) bn= 4; // we be damned
  d->div= d->bn= bn;
  d->az= sizeof(DaBlock) + bn * (z? z: 1);

  return d;
}

uchar* dptr(Darena* d, size_t i) {
  uchar j;
  size_t bi, offset;
  DaBlock* b;

 again:

  bi= i / d->div;
  if (bi < d->bn) b= (bi < d->bn)?( d->blocks[bi]): NULL;
  else {
    // shrink
    d->div*= 2;
    for(j= 2; j < DARENA_BLOCKS; j+= 2)
      d->blocks[j/2]= d->blocks[j];
    memset(d->blocks + DARENA_BLOCKS/2, 0, DARENA_BLOCKS * sizeof(void*));

    assert(!"Not ready yet");
    goto again;
  }  

  if (!b) {
    b= d->blocks[bi]= calloc(d->bz, 1);
    b->offset= bi * d->div;
  }

  offset= (i % d->div) * d->z;
  // TODO: follow b->next links till right block

  d->current= b;
  return (uchar*)((uchar*)&(b->bytes) + offset);
}
  
int main(void) {
  uchar i;
  Darena* d= darena(10);
  
  for(i=0; i<100; i++)
    printf("%5d: %p\n", i, dptr(d, i));

  return 0;
}
