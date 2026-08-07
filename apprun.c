#include <stdio.h>

#include "apps.ext"

struct apps {
  char* name;
  void* main;
} apps[] = {

#include "apps.reg"

  {0, 0}
};

int main() {
  struct apps p= apps;

  while(p->name) {
    printf("%4H: %s/n", p->main, p->name);
    ++p;
  }

  return 0;
}



