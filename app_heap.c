#include <stdio.h>
#include <stdlib.h>

//#include <_heap.h> // halluciatino

#include "orwin.h"

typedef struct usedblock {
  unsigned int size;
  char*        start;
} used;

void* __fastcall__ malloc (size_t size);
void* __fastcall__ calloc (size_t count, size_t size);
void* __fastcall__ realloc (void* block, size_t size);
void __fastcall__ free (void* block);

int __fastcall__ posix_memalign (void** memptr, size_t alignment, size_t size);
void __fastcall__ __heapadd (void* mem, size_t size);

size_t _heapmemavail(void);
size_t _heapmaxavail(void);

extern void* _heaporg;
extern void* _heapptr;
extern void* _heapend;

extern struct freelist* _heapfirst;
extern struct freelist* _heaplast;

typedef struct freelist {
  unsigned int     size;
  struct freelist* next;
  struct freelist* prev;
} freelist;

typedef struct APP {
  char c;
} APP;

void* last_heaporg;
void* last_heapptr;
void* last_heapend;

struct freelist* last_heapfirst;
struct freelist* last_heaplast;


int isfree(void* a) {
  struct freelist* f = _heapfirst;
  while (f) {
    if (f == a) return 1;
    f = f->next; // could loop forever"
  }
  return 0;
}

void dumpheap(char all) {
  char* m = (char*)_heaporg;
  struct freelist* p = (struct freelist*)_heapfirst;
  unsigned int n = 0, f = 0, sz;

  if (all < 2) {
    while (p) {
      ++f;
      printf("%04X:%u ", (unsigned int)p, p->size);
      if (p->size > 2048) { printf("Unprobable size..."); break; }
      p = p->next;
      if (p == (struct freelist*)_heapfirst) {
	printf("LOOP: %04X...", (unsigned int)p); break; }
    }
    printf("\n#free %u\n", f);
  }

  if (!all) return;
  
  f= 0;
  while (m < _heapptr) {
    sz = *(unsigned int*)m;
    if (sz == 0 || sz > 4096) break;
    if (isfree(m)) {
      printf("%u ", sz); ++f;
    } else {
      printf("[%u] ", sz); ++n;
    }
    m += sz;
  }
  printf("\n#free %u allocs: %u   ", f, n);
}

void* app_heap(void* voidapp, char* line) {
  if (!app) {
    window(3, 17, 32, 10, yellow, black);
    wstatus(-1, "Heap Viewer");
    return calloc(sizeof(APP), 1);
  } else if (app<EVENTS) return 0;

  // check if any change
  if (last_heaporg==_heaporg &&
      last_heapptr==_heapptr &&
      last_heapend==_heapptr &&
      last_heapfirst==_heapfirst &&
      last_heaplast ==_heaplast) return 0;

  // update scan
  printf(HOME "o%04X p%04X e%04X m%04X a%04X\n"
	 , _heaporg, _heapptr, _heapend
	 , _heapmaxavail(), _heapmemavail()
	 );

  dumpheap(2);

  // update
  last_heaporg= _heaporg;
  last_heapptr= _heapptr;
  last_heapend= _heapptr;

  last_heapfirst= _heapfirst;
  last_heaplast = _heaplast;

  return 0;
  
  (void)line;
}
