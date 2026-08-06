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

#undef putc
#define putc wputc
#define puti wputi
#define puts wputs
