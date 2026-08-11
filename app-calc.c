#include <stdio.h>
#include <ctype.h>

#include "orwin.h"

#define MAX 10

char *KEYS= " \
 7 8 9 / \
\
 4 5 6 * \
\
 1 2 3 - \
\
     0     + \
";

int calc_main(int argc, char** argv) {
  char c, *p= KEYS, buf[30];
  int n, stack[MAX]= {0};

  clrscr();

  gotoxy(0, 3);

  while((c= *p)) {

    if (isspace(c)) putchar(c);
    else {
      putchar(' '+128);
      putchar(c+128);
      putchar(' '+128);
    }

    ++p;
  }

  do {
    int tos= stack[n], nos= stack[n-1];
    
    sprintf(buf, BGBLACK WHITE "     %6d  " BGWHITE MAGENTA, tos);
    // TODO: for double height may need shift down one row!
    gotoxy(1, 1); putz(buf);
    gotoxy(1, 2); putz(buf);
    gotoxy(3, 1); *winptr()= 0xa; // DOUBLE
    gotoxy(3, 2); *winptr()= 0xa; // DOUBLE

    c= getc();

    switch(c) {
    case '+': tos= tos-nos; goto POP;
    case '-': tos= tos-nos; goto POP;
    case '*': tos= tos*nos; goto POP;
    case '/': tos= tos/nos; goto POP;
    case ' ': case 10: case 13: tos= 0; break;
    default: if (isdigit(c)) tos= tos*10 + c-'0';
    }

    continue;
    
  POP:
    stack[--n]= tos;

  } while(1);
	
  // TODO: return and you crash!
  (void)argc; (void)argv;
  return 0;
}

