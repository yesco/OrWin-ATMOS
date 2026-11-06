--- BYTECODE (256 bytes) ---
<02> *fc M07

-- TODO: recognize memset, lol                  -- MEMSETppnn
Lfb S08                                         -- COPYfb08 (4->3)
<02> S0d                                        -- OK, is 3 but overhead
J0d S0e
i080d07 
  Lfa )0708 
n0b 

Lfb )07fb                                       -- why loading 0 use 0 
Lfb )07fa                                       --       ...

Z0b Lfc S08 <02> S0f J0f S10                    -- lots of copy in loop init
i080f3f 
  (0708 ? G37                                   -- LONGEST JUMP dec: 53

    -- then
    I0b <08> <f9 ? G19                          -- because of < in loop use generic while
      <08> *08 
      S09 <09>                                  -- S09 <09>    redundant
      <02 ? G0a
        Lfb )0709 U0908
      B10 0  

    -- else
    ? G16
      L08 +08
      S09 <09>                                  -- S09 <09>    again...
      <02 ? G0a                                 -- because of < in loop use generic while
        Lfb )0709 U0908
      B10 
n43 

"LIMIT \0
<02> . N 

"primes \0
<0b> . N 

Z0a 

"last: \0

<02> -fa S08 <08> >fa                           -- S08 <08>   
? G1f (0708
  ? G15
    <08> . O20 L0a +fa S0a <0a> <f8 
      ? ? G04 Lfa S08 J08
B25 N 

-- return
F07 <0b> X 
X 

Lfd S02   Z16   Lf7 S17   Lfa S15   <17> S18    -- COPYfftt save 3b, -4 ops

i15180a 
  <16> S19 (00) +19 S16        -- sum = sum + Fun(): DO a Vss (add acc to ss)
n0e 

"SUM \0
<17> . O20 
"times = \0
<16> . N 

-- return (exit)
<16> X 
X 
q 
