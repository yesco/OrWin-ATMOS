// Config

#define WIN_MAX 16

// Screen printing atributes
// - putchar(red);
// - printf(RED "warning:" WHITE "cucumber" BGGREEN "OK");

#define black    0
#define red      1
#define green    2
#define yellow   3
#define blue     4
#define magenta  5
#define cyan     6
#define white    7

#define BG      16

#define NL       "\n"   // 10 \x0a
#define NLPURE   "\x0b" // 11 \x0b

#define CLRSCR   "\x0c"
#define CR       "\x0d"
#define CLREOL   "\x0e"
#define RPT      "\x0f"


#define HOME     "\x8c" // 'CLRSCR
#define CLNL     "\x8a" // 'NL

// text colours
#define BLACK    "\xff" // \x0 would terminate the string...
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
extern char putcraw(char);
extern void wputi(int);
extern void nl();
extern void clnl();
extern void nlpure();
extern void wputz(char*);
extern void wputs(char*);

extern char* winptr();

extern void wgotoxy(char x, char y);
extern void wclreol();
extern void wclrscr();

extern void wstatus(signed char, char*);
extern char togglecursor();
extern void setfocus(signed char);

char window(char x, char y, char w, char h, char bg, char fg);

#undef putchar
#define putchar wputc
#define puti wputi
#define putz wputz
#define puts wputs

#define clrscr wclrscr
#define clreol wclreol
#define gotoxy wgotoxy

extern char wkbhit(char win);
extern char wgetc(char win);

// Remapped ARROW KEYS
#define KEYLEFT  28
#define KEYRIGHT 29
#define KEYDOWN  30
#define KEYUP    31

#define kbhit wkbhit(wcur)
#undef getc
#define getc() wgetc(wcur)


//#include <types.h>

#define CLOCKS_PER_SEC 100
typedef unsigned int clock_t;

extern clock_t clock();



extern void wscreensize(char* w, char* h);

#define screensize wscreensize

// Some from shell.c - merge!
#define EOS     ((char*)1)
#define RESIZE  ((char*)2)       // TODO: not sent yet
#define IDLE    ((char*)3)
#define TICK    ((char*)4)

#define CLEANUP	((char*)0x4fe)

#define KEYEVENT(line) ((*(char)(line>>8))==1)
#define KEY(line) ((char)(unsigned int)line)

// request by process
#define WAITKEY ((char*)0x100)
#define SLEEP(seconds) ((char*)(0x200|((seconds)&0xff)))

// in app: if (line <= EVENTS) return line;
#define EVENTS  ((void*)0x4ff)

// Memory Allocations

extern void* walloc(size_t z);
extern void* wcalloc(size_t z, size_t n);
extern void* wrealloc(void* p, size_t z);
extern void  wfree(void* p);


#define malloc walloc
#define calloc wcalloc
#define realloc wrealloc
#define free wfree

////////////////////////////////////////////////////////////
// Internals - Don't use unless have to (ps)

typedef struct Window {
  char x, y, w, h;
  char r, c;
  char *p;
  char bg, fg;

  unsigned int nputc;
  
  // TASK
  // (TODO: separate out tasks? only 12 B overhead)
  // (only if can have several in one window...)
  // (or windowless backgound task)

  char status;
  char exit;
  char* ret; // TODO: should be using status?

  // light-weight process task
  void* fun;
  void* state;

  unsigned int nalloc;
  unsigned int abytes;

  unsigned long ticks;
  char cpu; // % cpu used last "round"

} Window;

extern Window wins[WIN_MAX];

extern char wcur;
extern char wfocus;
extern char nwin;

extern char* wname(char winid);

