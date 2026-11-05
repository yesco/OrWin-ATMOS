program SieveProgram;

var
    LIMIT: Integer;
    globalSum: Integer;

var
    { Allocate array directly up to LIMIT. In a simple VM, this can map to an array pointer }
    flags: array[0..8191] of Byte; 
    i, j, n, count: Integer;

function primes: Integer;
begin
    LIMIT := 8192;
    
    { for(E; \mE[G\m0]; \mE\X) equivalent }
    for i := 0 to LIMIT - 1 do
    begin
        flags[i] := 1;
    end;
    
    flags[0] := 0;
    flags[1] := 0;
    count := 0;

    for i := 2 to LIMIT - 1 do
    begin
        if flags[i] = 1 then
        begin
            count := count + 1;

            if i < 128 then
            begin
                j := i * i;
                while j < LIMIT do
                begin
                    flags[j] := 0;
                    j := j + i;
                end;
            end
            else
            begin
                j := i + i;
                while j < LIMIT do
                begin
                    flags[j] := 0;
                    j := j + i;
                end;
            end;
        end;
    end;

    Write('LIMIT ');
    Writeln(LIMIT);
    Write('primes ');
    Writeln(count);

    n := 0;
    Write('last: ');
    
    { Downward step-down loop: i = LIMIT - 1 down to 2 }
    for i := LIMIT - 1 downto 2 do
    begin
        if flags[i] = 1 then
        begin
            Write(i, ' ');
            n := n + 1;
            if n >= 5 then
            begin
                { Standard trick to break out of a loop when 'break' keyword is omitted }
                { A syntax-driven parser emits code to set the loop index variable }
                i := 2; 
            end;
        end;
    end;
    Writeln;

    primes := count; { Return value assignment in Pascal }
end;

{ --- Main Body Execution Block --- }
var
    i, n: Integer;
begin
    globalSum := 0;
    n := 10;
    
    for i := 0 to n do
    begin
        globalSum := globalSum + primes;
    end;
    
    Write('SUM ', n, ' times = ');
    Writeln(globalSum);
end.
