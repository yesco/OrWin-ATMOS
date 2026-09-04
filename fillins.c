// fillins.c - fill in generic functions missing
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// This is for the OrWIN-ATMOS project,
// to allow it to compile and run under a simulator.
//
// This file provides generic implementations of
// "missing" functions: strdup getline ...

//////////////////////////////
#ifndef STRDUP

#define STRDUP
char* strdup(const char* s) {
  char* r;
  if (!s) return 0;
  if (!(r= calloc(strlen(s)+1, 1))) return 0;
  return strcpy(r, s);
}

#endif // STRDUP

//////////////////////////////
#ifndef GETLINE

#define GETLINE

#define PR(...) (void)0

#if 1

// TODO: this one is working, cleanup
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
      char* new_s;
      PR("GET: 777\n");
      *z += 40;
      new_s = (char*)realloc(*s, *z);
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

// TODO: this oen maybe not working, at least not
//   on oscar64
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
#endif

#endif // GETLINE


//////////////////////////////
#ifndef BZERO

#define BZERO
#define bzero(p, z) memset((p), 0, (z))

#endif //BZERO

//////////////////////////////
#ifndef ZERO

// #self
#define ZERO(p) memset((p), 0, sizeof(*(p)))

#endif // ZERO

//////////////////////////////
#ifndef FILL

#define FILL
void fill(char x, char y, char w, char h, char c) {
  assert(0);
  // TODO: loop over terminal gotoxy...
}

#endif // FILL


//////////////////////////////
#ifndef CLOCK

  // if we don't know time, at least make
  // clock monotonically increasing!
  typedef unsigned int clock_t;

  #define CLOCK
  clock_t clock() {
    static counter= 0;
    return ++counter;
  }
    


#endif // CLOCK

//////////////////////////////
#ifndef HELP

  #define HELP
  void help() {
    // TODO: save part of screen, display HELPTEXT, restore
    assert(0);
  }

#endif // HELP

//////////////////////////////
#ifndef HEAPMEM

  #define HEAPMEM
  size_t _heapmemavail(void) { return 4711; }
  size_t _heapmaxavail(void) { return   42; }

#endif // HEAPMEME

//////////////////////////////
//#ifndef
