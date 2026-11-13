\ Byte sieve primes 8192 => 1899 by Gemini for Forth
\ about 61 byte codes on OpenFirmware
\ estimated 6502 cycles: ~96.8M cycles
\ and estmated for ivm:  ~44.7M cycles (zero page compiled)


: sieve ( n -- )
  dup 1- 2/                  \ compute array size: (n - 1) / 2
  dup 0<= if 2drop 0 . cr exit then
  dup allocate throw         \ malloc(size) -> addr
  swap 2dup 1 fill           \ fill sieve array with 1s
  0 >r                       \ push prime count (0) onto return stack
  0 do                       \ outer loop: i from 0 to size-1
    dup i + c@ if            \ if sieve[i] != 0:
      i 2* 3 +               \   step = 2*i + 3
      i over +               \   k = i + step
      begin                  \   inner loop: mark multiples
        2dup >               \   while k < size
      while
        0 3 pick 2 pick + c! \     sieve[k] = 0
        over +               \     k += step
      repeat
      2drop                  \   drop k and step
      r> 1+ >r               \   count++
    then
  loop
  drop r> . cr               \ drop addr, print count + newline
;
