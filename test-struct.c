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
// - the fields take a word cell each: a char field is a byte, its low byte
// - sizeof(struct pt) is 4 bytes here, 8 on gcc: the test prints it in ints
// - &s: a static struct is its address already (s alone works too)
// - chains: ->f after any pointer: a field (a.next->v), an array element
//   (ps[i]->x), a call (mk(6)->x), as often as it comes; loads and stores
// - an array of structs is SoA: field f of all the elements in a row, so
//   a[i].f is f's array [i] (like an int array). __striped (b4 here; parse
//   -S: all of them): f's lo bytes, then its hi bytes (values > 255 check
//   both). a alone is no pointer (test-struct-err.c)
//   Run it also with parse -S
// Values stay below 32768 (gcc int is 32 bit, ours 16).

#include <stdio.h>
#include <stdlib.h>

// not C: only our compiler knows it (oscar64 has one too)
#ifndef __MC65__
#define __striped
#endif

struct pt { int x; int y; };
struct pt4 { int a; int b; int c; int d; };
struct node { int v; struct node *next; };
struct pt3 { int x; int y; int z; };

struct pt3 s3[5];

struct pt4 g = {5, 6};

// a struct pointer param: its fields through the pointer
void move(struct pt *q, int d) {
  q->x += d;
  q->y = q->y + d;
}

// returns a struct pointer: a new one (malloc in an expression)
struct pt *mk(int x) {
  struct pt *r;
  r = malloc(sizeof(struct pt));
  r->x = x;
  r->y = x + 1;
  return r;
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
  struct pt *ps[4];
  int i;
  __striped struct pt b4[4];

  // --- a struct variable: its fields ---------------------------------------
  putz("fields .:\t3 4 7\n\t\t");
  p.x = 3;
  p.y = 4;
  putn(p.x);
  putn(p.y);
  putn(p.x + p.y);
  putz("\n");

  putz("++ += --:\t4 14 3\n\t\t");
  p.x++;
  putn(p.x);
  p.y += 10;
  putn(p.y);
  p.x--;
  putn(p.x);
  putz("\n");

  // --- initializers --------------------------------------------------------
  putz("init = {}:\t1 2 3 0\n\t\t");
  putn(r.a);
  putn(r.b);
  putn(r.c);
  putn(r.d);
  putz("\n");

  putz("init {0}:\t0 0 0\n\t\t");
  putn(z.a);
  putn(z.b);
  putn(z.d);
  putz("\n");

  putz("global:\t\t5 6 0\n\t\t");
  putn(g.a);
  putn(g.b);
  putn(g.c);
  putz("\n");

  // --- pointers: p->f ------------------------------------------------------
  putz("p->f:\t\t11 22 33\n\t\t");
  q = malloc(sizeof(struct pt));
  q->x = 11;
  q->y = 22;
  putn(q->x);
  putn(q->y);
  putn(q->x + q->y);
  putz("\n");

  putz("p->f ++ +=:\t12 32\n\t\t");
  q->x++;
  q->y += 10;
  putn(q->x);
  putn(q->y);
  putz("\n");

  putz("params:\t\t15 35 50\n\t\t");
  move(q, 3);
  putn(q->x);
  putn(q->y);
  putn(sum(q));
  putz("\n");

  // a pointer field
  putz("s.f = ptr:\t1 2\n\t\t");
  a.v = 1;
  a.next = malloc(sizeof(struct node));
  n = a.next;
  n->v = 2;
  putn(a.v);
  putn(n->v);
  putz("\n");

  // --- &, returned pointers, arrays of pointers, chains --------------------
  putz("&s:\t\t4 9\n\t\t");
  p.x = 1;
  move(&p, 3);
  putn(p.x);
  q = &p;
  q->y = 9;
  putn(p.y);
  putz("\n");

  putz("returned:\t6 7 13\n\t\t");
  q = mk(6);
  putn(q->x);
  putn(q->y);
  putn(sum(q));
  putz("\n");

  putz("mk()->f:\t20 0 2\n\t\t");
  putn(mk(20)->x);
  putn(mk(0)->x);
  putn(mk(1)->y);
  putz("\n");

  // an array of struct pointers: filled in a loop, indexed by var / const
  putz("ps[i]->f:\t0 30 31 25\n\t\t");
  for (i = 0; i < 4; i++) ps[i] = mk(i * 10);
  putn(ps[0]->x);
  i = 3;
  putn(ps[i]->x);
  putn(ps[i]->y);
  ps[i - 1]->y = 25;
  putn(ps[2]->y);
  putz("\n");

  putz("ps[i]->f +:\t40 70\n\t\t");
  putn(ps[1]->x + ps[3]->x);
  putn(ps[i]->x + sum(ps[1]) + 19);
  putz("\n");

  // chains through a pointer field: a.next->v, q->next->next->v
  putz("chains:\t\t7 7 4 4\n\t\t");
  a.next = n;
  a.next->v = 7;
  putn(n->v);
  putn(a.next->v);
  n->next = n;
  n->next->next->v = 4;
  putn(n->v);
  putn(a.next->next->next->v);
  putz("\n");

  // in conditions and loops
  putz("if while:\ty 13\n\t\t");
  q = mk(3);
  if (q->x == 3) putz("y ");
  while (q->x < 13) q->x++;
  putn(q->x);
  putz("\n");

  // --- an array of structs (SoA), a global and a local one ---------------
  putz("a[i].f:\t\t30 31 8 3 49 30 0\n\t\t");
  for (i = 0; i < 5; i++) s3[i].x = i * 10;
  for (i = 0; i < 5; i++) s3[i].y = s3[i].x + 1;
  s3[2].z = 7;
  s3[2].z++;
  s3[i - 1].z = s3[4].z + 3;
  b4[3].y = s3[2].z + s3[4].y;
  putn(s3[3].x);
  putn(s3[3].y);
  putn(s3[2].z);
  putn(s3[4].z);
  putn(b4[3].y);
  putn(s3[i - 2].x);
  putn(s3[0].z);
  putz("\n");

  putz("a[i].f += --:\t16 15\n\t\t");
  i = 1;
  s3[i].y += 5;
  putn(s3[1].y);
  s3[i].y--;
  putn(s3[i].y);
  putz("\n");

  // values > 255: the hi bytes (striped), var / const index, += ++
  putz("hi bytes:\t1001 4660 1301 513 30000\n\t\t");
  i = 1;
  s3[i].z = 1000 + i;
  s3[3].z = 0x1234;
  s3[4].y = 1000;
  s3[4].y += 301;
  b4[2].x = 512;
  b4[2].x++;
  s3[0].x = 30000;
  putn(s3[1].z);
  putn(s3[3].z);
  putn(s3[i + 3].y);
  putn(b4[2].x);
  putn(s3[0].x);
  putz("\n");

  putz("sizeof:\t\t2 4\n\t\t");
  putn(sizeof(struct pt) / sizeof(int));
  putn(sizeof(struct pt4) / sizeof(int));
  putz("\n");

  return 0;
}
