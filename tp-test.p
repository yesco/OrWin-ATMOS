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

var
   alpha : integer;
   beta	 : integer;
   i	 : integer;
   sum	 : integer;
   fsum	 : integer;
   
begin
   { Test: Direct literal string compilation }
   writeln("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!");

   { Test: 16-Bit variable immediate assignment }
   alpha := 7;

   { Test: Complex math addition using our functional JSR routine }
   beta := AddTen(Mul2(alpha)+1) + Mul2(11);
   { Test: Printing numeric 47 value from memory }
   writeln(beta);

   { Test: 54 = Two parameter function calling }
   writeln("Two:");
   writeln(Two(alpha, beta));

   writeln("Magical injected input parameter X=");
   writeln(X); { is 42 }

   { Test: looping sum }
   sum := 0;
   for i := 1 to 0 do sum := sum + i;
   writeln("Sum:");
   writeln(sum);

   { Test: looping funsum }
   fsum := 0;
   for i := 1 to 0 do fsum := Two(fsum, i);
   writeln("FumSum:");
   writeln(fsum);

   { Test Case 6: Literal subtraction }
   writeln("Subtraction tests:");
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
end.
