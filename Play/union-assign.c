#include <stdio.h>

union Value {
    int i;
    double d;
    const char* s;
};

// C++ "magic" matches the types automatically:
Value v1 = { 42 };       // Actives v1.i (int)
Value v2 = { 3.14 };     // Actives v2.d (double)
Value v3 = { "hello" };  // Actives v3.s (const char*)

