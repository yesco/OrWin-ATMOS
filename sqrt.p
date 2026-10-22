PROGRAM SqrtDemo;

{ Integer floor square root by binary search.
  Compare mid with n/mid so mid*mid never overflows 16-bit. }

VAR
  lo : INTEGER;
  hi : INTEGER;
  mid : INTEGER;
  q : INTEGER;
  i : INTEGER;
  n : INTEGER;
  r : INTEGER;

FUNCTION Min(a : INTEGER; b : INTEGER) : INTEGER;
BEGIN
  IF a < b THEN BEGIN
    Min := a;
  END ELSE BEGIN
    Min := b;
  END;
END;

FUNCTION ISqrt(n : INTEGER) : INTEGER;
BEGIN
  IF n < 2 THEN BEGIN
    ISqrt := n;
  END ELSE BEGIN
    lo := 1;
    hi := Min(n, 255);
    ISqrt := 1;
    FOR i := 1 TO 16 DO BEGIN
      mid := (lo + hi) / 2;
      IF mid = 0 THEN BEGIN
        mid := 1;
      END;
      q := n / mid;
      IF mid = q THEN BEGIN
        lo := mid;
        hi := mid;
      END ELSE BEGIN
        IF mid < q THEN BEGIN
          lo := mid;
          ISqrt := mid;
        END ELSE BEGIN
          hi := mid;
        END;
      END;
    END;
    ISqrt := lo;
  END;
END;

BEGIN
  WRITELN("Integer sqrt (floor)");

  n := 0;   r := ISqrt(n); WRITELN("sqrt 0 =");   WRITELN(r);
  n := 1;   r := ISqrt(n); WRITELN("sqrt 1 =");   WRITELN(r);
  n := 15;  r := ISqrt(n); WRITELN("sqrt 15 =");  WRITELN(r);
  n := 16;  r := ISqrt(n); WRITELN("sqrt 16 =");  WRITELN(r);
  n := 100; r := ISqrt(n); WRITELN("sqrt 100 ="); WRITELN(r);
  n := 200; r := ISqrt(n); WRITELN("sqrt 200 ="); WRITELN(r);
  n := 255; r := ISqrt(n); WRITELN("sqrt 255 ="); WRITELN(r);
END.
