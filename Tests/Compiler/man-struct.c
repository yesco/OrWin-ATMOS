// man-struct.c - MeteoriC-man.md 10: structs, pointers to them, chains,
// arrays of pointers, arrays of structs (SoA), __striped (more cases:
// ../../test-struct.c)
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>
#include <stdlib.h>

#ifndef __MC65__
#define __striped
#endif

struct pt { int x; int y; };
struct node { int v; struct node *next; };
struct cn { char c; int n; struct cn *nx; };

struct pt p;
struct pt g = {5, 6};
struct pt sa[8];
__striped struct pt st[8];
struct cn cv;
struct cn ca[8];
__striped struct cn cs[8];

void move(struct pt *q, int d) {
  q->x += d;
  q->y = q->y + d;
}

struct pt *mk(int x) {
  struct pt *r;
  r = malloc(sizeof(struct pt));
  r->x = x;
  r->y = x + 1;
  return r;
}

int main(void) {
  struct pt *q;
  struct pt *ps[4];
  struct node *n;
  struct cn *cp;
  int i;

  printf("fields:\t\t3 4 5 6\n\t\t");
  p.x = 3; p.y = 4;
  printf("%d ", p.x);
  printf("%d ", p.y);
  printf("%d ", g.x);
  printf("%d\n", g.y);

  printf("pointers:\t4 5 9 10\n\t\t");
  q = &p;
  q->x++;
  printf("%d ", p.x);
  move(&p, 1);
  printf("%d ", q->x);
  q = mk(9);
  printf("%d ", q->x);
  printf("%d\n", q->y);

  printf("chains:\t\t20 31 8 8\n\t\t");
  for (i = 0; i < 4; i++) ps[i] = mk(i * 10);
  printf("%d ", ps[2]->x);
  printf("%d ", ps[i - 1]->y);
  n = malloc(sizeof(struct node));
  n->next = n;
  n->next->next->v = 8;
  printf("%d ", n->v);
  printf("%d\n", mk(8)->x);

  // arrays of structs: SoA; __striped: lo / hi bytes apart (the JIT)
  printf("a[i].f:\t\t14 1000 21 1301\n\t\t");
  for (i = 0; i < 8; i++) { sa[i].x = i * 2; st[i].y = 1000 + i; }
  i = 7;
  printf("%d ", sa[i].x);
  printf("%d ", st[0].y);
  sa[3].y = 20; sa[3].y++;
  printf("%d ", sa[3].y);
  st[1].y += 300;
  printf("%d\n", st[1].y);

  // sizeof: in ints (gcc's int is 4 bytes, ours 2)
  printf("sizeof:\t\t2 2 16 16\n\t\t");
  i = sizeof(struct pt) / sizeof(int); printf("%d ", i);
  i = sizeof(p) / sizeof(int); printf("%d ", i);
  i = sizeof(sa) / sizeof(int); printf("%d ", i);
  i = sizeof(st) / sizeof(int); printf("%d\n", i);
  // a char field is a byte (as a char variable): [ ] ops, the lo byte
  printf("char fields:\t44 300 45 66 6 7 1\n\t\t");
  cv.c = 300; cv.n = 300;
  printf("%d ", cv.c);
  printf("%d ", cv.n);
  cv.c++;
  printf("%d ", cv.c);
  cp = &cv;
  cp->c = 'B';
  printf("%d ", cp->c);
  cv.nx = &cv;
  cp->nx->c = 6;
  printf("%d ", cv.c);
  cp->c += 1;
  printf("%d ", cp->nx->c);
  cv.c = 257;
  printf("%d\n", cv.c);

  printf("a[i].c:\t\t44 45 1 2 300\n\t\t");
  i = 3;
  ca[i].c = 300;  printf("%d ", ca[3].c);
  ca[i].c++;      printf("%d ", ca[i].c);
  cs[i].c = 257;  printf("%d ", cs[i].c);
  cs[i + 1].c = 2; printf("%d ", cs[4].c);
  cs[i].n = 300;  printf("%d\n", cs[3].n);
  return 0;
}
