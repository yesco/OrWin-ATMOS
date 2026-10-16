program ShowcaseSuite;

function AddTen(val : integer) : integer;
begin
  AddTen := val + 10;
end;

var
  alpha : integer;
  beta  : integer;

begin
  { Test Case 1: Direct literal string compilation }
  writeln("Hello World from CP/M 6502!");

  { Test Case 2: 16-Bit variable immediate assignment }
  alpha := 15;

  { Test Case 3: Complex math addition using our functional JSR routine }
  beta := AddTen(alpha) + 22;

  { Test Case 4: Printing numeric value from memory }
  writeln(beta);
end.
