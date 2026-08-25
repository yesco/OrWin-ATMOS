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
size_t __heapmemavail (void);

extern void* _heaporg;
extern void* _heapptr;
extern void* _heapend;

typedef struct freelist {
  unsigned int size;
  char*        next;
  char*        prev;
} freelist;

typedef struct APP {
  char c;
} APP;

void* app_heap(APP* app, char* line) {
  char buf[80];

  if (!app) {
    window(2, 16, 32, 10, yellow, black);
    wstatus(-1, "Heap Viewer");
    return calloc(sizeof(APP), 1);
  }
  
  // TODO: printf bleeds
  snprintf(buf, sizeof(buf),
	   HOME BGYELLOW BLACK "\n"
	   "o %04X\n"
	   "p %04X\n"
	   "e %04X"
	   , _heaporg
	   , _heapptr
	   , _heapend
	   );
  putz(buf);

  return 0;
}
