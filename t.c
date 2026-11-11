#include <stdio.h>

// Global Variables
int alpha = 1111;
int beta = 2222;
int i = 3333;
int sum = 4444;
int fsum = 5555;

int main(void) {
  // Test: Direct literal string compilation
  putz("zyxwvutsrqponmlkjihgfedcba\n");

  putz("12345: "); putnl(12345);
   
  putnl(1000);
  putnl(1001);
  putnl(1002);
  putnl(1003);
  putnl(1004);

  putz("7..21: ");
  for (i = 7; i <= 21; i++) {
    putn(i);
  }
  nl();

  putnl(2000);
  putnl(2001);
  putnl(2002);
  putnl(2003);
  putnl(2004);

  putz("1..10: ");
  for (i = 1; i <= 10; i++) {
    putn(i);
  }
  nl();
  
  putz("sum 1..10: ");
  sum = 0;
  for (i = 1; i <= 10; i++) {
//    sum = sum + i;
    sum += i;
  }
  putnl(sum);

  putz("sum ones: 1..10000: ");
  sum = 0;
  for (i = 1; i <= 10000; i++) {
//    sum = sum + 1;
    sum++;
  }
  putnl(sum);
  nl();
   
  for (i = 1; i <= 10000; i++) ;

  return 0;
}
