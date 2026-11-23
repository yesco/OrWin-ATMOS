// test-struct.c - structs: fields of a struct variable, initializers,
// pointers (p->f), struct pointer params, sizeof
//
// Plain C: gcc test-struct.c && ./a.out is the reference; the IVM VM and
// the JIT must print exactly the same.
// Each case: its name and the values C gives, then (indented) what we got:
//   fields .:       3 4 7
//                   3 4 7
// - a struct is declared like a function: its params are the fields,
//   field k is word k. A struct variable is a static array (zeroed, or
//   = { consts }: data), p.f and p->f are both word k of the memory
// - the fields are words: a char field would hold the whole value
// - sizeof(struct pt) is 4 bytes here, 8 on gcc: the test prints it in ints
// Values stay below 32768 (gcc int is 32 bit, ours 16).

#include <stdio.h>
#include <stdlib.h>

struct pt { int x; int y; };
struct pt4 { int a; int b; int c; int d; };
struct node { int v; struct node *next; };

struct pt4 g = {5, 6};

// a struct pointer param: its fields through the pointer
void move(struct pt *q, int d) {
  q->x += d;
  q->y = q->y + d;
}

int sum(struct pt *q) {
  return q->x + q->y;
}

int main(void) {
  struct pt p;
  struct pt4 r = {1, 2, 3};
  struct pt4 z = {0};
  struct pt *q;
  struct node a;
  struct node *n;

  // --- a struct variable: its fields ---------------------------------------
  printf("fields .:\t3 4 7\n\t\t");
  p.x = 3;
  p.y = 4;
  printf("%d ", p.x);
  printf("%d ", p.y);
  printf("%d ", p.x + p.y);
  printf("\n");

  printf("++ += --:\t4 14 3\n\t\t");
  p.x++;
  printf("%d ", p.x);
  p.y += 10;
  printf("%d ", p.y);
  p.x--;
  printf("%d ", p.x);
  printf("\n");

  // --- initializers --------------------------------------------------------
  printf("init = {}:\t1 2 3 0\n\t\t");
  printf("%d ", r.a);
  printf("%d ", r.b);
  printf("%d ", r.c);
  printf("%d ", r.d);
  printf("\n");

  printf("init {0}:\t0 0 0\n\t\t");
  printf("%d ", z.a);
  printf("%d ", z.b);
  printf("%d ", z.d);
  printf("\n");

  printf("global:\t\t5 6 0\n\t\t");
  printf("%d ", g.a);
  printf("%d ", g.b);
  printf("%d ", g.c);
  printf("\n");

  // --- pointers: p->f ------------------------------------------------------
  printf("p->f:\t\t11 22 33\n\t\t");
  q = malloc(sizeof(struct pt));
  q->x = 11;
  q->y = 22;
  printf("%d ", q->x);
  printf("%d ", q->y);
  printf("%d ", q->x + q->y);
  printf("\n");

  printf("p->f ++ +=:\t12 32\n\t\t");
  q->x++;
  q->y += 10;
  printf("%d ", q->x);
  printf("%d ", q->y);
  printf("\n");

  printf("params:\t\t15 35 50\n\t\t");
  move(q, 3);
  printf("%d ", q->x);
  printf("%d ", q->y);
  printf("%d ", sum(q));
  printf("\n");

  // a pointer field
  printf("s.f = ptr:\t1 2\n\t\t");
  a.v = 1;
  a.next = malloc(sizeof(struct node));
  n = a.next;
  n->v = 2;
  printf("%d ", a.v);
  printf("%d ", n->v);
  printf("\n");

  printf("sizeof:\t\t2 4\n\t\t");
  printf("%d ", sizeof(struct pt) / sizeof(int));
  printf("%d ", sizeof(struct pt4) / sizeof(int));
  printf("\n");

  return 0;
}
