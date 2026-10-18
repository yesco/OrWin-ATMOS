{ asm: Test program generates 574 bytes }
{ ivm: Test program generates 167 bytes }
{ asm: 1000: (/ (- 1418227 1290832) 1000) =  127c/loop }

{ ivm: 1000: (/ (- 6678940 2332536) 1000) = 4346c/loop }
{ ivm/asm = (/ 4346 127) = 34x slower, lol             }

{ ivm: 1000: (/ (- 7371646 1597825) 1000) = 5773c/loop }
{ ivm/asm = (/ 5773 127) = 43x slower, lol             }


Program ShowcaseSuite;

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
  alpha	: integer;
  beta	: integer;
  i	: integer;
  sum	: integer;

begin
  { Test Case 1: Direct literal string compilation }
  writeln("Hello 6502 World at OrWIN-ATMOS TinyPascal VM!");

  { Test Case 2: 16-Bit variable immediate assignment }
  alpha:= 7;

  { Test Case 3: Complex math addition using our functional JSR routine }
  beta:= AddTen(Mul2(alpha)+1) + Mul2(11);

  { Test Case 4: Printing numeric value from memory }
  writeln(beta);
   
  { Test Case 5: looping sum - 130c / loop}
  sum:= 0;
  for i:=1 to 0 do sum:= sum + i;
  writeln("Sum:"); writeln(sum);

  writeln("Magical injected input parameter X="); writeln(X); { is 42 }
end.
