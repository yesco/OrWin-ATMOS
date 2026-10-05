; Compiled with 1.32.273
--------------------------------------------------------------------
startup: ; startup
0801 : 0b __ __ INV
0802 : 08 __ __ PHP
0803 : 0a __ __ ASL
0804 : 00 __ __ BRK
0805 : 9e __ __ INV
0806 : 32 __ __ INV
0807 : 30 36 __ BMI $083f ; (startup + 62)
0809 : 31 00 __ AND ($00),y 
080b : 00 __ __ BRK
080c : 00 __ __ BRK
080d : ba __ __ TSX
080e : 8e 6b 1a STX $1a6b ; (spentry + 0)
0811 : a2 1a __ LDX #$1a
0813 : a0 8c __ LDY #$8c
0815 : a9 00 __ LDA #$00
0817 : 85 19 __ STA IP + 0 
0819 : 86 1a __ STX IP + 1 
081b : e0 1a __ CPX #$1a
081d : f0 0b __ BEQ $082a ; (startup + 41)
081f : 91 19 __ STA (IP + 0),y 
0821 : c8 __ __ INY
0822 : d0 fb __ BNE $081f ; (startup + 30)
0824 : e8 __ __ INX
0825 : d0 f2 __ BNE $0819 ; (startup + 24)
0827 : 91 19 __ STA (IP + 0),y 
0829 : c8 __ __ INY
082a : c0 8c __ CPY #$8c
082c : d0 f9 __ BNE $0827 ; (startup + 38)
082e : a9 00 __ LDA #$00
0830 : a2 f7 __ LDX #$f7
0832 : d0 03 __ BNE $0837 ; (startup + 54)
0834 : 95 00 __ STA $00,x 
0836 : e8 __ __ INX
0837 : e0 f7 __ CPX #$f7
0839 : d0 f9 __ BNE $0834 ; (startup + 51)
083b : a9 9c __ LDA #$9c
083d : 85 23 __ STA SP + 0 
083f : a9 9f __ LDA #$9f
0841 : 85 24 __ STA SP + 1 
0843 : 20 80 08 JSR $0880 ; (main.s4 + 0)
0846 : a9 4c __ LDA #$4c
0848 : 85 54 __ STA $54 
084a : a9 00 __ LDA #$00
084c : 85 13 __ STA P6 
084e : a9 19 __ LDA #$19
0850 : 85 16 __ STA P9 
0852 : 60 __ __ RTS
--------------------------------------------------------------------
main: ; main()->i16
;   3, "/data/data/com.termux/files/home/GIT/OrWin-ATMOS/Play/fprintc-oscar.c"
.s4:
0880 : a9 2a __ LDA #$2a
0882 : 8d fe 9f STA $9ffe ; (sstack + 12)
0885 : a9 00 __ LDA #$00
0887 : 8d ff 9f STA $9fff ; (sstack + 13)
088a : ad 89 1a LDA $1a89 ; (stdout + 0)
088d : 8d fa 9f STA $9ffa ; (sstack + 8)
0890 : ad 8a 1a LDA $1a8a ; (stdout + 1)
0893 : 8d fb 9f STA $9ffb ; (sstack + 9)
0896 : a9 54 __ LDA #$54
0898 : 8d fc 9f STA $9ffc ; (sstack + 10)
089b : a9 15 __ LDA #$15
089d : 8d fd 9f STA $9ffd ; (sstack + 11)
08a0 : 20 aa 08 JSR $08aa ; (fprintf.s4 + 0)
08a3 : a9 00 __ LDA #$00
08a5 : 85 1b __ STA ACCU + 0 
08a7 : 85 1c __ STA ACCU + 1 
.s3:
08a9 : 60 __ __ RTS
--------------------------------------------------------------------
fprintf: ; fprintf(struct FILE*,const u8*)->i16
;  58, "/data/data/com.termux/files/home/GIT/oscar64/include/stdio.h"
.s4:
08aa : ad fa 9f LDA $9ffa ; (sstack + 8)
08ad : 85 43 __ STA T0 + 0 
08af : ad fb 9f LDA $9ffb ; (sstack + 9)
08b2 : 85 44 __ STA T0 + 1 
08b4 : a0 00 __ LDY #$00
08b6 : b1 43 __ LDA (T0 + 0),y 
08b8 : 30 06 __ BMI $08c0 ; (fprintf.s5 + 0)
.s6:
08ba : 20 f6 08 JSR $08f6 ; (krnio_chkout.s4 + 0)
08bd : aa __ __ TAX
08be : f0 2f __ BEQ $08ef ; (fprintf.s7 + 0)
.s5:
08c0 : a9 9e __ LDA #$9e
08c2 : 8d f3 9f STA $9ff3 ; (sstack + 1)
08c5 : a9 9f __ LDA #$9f
08c7 : 8d f4 9f STA $9ff4 ; (sstack + 2)
08ca : a9 01 __ LDA #$01
08cc : 8d f9 9f STA $9ff9 ; (sstack + 7)
08cf : ad fc 9f LDA $9ffc ; (sstack + 10)
08d2 : 8d f5 9f STA $9ff5 ; (sstack + 3)
08d5 : ad fd 9f LDA $9ffd ; (sstack + 11)
08d8 : 8d f6 9f STA $9ff6 ; (sstack + 4)
08db : a9 fe __ LDA #$fe
08dd : 8d f7 9f STA $9ff7 ; (sstack + 5)
08e0 : a9 9f __ LDA #$9f
08e2 : 8d f8 9f STA $9ff8 ; (sstack + 6)
08e5 : 20 0b 09 JSR $090b ; (sformat.s1 + 0)
08e8 : 20 50 15 JSR $1550 ; (krnio_clrchn.s4 + 0)
08eb : a9 00 __ LDA #$00
08ed : f0 02 __ BEQ $08f1 ; (fprintf.s3 + 0)
.s7:
08ef : a9 ff __ LDA #$ff
.s3:
08f1 : 85 1b __ STA ACCU + 0 
08f3 : 85 1c __ STA ACCU + 1 
08f5 : 60 __ __ RTS
--------------------------------------------------------------------
krnio_chkout: ; krnio_chkout(u8)->bool
;  51, "/data/data/com.termux/files/home/GIT/oscar64/include/c64/kernalio.h"
.s4:
08f6 : 85 0d __ STA P0 
08f8 : a6 0d __ LDX P0 
08fa : 20 c9 ff JSR $ffc9 
08fd : a9 00 __ LDA #$00
08ff : 2a __ __ ROL
0900 : 49 01 __ EOR #$01
0902 : 85 1b __ STA ACCU + 0 
0904 : a5 1b __ LDA ACCU + 0 
0906 : f0 02 __ BEQ $090a ; (krnio_chkout.s3 + 0)
.s5:
0908 : a9 01 __ LDA #$01
.s3:
090a : 60 __ __ RTS
--------------------------------------------------------------------
sformat: ; sformat(u8*,const u8*,i16*,bool)->u8*
; 351, "/data/data/com.termux/files/home/GIT/oscar64/include/stdio.c"
.s1:
090b : a2 09 __ LDX #$09
090d : b5 53 __ LDA T1 + 0,x 
090f : 9d d0 9f STA $9fd0,x ; (sformat@stack + 0)
0912 : ca __ __ DEX
0913 : 10 f8 __ BPL $090d ; (sformat.s1 + 2)
.s4:
0915 : ad f5 9f LDA $9ff5 ; (sstack + 3)
0918 : 85 55 __ STA T3 + 0 
091a : a9 00 __ LDA #$00
091c : 85 5b __ STA T6 + 0 
091e : ad f6 9f LDA $9ff6 ; (sstack + 4)
0921 : 85 56 __ STA T3 + 1 
0923 : ad f3 9f LDA $9ff3 ; (sstack + 1)
0926 : 85 57 __ STA T4 + 0 
0928 : ad f4 9f LDA $9ff4 ; (sstack + 2)
092b : 85 58 __ STA T4 + 1 
.l5:
092d : a0 00 __ LDY #$00
092f : b1 55 __ LDA (T3 + 0),y 
0931 : d0 35 __ BNE $0968 ; (sformat.s6 + 0)
.s87:
0933 : a4 5b __ LDY T6 + 0 
0935 : 91 57 __ STA (T4 + 0),y 
0937 : f0 28 __ BEQ $0961 ; (sformat.s93 + 0)
.s88:
0939 : ad f9 9f LDA $9ff9 ; (sstack + 7)
093c : d0 18 __ BNE $0956 ; (sformat.s89 + 0)
.s90:
093e : 98 __ __ TYA
093f : 18 __ __ CLC
0940 : 65 57 __ ADC T4 + 0 
0942 : aa __ __ TAX
0943 : a5 58 __ LDA T4 + 1 
0945 : 69 00 __ ADC #$00
.s3:
0947 : 86 1b __ STX ACCU + 0 ; (buff + 1)
0949 : 85 1c __ STA ACCU + 1 ; (fmt + 0)
094b : a2 09 __ LDX #$09
094d : bd d0 9f LDA $9fd0,x ; (sformat@stack + 0)
0950 : 95 53 __ STA T1 + 0,x 
0952 : ca __ __ DEX
0953 : 10 f8 __ BPL $094d ; (sformat.s3 + 6)
0955 : 60 __ __ RTS
.s89:
0956 : a5 57 __ LDA T4 + 0 
0958 : 85 0e __ STA P1 
095a : a5 58 __ LDA T4 + 1 
095c : 85 0f __ STA P2 
095e : 20 4f 0d JSR $0d4f ; (puts.l4 + 0)
.s93:
0961 : a5 58 __ LDA T4 + 1 
0963 : a6 57 __ LDX T4 + 0 
0965 : 4c 47 09 JMP $0947 ; (sformat.s3 + 0)
.s6:
0968 : c9 25 __ CMP #$25
096a : f0 3e __ BEQ $09aa ; (sformat.s7 + 0)
.s83:
096c : a4 5b __ LDY T6 + 0 
096e : 91 57 __ STA (T4 + 0),y 
0970 : e6 55 __ INC T3 + 0 
0972 : d0 02 __ BNE $0976 ; (sformat.s115 + 0)
.s114:
0974 : e6 56 __ INC T3 + 1 
.s115:
0976 : c8 __ __ INY
0977 : 84 5b __ STY T6 + 0 
0979 : 98 __ __ TYA
097a : c0 28 __ CPY #$28
097c : 90 af __ BCC $092d ; (sformat.l5 + 0)
.s84:
097e : 85 43 __ STA T0 + 0 
0980 : a9 00 __ LDA #$00
0982 : 85 5b __ STA T6 + 0 
0984 : ad f9 9f LDA $9ff9 ; (sstack + 7)
0987 : f0 14 __ BEQ $099d ; (sformat.s86 + 0)
.s85:
0989 : a5 57 __ LDA T4 + 0 
098b : 85 0e __ STA P1 
098d : a5 58 __ LDA T4 + 1 
098f : 85 0f __ STA P2 
0991 : a9 00 __ LDA #$00
0993 : a4 43 __ LDY T0 + 0 
0995 : 91 0e __ STA (P1),y 
0997 : 20 4f 0d JSR $0d4f ; (puts.l4 + 0)
099a : 4c 2d 09 JMP $092d ; (sformat.l5 + 0)
.s86:
099d : 18 __ __ CLC
099e : a5 57 __ LDA T4 + 0 
09a0 : 65 43 __ ADC T0 + 0 
09a2 : 85 57 __ STA T4 + 0 
09a4 : 90 87 __ BCC $092d ; (sformat.l5 + 0)
.s116:
09a6 : e6 58 __ INC T4 + 1 
09a8 : b0 83 __ BCS $092d ; (sformat.l5 + 0)
.s7:
09aa : a5 5b __ LDA T6 + 0 
09ac : f0 27 __ BEQ $09d5 ; (sformat.s10 + 0)
.s8:
09ae : 84 5b __ STY T6 + 0 
09b0 : 85 43 __ STA T0 + 0 
09b2 : ad f9 9f LDA $9ff9 ; (sstack + 7)
09b5 : f0 13 __ BEQ $09ca ; (sformat.s82 + 0)
.s9:
09b7 : a5 57 __ LDA T4 + 0 
09b9 : 85 0e __ STA P1 
09bb : a5 58 __ LDA T4 + 1 
09bd : 85 0f __ STA P2 
09bf : 98 __ __ TYA
09c0 : a4 43 __ LDY T0 + 0 
09c2 : 91 0e __ STA (P1),y 
09c4 : 20 4f 0d JSR $0d4f ; (puts.l4 + 0)
09c7 : 4c d5 09 JMP $09d5 ; (sformat.s10 + 0)
.s82:
09ca : 18 __ __ CLC
09cb : a5 57 __ LDA T4 + 0 
09cd : 65 43 __ ADC T0 + 0 
09cf : 85 57 __ STA T4 + 0 
09d1 : 90 02 __ BCC $09d5 ; (sformat.s10 + 0)
.s113:
09d3 : e6 58 __ INC T4 + 1 
.s10:
09d5 : a9 00 __ LDA #$00
09d7 : 8d df 9f STA $9fdf ; (si.sign + 0)
09da : 8d e0 9f STA $9fe0 ; (si.left + 0)
09dd : 8d e1 9f STA $9fe1 ; (si.prefix + 0)
09e0 : a0 01 __ LDY #$01
09e2 : b1 55 __ LDA (T3 + 0),y 
09e4 : a2 20 __ LDX #$20
09e6 : 8e da 9f STX $9fda ; (si.fill + 0)
09e9 : a2 00 __ LDX #$00
09eb : 8e db 9f STX $9fdb ; (si.width + 0)
09ee : ca __ __ DEX
09ef : 8e dc 9f STX $9fdc ; (si.precision + 0)
09f2 : a2 0a __ LDX #$0a
09f4 : 8e de 9f STX $9fde ; (si.base + 0)
09f7 : aa __ __ TAX
09f8 : a9 02 __ LDA #$02
09fa : d0 07 __ BNE $0a03 ; (sformat.l11 + 0)
.s13:
09fc : a0 00 __ LDY #$00
09fe : b1 55 __ LDA (T3 + 0),y 
0a00 : aa __ __ TAX
0a01 : a9 01 __ LDA #$01
.l11:
0a03 : 18 __ __ CLC
0a04 : 65 55 __ ADC T3 + 0 
0a06 : 85 55 __ STA T3 + 0 
0a08 : 90 02 __ BCC $0a0c ; (sformat.s104 + 0)
.s103:
0a0a : e6 56 __ INC T3 + 1 
.s104:
0a0c : e0 2b __ CPX #$2b
0a0e : d0 07 __ BNE $0a17 ; (sformat.s14 + 0)
.s12:
0a10 : a9 01 __ LDA #$01
0a12 : 8d df 9f STA $9fdf ; (si.sign + 0)
0a15 : d0 e5 __ BNE $09fc ; (sformat.s13 + 0)
.s14:
0a17 : 8a __ __ TXA
0a18 : e0 30 __ CPX #$30
0a1a : d0 05 __ BNE $0a21 ; (sformat.s16 + 0)
.s15:
0a1c : 8d da 9f STA $9fda ; (si.fill + 0)
0a1f : f0 db __ BEQ $09fc ; (sformat.s13 + 0)
.s16:
0a21 : c9 23 __ CMP #$23
0a23 : d0 07 __ BNE $0a2c ; (sformat.s18 + 0)
.s17:
0a25 : a9 01 __ LDA #$01
0a27 : 8d e1 9f STA $9fe1 ; (si.prefix + 0)
0a2a : d0 d0 __ BNE $09fc ; (sformat.s13 + 0)
.s18:
0a2c : c9 2d __ CMP #$2d
0a2e : d0 07 __ BNE $0a37 ; (sformat.s20 + 0)
.s19:
0a30 : a9 01 __ LDA #$01
0a32 : 8d e0 9f STA $9fe0 ; (si.left + 0)
0a35 : d0 c5 __ BNE $09fc ; (sformat.s13 + 0)
.s20:
0a37 : 85 47 __ STA T2 + 0 
0a39 : c9 30 __ CMP #$30
0a3b : 90 4a __ BCC $0a87 ; (sformat.s25 + 0)
.s21:
0a3d : e0 3a __ CPX #$3a
0a3f : 90 03 __ BCC $0a44 ; (sformat.s22 + 0)
0a41 : 4c c5 0a JMP $0ac5 ; (sformat.s30 + 0)
.s22:
0a44 : a9 00 __ LDA #$00
0a46 : e0 3a __ CPX #$3a
0a48 : a6 1c __ LDX ACCU + 1 ; (fmt + 0)
0a4a : b0 34 __ BCS $0a80 ; (sformat.s102 + 0)
.s117:
0a4c : 85 43 __ STA T0 + 0 
0a4e : a4 55 __ LDY T3 + 0 
0a50 : 85 55 __ STA T3 + 0 
.l24:
0a52 : a5 43 __ LDA T0 + 0 
0a54 : 0a __ __ ASL
0a55 : 85 1b __ STA ACCU + 0 ; (buff + 1)
0a57 : a9 00 __ LDA #$00
0a59 : 2a __ __ ROL
0a5a : 06 1b __ ASL ACCU + 0 ; (buff + 1)
0a5c : 2a __ __ ROL
0a5d : aa __ __ TAX
0a5e : a5 1b __ LDA ACCU + 0 ; (buff + 1)
0a60 : 65 43 __ ADC T0 + 0 
0a62 : 0a __ __ ASL
0a63 : 18 __ __ CLC
0a64 : 65 47 __ ADC T2 + 0 
0a66 : 38 __ __ SEC
0a67 : e9 30 __ SBC #$30
0a69 : 85 43 __ STA T0 + 0 
0a6b : b1 55 __ LDA (T3 + 0),y 
0a6d : 85 47 __ STA T2 + 0 
0a6f : c8 __ __ INY
0a70 : d0 02 __ BNE $0a74 ; (sformat.s112 + 0)
.s111:
0a72 : e6 56 __ INC T3 + 1 
.s112:
0a74 : c9 30 __ CMP #$30
0a76 : 90 04 __ BCC $0a7c ; (sformat.s118 + 0)
.s23:
0a78 : c9 3a __ CMP #$3a
0a7a : 90 d6 __ BCC $0a52 ; (sformat.l24 + 0)
.s118:
0a7c : 84 55 __ STY T3 + 0 
0a7e : a5 43 __ LDA T0 + 0 
.s102:
0a80 : 86 1c __ STX ACCU + 1 ; (fmt + 0)
0a82 : 8d db 9f STA $9fdb ; (si.width + 0)
0a85 : a5 47 __ LDA T2 + 0 
.s25:
0a87 : c9 2e __ CMP #$2e
0a89 : d0 3a __ BNE $0ac5 ; (sformat.s30 + 0)
.s26:
0a8b : a9 00 __ LDA #$00
0a8d : a8 __ __ TAY
0a8e : a6 1c __ LDX ACCU + 1 ; (fmt + 0)
0a90 : 4c aa 0a JMP $0aaa ; (sformat.l27 + 0)
.s29:
0a93 : a5 43 __ LDA T0 + 0 
0a95 : 0a __ __ ASL
0a96 : 85 1b __ STA ACCU + 0 ; (buff + 1)
0a98 : 98 __ __ TYA
0a99 : 2a __ __ ROL
0a9a : 06 1b __ ASL ACCU + 0 ; (buff + 1)
0a9c : 2a __ __ ROL
0a9d : aa __ __ TAX
0a9e : 18 __ __ CLC
0a9f : a5 1b __ LDA ACCU + 0 ; (buff + 1)
0aa1 : 65 43 __ ADC T0 + 0 
0aa3 : 0a __ __ ASL
0aa4 : 18 __ __ CLC
0aa5 : 65 47 __ ADC T2 + 0 
0aa7 : 38 __ __ SEC
0aa8 : e9 30 __ SBC #$30
.l27:
0aaa : 85 43 __ STA T0 + 0 
0aac : b1 55 __ LDA (T3 + 0),y 
0aae : 85 47 __ STA T2 + 0 
0ab0 : e6 55 __ INC T3 + 0 
0ab2 : d0 02 __ BNE $0ab6 ; (sformat.s106 + 0)
.s105:
0ab4 : e6 56 __ INC T3 + 1 
.s106:
0ab6 : c9 30 __ CMP #$30
0ab8 : 90 04 __ BCC $0abe ; (sformat.s101 + 0)
.s28:
0aba : c9 3a __ CMP #$3a
0abc : 90 d5 __ BCC $0a93 ; (sformat.s29 + 0)
.s101:
0abe : 86 1c __ STX ACCU + 1 ; (fmt + 0)
0ac0 : a6 43 __ LDX T0 + 0 
0ac2 : 8e dc 9f STX $9fdc ; (si.precision + 0)
.s30:
0ac5 : c9 64 __ CMP #$64
0ac7 : f0 0c __ BEQ $0ad5 ; (sformat.s31 + 0)
.s33:
0ac9 : c9 44 __ CMP #$44
0acb : f0 08 __ BEQ $0ad5 ; (sformat.s31 + 0)
.s34:
0acd : c9 69 __ CMP #$69
0acf : f0 04 __ BEQ $0ad5 ; (sformat.s31 + 0)
.s35:
0ad1 : c9 49 __ CMP #$49
0ad3 : d0 05 __ BNE $0ada ; (sformat.s36 + 0)
.s31:
0ad5 : a9 01 __ LDA #$01
0ad7 : 4c 13 0d JMP $0d13 ; (sformat.s32 + 0)
.s36:
0ada : c9 75 __ CMP #$75
0adc : d0 03 __ BNE $0ae1 ; (sformat.s37 + 0)
0ade : 4c 11 0d JMP $0d11 ; (sformat.s119 + 0)
.s37:
0ae1 : c9 55 __ CMP #$55
0ae3 : f0 f9 __ BEQ $0ade ; (sformat.s36 + 4)
.s38:
0ae5 : c9 78 __ CMP #$78
0ae7 : f0 04 __ BEQ $0aed ; (sformat.s39 + 0)
.s40:
0ae9 : c9 58 __ CMP #$58
0aeb : d0 0f __ BNE $0afc ; (sformat.s41 + 0)
.s39:
0aed : 29 e0 __ AND #$e0
0aef : 09 01 __ ORA #$01
0af1 : 8d dd 9f STA $9fdd ; (si.cha + 0)
0af4 : a9 10 __ LDA #$10
0af6 : 8d de 9f STA $9fde ; (si.base + 0)
0af9 : 4c 11 0d JMP $0d11 ; (sformat.s119 + 0)
.s41:
0afc : c9 6c __ CMP #$6c
0afe : d0 03 __ BNE $0b03 ; (sformat.s54 + 0)
0b00 : 4c 83 0c JMP $0c83 ; (sformat.s42 + 0)
.s54:
0b03 : c9 4c __ CMP #$4c
0b05 : f0 f9 __ BEQ $0b00 ; (sformat.s41 + 4)
.s55:
0b07 : c9 66 __ CMP #$66
0b09 : f0 14 __ BEQ $0b1f ; (sformat.s56 + 0)
.s57:
0b0b : c9 67 __ CMP #$67
0b0d : f0 10 __ BEQ $0b1f ; (sformat.s56 + 0)
.s58:
0b0f : c9 65 __ CMP #$65
0b11 : f0 0c __ BEQ $0b1f ; (sformat.s56 + 0)
.s59:
0b13 : c9 46 __ CMP #$46
0b15 : f0 08 __ BEQ $0b1f ; (sformat.s56 + 0)
.s60:
0b17 : c9 47 __ CMP #$47
0b19 : f0 04 __ BEQ $0b1f ; (sformat.s56 + 0)
.s61:
0b1b : c9 45 __ CMP #$45
0b1d : d0 5c __ BNE $0b7b ; (sformat.s62 + 0)
.s56:
0b1f : a5 47 __ LDA T2 + 0 
0b21 : 29 e0 __ AND #$e0
0b23 : 09 01 __ ORA #$01
0b25 : 8d dd 9f STA $9fdd ; (si.cha + 0)
0b28 : ad f7 9f LDA $9ff7 ; (sstack + 5)
0b2b : 85 59 __ STA T5 + 0 
0b2d : ad f8 9f LDA $9ff8 ; (sstack + 6)
0b30 : 85 5a __ STA T5 + 1 
0b32 : a5 57 __ LDA T4 + 0 
0b34 : 85 13 __ STA P6 
0b36 : a5 58 __ LDA T4 + 1 
0b38 : 85 14 __ STA P7 
0b3a : a9 da __ LDA #$da
0b3c : 85 11 __ STA P4 
0b3e : a9 9f __ LDA #$9f
0b40 : 85 12 __ STA P5 
0b42 : a0 00 __ LDY #$00
0b44 : b1 59 __ LDA (T5 + 0),y 
0b46 : 85 15 __ STA P8 
0b48 : c8 __ __ INY
0b49 : b1 59 __ LDA (T5 + 0),y 
0b4b : 85 16 __ STA P9 
0b4d : c8 __ __ INY
0b4e : b1 59 __ LDA (T5 + 0),y 
0b50 : 85 17 __ STA P10 
0b52 : c8 __ __ INY
0b53 : b1 59 __ LDA (T5 + 0),y 
0b55 : 85 18 __ STA P11 
0b57 : a5 47 __ LDA T2 + 0 
0b59 : ed dd 9f SBC $9fdd ; (si.cha + 0)
0b5c : 18 __ __ CLC
0b5d : 69 61 __ ADC #$61
0b5f : 8d f2 9f STA $9ff2 ; (sstack + 0)
0b62 : 20 2b 10 JSR $102b ; (nformf.s1 + 0)
0b65 : a5 1b __ LDA ACCU + 0 ; (buff + 1)
0b67 : 85 5b __ STA T6 + 0 
0b69 : 18 __ __ CLC
0b6a : a5 59 __ LDA T5 + 0 
0b6c : 69 04 __ ADC #$04
0b6e : 8d f7 9f STA $9ff7 ; (sstack + 5)
0b71 : a5 5a __ LDA T5 + 1 
0b73 : 69 00 __ ADC #$00
0b75 : 8d f8 9f STA $9ff8 ; (sstack + 6)
0b78 : 4c 2d 09 JMP $092d ; (sformat.l5 + 0)
.s62:
0b7b : c9 73 __ CMP #$73
0b7d : f0 3b __ BEQ $0bba ; (sformat.s63 + 0)
.s75:
0b7f : c9 53 __ CMP #$53
0b81 : f0 37 __ BEQ $0bba ; (sformat.s63 + 0)
.s76:
0b83 : c9 63 __ CMP #$63
0b85 : f0 12 __ BEQ $0b99 ; (sformat.s77 + 0)
.s79:
0b87 : c9 43 __ CMP #$43
0b89 : f0 0e __ BEQ $0b99 ; (sformat.s77 + 0)
.s80:
0b8b : aa __ __ TAX
0b8c : f0 ea __ BEQ $0b78 ; (sformat.s56 + 89)
.s81:
0b8e : a0 00 __ LDY #$00
0b90 : 91 57 __ STA (T4 + 0),y 
.s78:
0b92 : a9 01 __ LDA #$01
.s94:
0b94 : 85 5b __ STA T6 + 0 
0b96 : 4c 2d 09 JMP $092d ; (sformat.l5 + 0)
.s77:
0b99 : ad f7 9f LDA $9ff7 ; (sstack + 5)
0b9c : 85 43 __ STA T0 + 0 
0b9e : ad f8 9f LDA $9ff8 ; (sstack + 6)
0ba1 : 85 44 __ STA T0 + 1 
0ba3 : a0 00 __ LDY #$00
0ba5 : b1 43 __ LDA (T0 + 0),y 
0ba7 : 91 57 __ STA (T4 + 0),y 
0ba9 : a5 43 __ LDA T0 + 0 
0bab : 69 01 __ ADC #$01
0bad : 8d f7 9f STA $9ff7 ; (sstack + 5)
0bb0 : a5 44 __ LDA T0 + 1 
0bb2 : 69 00 __ ADC #$00
0bb4 : 8d f8 9f STA $9ff8 ; (sstack + 6)
0bb7 : 4c 92 0b JMP $0b92 ; (sformat.s78 + 0)
.s63:
0bba : ad f7 9f LDA $9ff7 ; (sstack + 5)
0bbd : 85 43 __ STA T0 + 0 
0bbf : 69 01 __ ADC #$01
0bc1 : 8d f7 9f STA $9ff7 ; (sstack + 5)
0bc4 : ad f8 9f LDA $9ff8 ; (sstack + 6)
0bc7 : 85 44 __ STA T0 + 1 
0bc9 : 69 00 __ ADC #$00
0bcb : 8d f8 9f STA $9ff8 ; (sstack + 6)
0bce : a0 00 __ LDY #$00
0bd0 : 84 5c __ STY T7 + 0 
0bd2 : b1 43 __ LDA (T0 + 0),y 
0bd4 : 85 1b __ STA ACCU + 0 ; (buff + 1)
0bd6 : 85 53 __ STA T1 + 0 
0bd8 : c8 __ __ INY
0bd9 : b1 43 __ LDA (T0 + 0),y 
0bdb : 85 1c __ STA ACCU + 1 ; (fmt + 0)
0bdd : 85 54 __ STA T1 + 1 
0bdf : ad db 9f LDA $9fdb ; (si.width + 0)
0be2 : f0 0a __ BEQ $0bee ; (sformat.s65 + 0)
.s98:
0be4 : 88 __ __ DEY
0be5 : f0 01 __ BEQ $0be8 ; (sformat.l120 + 0)
.s64:
0be7 : c8 __ __ INY
.l120:
0be8 : b1 1b __ LDA (ACCU + 0),y ; (buff + 1)
0bea : d0 fb __ BNE $0be7 ; (sformat.s64 + 0)
.s99:
0bec : 84 5c __ STY T7 + 0 
.s65:
0bee : ad e0 9f LDA $9fe0 ; (si.left + 0)
0bf1 : 85 59 __ STA T5 + 0 
0bf3 : d0 19 __ BNE $0c0e ; (sformat.s66 + 0)
.s96:
0bf5 : a6 5c __ LDX T7 + 0 
0bf7 : ec db 9f CPX $9fdb ; (si.width + 0)
0bfa : a0 00 __ LDY #$00
0bfc : b0 0c __ BCS $0c0a ; (sformat.s97 + 0)
.l74:
0bfe : ad da 9f LDA $9fda ; (si.fill + 0)
0c01 : 91 57 __ STA (T4 + 0),y 
0c03 : c8 __ __ INY
0c04 : e8 __ __ INX
0c05 : ec db 9f CPX $9fdb ; (si.width + 0)
0c08 : 90 f4 __ BCC $0bfe ; (sformat.l74 + 0)
.s97:
0c0a : 86 5c __ STX T7 + 0 
0c0c : 84 5b __ STY T6 + 0 
.s66:
0c0e : ac f9 9f LDY $9ff9 ; (sstack + 7)
0c11 : d0 48 __ BNE $0c5b ; (sformat.s67 + 0)
.s71:
0c13 : b1 1b __ LDA (ACCU + 0),y ; (buff + 1)
0c15 : f0 23 __ BEQ $0c3a ; (sformat.s73 + 0)
.s72:
0c17 : 18 __ __ CLC
0c18 : a5 1b __ LDA ACCU + 0 ; (buff + 1)
0c1a : 69 01 __ ADC #$01
0c1c : 85 43 __ STA T0 + 0 
0c1e : a5 1c __ LDA ACCU + 1 ; (fmt + 0)
0c20 : 69 00 __ ADC #$00
0c22 : 85 44 __ STA T0 + 1 
0c24 : b1 1b __ LDA (ACCU + 0),y ; (buff + 1)
.l91:
0c26 : a4 5b __ LDY T6 + 0 
0c28 : 91 57 __ STA (T4 + 0),y 
0c2a : a0 00 __ LDY #$00
0c2c : b1 43 __ LDA (T0 + 0),y 
0c2e : a8 __ __ TAY
0c2f : e6 43 __ INC T0 + 0 
0c31 : d0 02 __ BNE $0c35 ; (sformat.s110 + 0)
.s109:
0c33 : e6 44 __ INC T0 + 1 
.s110:
0c35 : e6 5b __ INC T6 + 0 
0c37 : 98 __ __ TYA
0c38 : d0 ec __ BNE $0c26 ; (sformat.l91 + 0)
.s73:
0c3a : a5 59 __ LDA T5 + 0 
0c3c : d0 03 __ BNE $0c41 ; (sformat.s95 + 0)
0c3e : 4c 2d 09 JMP $092d ; (sformat.l5 + 0)
.s95:
0c41 : a6 5c __ LDX T7 + 0 
0c43 : ec db 9f CPX $9fdb ; (si.width + 0)
0c46 : a4 5b __ LDY T6 + 0 
0c48 : b0 0c __ BCS $0c56 ; (sformat.s100 + 0)
.l70:
0c4a : ad da 9f LDA $9fda ; (si.fill + 0)
0c4d : 91 57 __ STA (T4 + 0),y 
0c4f : c8 __ __ INY
0c50 : e8 __ __ INX
0c51 : ec db 9f CPX $9fdb ; (si.width + 0)
0c54 : 90 f4 __ BCC $0c4a ; (sformat.l70 + 0)
.s100:
0c56 : 84 5b __ STY T6 + 0 
0c58 : 4c 2d 09 JMP $092d ; (sformat.l5 + 0)
.s67:
0c5b : a4 5b __ LDY T6 + 0 
0c5d : f0 11 __ BEQ $0c70 ; (sformat.s69 + 0)
.s68:
0c5f : a5 57 __ LDA T4 + 0 
0c61 : 85 0e __ STA P1 
0c63 : a5 58 __ LDA T4 + 1 
0c65 : 85 0f __ STA P2 
0c67 : a9 00 __ LDA #$00
0c69 : 85 5b __ STA T6 + 0 
0c6b : 91 0e __ STA (P1),y 
0c6d : 20 4f 0d JSR $0d4f ; (puts.l4 + 0)
.s69:
0c70 : a5 53 __ LDA T1 + 0 
0c72 : 85 0e __ STA P1 
0c74 : a5 54 __ LDA T1 + 1 
0c76 : 85 0f __ STA P2 
0c78 : 20 4f 0d JSR $0d4f ; (puts.l4 + 0)
0c7b : ad e0 9f LDA $9fe0 ; (si.left + 0)
0c7e : d0 c1 __ BNE $0c41 ; (sformat.s95 + 0)
0c80 : 4c 2d 09 JMP $092d ; (sformat.l5 + 0)
.s42:
0c83 : ad f7 9f LDA $9ff7 ; (sstack + 5)
0c86 : 85 43 __ STA T0 + 0 
0c88 : 69 03 __ ADC #$03
0c8a : 8d f7 9f STA $9ff7 ; (sstack + 5)
0c8d : ad f8 9f LDA $9ff8 ; (sstack + 6)
0c90 : 85 44 __ STA T0 + 1 
0c92 : 69 00 __ ADC #$00
0c94 : 8d f8 9f STA $9ff8 ; (sstack + 6)
0c97 : a0 00 __ LDY #$00
0c99 : b1 55 __ LDA (T3 + 0),y 
0c9b : aa __ __ TAX
0c9c : e6 55 __ INC T3 + 0 
0c9e : d0 02 __ BNE $0ca2 ; (sformat.s108 + 0)
.s107:
0ca0 : e6 56 __ INC T3 + 1 
.s108:
0ca2 : b1 43 __ LDA (T0 + 0),y 
0ca4 : 85 1b __ STA ACCU + 0 ; (buff + 1)
0ca6 : 85 11 __ STA P4 
0ca8 : a0 01 __ LDY #$01
0caa : b1 43 __ LDA (T0 + 0),y 
0cac : 85 1c __ STA ACCU + 1 ; (fmt + 0)
0cae : 85 12 __ STA P5 
0cb0 : c8 __ __ INY
0cb1 : b1 43 __ LDA (T0 + 0),y 
0cb3 : 85 1d __ STA ACCU + 2 ; (fmt + 1)
0cb5 : 85 13 __ STA P6 
0cb7 : c8 __ __ INY
0cb8 : b1 43 __ LDA (T0 + 0),y 
0cba : 85 14 __ STA P7 
0cbc : e0 64 __ CPX #$64
0cbe : f0 0c __ BEQ $0ccc ; (sformat.s43 + 0)
.s45:
0cc0 : e0 44 __ CPX #$44
0cc2 : f0 08 __ BEQ $0ccc ; (sformat.s43 + 0)
.s46:
0cc4 : e0 69 __ CPX #$69
0cc6 : f0 04 __ BEQ $0ccc ; (sformat.s43 + 0)
.s47:
0cc8 : e0 49 __ CPX #$49
0cca : d0 1c __ BNE $0ce8 ; (sformat.s48 + 0)
.s43:
0ccc : a9 01 __ LDA #$01
.s92:
0cce : 85 15 __ STA P8 
.s44:
0cd0 : a5 57 __ LDA T4 + 0 
0cd2 : 85 0f __ STA P2 
0cd4 : a5 58 __ LDA T4 + 1 
0cd6 : 85 10 __ STA P3 
0cd8 : a9 da __ LDA #$da
0cda : 85 0d __ STA P0 
0cdc : a9 9f __ LDA #$9f
0cde : 85 0e __ STA P1 
0ce0 : 20 e1 0e JSR $0ee1 ; (nforml.s4 + 0)
0ce3 : a5 1b __ LDA ACCU + 0 ; (buff + 1)
0ce5 : 4c 94 0b JMP $0b94 ; (sformat.s94 + 0)
.s48:
0ce8 : e0 75 __ CPX #$75
0cea : f0 04 __ BEQ $0cf0 ; (sformat.s49 + 0)
.s50:
0cec : e0 55 __ CPX #$55
0cee : d0 04 __ BNE $0cf4 ; (sformat.s51 + 0)
.s49:
0cf0 : a9 00 __ LDA #$00
0cf2 : f0 da __ BEQ $0cce ; (sformat.s92 + 0)
.s51:
0cf4 : e0 78 __ CPX #$78
0cf6 : f0 06 __ BEQ $0cfe ; (sformat.s52 + 0)
.s53:
0cf8 : 85 1e __ STA ACCU + 3 ; (fps + 0)
0cfa : e0 58 __ CPX #$58
0cfc : d0 82 __ BNE $0c80 ; (sformat.s69 + 16)
.s52:
0cfe : a9 10 __ LDA #$10
0d00 : 8d de 9f STA $9fde ; (si.base + 0)
0d03 : 8a __ __ TXA
0d04 : 29 e0 __ AND #$e0
0d06 : 09 01 __ ORA #$01
0d08 : 8d dd 9f STA $9fdd ; (si.cha + 0)
0d0b : a9 00 __ LDA #$00
0d0d : 85 15 __ STA P8 
0d0f : f0 bf __ BEQ $0cd0 ; (sformat.s44 + 0)
.s119:
0d11 : a9 00 __ LDA #$00
.s32:
0d13 : 85 13 __ STA P6 
0d15 : ad f7 9f LDA $9ff7 ; (sstack + 5)
0d18 : 85 43 __ STA T0 + 0 
0d1a : ad f8 9f LDA $9ff8 ; (sstack + 6)
0d1d : 85 44 __ STA T0 + 1 
0d1f : a5 57 __ LDA T4 + 0 
0d21 : 85 0f __ STA P2 
0d23 : a5 58 __ LDA T4 + 1 
0d25 : 85 10 __ STA P3 
0d27 : a0 00 __ LDY #$00
0d29 : b1 43 __ LDA (T0 + 0),y 
0d2b : 85 11 __ STA P4 
0d2d : c8 __ __ INY
0d2e : b1 43 __ LDA (T0 + 0),y 
0d30 : 85 12 __ STA P5 
0d32 : 18 __ __ CLC
0d33 : a5 43 __ LDA T0 + 0 
0d35 : 69 02 __ ADC #$02
0d37 : 8d f7 9f STA $9ff7 ; (sstack + 5)
0d3a : a5 44 __ LDA T0 + 1 
0d3c : 69 00 __ ADC #$00
0d3e : 8d f8 9f STA $9ff8 ; (sstack + 6)
0d41 : a9 da __ LDA #$da
0d43 : 85 0d __ STA P0 
0d45 : a9 9f __ LDA #$9f
0d47 : 85 0e __ STA P1 
0d49 : 20 cc 0d JSR $0dcc ; (nformi.s4 + 0)
0d4c : 4c 94 0b JMP $0b94 ; (sformat.s94 + 0)
--------------------------------------------------------------------
puts: ; puts(const u8*)->void
;  12, "/data/data/com.termux/files/home/GIT/oscar64/include/stdio.h"
.l4:
0d4f : a0 00 __ LDY #$00
0d51 : b1 0e __ LDA (P1),y ; (str + 0)
0d53 : aa __ __ TAX
0d54 : 18 __ __ CLC
0d55 : a5 0e __ LDA P1 ; (str + 0)
0d57 : 69 01 __ ADC #$01
0d59 : 85 0e __ STA P1 ; (str + 0)
0d5b : 8a __ __ TXA
0d5c : d0 01 __ BNE $0d5f ; (puts.s5 + 0)
.s3:
0d5e : 60 __ __ RTS
.s5:
0d5f : 90 02 __ BCC $0d63 ; (puts.s7 + 0)
.s6:
0d61 : e6 0f __ INC P2 ; (str + 1)
.s7:
0d63 : 20 69 0d JSR $0d69 ; (putpch.s4 + 0)
0d66 : 4c 4f 0d JMP $0d4f ; (puts.l4 + 0)
--------------------------------------------------------------------
putpch: ; putpch(u8)->void
;  69, "/data/data/com.termux/files/home/GIT/oscar64/include/conio.h"
.s4:
0d69 : 85 0d __ STA P0 ; (c + 0)
0d6b : ad 6c 1a LDA $1a6c ; (giocharmap + 0)
0d6e : f0 32 __ BEQ $0da2 ; (putpch.s17 + 0)
.s5:
0d70 : a5 0d __ LDA P0 ; (c + 0)
0d72 : c9 0a __ CMP #$0a
0d74 : d0 04 __ BNE $0d7a ; (putpch.s8 + 0)
.s6:
0d76 : a9 0d __ LDA #$0d
0d78 : d0 32 __ BNE $0dac ; (putpch.s7 + 0)
.s8:
0d7a : c9 09 __ CMP #$09
0d7c : f0 36 __ BEQ $0db4 ; (putpch.s9 + 0)
.s11:
0d7e : ad 6c 1a LDA $1a6c ; (giocharmap + 0)
0d81 : c9 02 __ CMP #$02
0d83 : 90 1d __ BCC $0da2 ; (putpch.s17 + 0)
.s12:
0d85 : a5 0d __ LDA P0 ; (c + 0)
0d87 : c9 41 __ CMP #$41
0d89 : 90 17 __ BCC $0da2 ; (putpch.s17 + 0)
.s13:
0d8b : c9 7b __ CMP #$7b
0d8d : b0 13 __ BCS $0da2 ; (putpch.s17 + 0)
.s14:
0d8f : c9 61 __ CMP #$61
0d91 : b0 04 __ BCS $0d97 ; (putpch.s15 + 0)
.s18:
0d93 : c9 5b __ CMP #$5b
0d95 : b0 0b __ BCS $0da2 ; (putpch.s17 + 0)
.s15:
0d97 : 49 20 __ EOR #$20
0d99 : 85 0d __ STA P0 ; (c + 0)
0d9b : ad 6c 1a LDA $1a6c ; (giocharmap + 0)
0d9e : c9 02 __ CMP #$02
0da0 : f0 06 __ BEQ $0da8 ; (putpch.s16 + 0)
.s17:
0da2 : a5 0d __ LDA P0 ; (c + 0)
0da4 : 20 d2 ff JSR $ffd2 
.s3:
0da7 : 60 __ __ RTS
.s16:
0da8 : a5 0d __ LDA P0 ; (c + 0)
0daa : 29 5f __ AND #$5f
.s7:
0dac : 85 43 __ STA T0 + 0 
0dae : a5 43 __ LDA T0 + 0 
0db0 : 20 d2 ff JSR $ffd2 
0db3 : 60 __ __ RTS
.s9:
0db4 : a5 d3 __ LDA $d3 
0db6 : 29 03 __ AND #$03
0db8 : 85 43 __ STA T0 + 0 
0dba : a9 20 __ LDA #$20
0dbc : 85 44 __ STA T1 + 0 
.l10:
0dbe : a5 44 __ LDA T1 + 0 
0dc0 : 20 d2 ff JSR $ffd2 
0dc3 : e6 43 __ INC T0 + 0 
0dc5 : a5 43 __ LDA T0 + 0 
0dc7 : c9 04 __ CMP #$04
0dc9 : 90 f3 __ BCC $0dbe ; (putpch.l10 + 0)
0dcb : 60 __ __ RTS
--------------------------------------------------------------------
nformi: ; nformi(const struct sinfo*,u8*,i16,bool)->u8
;  79, "/data/data/com.termux/files/home/GIT/oscar64/include/stdio.c"
.s4:
0dcc : a9 00 __ LDA #$00
0dce : 85 43 __ STA T5 + 0 
0dd0 : a0 04 __ LDY #$04
0dd2 : b1 0d __ LDA (P0),y ; (si + 0)
0dd4 : 85 44 __ STA T6 + 0 
0dd6 : a5 13 __ LDA P6 ; (s + 0)
0dd8 : f0 13 __ BEQ $0ded ; (nformi.s7 + 0)
.s5:
0dda : 24 12 __ BIT P5 ; (v + 1)
0ddc : 10 0f __ BPL $0ded ; (nformi.s7 + 0)
.s6:
0dde : 38 __ __ SEC
0ddf : a9 00 __ LDA #$00
0de1 : e5 11 __ SBC P4 ; (v + 0)
0de3 : 85 11 __ STA P4 ; (v + 0)
0de5 : a9 00 __ LDA #$00
0de7 : e5 12 __ SBC P5 ; (v + 1)
0de9 : 85 12 __ STA P5 ; (v + 1)
0deb : e6 43 __ INC T5 + 0 
.s7:
0ded : a9 10 __ LDA #$10
0def : 85 45 __ STA T7 + 0 
0df1 : a5 11 __ LDA P4 ; (v + 0)
0df3 : 05 12 __ ORA P5 ; (v + 1)
0df5 : f0 2d __ BEQ $0e24 ; (nformi.s12 + 0)
.s8:
0df7 : a5 11 __ LDA P4 ; (v + 0)
0df9 : 85 1b __ STA ACCU + 0 
0dfb : a5 12 __ LDA P5 ; (v + 1)
0dfd : 85 1c __ STA ACCU + 1 
.l9:
0dff : a5 44 __ LDA T6 + 0 
0e01 : 20 6e 18 JSR $186e ; (divmod + 53)
0e04 : a5 05 __ LDA WORK + 2 
0e06 : c9 0a __ CMP #$0a
0e08 : b0 04 __ BCS $0e0e ; (nformi.s10 + 0)
.s35:
0e0a : a9 30 __ LDA #$30
0e0c : 90 06 __ BCC $0e14 ; (nformi.s11 + 0)
.s10:
0e0e : a0 03 __ LDY #$03
0e10 : b1 0d __ LDA (P0),y ; (si + 0)
0e12 : e9 0a __ SBC #$0a
.s11:
0e14 : 18 __ __ CLC
0e15 : 65 05 __ ADC WORK + 2 
0e17 : a6 45 __ LDX T7 + 0 
0e19 : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0e1c : c6 45 __ DEC T7 + 0 
0e1e : a5 1b __ LDA ACCU + 0 
0e20 : 05 1c __ ORA ACCU + 1 
0e22 : d0 db __ BNE $0dff ; (nformi.l9 + 0)
.s12:
0e24 : a9 ff __ LDA #$ff
0e26 : a0 02 __ LDY #$02
0e28 : d1 0d __ CMP (P0),y ; (si + 0)
0e2a : d0 04 __ BNE $0e30 ; (nformi.s13 + 0)
.s34:
0e2c : a9 0f __ LDA #$0f
0e2e : d0 05 __ BNE $0e35 ; (nformi.s40 + 0)
.s13:
0e30 : 38 __ __ SEC
0e31 : a9 10 __ LDA #$10
0e33 : f1 0d __ SBC (P0),y ; (si + 0)
.s40:
0e35 : a8 __ __ TAY
0e36 : c4 45 __ CPY T7 + 0 
0e38 : b0 0d __ BCS $0e47 ; (nformi.s15 + 0)
.s14:
0e3a : a9 30 __ LDA #$30
.l41:
0e3c : a6 45 __ LDX T7 + 0 
0e3e : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0e41 : c6 45 __ DEC T7 + 0 
0e43 : c4 45 __ CPY T7 + 0 
0e45 : 90 f5 __ BCC $0e3c ; (nformi.l41 + 0)
.s15:
0e47 : a0 07 __ LDY #$07
0e49 : b1 0d __ LDA (P0),y ; (si + 0)
0e4b : f0 1c __ BEQ $0e69 ; (nformi.s18 + 0)
.s16:
0e4d : a5 44 __ LDA T6 + 0 
0e4f : c9 10 __ CMP #$10
0e51 : d0 16 __ BNE $0e69 ; (nformi.s18 + 0)
.s17:
0e53 : a0 03 __ LDY #$03
0e55 : b1 0d __ LDA (P0),y ; (si + 0)
0e57 : a8 __ __ TAY
0e58 : a9 30 __ LDA #$30
0e5a : a6 45 __ LDX T7 + 0 
0e5c : 9d e0 9f STA $9fe0,x ; (si.left + 0)
0e5f : 98 __ __ TYA
0e60 : 69 16 __ ADC #$16
0e62 : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0e65 : ca __ __ DEX
0e66 : ca __ __ DEX
0e67 : 86 45 __ STX T7 + 0 
.s18:
0e69 : a9 00 __ LDA #$00
0e6b : 85 1b __ STA ACCU + 0 
0e6d : a5 43 __ LDA T5 + 0 
0e6f : f0 0c __ BEQ $0e7d ; (nformi.s32 + 0)
.s19:
0e71 : a9 2d __ LDA #$2d
.s20:
0e73 : a6 45 __ LDX T7 + 0 
0e75 : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0e78 : c6 45 __ DEC T7 + 0 
0e7a : 4c 87 0e JMP $0e87 ; (nformi.s21 + 0)
.s32:
0e7d : a0 05 __ LDY #$05
0e7f : b1 0d __ LDA (P0),y ; (si + 0)
0e81 : f0 04 __ BEQ $0e87 ; (nformi.s21 + 0)
.s33:
0e83 : a9 2b __ LDA #$2b
0e85 : d0 ec __ BNE $0e73 ; (nformi.s20 + 0)
.s21:
0e87 : a0 06 __ LDY #$06
0e89 : a6 45 __ LDX T7 + 0 
0e8b : b1 0d __ LDA (P0),y ; (si + 0)
0e8d : d0 2b __ BNE $0eba ; (nformi.s22 + 0)
.l27:
0e8f : 8a __ __ TXA
0e90 : 18 __ __ CLC
0e91 : a0 01 __ LDY #$01
0e93 : 71 0d __ ADC (P0),y ; (si + 0)
0e95 : b0 04 __ BCS $0e9b ; (nformi.s28 + 0)
.s31:
0e97 : c9 11 __ CMP #$11
0e99 : 90 0a __ BCC $0ea5 ; (nformi.s29 + 0)
.s28:
0e9b : a0 00 __ LDY #$00
0e9d : b1 0d __ LDA (P0),y ; (si + 0)
0e9f : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0ea2 : ca __ __ DEX
0ea3 : b0 ea __ BCS $0e8f ; (nformi.l27 + 0)
.s29:
0ea5 : e0 10 __ CPX #$10
0ea7 : b0 0e __ BCS $0eb7 ; (nformi.s26 + 0)
.s30:
0ea9 : 88 __ __ DEY
.l38:
0eaa : bd e2 9f LDA $9fe2,x ; (buffer[0] + 0)
0ead : 91 0f __ STA (P2),y ; (str + 0)
0eaf : c8 __ __ INY
0eb0 : e8 __ __ INX
0eb1 : e0 10 __ CPX #$10
0eb3 : 90 f5 __ BCC $0eaa ; (nformi.l38 + 0)
.s39:
0eb5 : 84 1b __ STY ACCU + 0 
.s26:
0eb7 : a5 1b __ LDA ACCU + 0 
.s3:
0eb9 : 60 __ __ RTS
.s22:
0eba : e0 10 __ CPX #$10
0ebc : b0 1a __ BCS $0ed8 ; (nformi.l24 + 0)
.s23:
0ebe : a0 00 __ LDY #$00
.l36:
0ec0 : bd e2 9f LDA $9fe2,x ; (buffer[0] + 0)
0ec3 : 91 0f __ STA (P2),y ; (str + 0)
0ec5 : c8 __ __ INY
0ec6 : e8 __ __ INX
0ec7 : e0 10 __ CPX #$10
0ec9 : 90 f5 __ BCC $0ec0 ; (nformi.l36 + 0)
.s37:
0ecb : 84 1b __ STY ACCU + 0 
0ecd : b0 09 __ BCS $0ed8 ; (nformi.l24 + 0)
.s25:
0ecf : 88 __ __ DEY
0ed0 : b1 0d __ LDA (P0),y ; (si + 0)
0ed2 : a4 1b __ LDY ACCU + 0 
0ed4 : 91 0f __ STA (P2),y ; (str + 0)
0ed6 : e6 1b __ INC ACCU + 0 
.l24:
0ed8 : a5 1b __ LDA ACCU + 0 
0eda : a0 01 __ LDY #$01
0edc : d1 0d __ CMP (P0),y ; (si + 0)
0ede : 90 ef __ BCC $0ecf ; (nformi.s25 + 0)
0ee0 : 60 __ __ RTS
--------------------------------------------------------------------
nforml: ; nforml(const struct sinfo*,u8*,i32,bool)->u8
; 137, "/data/data/com.termux/files/home/GIT/oscar64/include/stdio.c"
.s4:
0ee1 : a9 00 __ LDA #$00
0ee3 : 85 43 __ STA T4 + 0 
0ee5 : a5 15 __ LDA P8 ; (s + 0)
0ee7 : f0 1f __ BEQ $0f08 ; (nforml.s7 + 0)
.s5:
0ee9 : 24 14 __ BIT P7 ; (v + 3)
0eeb : 10 1b __ BPL $0f08 ; (nforml.s7 + 0)
.s6:
0eed : 38 __ __ SEC
0eee : a9 00 __ LDA #$00
0ef0 : e5 11 __ SBC P4 ; (v + 0)
0ef2 : 85 11 __ STA P4 ; (v + 0)
0ef4 : a9 00 __ LDA #$00
0ef6 : e5 12 __ SBC P5 ; (v + 1)
0ef8 : 85 12 __ STA P5 ; (v + 1)
0efa : a9 00 __ LDA #$00
0efc : e5 13 __ SBC P6 ; (v + 2)
0efe : 85 13 __ STA P6 ; (v + 2)
0f00 : a9 00 __ LDA #$00
0f02 : e5 14 __ SBC P7 ; (v + 3)
0f04 : 85 14 __ STA P7 ; (v + 3)
0f06 : e6 43 __ INC T4 + 0 
.s7:
0f08 : a9 10 __ LDA #$10
0f0a : 85 44 __ STA T5 + 0 
0f0c : a5 14 __ LDA P7 ; (v + 3)
0f0e : d0 0c __ BNE $0f1c ; (nforml.l43 + 0)
.s44:
0f10 : a5 13 __ LDA P6 ; (v + 2)
0f12 : d0 08 __ BNE $0f1c ; (nforml.l43 + 0)
.s34:
0f14 : a5 12 __ LDA P5 ; (v + 1)
0f16 : d0 04 __ BNE $0f1c ; (nforml.l43 + 0)
.s35:
0f18 : c5 11 __ CMP P4 ; (v + 0)
0f1a : b0 13 __ BCS $0f2f ; (nforml.s11 + 0)
.l43:
0f1c : a5 11 __ LDA P4 ; (v + 0)
0f1e : 85 1b __ STA ACCU + 0 
0f20 : a5 12 __ LDA P5 ; (v + 1)
0f22 : 85 1c __ STA ACCU + 1 
0f24 : a5 13 __ LDA P6 ; (v + 2)
0f26 : 85 1d __ STA ACCU + 2 
0f28 : a5 14 __ LDA P7 ; (v + 3)
0f2a : 85 1e __ STA ACCU + 3 
0f2c : 4c eb 0f JMP $0feb ; (nforml.l8 + 0)
.s11:
0f2f : a9 ff __ LDA #$ff
0f31 : a0 02 __ LDY #$02
0f33 : d1 0d __ CMP (P0),y ; (si + 0)
0f35 : d0 04 __ BNE $0f3b ; (nforml.s12 + 0)
.s32:
0f37 : a9 0f __ LDA #$0f
0f39 : d0 05 __ BNE $0f40 ; (nforml.s41 + 0)
.s12:
0f3b : 38 __ __ SEC
0f3c : a9 10 __ LDA #$10
0f3e : f1 0d __ SBC (P0),y ; (si + 0)
.s41:
0f40 : a8 __ __ TAY
0f41 : c4 44 __ CPY T5 + 0 
0f43 : b0 0d __ BCS $0f52 ; (nforml.s14 + 0)
.s13:
0f45 : a9 30 __ LDA #$30
.l42:
0f47 : a6 44 __ LDX T5 + 0 
0f49 : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0f4c : c6 44 __ DEC T5 + 0 
0f4e : c4 44 __ CPY T5 + 0 
0f50 : 90 f5 __ BCC $0f47 ; (nforml.l42 + 0)
.s14:
0f52 : a0 07 __ LDY #$07
0f54 : b1 0d __ LDA (P0),y ; (si + 0)
0f56 : f0 1d __ BEQ $0f75 ; (nforml.s17 + 0)
.s15:
0f58 : a9 10 __ LDA #$10
0f5a : a0 04 __ LDY #$04
0f5c : d1 0d __ CMP (P0),y ; (si + 0)
0f5e : d0 15 __ BNE $0f75 ; (nforml.s17 + 0)
.s16:
0f60 : 88 __ __ DEY
0f61 : b1 0d __ LDA (P0),y ; (si + 0)
0f63 : a8 __ __ TAY
0f64 : a9 30 __ LDA #$30
0f66 : a6 44 __ LDX T5 + 0 
0f68 : 9d e0 9f STA $9fe0,x ; (si.left + 0)
0f6b : 98 __ __ TYA
0f6c : 69 16 __ ADC #$16
0f6e : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0f71 : ca __ __ DEX
0f72 : ca __ __ DEX
0f73 : 86 44 __ STX T5 + 0 
.s17:
0f75 : a9 00 __ LDA #$00
0f77 : 85 1b __ STA ACCU + 0 
0f79 : a5 43 __ LDA T4 + 0 
0f7b : f0 0c __ BEQ $0f89 ; (nforml.s30 + 0)
.s18:
0f7d : a9 2d __ LDA #$2d
.s19:
0f7f : a6 44 __ LDX T5 + 0 
0f81 : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0f84 : c6 44 __ DEC T5 + 0 
0f86 : 4c 93 0f JMP $0f93 ; (nforml.s20 + 0)
.s30:
0f89 : a0 05 __ LDY #$05
0f8b : b1 0d __ LDA (P0),y ; (si + 0)
0f8d : f0 04 __ BEQ $0f93 ; (nforml.s20 + 0)
.s31:
0f8f : a9 2b __ LDA #$2b
0f91 : d0 ec __ BNE $0f7f ; (nforml.s19 + 0)
.s20:
0f93 : a0 06 __ LDY #$06
0f95 : a6 44 __ LDX T5 + 0 
0f97 : b1 0d __ LDA (P0),y ; (si + 0)
0f99 : d0 29 __ BNE $0fc4 ; (nforml.s21 + 0)
.l25:
0f9b : 8a __ __ TXA
0f9c : 18 __ __ CLC
0f9d : a0 01 __ LDY #$01
0f9f : 71 0d __ ADC (P0),y ; (si + 0)
0fa1 : b0 04 __ BCS $0fa7 ; (nforml.s26 + 0)
.s29:
0fa3 : c9 11 __ CMP #$11
0fa5 : 90 0a __ BCC $0fb1 ; (nforml.s27 + 0)
.s26:
0fa7 : a0 00 __ LDY #$00
0fa9 : b1 0d __ LDA (P0),y ; (si + 0)
0fab : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
0fae : ca __ __ DEX
0faf : b0 ea __ BCS $0f9b ; (nforml.l25 + 0)
.s27:
0fb1 : e0 10 __ CPX #$10
0fb3 : b0 0e __ BCS $0fc3 ; (nforml.s3 + 0)
.s28:
0fb5 : 88 __ __ DEY
.l39:
0fb6 : bd e2 9f LDA $9fe2,x ; (buffer[0] + 0)
0fb9 : 91 0f __ STA (P2),y ; (str + 0)
0fbb : c8 __ __ INY
0fbc : e8 __ __ INX
0fbd : e0 10 __ CPX #$10
0fbf : 90 f5 __ BCC $0fb6 ; (nforml.l39 + 0)
.s40:
0fc1 : 84 1b __ STY ACCU + 0 
.s3:
0fc3 : 60 __ __ RTS
.s21:
0fc4 : e0 10 __ CPX #$10
0fc6 : b0 1a __ BCS $0fe2 ; (nforml.l23 + 0)
.s22:
0fc8 : a0 00 __ LDY #$00
.l37:
0fca : bd e2 9f LDA $9fe2,x ; (buffer[0] + 0)
0fcd : 91 0f __ STA (P2),y ; (str + 0)
0fcf : c8 __ __ INY
0fd0 : e8 __ __ INX
0fd1 : e0 10 __ CPX #$10
0fd3 : 90 f5 __ BCC $0fca ; (nforml.l37 + 0)
.s38:
0fd5 : 84 1b __ STY ACCU + 0 
0fd7 : b0 09 __ BCS $0fe2 ; (nforml.l23 + 0)
.s24:
0fd9 : 88 __ __ DEY
0fda : b1 0d __ LDA (P0),y ; (si + 0)
0fdc : a4 1b __ LDY ACCU + 0 
0fde : 91 0f __ STA (P2),y ; (str + 0)
0fe0 : e6 1b __ INC ACCU + 0 
.l23:
0fe2 : a5 1b __ LDA ACCU + 0 
0fe4 : a0 01 __ LDY #$01
0fe6 : d1 0d __ CMP (P0),y ; (si + 0)
0fe8 : 90 ef __ BCC $0fd9 ; (nforml.s24 + 0)
0fea : 60 __ __ RTS
.l8:
0feb : a0 04 __ LDY #$04
0fed : b1 0d __ LDA (P0),y ; (si + 0)
0fef : 85 03 __ STA WORK + 0 
0ff1 : a9 00 __ LDA #$00
0ff3 : 85 04 __ STA WORK + 1 
0ff5 : 85 05 __ STA WORK + 2 
0ff7 : 85 06 __ STA WORK + 3 
0ff9 : 20 84 19 JSR $1984 ; (divmod32 + 0)
0ffc : a5 07 __ LDA WORK + 4 
0ffe : c9 0a __ CMP #$0a
1000 : b0 04 __ BCS $1006 ; (nforml.s9 + 0)
.s36:
1002 : a9 30 __ LDA #$30
1004 : 90 06 __ BCC $100c ; (nforml.s10 + 0)
.s9:
1006 : a0 03 __ LDY #$03
1008 : b1 0d __ LDA (P0),y ; (si + 0)
100a : e9 0a __ SBC #$0a
.s10:
100c : 18 __ __ CLC
100d : 65 07 __ ADC WORK + 4 
100f : a6 44 __ LDX T5 + 0 
1011 : 9d e1 9f STA $9fe1,x ; (si.prefix + 0)
1014 : c6 44 __ DEC T5 + 0 
1016 : a5 1b __ LDA ACCU + 0 
1018 : 85 11 __ STA P4 ; (v + 0)
101a : a5 1c __ LDA ACCU + 1 
101c : 85 12 __ STA P5 ; (v + 1)
101e : a5 1d __ LDA ACCU + 2 
1020 : 85 13 __ STA P6 ; (v + 2)
1022 : a5 1e __ LDA ACCU + 3 
1024 : d0 c5 __ BNE $0feb ; (nforml.l8 + 0)
.s33:
1026 : 85 14 __ STA P7 ; (v + 3)
1028 : 4c 10 0f JMP $0f10 ; (nforml.s44 + 0)
--------------------------------------------------------------------
nformf: ; nformf(const struct sinfo*,u8*,float,u8)->u8
; 199, "/data/data/com.termux/files/home/GIT/oscar64/include/stdio.c"
.s1:
102b : a2 03 __ LDX #$03
102d : b5 53 __ LDA T7 + 0,x 
102f : 9d e9 9f STA $9fe9,x ; (nformf@stack + 0)
1032 : ca __ __ DEX
1033 : 10 f8 __ BPL $102d ; (nformf.s1 + 2)
.s4:
1035 : a5 16 __ LDA P9 ; (f + 1)
1037 : 85 44 __ STA T0 + 1 
1039 : a5 17 __ LDA P10 ; (f + 2)
103b : 85 45 __ STA T0 + 2 
103d : a5 18 __ LDA P11 ; (f + 3)
103f : 29 7f __ AND #$7f
1041 : 05 17 __ ORA P10 ; (f + 2)
1043 : 05 16 __ ORA P9 ; (f + 1)
1045 : 05 15 __ ORA P8 ; (f + 0)
1047 : f0 21 __ BEQ $106a ; (nformf.s86 + 0)
.s90:
1049 : 24 18 __ BIT P11 ; (f + 3)
104b : 10 1d __ BPL $106a ; (nformf.s86 + 0)
.s5:
104d : a9 2d __ LDA #$2d
104f : a0 00 __ LDY #$00
1051 : 91 13 __ STA (P6),y ; (str + 0)
1053 : a5 18 __ LDA P11 ; (f + 3)
1055 : 49 80 __ EOR #$80
1057 : 85 18 __ STA P11 ; (f + 3)
1059 : 85 10 __ STA P3 
105b : a5 15 __ LDA P8 ; (f + 0)
105d : 85 0d __ STA P0 
105f : a5 16 __ LDA P9 ; (f + 1)
1061 : 85 0e __ STA P1 
1063 : a5 17 __ LDA P10 ; (f + 2)
1065 : 85 0f __ STA P2 
1067 : 4c 30 15 JMP $1530 ; (nformf.s6 + 0)
.s86:
106a : a5 15 __ LDA P8 ; (f + 0)
106c : 85 0d __ STA P0 
106e : a5 16 __ LDA P9 ; (f + 1)
1070 : 85 0e __ STA P1 
1072 : a5 17 __ LDA P10 ; (f + 2)
1074 : 85 0f __ STA P2 
1076 : a5 18 __ LDA P11 ; (f + 3)
1078 : 85 10 __ STA P3 
107a : a0 05 __ LDY #$05
107c : b1 11 __ LDA (P4),y ; (si + 0)
107e : f0 09 __ BEQ $1089 ; (nformf.s88 + 0)
.s87:
1080 : a9 2b __ LDA #$2b
1082 : a0 00 __ LDY #$00
1084 : 91 13 __ STA (P6),y ; (str + 0)
1086 : 4c 30 15 JMP $1530 ; (nformf.s6 + 0)
.s88:
1089 : 20 41 15 JSR $1541 ; (isinf.s4 + 0)
108c : a2 00 __ LDX #$00
108e : 86 54 __ STX T9 + 0 
1090 : a8 __ __ TAY
1091 : f0 05 __ BEQ $1098 ; (nformf.s20 + 0)
.s89:
1093 : a9 02 __ LDA #$02
1095 : 4c 00 15 JMP $1500 ; (nformf.s8 + 0)
.s20:
1098 : a5 11 __ LDA P4 ; (si + 0)
109a : 85 4b __ STA T2 + 0 
109c : a5 12 __ LDA P5 ; (si + 1)
109e : 85 4c __ STA T2 + 1 
10a0 : a0 02 __ LDY #$02
10a2 : b1 11 __ LDA (P4),y ; (si + 0)
10a4 : c9 ff __ CMP #$ff
10a6 : d0 02 __ BNE $10aa ; (nformf.s21 + 0)
.s85:
10a8 : a9 06 __ LDA #$06
.s21:
10aa : 85 52 __ STA T6 + 0 
10ac : a9 00 __ LDA #$00
10ae : 85 4f __ STA T4 + 0 
10b0 : 85 50 __ STA T4 + 1 
10b2 : a5 15 __ LDA P8 ; (f + 0)
10b4 : 85 43 __ STA T0 + 0 
10b6 : a5 18 __ LDA P11 ; (f + 3)
10b8 : 85 46 __ STA T0 + 3 
10ba : 29 7f __ AND #$7f
10bc : 05 17 __ ORA P10 ; (f + 2)
10be : 05 16 __ ORA P9 ; (f + 1)
10c0 : 05 15 __ ORA P8 ; (f + 0)
10c2 : d0 03 __ BNE $10c7 ; (nformf.s22 + 0)
10c4 : 4c c5 11 JMP $11c5 ; (nformf.s28 + 0)
.s22:
10c7 : a5 18 __ LDA P11 ; (f + 3)
10c9 : 30 66 __ BMI $1131 ; (nformf.l25 + 0)
.s82:
10cb : c9 44 __ CMP #$44
10cd : d0 06 __ BNE $10d5 ; (nformf.l84 + 0)
.s83:
10cf : a5 17 __ LDA P10 ; (f + 2)
10d1 : c9 7a __ CMP #$7a
10d3 : f0 02 __ BEQ $10d7 ; (nformf.l23 + 0)
.l84:
10d5 : 90 4a __ BCC $1121 ; (nformf.s24 + 0)
.l23:
10d7 : a5 4f __ LDA T4 + 0 
10d9 : 69 02 __ ADC #$02
10db : 85 4f __ STA T4 + 0 
10dd : 90 02 __ BCC $10e1 ; (nformf.s104 + 0)
.s103:
10df : e6 50 __ INC T4 + 1 
.s104:
10e1 : a5 43 __ LDA T0 + 0 
10e3 : 85 1b __ STA ACCU + 0 
10e5 : a5 44 __ LDA T0 + 1 
10e7 : 85 1c __ STA ACCU + 1 
10e9 : a5 45 __ LDA T0 + 2 
10eb : 85 1d __ STA ACCU + 2 
10ed : a5 46 __ LDA T0 + 3 
10ef : 85 1e __ STA ACCU + 3 
10f1 : a9 00 __ LDA #$00
10f3 : 85 03 __ STA WORK + 0 
10f5 : 85 04 __ STA WORK + 1 
10f7 : a9 7a __ LDA #$7a
10f9 : 85 05 __ STA WORK + 2 
10fb : a9 44 __ LDA #$44
10fd : 85 06 __ STA WORK + 3 
10ff : 20 6b 15 JSR $156b ; (freg + 20)
1102 : 20 51 17 JSR $1751 ; (crt_fdiv + 0)
1105 : a5 1b __ LDA ACCU + 0 
1107 : 85 43 __ STA T0 + 0 
1109 : a5 1c __ LDA ACCU + 1 
110b : 85 44 __ STA T0 + 1 
110d : a6 1d __ LDX ACCU + 2 
110f : 86 45 __ STX T0 + 2 
1111 : a5 1e __ LDA ACCU + 3 
1113 : 85 46 __ STA T0 + 3 
1115 : 30 0a __ BMI $1121 ; (nformf.s24 + 0)
.s80:
1117 : c9 44 __ CMP #$44
1119 : d0 ba __ BNE $10d5 ; (nformf.l84 + 0)
.s81:
111b : e0 7a __ CPX #$7a
111d : f0 b8 __ BEQ $10d7 ; (nformf.l23 + 0)
111f : d0 b4 __ BNE $10d5 ; (nformf.l84 + 0)
.s24:
1121 : a5 46 __ LDA T0 + 3 
1123 : 30 0c __ BMI $1131 ; (nformf.l25 + 0)
.s78:
1125 : c9 3f __ CMP #$3f
1127 : d0 06 __ BNE $112f ; (nformf.s77 + 0)
.s79:
1129 : a5 45 __ LDA T0 + 2 
112b : c9 80 __ CMP #$80
112d : f0 40 __ BEQ $116f ; (nformf.s26 + 0)
.s77:
112f : b0 3e __ BCS $116f ; (nformf.s26 + 0)
.l25:
1131 : 38 __ __ SEC
1132 : a5 4f __ LDA T4 + 0 
1134 : e9 03 __ SBC #$03
1136 : 85 4f __ STA T4 + 0 
1138 : b0 02 __ BCS $113c ; (nformf.s99 + 0)
.s98:
113a : c6 50 __ DEC T4 + 1 
.s99:
113c : a9 00 __ LDA #$00
113e : 85 1b __ STA ACCU + 0 
1140 : 85 1c __ STA ACCU + 1 
1142 : a9 7a __ LDA #$7a
1144 : 85 1d __ STA ACCU + 2 
1146 : a9 44 __ LDA #$44
1148 : 85 1e __ STA ACCU + 3 
114a : a2 43 __ LDX #$43
114c : 20 5b 15 JSR $155b ; (freg + 4)
114f : 20 89 16 JSR $1689 ; (crt_fmul + 0)
1152 : a5 1b __ LDA ACCU + 0 
1154 : 85 43 __ STA T0 + 0 
1156 : a5 1c __ LDA ACCU + 1 
1158 : 85 44 __ STA T0 + 1 
115a : a6 1d __ LDX ACCU + 2 
115c : 86 45 __ STX T0 + 2 
115e : a5 1e __ LDA ACCU + 3 
1160 : 85 46 __ STA T0 + 3 
1162 : 30 cd __ BMI $1131 ; (nformf.l25 + 0)
.s75:
1164 : c9 3f __ CMP #$3f
1166 : 90 c9 __ BCC $1131 ; (nformf.l25 + 0)
.s108:
1168 : d0 05 __ BNE $116f ; (nformf.s26 + 0)
.s76:
116a : e0 80 __ CPX #$80
116c : 4c 2f 11 JMP $112f ; (nformf.s77 + 0)
.s26:
116f : a5 46 __ LDA T0 + 3 
1171 : 30 52 __ BMI $11c5 ; (nformf.s28 + 0)
.s72:
1173 : c9 41 __ CMP #$41
1175 : d0 06 __ BNE $117d ; (nformf.l74 + 0)
.s73:
1177 : a5 45 __ LDA T0 + 2 
1179 : c9 20 __ CMP #$20
117b : f0 02 __ BEQ $117f ; (nformf.l27 + 0)
.l74:
117d : 90 46 __ BCC $11c5 ; (nformf.s28 + 0)
.l27:
117f : e6 4f __ INC T4 + 0 
1181 : d0 02 __ BNE $1185 ; (nformf.s102 + 0)
.s101:
1183 : e6 50 __ INC T4 + 1 
.s102:
1185 : a5 43 __ LDA T0 + 0 
1187 : 85 1b __ STA ACCU + 0 
1189 : a5 44 __ LDA T0 + 1 
118b : 85 1c __ STA ACCU + 1 
118d : a5 45 __ LDA T0 + 2 
118f : 85 1d __ STA ACCU + 2 
1191 : a5 46 __ LDA T0 + 3 
1193 : 85 1e __ STA ACCU + 3 
1195 : a9 00 __ LDA #$00
1197 : 85 03 __ STA WORK + 0 
1199 : 85 04 __ STA WORK + 1 
119b : a9 20 __ LDA #$20
119d : 85 05 __ STA WORK + 2 
119f : a9 41 __ LDA #$41
11a1 : 85 06 __ STA WORK + 3 
11a3 : 20 6b 15 JSR $156b ; (freg + 20)
11a6 : 20 51 17 JSR $1751 ; (crt_fdiv + 0)
11a9 : a5 1b __ LDA ACCU + 0 
11ab : 85 43 __ STA T0 + 0 
11ad : a5 1c __ LDA ACCU + 1 
11af : 85 44 __ STA T0 + 1 
11b1 : a6 1d __ LDX ACCU + 2 
11b3 : 86 45 __ STX T0 + 2 
11b5 : a5 1e __ LDA ACCU + 3 
11b7 : 85 46 __ STA T0 + 3 
11b9 : 30 0a __ BMI $11c5 ; (nformf.s28 + 0)
.s70:
11bb : c9 41 __ CMP #$41
11bd : d0 be __ BNE $117d ; (nformf.l74 + 0)
.s71:
11bf : e0 20 __ CPX #$20
11c1 : f0 bc __ BEQ $117f ; (nformf.l27 + 0)
11c3 : d0 b8 __ BNE $117d ; (nformf.l74 + 0)
.s28:
11c5 : ad f2 9f LDA $9ff2 ; (sstack + 0)
11c8 : c9 65 __ CMP #$65
11ca : d0 04 __ BNE $11d0 ; (nformf.s30 + 0)
.s29:
11cc : a9 01 __ LDA #$01
11ce : d0 02 __ BNE $11d2 ; (nformf.s31 + 0)
.s30:
11d0 : a9 00 __ LDA #$00
.s31:
11d2 : 85 55 __ STA T10 + 0 
11d4 : a6 52 __ LDX T6 + 0 
11d6 : e8 __ __ INX
11d7 : 86 51 __ STX T5 + 0 
11d9 : ad f2 9f LDA $9ff2 ; (sstack + 0)
11dc : c9 67 __ CMP #$67
11de : d0 13 __ BNE $11f3 ; (nformf.s57 + 0)
.s32:
11e0 : a5 50 __ LDA T4 + 1 
11e2 : 30 08 __ BMI $11ec ; (nformf.s33 + 0)
.s69:
11e4 : d0 06 __ BNE $11ec ; (nformf.s33 + 0)
.s68:
11e6 : a5 4f __ LDA T4 + 0 
11e8 : c9 04 __ CMP #$04
11ea : 90 07 __ BCC $11f3 ; (nformf.s57 + 0)
.s33:
11ec : a9 01 __ LDA #$01
11ee : 85 55 __ STA T10 + 0 
11f0 : 4c 6d 14 JMP $146d ; (nformf.s34 + 0)
.s57:
11f3 : a5 55 __ LDA T10 + 0 
11f5 : d0 f9 __ BNE $11f0 ; (nformf.s33 + 4)
.s58:
11f7 : 24 50 __ BIT T4 + 1 
11f9 : 10 43 __ BPL $123e ; (nformf.s60 + 0)
.s59:
11fb : a5 43 __ LDA T0 + 0 
11fd : 85 1b __ STA ACCU + 0 
11ff : a5 44 __ LDA T0 + 1 
1201 : 85 1c __ STA ACCU + 1 
1203 : a5 45 __ LDA T0 + 2 
1205 : 85 1d __ STA ACCU + 2 
1207 : a5 46 __ LDA T0 + 3 
1209 : 85 1e __ STA ACCU + 3 
.l91:
120b : a9 00 __ LDA #$00
120d : 85 03 __ STA WORK + 0 
120f : 85 04 __ STA WORK + 1 
1211 : a9 20 __ LDA #$20
1213 : 85 05 __ STA WORK + 2 
1215 : a9 41 __ LDA #$41
1217 : 85 06 __ STA WORK + 3 
1219 : 20 6b 15 JSR $156b ; (freg + 20)
121c : 20 51 17 JSR $1751 ; (crt_fdiv + 0)
121f : 18 __ __ CLC
1220 : a5 4f __ LDA T4 + 0 
1222 : 69 01 __ ADC #$01
1224 : 85 4f __ STA T4 + 0 
1226 : a5 50 __ LDA T4 + 1 
1228 : 69 00 __ ADC #$00
122a : 85 50 __ STA T4 + 1 
122c : 30 dd __ BMI $120b ; (nformf.l91 + 0)
.s92:
122e : a5 1e __ LDA ACCU + 3 
1230 : 85 46 __ STA T0 + 3 
1232 : a5 1d __ LDA ACCU + 2 
1234 : 85 45 __ STA T0 + 2 
1236 : a5 1c __ LDA ACCU + 1 
1238 : 85 44 __ STA T0 + 1 
123a : a5 1b __ LDA ACCU + 0 
123c : 85 43 __ STA T0 + 0 
.s60:
123e : 18 __ __ CLC
123f : a5 52 __ LDA T6 + 0 
1241 : 65 4f __ ADC T4 + 0 
1243 : 18 __ __ CLC
1244 : 69 01 __ ADC #$01
1246 : 85 51 __ STA T5 + 0 
1248 : c9 07 __ CMP #$07
124a : 90 14 __ BCC $1260 ; (nformf.s61 + 0)
.s67:
124c : ad 85 1a LDA $1a85 ; (fround5[0] + 24)
124f : 85 47 __ STA T1 + 0 
1251 : ad 86 1a LDA $1a86 ; (fround5[0] + 25)
1254 : 85 48 __ STA T1 + 1 
1256 : ad 87 1a LDA $1a87 ; (fround5[0] + 26)
1259 : 85 49 __ STA T1 + 2 
125b : ad 88 1a LDA $1a88 ; (fround5[0] + 27)
125e : b0 15 __ BCS $1275 ; (nformf.s62 + 0)
.s61:
1260 : 0a __ __ ASL
1261 : 0a __ __ ASL
1262 : aa __ __ TAX
1263 : bd 69 1a LDA $1a69,x ; (divmod32 + 229)
1266 : 85 47 __ STA T1 + 0 
1268 : bd 6a 1a LDA $1a6a,x ; (divmod32 + 230)
126b : 85 48 __ STA T1 + 1 
126d : bd 6b 1a LDA $1a6b,x ; (spentry + 0)
1270 : 85 49 __ STA T1 + 2 
1272 : bd 6c 1a LDA $1a6c,x ; (giocharmap + 0)
.s62:
1275 : 85 4a __ STA T1 + 3 
1277 : a5 43 __ LDA T0 + 0 
1279 : 85 1b __ STA ACCU + 0 
127b : a5 44 __ LDA T0 + 1 
127d : 85 1c __ STA ACCU + 1 
127f : a5 45 __ LDA T0 + 2 
1281 : 85 1d __ STA ACCU + 2 
1283 : a5 46 __ LDA T0 + 3 
1285 : 85 1e __ STA ACCU + 3 
1287 : a2 47 __ LDX #$47
1289 : 20 5b 15 JSR $155b ; (freg + 4)
128c : 20 a2 15 JSR $15a2 ; (faddsub + 6)
128f : a6 1b __ LDX ACCU + 0 
1291 : a5 1c __ LDA ACCU + 1 
1293 : 85 16 __ STA P9 ; (f + 1)
1295 : a5 1d __ LDA ACCU + 2 
1297 : 85 17 __ STA P10 ; (f + 2)
1299 : a5 1e __ LDA ACCU + 3 
129b : 85 18 __ STA P11 ; (f + 3)
129d : 30 30 __ BMI $12cf ; (nformf.s38 + 0)
.s64:
129f : c9 41 __ CMP #$41
12a1 : d0 06 __ BNE $12a9 ; (nformf.s66 + 0)
.s65:
12a3 : a5 17 __ LDA P10 ; (f + 2)
12a5 : c9 20 __ CMP #$20
12a7 : f0 02 __ BEQ $12ab ; (nformf.s63 + 0)
.s66:
12a9 : 90 24 __ BCC $12cf ; (nformf.s38 + 0)
.s63:
12ab : a9 00 __ LDA #$00
12ad : 85 03 __ STA WORK + 0 
12af : 85 04 __ STA WORK + 1 
12b1 : a9 20 __ LDA #$20
12b3 : 85 05 __ STA WORK + 2 
12b5 : a9 41 __ LDA #$41
12b7 : 85 06 __ STA WORK + 3 
12b9 : 20 6b 15 JSR $156b ; (freg + 20)
12bc : 20 51 17 JSR $1751 ; (crt_fdiv + 0)
12bf : c6 52 __ DEC T6 + 0 
12c1 : a6 1b __ LDX ACCU + 0 
12c3 : a5 1c __ LDA ACCU + 1 
12c5 : 85 16 __ STA P9 ; (f + 1)
12c7 : a5 1d __ LDA ACCU + 2 
12c9 : 85 17 __ STA P10 ; (f + 2)
12cb : a5 1e __ LDA ACCU + 3 
12cd : 85 18 __ STA P11 ; (f + 3)
.s38:
12cf : 38 __ __ SEC
12d0 : a5 51 __ LDA T5 + 0 
12d2 : e5 52 __ SBC T6 + 0 
12d4 : 85 4d __ STA T3 + 0 
12d6 : a5 51 __ LDA T5 + 0 
12d8 : c9 15 __ CMP #$15
12da : 90 04 __ BCC $12e0 ; (nformf.s52 + 0)
.s39:
12dc : a9 14 __ LDA #$14
12de : 85 51 __ STA T5 + 0 
.s52:
12e0 : a5 4d __ LDA T3 + 0 
12e2 : d0 08 __ BNE $12ec ; (nformf.s40 + 0)
.s51:
12e4 : a9 30 __ LDA #$30
12e6 : a4 54 __ LDY T9 + 0 
12e8 : 91 13 __ STA (P6),y ; (str + 0)
12ea : e6 54 __ INC T9 + 0 
.s40:
12ec : a9 00 __ LDA #$00
12ee : 85 56 __ STA T11 + 0 
.l109:
12f0 : c5 4d __ CMP T3 + 0 
12f2 : d0 03 __ BNE $12f7 ; (nformf.s42 + 0)
12f4 : 4c 59 14 JMP $1459 ; (nformf.s41 + 0)
.s42:
12f7 : c9 07 __ CMP #$07
12f9 : 90 04 __ BCC $12ff ; (nformf.s50 + 0)
.s43:
12fb : a9 30 __ LDA #$30
12fd : b0 55 __ BCS $1354 ; (nformf.s44 + 0)
.s50:
12ff : 86 1b __ STX ACCU + 0 
1301 : 86 43 __ STX T0 + 0 
1303 : a5 16 __ LDA P9 ; (f + 1)
1305 : 85 1c __ STA ACCU + 1 
1307 : 85 44 __ STA T0 + 1 
1309 : a5 17 __ LDA P10 ; (f + 2)
130b : 85 1d __ STA ACCU + 2 
130d : 85 45 __ STA T0 + 2 
130f : a5 18 __ LDA P11 ; (f + 3)
1311 : 85 1e __ STA ACCU + 3 
1313 : 85 46 __ STA T0 + 3 
1315 : 20 f1 18 JSR $18f1 ; (f32_to_i16 + 0)
1318 : a5 1b __ LDA ACCU + 0 
131a : 85 53 __ STA T7 + 0 
131c : 20 3d 19 JSR $193d ; (sint16_to_float + 0)
131f : a2 43 __ LDX #$43
1321 : 20 5b 15 JSR $155b ; (freg + 4)
1324 : a5 1e __ LDA ACCU + 3 
1326 : 49 80 __ EOR #$80
1328 : 85 1e __ STA ACCU + 3 
132a : 20 a2 15 JSR $15a2 ; (faddsub + 6)
132d : a9 00 __ LDA #$00
132f : 85 03 __ STA WORK + 0 
1331 : 85 04 __ STA WORK + 1 
1333 : a9 20 __ LDA #$20
1335 : 85 05 __ STA WORK + 2 
1337 : a9 41 __ LDA #$41
1339 : 85 06 __ STA WORK + 3 
133b : 20 6b 15 JSR $156b ; (freg + 20)
133e : 20 89 16 JSR $1689 ; (crt_fmul + 0)
1341 : 18 __ __ CLC
1342 : a5 1c __ LDA ACCU + 1 
1344 : 85 16 __ STA P9 ; (f + 1)
1346 : a5 1d __ LDA ACCU + 2 
1348 : 85 17 __ STA P10 ; (f + 2)
134a : a5 1e __ LDA ACCU + 3 
134c : 85 18 __ STA P11 ; (f + 3)
134e : a5 53 __ LDA T7 + 0 
1350 : 69 30 __ ADC #$30
1352 : a6 1b __ LDX ACCU + 0 
.s44:
1354 : a4 54 __ LDY T9 + 0 
1356 : 91 13 __ STA (P6),y ; (str + 0)
1358 : e6 54 __ INC T9 + 0 
135a : e6 56 __ INC T11 + 0 
135c : a5 56 __ LDA T11 + 0 
135e : c5 51 __ CMP T5 + 0 
1360 : 90 8e __ BCC $12f0 ; (nformf.l109 + 0)
.s45:
1362 : a5 55 __ LDA T10 + 0 
1364 : f0 66 __ BEQ $13cc ; (nformf.s9 + 0)
.s46:
1366 : a0 03 __ LDY #$03
1368 : b1 4b __ LDA (T2 + 0),y 
136a : 69 03 __ ADC #$03
136c : a4 54 __ LDY T9 + 0 
136e : 91 13 __ STA (P6),y ; (str + 0)
1370 : c8 __ __ INY
1371 : 84 54 __ STY T9 + 0 
1373 : 24 50 __ BIT T4 + 1 
1375 : 30 06 __ BMI $137d ; (nformf.s47 + 0)
.s49:
1377 : a9 2b __ LDA #$2b
1379 : 91 13 __ STA (P6),y ; (str + 0)
137b : d0 11 __ BNE $138e ; (nformf.s48 + 0)
.s47:
137d : a9 2d __ LDA #$2d
137f : 91 13 __ STA (P6),y ; (str + 0)
1381 : 38 __ __ SEC
1382 : a9 00 __ LDA #$00
1384 : e5 4f __ SBC T4 + 0 
1386 : 85 4f __ STA T4 + 0 
1388 : a9 00 __ LDA #$00
138a : e5 50 __ SBC T4 + 1 
138c : 85 50 __ STA T4 + 1 
.s48:
138e : a5 4f __ LDA T4 + 0 
1390 : 85 1b __ STA ACCU + 0 
1392 : a5 50 __ LDA T4 + 1 
1394 : 85 1c __ STA ACCU + 1 
1396 : e6 54 __ INC T9 + 0 
1398 : a9 0a __ LDA #$0a
139a : 85 03 __ STA WORK + 0 
139c : a9 00 __ LDA #$00
139e : 85 04 __ STA WORK + 1 
13a0 : 20 ff 17 JSR $17ff ; (divs16 + 0)
13a3 : 18 __ __ CLC
13a4 : a5 1b __ LDA ACCU + 0 
13a6 : 69 30 __ ADC #$30
13a8 : a4 54 __ LDY T9 + 0 
13aa : 91 13 __ STA (P6),y ; (str + 0)
13ac : a5 4f __ LDA T4 + 0 
13ae : 85 1b __ STA ACCU + 0 
13b0 : a5 50 __ LDA T4 + 1 
13b2 : 85 1c __ STA ACCU + 1 
13b4 : e6 54 __ INC T9 + 0 
13b6 : a9 0a __ LDA #$0a
13b8 : 85 03 __ STA WORK + 0 
13ba : a9 00 __ LDA #$00
13bc : 85 04 __ STA WORK + 1 
13be : 20 c4 18 JSR $18c4 ; (mods16 + 0)
13c1 : 18 __ __ CLC
13c2 : a5 05 __ LDA WORK + 2 
13c4 : 69 30 __ ADC #$30
13c6 : a4 54 __ LDY T9 + 0 
13c8 : 91 13 __ STA (P6),y ; (str + 0)
13ca : e6 54 __ INC T9 + 0 
.s9:
13cc : a5 54 __ LDA T9 + 0 
.s105:
13ce : a0 01 __ LDY #$01
13d0 : d1 11 __ CMP (P4),y ; (si + 0)
13d2 : b0 69 __ BCS $143d ; (nformf.s3 + 0)
.s10:
13d4 : a0 06 __ LDY #$06
13d6 : b1 11 __ LDA (P4),y ; (si + 0)
13d8 : f0 04 __ BEQ $13de ; (nformf.s14 + 0)
.s93:
13da : a6 54 __ LDX T9 + 0 
13dc : 90 6c __ BCC $144a ; (nformf.l11 + 0)
.s14:
13de : a5 54 __ LDA T9 + 0 
13e0 : f0 3c __ BEQ $141e ; (nformf.s16 + 0)
.s15:
13e2 : e9 00 __ SBC #$00
13e4 : a2 00 __ LDX #$00
13e6 : b0 01 __ BCS $13e9 ; (nformf.s107 + 0)
.s106:
13e8 : ca __ __ DEX
.s107:
13e9 : 18 __ __ CLC
13ea : 65 13 __ ADC P6 ; (str + 0)
13ec : 85 47 __ STA T1 + 0 
13ee : 8a __ __ TXA
13ef : 65 14 __ ADC P7 ; (str + 1)
13f1 : 85 48 __ STA T1 + 1 
13f3 : a9 01 __ LDA #$01
13f5 : 85 4b __ STA T2 + 0 
13f7 : a6 14 __ LDX P7 ; (str + 1)
13f9 : 38 __ __ SEC
.l94:
13fa : a0 01 __ LDY #$01
13fc : b1 11 __ LDA (P4),y ; (si + 0)
13fe : e5 4b __ SBC T2 + 0 
1400 : 85 4d __ STA T3 + 0 
1402 : 8a __ __ TXA
1403 : 69 ff __ ADC #$ff
1405 : 85 4e __ STA T3 + 1 
1407 : 88 __ __ DEY
1408 : b1 47 __ LDA (T1 + 0),y 
140a : a4 13 __ LDY P6 ; (str + 0)
140c : 91 4d __ STA (T3 + 0),y 
140e : a5 47 __ LDA T1 + 0 
1410 : d0 02 __ BNE $1414 ; (nformf.s97 + 0)
.s96:
1412 : c6 48 __ DEC T1 + 1 
.s97:
1414 : c6 47 __ DEC T1 + 0 
1416 : e6 4b __ INC T2 + 0 
1418 : a5 54 __ LDA T9 + 0 
141a : c5 4b __ CMP T2 + 0 
141c : b0 dc __ BCS $13fa ; (nformf.l94 + 0)
.s16:
141e : a9 00 __ LDA #$00
1420 : 85 4d __ STA T3 + 0 
1422 : 90 08 __ BCC $142c ; (nformf.l17 + 0)
.s18:
1424 : a9 20 __ LDA #$20
1426 : a4 4d __ LDY T3 + 0 
1428 : 91 13 __ STA (P6),y ; (str + 0)
142a : e6 4d __ INC T3 + 0 
.l17:
142c : a0 01 __ LDY #$01
142e : b1 11 __ LDA (P4),y ; (si + 0)
1430 : 38 __ __ SEC
1431 : e5 54 __ SBC T9 + 0 
1433 : 90 ef __ BCC $1424 ; (nformf.s18 + 0)
.s19:
1435 : c5 4d __ CMP T3 + 0 
1437 : 90 02 __ BCC $143b ; (nformf.s13 + 0)
.s95:
1439 : d0 e9 __ BNE $1424 ; (nformf.s18 + 0)
.s13:
143b : b1 11 __ LDA (P4),y ; (si + 0)
.s3:
143d : 85 1b __ STA ACCU + 0 
143f : a2 03 __ LDX #$03
1441 : bd e9 9f LDA $9fe9,x ; (nformf@stack + 0)
1444 : 95 53 __ STA T7 + 0,x 
1446 : ca __ __ DEX
1447 : 10 f8 __ BPL $1441 ; (nformf.s3 + 4)
1449 : 60 __ __ RTS
.l11:
144a : 8a __ __ TXA
144b : a0 01 __ LDY #$01
144d : d1 11 __ CMP (P4),y ; (si + 0)
144f : b0 ea __ BCS $143b ; (nformf.s13 + 0)
.s12:
1451 : a8 __ __ TAY
1452 : a9 20 __ LDA #$20
1454 : 91 13 __ STA (P6),y ; (str + 0)
1456 : e8 __ __ INX
1457 : 90 f1 __ BCC $144a ; (nformf.l11 + 0)
.s41:
1459 : a9 2e __ LDA #$2e
145b : a4 54 __ LDY T9 + 0 
145d : 91 13 __ STA (P6),y ; (str + 0)
145f : a5 56 __ LDA T11 + 0 
1461 : c9 07 __ CMP #$07
1463 : e6 54 __ INC T9 + 0 
1465 : b0 03 __ BCS $146a ; (nformf.s41 + 17)
1467 : 4c ff 12 JMP $12ff ; (nformf.s50 + 0)
146a : 4c fb 12 JMP $12fb ; (nformf.s43 + 0)
.s34:
146d : a5 51 __ LDA T5 + 0 
146f : c9 07 __ CMP #$07
1471 : 90 14 __ BCC $1487 ; (nformf.s35 + 0)
.s56:
1473 : ad 85 1a LDA $1a85 ; (fround5[0] + 24)
1476 : 85 47 __ STA T1 + 0 
1478 : ad 86 1a LDA $1a86 ; (fround5[0] + 25)
147b : 85 48 __ STA T1 + 1 
147d : ad 87 1a LDA $1a87 ; (fround5[0] + 26)
1480 : 85 49 __ STA T1 + 2 
1482 : ad 88 1a LDA $1a88 ; (fround5[0] + 27)
1485 : b0 15 __ BCS $149c ; (nformf.s36 + 0)
.s35:
1487 : 0a __ __ ASL
1488 : 0a __ __ ASL
1489 : aa __ __ TAX
148a : bd 69 1a LDA $1a69,x ; (divmod32 + 229)
148d : 85 47 __ STA T1 + 0 
148f : bd 6a 1a LDA $1a6a,x ; (divmod32 + 230)
1492 : 85 48 __ STA T1 + 1 
1494 : bd 6b 1a LDA $1a6b,x ; (spentry + 0)
1497 : 85 49 __ STA T1 + 2 
1499 : bd 6c 1a LDA $1a6c,x ; (giocharmap + 0)
.s36:
149c : 85 4a __ STA T1 + 3 
149e : a5 43 __ LDA T0 + 0 
14a0 : 85 1b __ STA ACCU + 0 
14a2 : a5 44 __ LDA T0 + 1 
14a4 : 85 1c __ STA ACCU + 1 
14a6 : a5 45 __ LDA T0 + 2 
14a8 : 85 1d __ STA ACCU + 2 
14aa : a5 46 __ LDA T0 + 3 
14ac : 85 1e __ STA ACCU + 3 
14ae : a2 47 __ LDX #$47
14b0 : 20 5b 15 JSR $155b ; (freg + 4)
14b3 : 20 a2 15 JSR $15a2 ; (faddsub + 6)
14b6 : a6 1b __ LDX ACCU + 0 
14b8 : a5 1c __ LDA ACCU + 1 
14ba : 85 16 __ STA P9 ; (f + 1)
14bc : a5 1d __ LDA ACCU + 2 
14be : 85 17 __ STA P10 ; (f + 2)
14c0 : a5 1e __ LDA ACCU + 3 
14c2 : 85 18 __ STA P11 ; (f + 3)
14c4 : 10 03 __ BPL $14c9 ; (nformf.s53 + 0)
14c6 : 4c cf 12 JMP $12cf ; (nformf.s38 + 0)
.s53:
14c9 : c9 41 __ CMP #$41
14cb : d0 06 __ BNE $14d3 ; (nformf.s55 + 0)
.s54:
14cd : a5 17 __ LDA P10 ; (f + 2)
14cf : c9 20 __ CMP #$20
14d1 : f0 02 __ BEQ $14d5 ; (nformf.s37 + 0)
.s55:
14d3 : 90 f1 __ BCC $14c6 ; (nformf.s36 + 42)
.s37:
14d5 : a9 00 __ LDA #$00
14d7 : 85 03 __ STA WORK + 0 
14d9 : 85 04 __ STA WORK + 1 
14db : a9 20 __ LDA #$20
14dd : 85 05 __ STA WORK + 2 
14df : a9 41 __ LDA #$41
14e1 : 85 06 __ STA WORK + 3 
14e3 : 20 6b 15 JSR $156b ; (freg + 20)
14e6 : 20 51 17 JSR $1751 ; (crt_fdiv + 0)
14e9 : a6 1b __ LDX ACCU + 0 
14eb : a5 1c __ LDA ACCU + 1 
14ed : 85 16 __ STA P9 ; (f + 1)
14ef : a5 1d __ LDA ACCU + 2 
14f1 : 85 17 __ STA P10 ; (f + 2)
14f3 : a5 1e __ LDA ACCU + 3 
14f5 : 85 18 __ STA P11 ; (f + 3)
14f7 : e6 4f __ INC T4 + 0 
14f9 : d0 cb __ BNE $14c6 ; (nformf.s36 + 42)
.s100:
14fb : e6 50 __ INC T4 + 1 
14fd : 4c cf 12 JMP $12cf ; (nformf.s38 + 0)
.s8:
1500 : 86 43 __ STX T0 + 0 
1502 : 85 47 __ STA T1 + 0 
1504 : a0 03 __ LDY #$03
1506 : b1 11 __ LDA (P4),y ; (si + 0)
1508 : 18 __ __ CLC
1509 : 69 08 __ ADC #$08
150b : a4 43 __ LDY T0 + 0 
150d : 91 13 __ STA (P6),y ; (str + 0)
150f : 18 __ __ CLC
1510 : a0 03 __ LDY #$03
1512 : b1 11 __ LDA (P4),y ; (si + 0)
1514 : 69 0d __ ADC #$0d
1516 : a4 43 __ LDY T0 + 0 
1518 : c8 __ __ INY
1519 : 91 13 __ STA (P6),y ; (str + 0)
151b : a0 03 __ LDY #$03
151d : b1 11 __ LDA (P4),y ; (si + 0)
151f : 18 __ __ CLC
1520 : 69 05 __ ADC #$05
1522 : a4 47 __ LDY T1 + 0 
1524 : 91 13 __ STA (P6),y ; (str + 0)
1526 : 18 __ __ CLC
1527 : a5 54 __ LDA T9 + 0 
1529 : 69 03 __ ADC #$03
152b : 85 54 __ STA T9 + 0 
152d : 4c ce 13 JMP $13ce ; (nformf.s105 + 0)
.s6:
1530 : 20 41 15 JSR $1541 ; (isinf.s4 + 0)
1533 : a2 01 __ LDX #$01
1535 : 86 54 __ STX T9 + 0 
1537 : a8 __ __ TAY
1538 : d0 03 __ BNE $153d ; (nformf.s7 + 0)
153a : 4c 98 10 JMP $1098 ; (nformf.s20 + 0)
.s7:
153d : a9 03 __ LDA #$03
153f : d0 bf __ BNE $1500 ; (nformf.s8 + 0)
--------------------------------------------------------------------
isinf: ; isinf(float)->bool
;  26, "/data/data/com.termux/files/home/GIT/oscar64/include/math.h"
.s4:
1541 : 06 0f __ ASL P2 ; (f + 2)
1543 : a5 10 __ LDA P3 ; (f + 3)
1545 : 2a __ __ ROL
1546 : c9 ff __ CMP #$ff
1548 : d0 03 __ BNE $154d ; (isinf.s6 + 0)
.s5:
154a : a9 01 __ LDA #$01
154c : 60 __ __ RTS
.s6:
154d : a9 00 __ LDA #$00
.s3:
154f : 60 __ __ RTS
--------------------------------------------------------------------
krnio_clrchn: ; krnio_clrchn()->void
;  59, "/data/data/com.termux/files/home/GIT/oscar64/include/c64/kernalio.h"
.s4:
1550 : 20 cc ff JSR $ffcc 
.s3:
1553 : 60 __ __ RTS
--------------------------------------------------------------------
1554 : __ __ __ BYT 25 63 00                                        : %c.
--------------------------------------------------------------------
freg: ; freg
1557 : b1 19 __ LDA (IP + 0),y 
1559 : c8 __ __ INY
155a : aa __ __ TAX
155b : b5 00 __ LDA $00,x 
155d : 85 03 __ STA WORK + 0 
155f : b5 01 __ LDA $01,x 
1561 : 85 04 __ STA WORK + 1 
1563 : b5 02 __ LDA $02,x 
1565 : 85 05 __ STA WORK + 2 
1567 : b5 03 __ LDA WORK + 0,x 
1569 : 85 06 __ STA WORK + 3 
156b : a5 05 __ LDA WORK + 2 
156d : 0a __ __ ASL
156e : a5 06 __ LDA WORK + 3 
1570 : 2a __ __ ROL
1571 : 85 08 __ STA WORK + 5 
1573 : f0 06 __ BEQ $157b ; (freg + 36)
1575 : a5 05 __ LDA WORK + 2 
1577 : 09 80 __ ORA #$80
1579 : 85 05 __ STA WORK + 2 
157b : a5 1d __ LDA ACCU + 2 
157d : 0a __ __ ASL
157e : a5 1e __ LDA ACCU + 3 
1580 : 2a __ __ ROL
1581 : 85 07 __ STA WORK + 4 
1583 : f0 06 __ BEQ $158b ; (freg + 52)
1585 : a5 1d __ LDA ACCU + 2 
1587 : 09 80 __ ORA #$80
1589 : 85 1d __ STA ACCU + 2 
158b : 60 __ __ RTS
158c : 06 1e __ ASL ACCU + 3 
158e : a5 07 __ LDA WORK + 4 
1590 : 6a __ __ ROR
1591 : 85 1e __ STA ACCU + 3 
1593 : b0 06 __ BCS $159b ; (freg + 68)
1595 : a5 1d __ LDA ACCU + 2 
1597 : 29 7f __ AND #$7f
1599 : 85 1d __ STA ACCU + 2 
159b : 60 __ __ RTS
--------------------------------------------------------------------
faddsub: ; faddsub
159c : a5 06 __ LDA WORK + 3 
159e : 49 80 __ EOR #$80
15a0 : 85 06 __ STA WORK + 3 
15a2 : a9 ff __ LDA #$ff
15a4 : c5 07 __ CMP WORK + 4 
15a6 : f0 04 __ BEQ $15ac ; (faddsub + 16)
15a8 : c5 08 __ CMP WORK + 5 
15aa : d0 11 __ BNE $15bd ; (faddsub + 33)
15ac : a5 1e __ LDA ACCU + 3 
15ae : 09 7f __ ORA #$7f
15b0 : 85 1e __ STA ACCU + 3 
15b2 : a9 80 __ LDA #$80
15b4 : 85 1d __ STA ACCU + 2 
15b6 : a9 00 __ LDA #$00
15b8 : 85 1b __ STA ACCU + 0 
15ba : 85 1c __ STA ACCU + 1 
15bc : 60 __ __ RTS
15bd : 38 __ __ SEC
15be : a5 07 __ LDA WORK + 4 
15c0 : e5 08 __ SBC WORK + 5 
15c2 : f0 38 __ BEQ $15fc ; (faddsub + 96)
15c4 : aa __ __ TAX
15c5 : b0 25 __ BCS $15ec ; (faddsub + 80)
15c7 : e0 e9 __ CPX #$e9
15c9 : b0 0e __ BCS $15d9 ; (faddsub + 61)
15cb : a5 08 __ LDA WORK + 5 
15cd : 85 07 __ STA WORK + 4 
15cf : a9 00 __ LDA #$00
15d1 : 85 1b __ STA ACCU + 0 
15d3 : 85 1c __ STA ACCU + 1 
15d5 : 85 1d __ STA ACCU + 2 
15d7 : f0 23 __ BEQ $15fc ; (faddsub + 96)
15d9 : a5 1d __ LDA ACCU + 2 
15db : 4a __ __ LSR
15dc : 66 1c __ ROR ACCU + 1 
15de : 66 1b __ ROR ACCU + 0 
15e0 : e8 __ __ INX
15e1 : d0 f8 __ BNE $15db ; (faddsub + 63)
15e3 : 85 1d __ STA ACCU + 2 
15e5 : a5 08 __ LDA WORK + 5 
15e7 : 85 07 __ STA WORK + 4 
15e9 : 4c fc 15 JMP $15fc ; (faddsub + 96)
15ec : e0 18 __ CPX #$18
15ee : b0 33 __ BCS $1623 ; (faddsub + 135)
15f0 : a5 05 __ LDA WORK + 2 
15f2 : 4a __ __ LSR
15f3 : 66 04 __ ROR WORK + 1 
15f5 : 66 03 __ ROR WORK + 0 
15f7 : ca __ __ DEX
15f8 : d0 f8 __ BNE $15f2 ; (faddsub + 86)
15fa : 85 05 __ STA WORK + 2 
15fc : a5 1e __ LDA ACCU + 3 
15fe : 29 80 __ AND #$80
1600 : 85 1e __ STA ACCU + 3 
1602 : 45 06 __ EOR WORK + 3 
1604 : 30 31 __ BMI $1637 ; (faddsub + 155)
1606 : 18 __ __ CLC
1607 : a5 1b __ LDA ACCU + 0 
1609 : 65 03 __ ADC WORK + 0 
160b : 85 1b __ STA ACCU + 0 
160d : a5 1c __ LDA ACCU + 1 
160f : 65 04 __ ADC WORK + 1 
1611 : 85 1c __ STA ACCU + 1 
1613 : a5 1d __ LDA ACCU + 2 
1615 : 65 05 __ ADC WORK + 2 
1617 : 85 1d __ STA ACCU + 2 
1619 : 90 08 __ BCC $1623 ; (faddsub + 135)
161b : 66 1d __ ROR ACCU + 2 
161d : 66 1c __ ROR ACCU + 1 
161f : 66 1b __ ROR ACCU + 0 
1621 : e6 07 __ INC WORK + 4 
1623 : a5 07 __ LDA WORK + 4 
1625 : c9 ff __ CMP #$ff
1627 : f0 83 __ BEQ $15ac ; (faddsub + 16)
1629 : 4a __ __ LSR
162a : 05 1e __ ORA ACCU + 3 
162c : 85 1e __ STA ACCU + 3 
162e : b0 06 __ BCS $1636 ; (faddsub + 154)
1630 : a5 1d __ LDA ACCU + 2 
1632 : 29 7f __ AND #$7f
1634 : 85 1d __ STA ACCU + 2 
1636 : 60 __ __ RTS
1637 : 38 __ __ SEC
1638 : a5 1b __ LDA ACCU + 0 
163a : e5 03 __ SBC WORK + 0 
163c : 85 1b __ STA ACCU + 0 
163e : a5 1c __ LDA ACCU + 1 
1640 : e5 04 __ SBC WORK + 1 
1642 : 85 1c __ STA ACCU + 1 
1644 : a5 1d __ LDA ACCU + 2 
1646 : e5 05 __ SBC WORK + 2 
1648 : 85 1d __ STA ACCU + 2 
164a : b0 19 __ BCS $1665 ; (faddsub + 201)
164c : 38 __ __ SEC
164d : a9 00 __ LDA #$00
164f : e5 1b __ SBC ACCU + 0 
1651 : 85 1b __ STA ACCU + 0 
1653 : a9 00 __ LDA #$00
1655 : e5 1c __ SBC ACCU + 1 
1657 : 85 1c __ STA ACCU + 1 
1659 : a9 00 __ LDA #$00
165b : e5 1d __ SBC ACCU + 2 
165d : 85 1d __ STA ACCU + 2 
165f : a5 1e __ LDA ACCU + 3 
1661 : 49 80 __ EOR #$80
1663 : 85 1e __ STA ACCU + 3 
1665 : a5 1d __ LDA ACCU + 2 
1667 : 30 ba __ BMI $1623 ; (faddsub + 135)
1669 : 05 1c __ ORA ACCU + 1 
166b : 05 1b __ ORA ACCU + 0 
166d : f0 0f __ BEQ $167e ; (faddsub + 226)
166f : c6 07 __ DEC WORK + 4 
1671 : f0 0b __ BEQ $167e ; (faddsub + 226)
1673 : 06 1b __ ASL ACCU + 0 
1675 : 26 1c __ ROL ACCU + 1 
1677 : 26 1d __ ROL ACCU + 2 
1679 : 10 f4 __ BPL $166f ; (faddsub + 211)
167b : 4c 23 16 JMP $1623 ; (faddsub + 135)
167e : a9 00 __ LDA #$00
1680 : 85 1b __ STA ACCU + 0 
1682 : 85 1c __ STA ACCU + 1 
1684 : 85 1d __ STA ACCU + 2 
1686 : 85 1e __ STA ACCU + 3 
1688 : 60 __ __ RTS
--------------------------------------------------------------------
crt_fmul: ; crt_fmul
1689 : a5 1b __ LDA ACCU + 0 
168b : 05 1c __ ORA ACCU + 1 
168d : 05 1d __ ORA ACCU + 2 
168f : f0 0e __ BEQ $169f ; (crt_fmul + 22)
1691 : a5 03 __ LDA WORK + 0 
1693 : 05 04 __ ORA WORK + 1 
1695 : 05 05 __ ORA WORK + 2 
1697 : d0 09 __ BNE $16a2 ; (crt_fmul + 25)
1699 : 85 1b __ STA ACCU + 0 
169b : 85 1c __ STA ACCU + 1 
169d : 85 1d __ STA ACCU + 2 
169f : 85 1e __ STA ACCU + 3 
16a1 : 60 __ __ RTS
16a2 : a5 1e __ LDA ACCU + 3 
16a4 : 45 06 __ EOR WORK + 3 
16a6 : 29 80 __ AND #$80
16a8 : 85 1e __ STA ACCU + 3 
16aa : a9 ff __ LDA #$ff
16ac : c5 07 __ CMP WORK + 4 
16ae : f0 42 __ BEQ $16f2 ; (crt_fmul + 105)
16b0 : c5 08 __ CMP WORK + 5 
16b2 : f0 3e __ BEQ $16f2 ; (crt_fmul + 105)
16b4 : a9 00 __ LDA #$00
16b6 : 85 09 __ STA WORK + 6 
16b8 : 85 0a __ STA WORK + 7 
16ba : 85 0b __ STA WORK + 8 
16bc : a4 1b __ LDY ACCU + 0 
16be : a5 03 __ LDA WORK + 0 
16c0 : d0 06 __ BNE $16c8 ; (crt_fmul + 63)
16c2 : a5 04 __ LDA WORK + 1 
16c4 : f0 0a __ BEQ $16d0 ; (crt_fmul + 71)
16c6 : d0 05 __ BNE $16cd ; (crt_fmul + 68)
16c8 : 20 23 17 JSR $1723 ; (crt_fmul8 + 0)
16cb : a5 04 __ LDA WORK + 1 
16cd : 20 23 17 JSR $1723 ; (crt_fmul8 + 0)
16d0 : a5 05 __ LDA WORK + 2 
16d2 : 20 23 17 JSR $1723 ; (crt_fmul8 + 0)
16d5 : 38 __ __ SEC
16d6 : a5 0b __ LDA WORK + 8 
16d8 : 30 06 __ BMI $16e0 ; (crt_fmul + 87)
16da : 06 09 __ ASL WORK + 6 
16dc : 26 0a __ ROL WORK + 7 
16de : 2a __ __ ROL
16df : 18 __ __ CLC
16e0 : 29 7f __ AND #$7f
16e2 : 85 0b __ STA WORK + 8 
16e4 : a5 07 __ LDA WORK + 4 
16e6 : 65 08 __ ADC WORK + 5 
16e8 : 90 19 __ BCC $1703 ; (crt_fmul + 122)
16ea : e9 7f __ SBC #$7f
16ec : b0 04 __ BCS $16f2 ; (crt_fmul + 105)
16ee : c9 ff __ CMP #$ff
16f0 : d0 15 __ BNE $1707 ; (crt_fmul + 126)
16f2 : a5 1e __ LDA ACCU + 3 
16f4 : 09 7f __ ORA #$7f
16f6 : 85 1e __ STA ACCU + 3 
16f8 : a9 80 __ LDA #$80
16fa : 85 1d __ STA ACCU + 2 
16fc : a9 00 __ LDA #$00
16fe : 85 1b __ STA ACCU + 0 
1700 : 85 1c __ STA ACCU + 1 
1702 : 60 __ __ RTS
1703 : e9 7e __ SBC #$7e
1705 : 90 15 __ BCC $171c ; (crt_fmul + 147)
1707 : 4a __ __ LSR
1708 : 05 1e __ ORA ACCU + 3 
170a : 85 1e __ STA ACCU + 3 
170c : a9 00 __ LDA #$00
170e : 6a __ __ ROR
170f : 05 0b __ ORA WORK + 8 
1711 : 85 1d __ STA ACCU + 2 
1713 : a5 0a __ LDA WORK + 7 
1715 : 85 1c __ STA ACCU + 1 
1717 : a5 09 __ LDA WORK + 6 
1719 : 85 1b __ STA ACCU + 0 
171b : 60 __ __ RTS
171c : a9 00 __ LDA #$00
171e : 85 1e __ STA ACCU + 3 
1720 : f0 d8 __ BEQ $16fa ; (crt_fmul + 113)
1722 : 60 __ __ RTS
--------------------------------------------------------------------
crt_fmul8: ; crt_fmul8
1723 : 38 __ __ SEC
1724 : 6a __ __ ROR
1725 : 90 1e __ BCC $1745 ; (crt_fmul8 + 34)
1727 : aa __ __ TAX
1728 : 18 __ __ CLC
1729 : 98 __ __ TYA
172a : 65 09 __ ADC WORK + 6 
172c : 85 09 __ STA WORK + 6 
172e : a5 0a __ LDA WORK + 7 
1730 : 65 1c __ ADC ACCU + 1 
1732 : 85 0a __ STA WORK + 7 
1734 : a5 0b __ LDA WORK + 8 
1736 : 65 1d __ ADC ACCU + 2 
1738 : 6a __ __ ROR
1739 : 85 0b __ STA WORK + 8 
173b : 8a __ __ TXA
173c : 66 0a __ ROR WORK + 7 
173e : 66 09 __ ROR WORK + 6 
1740 : 4a __ __ LSR
1741 : f0 0d __ BEQ $1750 ; (crt_fmul8 + 45)
1743 : b0 e2 __ BCS $1727 ; (crt_fmul8 + 4)
1745 : 66 0b __ ROR WORK + 8 
1747 : 66 0a __ ROR WORK + 7 
1749 : 66 09 __ ROR WORK + 6 
174b : 4a __ __ LSR
174c : 90 f7 __ BCC $1745 ; (crt_fmul8 + 34)
174e : d0 d7 __ BNE $1727 ; (crt_fmul8 + 4)
1750 : 60 __ __ RTS
--------------------------------------------------------------------
crt_fdiv: ; crt_fdiv
1751 : a5 1b __ LDA ACCU + 0 
1753 : 05 1c __ ORA ACCU + 1 
1755 : 05 1d __ ORA ACCU + 2 
1757 : d0 03 __ BNE $175c ; (crt_fdiv + 11)
1759 : 85 1e __ STA ACCU + 3 
175b : 60 __ __ RTS
175c : a5 1e __ LDA ACCU + 3 
175e : 45 06 __ EOR WORK + 3 
1760 : 29 80 __ AND #$80
1762 : 85 1e __ STA ACCU + 3 
1764 : a5 08 __ LDA WORK + 5 
1766 : f0 62 __ BEQ $17ca ; (crt_fdiv + 121)
1768 : a5 07 __ LDA WORK + 4 
176a : c9 ff __ CMP #$ff
176c : f0 5c __ BEQ $17ca ; (crt_fdiv + 121)
176e : a9 00 __ LDA #$00
1770 : 85 09 __ STA WORK + 6 
1772 : 85 0a __ STA WORK + 7 
1774 : 85 0b __ STA WORK + 8 
1776 : a2 18 __ LDX #$18
1778 : a5 1b __ LDA ACCU + 0 
177a : c5 03 __ CMP WORK + 0 
177c : a5 1c __ LDA ACCU + 1 
177e : e5 04 __ SBC WORK + 1 
1780 : a5 1d __ LDA ACCU + 2 
1782 : e5 05 __ SBC WORK + 2 
1784 : 90 13 __ BCC $1799 ; (crt_fdiv + 72)
1786 : a5 1b __ LDA ACCU + 0 
1788 : e5 03 __ SBC WORK + 0 
178a : 85 1b __ STA ACCU + 0 
178c : a5 1c __ LDA ACCU + 1 
178e : e5 04 __ SBC WORK + 1 
1790 : 85 1c __ STA ACCU + 1 
1792 : a5 1d __ LDA ACCU + 2 
1794 : e5 05 __ SBC WORK + 2 
1796 : 85 1d __ STA ACCU + 2 
1798 : 38 __ __ SEC
1799 : 26 09 __ ROL WORK + 6 
179b : 26 0a __ ROL WORK + 7 
179d : 26 0b __ ROL WORK + 8 
179f : ca __ __ DEX
17a0 : f0 0a __ BEQ $17ac ; (crt_fdiv + 91)
17a2 : 06 1b __ ASL ACCU + 0 
17a4 : 26 1c __ ROL ACCU + 1 
17a6 : 26 1d __ ROL ACCU + 2 
17a8 : b0 dc __ BCS $1786 ; (crt_fdiv + 53)
17aa : 90 cc __ BCC $1778 ; (crt_fdiv + 39)
17ac : 38 __ __ SEC
17ad : a5 0b __ LDA WORK + 8 
17af : 30 06 __ BMI $17b7 ; (crt_fdiv + 102)
17b1 : 06 09 __ ASL WORK + 6 
17b3 : 26 0a __ ROL WORK + 7 
17b5 : 2a __ __ ROL
17b6 : 18 __ __ CLC
17b7 : 29 7f __ AND #$7f
17b9 : 85 0b __ STA WORK + 8 
17bb : a5 07 __ LDA WORK + 4 
17bd : e5 08 __ SBC WORK + 5 
17bf : 90 1a __ BCC $17db ; (crt_fdiv + 138)
17c1 : 18 __ __ CLC
17c2 : 69 7f __ ADC #$7f
17c4 : b0 04 __ BCS $17ca ; (crt_fdiv + 121)
17c6 : c9 ff __ CMP #$ff
17c8 : d0 15 __ BNE $17df ; (crt_fdiv + 142)
17ca : a5 1e __ LDA ACCU + 3 
17cc : 09 7f __ ORA #$7f
17ce : 85 1e __ STA ACCU + 3 
17d0 : a9 80 __ LDA #$80
17d2 : 85 1d __ STA ACCU + 2 
17d4 : a9 00 __ LDA #$00
17d6 : 85 1c __ STA ACCU + 1 
17d8 : 85 1b __ STA ACCU + 0 
17da : 60 __ __ RTS
17db : 69 7f __ ADC #$7f
17dd : 90 15 __ BCC $17f4 ; (crt_fdiv + 163)
17df : 4a __ __ LSR
17e0 : 05 1e __ ORA ACCU + 3 
17e2 : 85 1e __ STA ACCU + 3 
17e4 : a9 00 __ LDA #$00
17e6 : 6a __ __ ROR
17e7 : 05 0b __ ORA WORK + 8 
17e9 : 85 1d __ STA ACCU + 2 
17eb : a5 0a __ LDA WORK + 7 
17ed : 85 1c __ STA ACCU + 1 
17ef : a5 09 __ LDA WORK + 6 
17f1 : 85 1b __ STA ACCU + 0 
17f3 : 60 __ __ RTS
17f4 : a9 00 __ LDA #$00
17f6 : 85 1e __ STA ACCU + 3 
17f8 : 85 1d __ STA ACCU + 2 
17fa : 85 1c __ STA ACCU + 1 
17fc : 85 1b __ STA ACCU + 0 
17fe : 60 __ __ RTS
--------------------------------------------------------------------
divs16: ; divs16
17ff : 24 1c __ BIT ACCU + 1 
1801 : 10 0d __ BPL $1810 ; (divs16 + 17)
1803 : 20 1d 18 JSR $181d ; (negaccu + 0)
1806 : 24 04 __ BIT WORK + 1 
1808 : 10 0d __ BPL $1817 ; (divs16 + 24)
180a : 20 2b 18 JSR $182b ; (negtmp + 0)
180d : 4c 39 18 JMP $1839 ; (divmod + 0)
1810 : 24 04 __ BIT WORK + 1 
1812 : 10 f9 __ BPL $180d ; (divs16 + 14)
1814 : 20 2b 18 JSR $182b ; (negtmp + 0)
1817 : 20 39 18 JSR $1839 ; (divmod + 0)
181a : 4c 1d 18 JMP $181d ; (negaccu + 0)
--------------------------------------------------------------------
negaccu: ; negaccu
181d : 38 __ __ SEC
181e : a9 00 __ LDA #$00
1820 : e5 1b __ SBC ACCU + 0 
1822 : 85 1b __ STA ACCU + 0 
1824 : a9 00 __ LDA #$00
1826 : e5 1c __ SBC ACCU + 1 
1828 : 85 1c __ STA ACCU + 1 
182a : 60 __ __ RTS
--------------------------------------------------------------------
negtmp: ; negtmp
182b : 38 __ __ SEC
182c : a9 00 __ LDA #$00
182e : e5 03 __ SBC WORK + 0 
1830 : 85 03 __ STA WORK + 0 
1832 : a9 00 __ LDA #$00
1834 : e5 04 __ SBC WORK + 1 
1836 : 85 04 __ STA WORK + 1 
1838 : 60 __ __ RTS
--------------------------------------------------------------------
divmod: ; divmod
1839 : a5 1c __ LDA ACCU + 1 
183b : d0 3b __ BNE $1878 ; (divmod + 63)
183d : a5 04 __ LDA WORK + 1 
183f : d0 1e __ BNE $185f ; (divmod + 38)
1841 : 85 06 __ STA WORK + 3 
1843 : a2 04 __ LDX #$04
1845 : 06 1b __ ASL ACCU + 0 
1847 : 2a __ __ ROL
1848 : c5 03 __ CMP WORK + 0 
184a : 90 02 __ BCC $184e ; (divmod + 21)
184c : e5 03 __ SBC WORK + 0 
184e : 26 1b __ ROL ACCU + 0 
1850 : 2a __ __ ROL
1851 : c5 03 __ CMP WORK + 0 
1853 : 90 02 __ BCC $1857 ; (divmod + 30)
1855 : e5 03 __ SBC WORK + 0 
1857 : 26 1b __ ROL ACCU + 0 
1859 : ca __ __ DEX
185a : d0 eb __ BNE $1847 ; (divmod + 14)
185c : 85 05 __ STA WORK + 2 
185e : 60 __ __ RTS
185f : a5 1b __ LDA ACCU + 0 
1861 : 85 05 __ STA WORK + 2 
1863 : a5 1c __ LDA ACCU + 1 
1865 : 85 06 __ STA WORK + 3 
1867 : a9 00 __ LDA #$00
1869 : 85 1b __ STA ACCU + 0 
186b : 85 1c __ STA ACCU + 1 
186d : 60 __ __ RTS
186e : 85 03 __ STA WORK + 0 
1870 : a9 00 __ LDA #$00
1872 : 85 04 __ STA WORK + 1 
1874 : a5 1c __ LDA ACCU + 1 
1876 : f0 c9 __ BEQ $1841 ; (divmod + 8)
1878 : a5 04 __ LDA WORK + 1 
187a : d0 1f __ BNE $189b ; (divmod + 98)
187c : a5 03 __ LDA WORK + 0 
187e : 30 1b __ BMI $189b ; (divmod + 98)
1880 : a9 00 __ LDA #$00
1882 : 85 06 __ STA WORK + 3 
1884 : a2 10 __ LDX #$10
1886 : 06 1b __ ASL ACCU + 0 
1888 : 26 1c __ ROL ACCU + 1 
188a : 2a __ __ ROL
188b : c5 03 __ CMP WORK + 0 
188d : 90 02 __ BCC $1891 ; (divmod + 88)
188f : e5 03 __ SBC WORK + 0 
1891 : 26 1b __ ROL ACCU + 0 
1893 : 26 1c __ ROL ACCU + 1 
1895 : ca __ __ DEX
1896 : d0 f2 __ BNE $188a ; (divmod + 81)
1898 : 85 05 __ STA WORK + 2 
189a : 60 __ __ RTS
189b : a9 00 __ LDA #$00
189d : 85 05 __ STA WORK + 2 
189f : 85 06 __ STA WORK + 3 
18a1 : a0 10 __ LDY #$10
18a3 : 18 __ __ CLC
18a4 : 26 1b __ ROL ACCU + 0 
18a6 : 26 1c __ ROL ACCU + 1 
18a8 : 26 05 __ ROL WORK + 2 
18aa : 26 06 __ ROL WORK + 3 
18ac : 38 __ __ SEC
18ad : a5 05 __ LDA WORK + 2 
18af : e5 03 __ SBC WORK + 0 
18b1 : aa __ __ TAX
18b2 : a5 06 __ LDA WORK + 3 
18b4 : e5 04 __ SBC WORK + 1 
18b6 : 90 04 __ BCC $18bc ; (divmod + 131)
18b8 : 86 05 __ STX WORK + 2 
18ba : 85 06 __ STA WORK + 3 
18bc : 88 __ __ DEY
18bd : d0 e5 __ BNE $18a4 ; (divmod + 107)
18bf : 26 1b __ ROL ACCU + 0 
18c1 : 26 1c __ ROL ACCU + 1 
18c3 : 60 __ __ RTS
--------------------------------------------------------------------
mods16: ; mods16
18c4 : 24 1c __ BIT ACCU + 1 
18c6 : 10 10 __ BPL $18d8 ; (mods16 + 20)
18c8 : 20 1d 18 JSR $181d ; (negaccu + 0)
18cb : 24 04 __ BIT WORK + 1 
18cd : 10 03 __ BPL $18d2 ; (mods16 + 14)
18cf : 20 2b 18 JSR $182b ; (negtmp + 0)
18d2 : 20 39 18 JSR $1839 ; (divmod + 0)
18d5 : 4c e3 18 JMP $18e3 ; (negtmpb + 0)
18d8 : 24 04 __ BIT WORK + 1 
18da : 10 03 __ BPL $18df ; (mods16 + 27)
18dc : 20 2b 18 JSR $182b ; (negtmp + 0)
18df : 4c 39 18 JMP $1839 ; (divmod + 0)
18e2 : 60 __ __ RTS
--------------------------------------------------------------------
negtmpb: ; negtmpb
18e3 : 38 __ __ SEC
18e4 : a9 00 __ LDA #$00
18e6 : e5 05 __ SBC WORK + 2 
18e8 : 85 05 __ STA WORK + 2 
18ea : a9 00 __ LDA #$00
18ec : e5 06 __ SBC WORK + 3 
18ee : 85 06 __ STA WORK + 3 
18f0 : 60 __ __ RTS
--------------------------------------------------------------------
f32_to_i16: ; f32_to_i16
18f1 : 20 7b 15 JSR $157b ; (freg + 36)
18f4 : a5 07 __ LDA WORK + 4 
18f6 : c9 7f __ CMP #$7f
18f8 : b0 07 __ BCS $1901 ; (f32_to_i16 + 16)
18fa : a9 00 __ LDA #$00
18fc : 85 1b __ STA ACCU + 0 
18fe : 85 1c __ STA ACCU + 1 
1900 : 60 __ __ RTS
1901 : e9 8e __ SBC #$8e
1903 : 90 16 __ BCC $191b ; (f32_to_i16 + 42)
1905 : 24 1e __ BIT ACCU + 3 
1907 : 30 09 __ BMI $1912 ; (f32_to_i16 + 33)
1909 : a9 ff __ LDA #$ff
190b : 85 1b __ STA ACCU + 0 
190d : a9 7f __ LDA #$7f
190f : 85 1c __ STA ACCU + 1 
1911 : 60 __ __ RTS
1912 : a9 00 __ LDA #$00
1914 : 85 1b __ STA ACCU + 0 
1916 : a9 80 __ LDA #$80
1918 : 85 1c __ STA ACCU + 1 
191a : 60 __ __ RTS
191b : aa __ __ TAX
191c : a5 1c __ LDA ACCU + 1 
191e : 46 1d __ LSR ACCU + 2 
1920 : 6a __ __ ROR
1921 : e8 __ __ INX
1922 : d0 fa __ BNE $191e ; (f32_to_i16 + 45)
1924 : 24 1e __ BIT ACCU + 3 
1926 : 10 0e __ BPL $1936 ; (f32_to_i16 + 69)
1928 : 38 __ __ SEC
1929 : 49 ff __ EOR #$ff
192b : 69 00 __ ADC #$00
192d : 85 1b __ STA ACCU + 0 
192f : a9 00 __ LDA #$00
1931 : e5 1d __ SBC ACCU + 2 
1933 : 85 1c __ STA ACCU + 1 
1935 : 60 __ __ RTS
1936 : 85 1b __ STA ACCU + 0 
1938 : a5 1d __ LDA ACCU + 2 
193a : 85 1c __ STA ACCU + 1 
193c : 60 __ __ RTS
--------------------------------------------------------------------
sint16_to_float: ; sint16_to_float
193d : 24 1c __ BIT ACCU + 1 
193f : 30 03 __ BMI $1944 ; (sint16_to_float + 7)
1941 : 4c 5b 19 JMP $195b ; (uint16_to_float + 0)
1944 : 38 __ __ SEC
1945 : a9 00 __ LDA #$00
1947 : e5 1b __ SBC ACCU + 0 
1949 : 85 1b __ STA ACCU + 0 
194b : a9 00 __ LDA #$00
194d : e5 1c __ SBC ACCU + 1 
194f : 85 1c __ STA ACCU + 1 
1951 : 20 5b 19 JSR $195b ; (uint16_to_float + 0)
1954 : a5 1e __ LDA ACCU + 3 
1956 : 09 80 __ ORA #$80
1958 : 85 1e __ STA ACCU + 3 
195a : 60 __ __ RTS
--------------------------------------------------------------------
uint16_to_float: ; uint16_to_float
195b : a5 1b __ LDA ACCU + 0 
195d : 05 1c __ ORA ACCU + 1 
195f : d0 05 __ BNE $1966 ; (uint16_to_float + 11)
1961 : 85 1d __ STA ACCU + 2 
1963 : 85 1e __ STA ACCU + 3 
1965 : 60 __ __ RTS
1966 : a2 8e __ LDX #$8e
1968 : a5 1c __ LDA ACCU + 1 
196a : 30 06 __ BMI $1972 ; (uint16_to_float + 23)
196c : ca __ __ DEX
196d : 06 1b __ ASL ACCU + 0 
196f : 2a __ __ ROL
1970 : 10 fa __ BPL $196c ; (uint16_to_float + 17)
1972 : 0a __ __ ASL
1973 : 85 1d __ STA ACCU + 2 
1975 : a5 1b __ LDA ACCU + 0 
1977 : 85 1c __ STA ACCU + 1 
1979 : 8a __ __ TXA
197a : 4a __ __ LSR
197b : 85 1e __ STA ACCU + 3 
197d : a9 00 __ LDA #$00
197f : 85 1b __ STA ACCU + 0 
1981 : 66 1d __ ROR ACCU + 2 
1983 : 60 __ __ RTS
--------------------------------------------------------------------
divmod32: ; divmod32
1984 : a9 00 __ LDA #$00
1986 : 85 07 __ STA WORK + 4 
1988 : 85 08 __ STA WORK + 5 
198a : 85 09 __ STA WORK + 6 
198c : 85 0a __ STA WORK + 7 
198e : a5 05 __ LDA WORK + 2 
1990 : 05 06 __ ORA WORK + 3 
1992 : f0 4b __ BEQ $19df ; (divmod32 + 91)
1994 : a0 10 __ LDY #$10
1996 : a5 1e __ LDA ACCU + 3 
1998 : 85 08 __ STA WORK + 5 
199a : a5 1d __ LDA ACCU + 2 
199c : 85 07 __ STA WORK + 4 
199e : a9 00 __ LDA #$00
19a0 : 85 1d __ STA ACCU + 2 
19a2 : 85 1e __ STA ACCU + 3 
19a4 : 18 __ __ CLC
19a5 : 26 1b __ ROL ACCU + 0 
19a7 : 26 1c __ ROL ACCU + 1 
19a9 : 26 07 __ ROL WORK + 4 
19ab : 26 08 __ ROL WORK + 5 
19ad : 26 09 __ ROL WORK + 6 
19af : 26 0a __ ROL WORK + 7 
19b1 : a5 07 __ LDA WORK + 4 
19b3 : c5 03 __ CMP WORK + 0 
19b5 : a5 08 __ LDA WORK + 5 
19b7 : e5 04 __ SBC WORK + 1 
19b9 : a5 09 __ LDA WORK + 6 
19bb : e5 05 __ SBC WORK + 2 
19bd : aa __ __ TAX
19be : a5 0a __ LDA WORK + 7 
19c0 : e5 06 __ SBC WORK + 3 
19c2 : 90 11 __ BCC $19d5 ; (divmod32 + 81)
19c4 : 86 09 __ STX WORK + 6 
19c6 : 85 0a __ STA WORK + 7 
19c8 : a5 07 __ LDA WORK + 4 
19ca : e5 03 __ SBC WORK + 0 
19cc : 85 07 __ STA WORK + 4 
19ce : a5 08 __ LDA WORK + 5 
19d0 : e5 04 __ SBC WORK + 1 
19d2 : 85 08 __ STA WORK + 5 
19d4 : 38 __ __ SEC
19d5 : 88 __ __ DEY
19d6 : d0 cd __ BNE $19a5 ; (divmod32 + 33)
19d8 : 26 1b __ ROL ACCU + 0 
19da : 26 1c __ ROL ACCU + 1 
19dc : a4 02 __ LDY $02 
19de : 60 __ __ RTS
19df : a5 1d __ LDA ACCU + 2 
19e1 : 05 1e __ ORA ACCU + 3 
19e3 : d0 0c __ BNE $19f1 ; (divmod32 + 109)
19e5 : 20 39 18 JSR $1839 ; (divmod + 0)
19e8 : a5 05 __ LDA WORK + 2 
19ea : 85 07 __ STA WORK + 4 
19ec : a5 06 __ LDA WORK + 3 
19ee : 85 08 __ STA WORK + 5 
19f0 : 60 __ __ RTS
19f1 : a0 20 __ LDY #$20
19f3 : a5 04 __ LDA WORK + 1 
19f5 : d0 27 __ BNE $1a1e ; (divmod32 + 154)
19f7 : 18 __ __ CLC
19f8 : 26 1b __ ROL ACCU + 0 
19fa : 26 1c __ ROL ACCU + 1 
19fc : 26 1d __ ROL ACCU + 2 
19fe : 26 1e __ ROL ACCU + 3 
1a00 : 2a __ __ ROL
1a01 : 90 05 __ BCC $1a08 ; (divmod32 + 132)
1a03 : e5 03 __ SBC WORK + 0 
1a05 : 38 __ __ SEC
1a06 : b0 06 __ BCS $1a0e ; (divmod32 + 138)
1a08 : c5 03 __ CMP WORK + 0 
1a0a : 90 02 __ BCC $1a0e ; (divmod32 + 138)
1a0c : e5 03 __ SBC WORK + 0 
1a0e : 88 __ __ DEY
1a0f : d0 e7 __ BNE $19f8 ; (divmod32 + 116)
1a11 : 85 07 __ STA WORK + 4 
1a13 : 26 1b __ ROL ACCU + 0 
1a15 : 26 1c __ ROL ACCU + 1 
1a17 : 26 1d __ ROL ACCU + 2 
1a19 : 26 1e __ ROL ACCU + 3 
1a1b : a4 02 __ LDY $02 
1a1d : 60 __ __ RTS
1a1e : a5 1e __ LDA ACCU + 3 
1a20 : d0 10 __ BNE $1a32 ; (divmod32 + 174)
1a22 : a6 1d __ LDX ACCU + 2 
1a24 : 86 1e __ STX ACCU + 3 
1a26 : a6 1c __ LDX ACCU + 1 
1a28 : 86 1d __ STX ACCU + 2 
1a2a : a6 1b __ LDX ACCU + 0 
1a2c : 86 1c __ STX ACCU + 1 
1a2e : 85 1b __ STA ACCU + 0 
1a30 : a0 18 __ LDY #$18
1a32 : 18 __ __ CLC
1a33 : 26 1b __ ROL ACCU + 0 
1a35 : 26 1c __ ROL ACCU + 1 
1a37 : 26 1d __ ROL ACCU + 2 
1a39 : 26 1e __ ROL ACCU + 3 
1a3b : 26 07 __ ROL WORK + 4 
1a3d : 26 08 __ ROL WORK + 5 
1a3f : 90 0c __ BCC $1a4d ; (divmod32 + 201)
1a41 : a5 07 __ LDA WORK + 4 
1a43 : e5 03 __ SBC WORK + 0 
1a45 : aa __ __ TAX
1a46 : a5 08 __ LDA WORK + 5 
1a48 : e5 04 __ SBC WORK + 1 
1a4a : 38 __ __ SEC
1a4b : b0 0c __ BCS $1a59 ; (divmod32 + 213)
1a4d : 38 __ __ SEC
1a4e : a5 07 __ LDA WORK + 4 
1a50 : e5 03 __ SBC WORK + 0 
1a52 : aa __ __ TAX
1a53 : a5 08 __ LDA WORK + 5 
1a55 : e5 04 __ SBC WORK + 1 
1a57 : 90 04 __ BCC $1a5d ; (divmod32 + 217)
1a59 : 86 07 __ STX WORK + 4 
1a5b : 85 08 __ STA WORK + 5 
1a5d : 88 __ __ DEY
1a5e : d0 d3 __ BNE $1a33 ; (divmod32 + 175)
1a60 : 26 1b __ ROL ACCU + 0 
1a62 : 26 1c __ ROL ACCU + 1 
1a64 : 26 1d __ ROL ACCU + 2 
1a66 : 26 1e __ ROL ACCU + 3 
1a68 : a4 02 __ LDY $02 
1a6a : 60 __ __ RTS
--------------------------------------------------------------------
spentry:
1a6b : __ __ __ BYT 00                                              : .
--------------------------------------------------------------------
giocharmap:
1a6c : __ __ __ BYT 01                                              : .
--------------------------------------------------------------------
fround5:
1a6d : __ __ __ BYT 00 00 00 3f cd cc 4c 3d 0a d7 a3 3b 6f 12 03 3a : ...?..L=...;o..:
1a7d : __ __ __ BYT 17 b7 51 38 ac c5 a7 36 bd 37 06 35             : ..Q8...6.7.5
--------------------------------------------------------------------
stdout:
1a89 : __ __ __ BYT 8b 1a                                           : ..
--------------------------------------------------------------------
stdio_file:
1a8b : __ __ __ BYT ff                                              : .
