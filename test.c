#include <stdio.h>

/*

14      _Local1
14      _LocalLast
19      _AddTen
21      _Mul2
29      _TShl                      
29      _TShr
34      _TNot
64      _TCmp

118     _TestWrites
123     _fwrite
247     _TestMath
318     _TestIf
318     _TestOps
2560    __MAIN_SIZE__

-- BYTES FROM test.c

  (+ 14 14 19 21 29 29 34 64 118 123 247 318 318 2560) = 3908

-- CC65 stuff (and maybe strings?)

  ls -l test.sim   => 5656

*/

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
    printf("--Math checks: a: %u b: %u\n", a, b);
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
    printf("--IF THEN ELSE checks:\n");
    printf("%u\n", x);
    
    if (x) {
        printf("1:TRUE\n");
    }
    printf("1: optional\n"); // Out of if scope
    
    if (x) {
        printf("2:TRUE\n");
    } else {
        printf("2:FALSE\n");
    }
    printf("2: both\n"); // Out of if-else scope

    if (x) {
        printf("3:TRUE\n");
    }
    printf("3: optional\n");
    
    if (x) {
        printf("4:TRUE\n");
    } else {
        printf("4:FALSE\n");
    }
    printf("4: both\n");
    
    if (x) {
        if (x) {
            printf("5:TRUE\n");
        } else {
            printf("5:FALSE\n");
        }
    }
    printf("5: both\n"); // Out of the entire outer nested if

    if (x) {
        if (x) {
            printf("7:TRUE\n");
        } else {
            printf("7:FALSE\n");
        }
    } else {
        printf("6: FALSE\n");
    }

    printf("7:NOT:\n");

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
    
    printf("cmp 5=5\n"); printf("%u\n", TCmp(5, 5));
    printf("cmp 3<9\n"); printf("%u\n", TCmp(3, 9));
    printf("cmp 9>3\n"); printf("%u\n", TCmp(9, 3));
    printf("not 0\n"); printf("%u\n", TNot(0));
    printf("not 7\n"); printf("%u\n", TNot(7));
    
    a = 1;
    r = TShl(a, 3);
    printf("1 shl 3\n"); printf("%u\n", r);
    
    r = TShr(16, 2);
    printf("16 shr 2\n"); printf("%u\n", r);
    
    r = (3 << 2) + (20 >> 2);
    printf("3shl2+20shr2\n"); printf("%u\n", r);
    
    return 0; // Return dummy/default value for function type matching Pascal wrapper
}

int TestWrites(void) {
    printf("--TestWrites\n");
    printf("abc\n");
    printf("%u%u%u\n", 1, 2, 3);
    printf("abc"); printf("\n");
    printf("%u%u%u", 1, 2, 3); printf("\n");
    
    return 0; // Return default value
}

int main(void) {
    // Test: Direct literal string compilation
    printf("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!\n");

    printf("12345: %u\n", 12345);
   
    // Test: 16-Bit variable immediate assignment
    alpha = 7;

    printf("alpha: %u\n", alpha);
   
    printf("3+4: %u\n", 3 + 4);

    printf("Mul2(33): %u\n", Mul2(33));

    printf("AddTen(7): %u\n", AddTen(7));

    printf("Mul2(11)+Mul2(22): %u\n", Mul2(11) + Mul2(22));

    // Test: Complex math addition using our functional JSR routine
    beta = AddTen(Mul2(alpha) + 1) + Mul2(11);

    // Test: Printing numeric 47 value from memory
    printf("%u\n", beta);

    // Test: 54 = Two parameter function calling
    printf("--Two:\n");
    printf("%u\n", Two(alpha, beta));

    sum = 40;
    sum = sum + 2;
   
    printf("sum := 40; sum := sum + 2; : %u\n", sum);

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
    
    return 0;
}
