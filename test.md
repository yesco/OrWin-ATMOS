# IVM bytecode showcase

- test.p   - 256 lines, 5400 chars, 196 LOC
- test.c   - 285 lines, 5979 chars, 155 LOC
- test.ivm


## bytes

```
-- ./tpi vs ./parse

1345 - test.ivm (bytecode + header + const)
1228 - BYTECODE, SSTACK compacted STACK
1316 - parse.c bytecode, no opt

--- cc65 -Oirs

5656 - test.sim
 908 - code for MAIN and all the func in test.c

--- oscar64 -Os

6648 - test.prg

(/ 1228 3908.0) = 31% size, relocated unoptimized bytecode

1/3 size 3s slower (now 5x, but with new NEXT...)
```

### cc65

```
14      _Local1
14      _LocalLast
19      _AddTen
21      _Mul2
29      _TShl                      
29      _TShr
34      _TNot
64      _TCmp

118     _TestWrites
123     _fwrite
247     _TestMath
318     _TestIf
318     _TestOps
2560    __MAIN_SIZE__

-- BYTES FROM test.c

  (+ 14 14 19 21 29 29 34 64 118 123 247 318 318 2560) = 3908

-- CC65 stuff (and maybe strings?)

  ls -l test.sim   => 5656

```
## running time

```
> ./tpi test | grep cycles

55,231,743 cycles - tpi.c compile & RUN
 7,165,070 cycles - ./tpi test.ivm
 6,949,236 cycles - ./ivm test from BLOB
 2,752,774 cycles - ./ivm-asm  test from BLOB
 1,011,905 cycles - cc65
   186,430 cycles - oscar super-inlining! + recalc?

(- 55231743 7165070) 48,066,673 = 48s - compilation

(/ 255 48.0)  =   5.3 lines / s
(/ 5400 48.0) = 112.5 char  / s 

(/ 6949236.0 2252774) = 3.08x faster asm than C impl of IVM
(/ 2252774 1011905.0) = 2.23x slower than cc65

(/ 1345 5656.0) .ivm blobl: 24% of size of cc65 binary
(/ 5656.0 1345) .ivm blobl: 4.2x smaller!


```
