PROGRAM SqrtDemo;

{ Integer floor square root by binary search.
  Compare mid with n/mid so mid*mid never overflows 16-bit. }

FUNCTION Min(a : INTEGER; b : INTEGER) : INTEGER;
BEGIN
  IF a < b THEN BEGIN
    Min := a;
  END ELSE BEGIN
    Min := b;
  END;
END;

FUNCTION ISqrt(n : INTEGER) : INTEGER;
VAR
  lo : INTEGER;
  hi : INTEGER;
  mid : INTEGER;
  q : INTEGER;
  i : INTEGER;

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

FUNCTION Test(n	: INTEGER) : INTEGER;
BEGIN
  Test := ISqrt(n);
  WRITELN();
  WRITE("sqrt ", n, " = ");
END;

VAR
  i : INTEGER;
  r : INTEGER;

BEGIN
  WRITELN("Integer sqrt (floor)");

  WRITE(Test(0), Test(1), Test(15), Test(16), Test(100),
	Test(200), Test(255), Test(49));
END.
