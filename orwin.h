#define BLACK    0
#define RED      1
#define GREEN    2
#define YELLOW   3
#define BLUE     4
#define MAGNENTA 5
#define CYAN     6
#define WHITE    7

#define BG      16

// TODO: "orwin.h"

extern char wputc(char);
extern void wputi(int);
extern void wputs(char*);
extern void wstatus(signed char, char*);
extern char togglecursor();
extern void setfocus();

#undef putc
#define putc wputc
#define puti wputi
#define puts wputs

#include <setjmp.h>

extern char yield();

extern jmp_buf orwinjmp;

#define YIELD() yield()
