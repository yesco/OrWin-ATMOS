{ asm: Test program generates 574 bytes }
{ ivm: Test program generates 167 bytes }
{ asm: 1000: (/ (- 1418227 1290832) 1000) =  127c/loop }

{ ivm: 1000: (/ (- 9734015 3970047) 1000) = 5763c/loop }
{ ivm/asm = (/ 5763 127) = 45x slower, lol             }

{ ivm: 1000: (/ (- 6069944 3947071) 1000) = 2122c/loop  }
{ ivm/asm = (/ 2122 127) = 16x slower, but improved 3x! }

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

function Two(a : integer; b: integer) : integer;
begin
   Two := a + b;
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
  for i:=1 to 1000 do sum:= sum + i;
  writeln("Sum:"); writeln(sum);

  writeln("Two:"); writeln(Two(alpha, beta));
   
  writeln("Magical injected input parameter X="); writeln(X); { is 42 }
end.
