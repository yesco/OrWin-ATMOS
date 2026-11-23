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

struct pt p;
struct pt g = {5, 6};
struct pt sa[8];
__striped struct pt st[8];

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
  return 0;
}
