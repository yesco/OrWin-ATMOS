#include <stdio.h>

int main() {
  int c;
  
  // TODO: < borken!
  //for(c = 0; c < 69; c++) {
  
  for(c = 0; c <= 69; c++) {
    printf("%u ", c);

    //  simple compares
    if (c <  42) printf("\t<  42\n");
    if (c <= 42) printf("\t<= 42\n");
    if (c == 42) printf("\t== 42\n");
    if (c >= 42) printf("\t>= 42\n");
    if (c >  42) printf("\t>  42\n");

    // combined compares
    //if (c >= 17 && c <= 42) printf("\t17..42\n");
    if (c >= 17) if (c <= 42) printf("\t17..42\n");

  }
}
