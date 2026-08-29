#include <stdio.h>

void putwz(void* arr) {
  unsigned int x, *a = arr;
  while((x = *a++)) {
    if (x < 256)
      putchar((int)x);
    else
      fputs((char*)x, stdout);
  }
}

// Your explicit cast wrapper
#define X(val) ((unsigned int)(val))

// --- CC65 FORCE EXPANSION LAYER ---
// These definitions are required so cc65 evaluates the text string "PRINT7" into the actual macro block
#define INVOKE(macro, args) macro args
#define GLUE(a, b) a ## b
// ----------------------------------

#define PRINT1(a) \
  do { unsigned int _w[] = { X(a), 0 }; putwz(_w); } while(0)

#define PRINT2(a,b) \
  do { unsigned int _w[] = { X(a), X(b), 0 }; putwz(_w); } while(0)

#define PRINT3(a,b,c) \
  do { unsigned int _w[] = { X(a), X(b), X(c), 0 }; putwz(_w); } while(0)

#define PRINT4(a,b,c,d) \
  do { unsigned int _w[] = { X(a), X(b), X(c), X(d), 0 }; putwz(_w); } while(0)

#define PRINT5(a,b,c,d,e) \
  do { unsigned int _w[] = { X(a), X(b), X(c), X(d), X(e), 0 }; putwz(_w); } while(0)

#define PRINT6(a,b,c,d,e,f) \
  do { unsigned int _w[] = { X(a), X(b), X(c), X(d), X(e), X(f), 0 }; putwz(_w); } while(0)

#define PRINT7(a,b,c,d,e,f,g) \
  do { unsigned int _w[] = { X(a), X(b), X(c), X(d), X(e), X(f), X(g), 0 }; putwz(_w); } while(0)

#define PRINT8(a,b,c,d,e,f,g,h) \
  do { unsigned int _w[] = { X(a), X(b), X(c), X(d), X(e), X(f), X(g), X(h), 0 }; putwz(_w); } while(0)

// Helper macro to select the name text
#define GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME

// The wrapper that forces cc65 to execute the selected macro text with the given arguments
#define PRINT(...) INVOKE(GET_MACRO(__VA_ARGS__, PRINT8, PRINT7, PRINT6, PRINT5, PRINT4, PRINT3, PRINT2, PRINT1), (__VA_ARGS__))

#define CDE "CDE"

int main(void) {
    char warning[] = "BOOM";
    
    // This will now successfully expand and run inside sim65!
    PRINT('a', 'b', CDE, "FISH", 'x', 'y', 65);
    
    return 0;
}
