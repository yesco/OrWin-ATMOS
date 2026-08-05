#define WIN_MAX 16

#define TEXTSCREEN ((char*)0xBB80) // $BB80-BF3F
#define SCREENROWS 28
#define SCREENCOLS 40
#define SCREENSIZE (SCREENROWS*SCREENCOLS)
#define SCREENLAST (TEXTSCREEN+SCREENSIZE-1)

#define BLACK    0
#define RED      1
#define GREEN    2
#define YELLOW   3
#define BLUE     4
#define MAGNENTA 5
#define CYAN     6
#define WHITE    7

#define BG      16


#define curscr TEXTSCREEN

#define SCREENXY(x, y) ((char*)(curscr+(5*(y))*8+(x)))


void fill(char x, char y, char w, char h, char c) {
  char* p= SCREENXY(x, y);
  for(; h; --h) {
    // TODO: is memset faster?
    for(x= w; x; --x) *p=c,++p;
    p+= 40-w;
  }
}


typedef struct Window {
  char x, y, w, h;
  char r, c;
  char *p;
  char bg, fg;
} Window;

char nwin= 0;
Window win[WIN_MAX], *winp;



void updatewinptr() {
  winp->p= SCREENXY(winp->x + winp->c, winp->y + winp->r);
}

void clreol() {
  fill(winp->x, winp->y, winp->w, 1, winp->bg);
}

char putc(char c) {
  if (c!=10) *winp->p++= c;
  // overflow line?
  if (c==10 || winp->c++ >= winp->w) {
    winp->c= 0;
    // overflow rows?
    if (winp->r++ >= winp->h) winp->r= 0;
    clreol();
    updatewinptr();
  }
  return c;
}

void clrscr() {
  fill(winp->x, winp->y, winp->w, winp->h, winp->bg);
  winp->r= 0;
  winp->c= 0;
}

void setwin(char w) {
  // TODO: set curwin?
  winp= win+w;
}

// TODO: title+= bar?
char window(char x, char y, char w, char h, char bg, char fg) {
  if (nwin==WIN_MAX) return 0;
  winp= win+ ++nwin;
  winp->x= x;
  winp->y= y;
  winp->w= w;
  winp->h= h;
  winp->bg= BG+bg;
  winp->fg= fg;
  // TODO: verify space/clash/overflow?
  
  // shadow (set black)
  fill(x+1, y+1, w+1, h+1, BG+BLACK);

  // text area background
  clrscr();

  // set text color before! (TODO: require "spacing" between frames
  fill(x-1, y, 1, h, fg);

  return nwin;
}

int main() {
  char a, b, c;
  int i= 0;
  
  // clear background to "gray" checkerboard
  fill(0, 0, SCREENCOLS, SCREENROWS, 127);

  a= window(3,  3,  10, 10, WHITE, BLACK);
  b= window(15, 15, 10, 10, GREEN, BLACK);
  c= window(5,  13,  7,  8, BLUE,  WHITE);

  while(++i) {
    if (i & 1) { setwin(a); putc('a'); }
    if (i & 2) { setwin(b); putc('b'); }
    if (i & 4) { setwin(c); putc('c'); }
  }
  
  return 0;
}
