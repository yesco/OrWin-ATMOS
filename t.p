Program Test;

var
   a   : integer;
   b   : integer;
   c   : integer;
   i   : integer;
   sum : integer;
   
begin
   writeln("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!");
   writeln("12345: ", 12345);

   a := 7;
   writeln("alpha: ", a);
   writeln("3+4: ", 3+4);

{
   writeln("Mul2(33): ", Mul2(33));
   writeln("AddTen(7): ", AddTen(7));

   writeln("Mul2(11)+Mul2(22): ", Mul2(11)+Mul2(22));

   b := AddTen(Mul2(a)+1) + Mul2(11);
   writeln(b);

   writeln("--Two:");
   writeln(Two(a, b));

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
}
end.
