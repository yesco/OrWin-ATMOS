#include <stdio.h>

// Global Variables
int alpha;
int beta;
int i;
int sum;
int fsum;

int Mul2(int val) {
  return val + val;
}

int AddTen(int val) {
  return val + Mul2(5);
}

int Two(int a, int b) {
  return a + b;
}

int Local1(int a, int b) {
  return a;
}

int LocalLast(int a, int b) {
  return b;
}

int TestMath(int a, int b) {
  // Standard baseline tests
  putz("--Math checks: a: "); putn(a); putz(" b: "); putn(b);
  printf("%u\n", a + b);
  printf("%u\n", a - b);
  printf("%u\n", a * b);
  printf("%u\n", a / b); // Note: Integer division
    
  // Bitwise operators via table lookup
  printf("%u\n", a & 15);
  printf("%u\n", a | 1);

  // Commutative poop-hole trigger: complex right side inside addition/bitwise operations
  return (a * b) + (a & b);
}

int TestIf(int x) {
  int test_if_ret;

  // Ported strictly to replicate the flawed Pascal indentation and block scope logic
  putz("--IF THEN ELSE checks:\n");
  printf("%u\n", x);
    
  if (x) {
    putz("1:TRUE\n");
  }
  putz("1: optional\n"); // Out of if scope
    
  if (x) {
    putz("2:TRUE\n");
  } else {
    putz("2:FALSE\n");
  }
  putz("2: both\n"); // Out of if-else scope

  if (x) {
    putz("3:TRUE\n");
  }
  putz("3: optional\n");
    
  if (x) {
    putz("4:TRUE\n");
  } else {
    putz("4:FALSE\n");
  }
  putz("4: both\n");
    
  if (x) {
    if (x) {
      putz("5:TRUE\n");
    } else {
      putz("5:FALSE\n");
    }
  }
  putz("5: both\n"); // Out of the entire outer nested if

  if (x) {
    if (x) {
      putz("7:TRUE\n");
    } else {
      putz("7:FALSE\n");
    }
  } else {
    putz("6: FALSE\n");
  }

  putz("7:NOT:\n");

  if (x) {
    test_if_ret = 1;
  } else {
    test_if_ret = 0;
  }
    
  return test_if_ret;
}

int TCmp(int x, int y) {
  if (x == y) {
    return 1;
  } else {
    if (x < y) {
      return 2;
    } else {
      return 3;
    }
  }
}

int TNot(int x) {
  if (!(x == 0)) {
    return 1;
  } else {
    return 0;
  }
}

int TShl(int x, int n) {
  return x << n;
}

int TShr(int x, int n) {
  return x >> n;
}

int TestOps(int dummy) {
  int a;
  int b;
  int r;
    
  putz("cmp 5=5\n"); printf("%u\n", TCmp(5, 5));
  putz("cmp 3<9\n"); printf("%u\n", TCmp(3, 9));
  putz("cmp 9>3\n"); printf("%u\n", TCmp(9, 3));
  putz("not 0\n"); printf("%u\n", TNot(0));
  putz("not 7\n"); printf("%u\n", TNot(7));
    
  a = 1;
  r = TShl(a, 3);
  putz("1 shl 3\n"); printf("%u\n", r);
    
  r = TShr(16, 2);
  putz("16 shr 2\n"); printf("%u\n", r);
    
  r = (3 << 2) + (20 >> 2);
  putz("3shl2+20shr2\n"); printf("%u\n", r);
    
  return 0; // Return dummy/default value for function type matching Pascal wrapper
}

void TestPrios(void) {
  putz("3 + 4 * 5 = "); printf("%u\n", 3 + 4 * 5);
  putz("5 * 4 + 3 = "); printf("%u\n", 5 * 4 + 3);
}

int TestWrites(void) {
  putz("--TestWrites\n");
  putz("abc\n");

  // printf("%u%u%u\n", 1, 2, 3);
  printf("%u", 1);
  printf("%u", 2);
  printf("%u", 3);
  putchar('\n');

  printf("abc");
  putchar('\n');

  // printf("%u%u%u", 1, 2, 3); putchar('\n');
  printf("%u", 1);
  printf("%u", 2);
  printf("%u", 3);
  putchar('\n');
    
  return 0; // Return default value
}

int main(void) {
  // Test: Direct literal string compilation
  putz("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!\n");

  putz("12345: "); printf("%u\n", 12345);
   
  // Test: 16-Bit variable immediate assignment
  alpha = 7;

  putz("alpha: "); putn(alpha); putchar('\n');
   
  putz("3+4: "); putn(3 + 4); putchar('\n');

  printf("Mul2(33): "); printf("%u\n", Mul2(33));

  printf("AddTen(7): "); printf("%u\n", AddTen(7));

  printf("Mul2(11)+Mul2(22): "); printf("%u\n", Mul2(11) + Mul2(22));

  // Test: Complex math addition using our functional JSR routine
  beta = AddTen(Mul2(alpha) + 1) + Mul2(11);

  // Test: Printing numeric 47 value from memory
  printf("%u\n", beta);

  // Test: 54 = Two parameter function calling
  printf("--Two:\n");
  printf("%u\n", Two(alpha, beta));

  sum = 40;
  sum = sum + 2;
   
  putz("sum := 40; sum := sum + 2; ");
  printf("%u\n", sum);

  printf("1..10: ");
  for (i = 1; i <= 10; i++) {
    printf("%u ", i);
  }
  printf("\n");

  printf("7..21: ");
  for (i = 7; i <= 21; i++) {
    printf("%u ", i);
  }
  printf("\n");

  printf("sum 1..10: ");
  sum = 0;
  for (i = 1; i <= 10; i++) {
    sum = sum + i;
  }
  printf("%u\n", sum);

  printf("\n");
   
  // again...
  alpha = 7;
   
  // Test: Complex math addition using our functional JSR routine
  beta = AddTen(Mul2(alpha) + 1) + Mul2(11);

  // Test: Printing numeric 47 value from memory
  printf("%u\n", beta);

  // Test: 54 = Two parameter function calling
  printf("--Two:\n");
  printf("%u\n", Two(alpha, beta));

  // Test: looping sum
  sum = 0;
  for (i = 1; i <= 1000; i++) {
    sum = sum + i;
  }
  printf("--Sum:\n");
  printf("%u\n", sum);

  // Test: looping funsum
  fsum = 0;
  for (i = 1; i <= 1000; i++) {
    fsum = Two(fsum, i);
  }
  printf("--FunSum:\n");
  printf("%u\n", fsum);

  // Test Case 6: Literal subtraction
  printf("--Subtraction tests:\n");
  printf("%u\n", 20 - 3);       // expected 17
  printf("%u\n", 100 - 10 - 5); // expected 85

  // Test Case 7: Subtract from a variable
  printf("%u\n", alpha - 2);    // expected 5

  // Test Case 8: Function call on the left
  printf("%u\n", Mul2(alpha) - 3); // expected 11

  // Test Case 9: Function call on the right
  printf("%u\n", 100 - Mul2(7));   // expected 86

  // Test Case 10: Subtraction with a function on each side
  printf("%u\n", Mul2(10) - Mul2(3)); // expected 14

  {
    int t1, t2;
      
    printf("TestMath(42, 33): ");  t1 = TestMath(42, 33);  printf("%u\n", t1);
    printf("TestMath(17, 255): "); t2 = TestMath(17, 255); printf("%u\n", t2);
  }

  printf("%u\n", TestIf(0));
  printf("%u\n", TestIf(1));

  printf("%u\n", TestOps(666));

  printf("%u\n", TestWrites());
    
  // TODO: even void funs need to be assigned, lol
  alpha= TestPrios();
  //(void)TestPrios();
    
  return 0;
}
