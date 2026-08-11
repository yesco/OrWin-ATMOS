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
extern void nl();
extern void wputz(char*);
extern void wputs(char*);
extern void wstatus(signed char, char*);
extern char togglecursor();
extern void setfocus(signed char);

#undef putchar
#define putchar wputc
#define puti wputi
#define putz wputz
#define puts wputs

#include <setjmp.h>

extern char yield();

extern jmp_buf orwinjmp;

#define YIELD() yield()


extern char wcur;

extern char wkbhit(char win);
extern char wgetc(char win);

#define kbhit wkbhit(wcur)
#undef getc
#define getc() wgetc(wcur)

#define CLOCKS_PER_SEC 100
typedef unsigned int clock_t;

extern clock_t clock();

