#define YMAX 1024

typedef struct Yalloc {
  char nfree;
  size_t z;
  void* p;

  int line;
  char* file;
} Yalloc;

Yalloc yall[YMAX]; // = {0};

size_t yn= 0;

void* ymalloc(size_t z, int line, char* file) {
  if (!yn) memset(yall, 0, sizeof(yall));
  assert(++yn < YMAX);
  
  yall[yn].z= z;
  yall[yn].line= line;
  yall[yn].file= file;
  return yall[yn].p= malloc(z);
}

void* ycalloc(size_t n, size_t z) {
  assert(++yn < YMAX);
  
  yall[yn].z= z;
  return yall[yn].p= calloc(n, z);
}

void yerror(void *p, char* msg) {
  fprintf(stderr, "%%YALLOC.ERROR: %s %p %s\n", msg, p, (char*)p);
  assert(!msg);
}

void yfree(void* p) {
  size_t i;
  if (!p) return;
  for(i=YMAX; --i; ) {
    if (yall[i].p==p) {
      if (++yall[i].nfree >= 2) 
        yerror(p, "Deallocated twice");
      return;
    }
  }
  fprintf(stderr, "%%yfree: never alloc %p: \"%s\"\n", p, p);
}

void* yrealloc(void* p, size_t z, int line, char* file) {
  char* n= ymalloc(z, line, file);
  if (p) memcpy(n, p, z);
  yfree(p);
  return n;
}

void yreport() {
  size_t i;
  putchar('\n');
  for(i=1; i<=yn; ++i) {
    //if (yall[i].nfree==1) continue; // been deallcoated
    printf("HEAP: %4d %d %s  #%4d  %5p %s.%d\n",
      i, yall[i].nfree,
      yall[i].nfree==1? "free": yall[i].nfree==0? "allo": "ERR!",
      yall[i].z, yall[i].p,
      yall[i].file, yall[i].line);
  }
}

#define malloc(z)  ymalloc((z), __LINE__, __FILE__)
#define realloc    yrealloc((z), __LINE__, __FILE__)
#define free       yfree
#define calloc     ycalloc

