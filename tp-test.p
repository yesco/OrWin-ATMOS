program ShowcaseSuite;

{ TODO: do function var names hide/obstruct lgobals? }

function Mul2(val : integer) : integer;
begin
   Mul2 := val+val;
end;

{ TODO: extend for several args }
function AddTen(val : integer) : integer;
begin
  AddTen := val + Mul2(5);
end;

var
  alpha : integer;
  beta	: integer;

begin
  { Test Case 1: Direct literal string compilation }
  writeln("Hello World from CP/M 6502!");

  { Test Case 2: 16-Bit variable immediate assignment }
  alpha := 7;

  { Test Case 3: Complex math addition using our functional JSR routine }
  beta := AddTen(Mul2(alpha)+1) + Mul2(11);

  { Test Case 4: Printing numeric value from memory }
  writeln(beta);
   
  writeln("Magitcal injected input parameter X="); writeln(X); { is 42 }
end.
