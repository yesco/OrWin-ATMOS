#define black    0
#define red      1
#define green    2
#define yellow   3
#define blue     4
#define magenta  5
#define cyan     6
#define white    7

#define BG      16

// text colours
#define BLACK    "\x0"
#define RED      "\x1"
#define GREEN    "\x2"
#define YELLOW   "\x3"
#define BLUE     "\x4"
#define MAGENTA  "\x5"
#define CYAN     "\x6"
#define WHITE    "\x7"

// background colours
#define BGBLACK    "\x10"
#define BGRED      "\x11"
#define BGGREEN    "\x12"
#define BGYELLOW   "\x13"
#define BGBLUE     "\x14"
#define BGMAGENTA  "\x15"
#define BGCYAN     "\x16"
#define BGWHITE    "\x17"


extern char wputc(char);
extern void wputi(int);
extern void nl();
extern void wputz(char*);
extern void wputs(char*);
extern char* winptr();

extern void wgotoxy(char x, char y);
extern void wclreol();
extern void wclrscr();

extern void wstatus(signed char, char*);
extern char togglecursor();
extern void setfocus(signed char);

#undef putchar
#define putchar wputc
#define puti wputi
#define putz wputz
#define puts wputs

#define clrscr wclrscr
#define clreol wclreol
#define gotoxy wgotoxy


#include <setjmp.h>

extern void yield();

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

