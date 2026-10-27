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
   
end.
