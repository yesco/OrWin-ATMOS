// test-pluseq.c - += -= and x = x + 2: H for int += 2 and int* ++, int*
// += n moves 2n bytes; char wraps. gcc is the reference.

#include <stdio.h>
#include <stdlib.h>

int main() {
  int x, n;
  int *p;
  int *a;
  unsigned char c;
  a = malloc(20);
  a[0] = 10; a[1] = 11; a[2] = 12; a[3] = 13; a[4] = 14;

  x = 65534;	x+= 2;		printf("%u ", x);
  x = 5;	x= x + 2; 	printf("%u ", x);
  x = 7; 	x = 2 + x; 	printf("%u ", x);
  x = 7;	x+= 20;		printf("%u\n", x);

  p = a;	p+= 1;	 	printf("%u ", *p);	
  p = a;	p+= 2;	 	printf("%u ", *p);	
  p = a ;n = 3; p += n;		printf("%u ", *p);	
  p = a;	p = p + 1;	printf("%u\n", *p);	

  c = 255;	c+= 2;	 	printf("%u ", c);	
  c = 1;	c-= 2;	 	printf("%u ", c);	
  x = 5;	x-= 7;	 	printf("%u ", x);	
  x = 50;n = 8;	x -= n + 2;	printf("%u\n", x);	

  p = a; p+= 4;	p -= 2;		printf("%u ", *p);	
  p = a; n = 1;	p += n + 2;	printf("%u ", *p);	
  p = a; p+= 4;	p -= n;		printf("%u\n", *p);	

  return 0;
}
