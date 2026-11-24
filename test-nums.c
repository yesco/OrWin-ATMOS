#include <stdio.h>

#ifdef __CC65__

#ifndef putz
#define putz printf
#endif

#define putn(x) printf("%d ", (x))
#define putnl(x) printf("%d\n", (x))
#define nl() putchar('\n')

#endif

int main() {
  int i;

  putz("42: "); putn(42); nl();
  putz("17: "); putnl(17);
  putz("42: "); putnl(0x2a);
  putz("42: "); putnl(0x2a);
  putz("17: "); putnl(021);
  putz("42: "); putnl(0b101010);
  putz("32767: "); putnl(32767);
  putz("32767: "); putnl(0x8fff);
  putz("66535: "); putnl(65535);
  putz("66535: "); putnl(0xffff);

  return 42;
}
