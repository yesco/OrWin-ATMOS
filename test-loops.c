#include <stdio.h>

int main(void) {
  int i, j;

  for(i=7; i<=42; i++) printf("%d ", i);
  putchar('\n');

  for(i=7; i<43; i++) printf("%d ", i);
  putchar('\n');
  
  for(i=3+4; i<42+1; i++) printf("%d ", i);
  putchar('\n');

  for(i=7; i<=10; i++) {
//    for(j=i; j<=i*2; j++) { // correct
    for(j=i; i<=i*2; j++) { // is wrong, but con't be compiled!
      printf("%d ", i);    
      printf("%d ", j);
      putchar('\n');
    }
  }
  putchar('\n');
  
  return 42;
}
