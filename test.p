Program ShowcaseSuite;

{ TODO: do function var names hide/obstruct globals? }

function Mul2(val : integer) : integer;
begin
   Mul2 := val+val;
end;

{ TODO: extend for several args }
function AddTen(val : integer) : integer;
begin
   AddTen := val + Mul2(5);
end;

function Two(a : integer; b: integer) : integer;
begin
   Two := a + b;
end;

function Local1(a : integer; b: integer) : integer;
begin
   Two := a;
end;

function LocalLast(a : integer; b: integer) : integer;
begin
   Two := b;
end;

FUNCTION TestMath(a: INTEGER; b: INTEGER): INTEGER;
BEGIN
  { Standard baseline tests }
  WRITELN("--Math checks: a: ", a, " b: ", b);
  WRITELN(a + b);
  WRITELN(a - b);
  WRITELN(a * b);
  WRITELN(a / b);
  
  { Bitwise operators via table lookup }
  WRITELN(a & 15);
  WRITELN(a | 1);

{ TODO: ^ isn't valid pascal?
  WRITELN(a ^ b);
}

  { Commutative poop-hole trigger: complex right side inside addition/bitwise operations }
  TestMath := (a * b) + (a & b);
END;

function TestIf(x : integer) : integer;
begin
   { TODO: this is not correct, I think it runs both then else! }

   writeln("--IF THEN ELSE checks:");
   writeln(x);
   
   if x then writeln("1:TRUE"); writeln("1: optional");
   if x then writeln("2:TRUE"); else writeln("2:FALSE"); writeln("2: both");

   if x then begin writeln("3:TRUE"); end; writeln("3: optional");
   if x then begin writeln("4:TRUE"); end
   else begin writeln("4:FALSE"); end; writeln("4: both");
   
   if x then
      if x then writeln("5:TRUE");
      else writeln("5:FALSE"); writeln("5: both");

   if x then
      if x then writeln("7:TRUE");
      else writeln("7:FALSE");
   else writeln("6: FALSE");

   writeln("7:NOT:");

   if x then TestIf := 1; else TestIf := 0;
   
end;

{ Test = < > NOT SHL SHR }

FUNCTION TCmp(x : INTEGER; y : INTEGER) : INTEGER;
BEGIN
  IF x = y THEN BEGIN
    TCmp := 1;
  END ELSE BEGIN
    IF x < y THEN BEGIN
      TCmp := 2;
    END ELSE BEGIN
      TCmp := 3;
    END;
  END;
END;

FUNCTION TNot(x : INTEGER) : INTEGER;
BEGIN
  IF NOT (x = 0) THEN BEGIN
    TNot := 1;
  END ELSE BEGIN
    TNot := 0;
  END;
END;

FUNCTION TShl(x : INTEGER; n : INTEGER) : INTEGER;
BEGIN
  TShl := x SHL n;
END;

FUNCTION TShr(x : INTEGER; n : INTEGER) : INTEGER;
BEGIN
  TShr := x SHR n;
END;

FUNCTION TestOps(dummy : INTEGER) : INTEGER;
VAR
  a : INTEGER;
  b : INTEGER;
  r : INTEGER;
BEGIN
  WRITELN("cmp 5=5"); WRITELN(TCmp(5, 5));
  WRITELN("cmp 3<9"); WRITELN(TCmp(3, 9));
  WRITELN("cmp 9>3"); WRITELN(TCmp(9, 3));
  WRITELN("not 0"); WRITELN(TNot(0));
  WRITELN("not 7"); WRITELN(TNot(7));
  a := 1;
  r := TShl(a, 3);
  WRITELN("1 shl 3"); WRITELN(r);
  r := TShr(16, 2);
  WRITELN("16 shr 2"); WRITELN(r);
  r := (3 SHL 2) + (20 SHR 2);
  WRITELN("3shl2+20shr2"); WRITELN(r);
END;

{ TODO: Procedure }
function TestWrites() : integer;
begin
   writeln("--TestWrrites");
   writeln("a", "b", "c");
   writeln(1,2,3);
   write("a", "b", "c"); writeln();
   write(1,2,3); writeln();
end;

var
   alpha : integer;
   beta	 : integer;
   i	 : integer;
   sum	 : integer;
   fsum	 : integer;
   
begin
   { Test: Direct literal string compilation }
   writeln("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!");

   writeln("12345: ", 12345);
   
   { Test: 16-Bit variable immediate assignment }
   alpha := 7;

   writeln("alpha: ", alpha);
   
   writeln("3+4: ", 3+4);

   writeln("Mul2(33): ", Mul2(33));

   writeln("AddTen(7): ", AddTen(7));

   writeln("Mul2(11)+Mul2(22): ", Mul2(11)+Mul2(22));

   { Test: Complex math addition using our functional JSR routine }
   beta := AddTen(Mul2(alpha)+1) + Mul2(11);

   { Test: Printing numeric 47 value from memory }
   writeln(beta);

   { Test: 54 = Two parameter function calling }
   writeln("--Two:");
   writeln(Two(alpha, beta));

   sum := 40;
   sum := sum + 2;
   
   writeln("sum := 40; sum := sum + 2; : ", sum);

   write("1..10: ");
   for i:=1 to 10 do
      write(i, " ");
   writeln();

   write("7..21: ");
   for i:=7 to 21 do
      write(i, " ");
   writeln();

   write("sum 1..10: ");
   sum := 0;
   for i:=1 to 10 do
      sum := sum + i;
   writeln(sum);

   writeln();
   
   { again... }
   alpha := 7;
   
   { Test: Complex math addition using our functional JSR routine }
   beta := AddTen(Mul2(alpha)+1) + Mul2(11);

   { Test: Printing numeric 47 value from memory }
   writeln(beta);

   { Test: 54 = Two parameter function calling }
   writeln("--Two:");
   writeln(Two(alpha, beta));

   { Test: looping sum }
   sum := 0;
   for i := 1 to 1000 do sum := sum + i;
   writeln("--Sum:");
   writeln(sum);

   { Test: looping funsum }
   fsum := 0;
   for i := 1 to 1000 do fsum := Two(fsum, i);
   writeln("--FunSum:");
   writeln(fsum);

   { Test Case 6: Literal subtraction }
   writeln("--Subtraction tests:");
   writeln(20 - 3);       { expected 17 }
   writeln(100 - 10 - 5); { expected 85 }

   { Test Case 7: Subtract from a variable }
   writeln(alpha - 2);    { expected 5 }

   { Test Case 8: Function call on the left }
   writeln(Mul2(alpha) - 3); { expected 11 }

   { Test Case 9: Function call on the right }
   writeln(100 - Mul2(7));   { expected 86 }

   { Test Case 10: Subtraction with a function on each side }
   writeln(Mul2(10) - Mul2(3)); { expected 14 }

   writeln("TestMath(42, 33): ", TestMath(42,  33));
   writeln("TestMath(17, 255): ", TestMath(17, 255));

   writeln(TestIf(0));
   writeln(TestIf(1));

   writeln(TestOps(666));

   writeln(TestWrites());
   { TestWrites(); }
end.
