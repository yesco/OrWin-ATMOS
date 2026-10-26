{ Pointer smoke test: New / Dispose / indexed / SizeOf }

Program TestPointers;

FUNCTION TestPointers() : INTEGER;
VAR
  p : ^WORD;
  i : INTEGER;
BEGIN
  SetLength(p, 3);   { 3 WORDs on the heap }

  i := 0; p[i] := 100;
  i := 1; p[i] := 200;
  i := 2; p[i] := 300;

  i := 0; writeln("p[0]=", p[i]);
  i := 1; writeln("p[1]=", p[i]);
  i := 2; writeln("p[2]=", p[i]);

  p^ := 55;          { first element }
  writeln("p^="); writeln(p^);

  writeln("SizeOf(WORD)="); writeln(SizeOf(WORD));

  Dispose(p);
  TestPointers := 1;
END;

BEGIN
  writeln("TestPointers=");
  writeln(TestPointers());
END.
