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
extern struct FREELIST* _heaplast;

typedef struct freelist {
  unsigned int     size;
  struct freelist* next;
  struct freelist* prev;
} freelist;

typedef struct APP {
  char c;
} APP;

void* app_heap(APP* app, char* line) {
  freelist* p;
  freelist* start;
  int n= 0;
  
  if (!app) {
    window(2, 16, 32, 10, yellow, black);
    wstatus(-1, "Heap Viewer");
    return calloc(sizeof(APP), 1);
  } else if (app<EVENTS) return 0;
  
  printf(HOME "o%04X p%04X e%04X m%04X a%04X\n"
	 , _heaporg, _heapptr, _heapend
	 , _heapmaxavail(), _heapmemavail()
	 );

  // Cast the internal heap pointer to your freelist structure
  start= p= (freelist*)_heapfirst;

  while(p) {
    ++n;
    printf("%04X:%u ", p, p->size);
    if (p->size > 2048) { printf("Unprobable size..."); break; }
    p= p->next;
    if (p == start) { printf("LOOP: %04X...", p); break; }
			   
  }
  printf("\n# %u  ", n);

  return WAITKEY;
  (void)line;
}
