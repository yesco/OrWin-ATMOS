#include <stdio.h>

int main(void) {
  int i, j;

  for(i=7; i<=42; i++) printf("%d ", i);
  putchar('\n');

  for(i=7; i<43; i++) printf("%d ", i);
  putchar('\n');
  
  for(i=3+4; i<42+1; i++) printf("%d ", i);
  putchar('\n');

  // nested
  for(i=7; i<=10; i++) {
    for(j=i; j<=i*2; j++) {
      printf("%d ", i);    
      printf("%d ", j);
      putchar('\n');
    }
  }
  putchar('\n');
  
  // E: generic-for (step 2, strict '<') — exercises catch-all, not i/n.
  //    Must print 0 2 4 6 8 10 12 14 16 18 (no 20).
  {
    int k;

    putz("E 0,2,-18: ");

    //for(k=0; k<20; k=k+2) printf("%d ", k);
    for (k=0; k<=20; k++) printf("%d ", k);

    putchar('\n');
  }

  // F: generic-for one-past-the-end detector.
  //    Body stores a[k]; the buggy catch-all runs the body one extra time
  //    with k==20, so a[20] becomes 1 and last becomes 20.
  {
    int k, last; //, a[32];

    //for(k=0; k<32; k=k+1) a[k] = 0;
    //for(k=0; k<32; k++) a[k] = 0;

    last = 0xffff;

    //for(k=0; k<20; k=k+2) { last = k; a[k] = 1; }
    for(k=0; k<20; k=k+2) { last = k; }

    putz("F last, a[18],, a[19], a[20]: 18 1 0 0\n");
    putz("F last, a[18],, a[19], a[20]: ");
    printf("%u ", last);
    // printf("%u ", a[18]); printf("%u ", a[19]); printf("%u ", a[20]);
    putchar('\n');
    // want: last=18 a[18]=1 a[19]=0 a[20]=0
  }

  // G: nested generic-for — per-frame mark/patch isolation.
  //    Both loops step != 1, so both hit the catch-all.
  //    Inner loop count 2, outer count 3 → cnt = 6.
  {
    int p, q, cnt;

    cnt = 0;
    for(p=0; p<6; p=p+2) {
      for(q=0; q<6; q=q+3) {
        cnt = cnt + 1;
      }
    }
    putz("G G: cnt=%d"); printf("%u ", cnt); putchar('\n');
    // want: cnt=6
  }

  return 42;
}
