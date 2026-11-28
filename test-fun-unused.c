// test-fun-unused.c — unused-function elimination + frame-slot release
//
// Compile with -O (./pc or parse with IVM_OPT). Expected effects:
//
//   1. Bytecode of never-called functions is deleted (gen_drop_unused).
//   2. Their frame slots are not reserved in the packed layout (gen_pack
//      skips !func_live), so memsz stays close to main + the small live
//      helpers — not inflated by fat_dead's many locals.
//
// Live graph:  main -> used_leaf
//              main -> used_mid -> used_leaf
// Dead graph:  fat_dead (many locals, never called)
//              dead_mid -> dead_leaf   (only called from each other)
//
// Runtime check: same shape as man-func.c — each case prints
//   name:<TAB>expected
// then an indented line of what we got; run.sh compares them.
//
// Without -O the program still runs; only the image size / memsz differ.

#include <stdio.h>

// ---- live helpers (must survive -O) ---------------------------------

int used_leaf(int a) {
  int t;
  t = a + 1;
  return t;
}

int used_mid(int a, int b) {
  int x, y;
  x = used_leaf(a);
  y = used_leaf(b);
  return x + y;
}

// ---- dead: large frame, never reachable from main -------------------

// Many locals so a missing drop would show up as a fat memsz / frame band.
// Deliberately not called from main (or from any live function).
int fat_dead(int n) {
  int a0, a1, a2, a3, a4, a5, a6, a7;
  int b0, b1, b2, b3, b4, b5, b6, b7;
  int c0, c1, c2, c3, s, i;

  a0 = n;     a1 = n + 1; a2 = n + 2; a3 = n + 3;
  a4 = n + 4; a5 = n + 5; a6 = n + 6; a7 = n + 7;
  b0 = a0;    b1 = a1;    b2 = a2;    b3 = a3;
  b4 = a4;    b5 = a5;    b6 = a6;    b7 = a7;
  c0 = b0 + b1; c1 = b2 + b3; c2 = b4 + b5; c3 = b6 + b7;
  s = 0;
  i = 0;
  while (i < 8) {
    s = s + a0;
    i = i + 1;
  }
  return s + c0 + c1 + c2 + c3;
}

// ---- dead chain (only references each other) ------------------------

int dead_leaf(int a) {
  int t, u;
  t = a * 2;
  u = t + 3;
  return u;
}

int dead_mid(int a) {
  int x;
  x = dead_leaf(a);
  return x + dead_leaf(a + 1);
}

// ---- entry ----------------------------------------------------------

int main(void) {
  int r;

  // used_leaf alone
  putz("leaf:\t\t4\n\t\t");
  putnl(used_leaf(3));

  // used_mid -> used_leaf: (2+1)+(4+1) = 8
  putz("mid:\t\t8\n\t\t");
  putnl(used_mid(2, 4));

  // used_leaf(5)+used_mid(1,2) = 6+(2+3) = 11
  putz("combo:\t\t11\n\t\t");
  r = used_leaf(5) + used_mid(1, 2);
  putnl(r);

  // If fat_dead / dead_mid / dead_leaf were kept, their frames would still
  // occupy slots under -O packing. After gen_drop_unused they must not:
  // compare memsz (or the .ivm size) with a build that forces a call to
  // fat_dead. This file never calls them, so a correct -O image has no
  // call vector sites targeting those bodies and no reserved frame cells
  // for them.
  //
  // Sanity: program result does not depend on the dead functions.
  putz("ok:\t\t1\n\t\t");
  putnl(1);

  return 0;
}
