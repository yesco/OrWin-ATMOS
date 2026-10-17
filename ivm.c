// ivm.c - Indirect Virtual Machine
// (C) 2026 Jonas S Karlsson

//
// 
// ultrafast dispatch?
//
// ;;; 23c - called if 
// ;;; call if AX is set
// next:
//   tay
//
// ;;; 21c - if in ZP (otherwise +2)
// ;;; call if Y is undisturbed (passthroug)
// nextay:
//   lda $ffff
//   inc nextay+1
//   asl
//   bcs enter
//   sta jump+1
// jump:
//   jmp (VECTOR)
//
// ;;; +40c - BAD! subroutine
// enter:
//   sta sub+1
//
//   ;; 5c? - save "AX"
//   tya
//   pha
//
//   ;; 22c - store current "pc"
//   lda nextay+1
//   sta (funparamoffset),y
//   iny
//   lda nextay+1
//   sta (funparamoffset),y
//   iny
//
//   pla
// sub:
//   jmp (PROGRAMM)




char* ipc;
char op;
unsigned int acc= 0;

#define PAR (MEM(*ipc))

// perl -e 'foreach $c (32..128) { printf("%c", $c); }'

// Accummulator Alphabetical Virtual Machine
// =========================================
//=' ' - nop
// !
//=" - inline print string end with " or 0
// # - number?
// $ - string?
//=%& - mod and
// '
// ( - push acc == dup
// ) - drop
//=*+ - mul/add $
// ,
//=- - sub $
//=. - print number
//='/' - div $
//=0 - acc = 0 nul/nil
// 12345678 - local var read ?
//=9 - -1
// :
// ; - return /enddef
// <=> - cmp ???
// ? - conditional jump?
// @ - read *$ - Load
//=A - car acc= acc[0] == @
// B
// C - cons(acc, $) - a variant of alloc cell and , ???
//=D - cdr acc= acc[1] == 2 + @, lol
//=E - emit char (acc)
//=F - free *$
// GHIJ
//=K - key
//=L - load $
//=M - $= malloc #
//=N - newline 
//=O - output (emit #)
//=P - print acc as string
// Q
// R - recurse (save current frame, malloc?)
//=S - store $
// T - type n chars from $ (?)
// U - null?
// V
//(W - write var string $)
// XY
// Z - zero # (fill)
// [ - shift left $
// '\'
// ] - shift rigth $
//=^ - xor $
// _ - dynamic lookup(acc) => addr
// `
// abcdefghijklmnopqrstuvwxyz - global API
// { - shift left #
//=| - or $
// } - shift right #
//=~ - inverse
// 
   
void ivm() {
  // return here if ate one parameter byte
 pnext:
  ++ipc;
 next:
  switch((op= *ipc++)) {
  case 0:   break;
  case $ff: break;

    // Parameter binary ops
  case '+': acc+= PAR; goto pnext;
  case '-': acc-= PAR; goto pnext;
  case '*': acc*= PAR; goto pnext;
  case '/': acc*= PAR; goto pnext;

  case '&': acc&= PAR; goto pnext;
  case '|': acc|= PAR; goto pnext;
  case '^': acc^= PAR; goto pnext;

  case 'L': acc= PAR;  goto pnext;
  case 'S': PAR= acc;  goto pnext;
                         
  case 'M': PAR= malloc(acc); goto pnext;
  case 'F': free(PAR); goto pnext;

    // No parameter ops
  case ' ': goto next; // nop
  case '0': acc=  0; goto next;
  case '9': acc= -1; goto next;

  case 'A': acc= MEM(acc);   goto next;
  case 'D': acc= MEM(acc+2); goto next;

  case 'I': ++acc; goto next;
  case 'J': --acc; goto next;
  case '~': acc= ~acc; goto next;
  // case '!': acc= !acc; goto next;
      
  case 'K': acc= getchar(); goto next;
  case 'E': putchar(acc); goto next;
  case 'N': putchar('\n'); goto next;
  case 'P': printf("%s", acc); goto next;
  case 'O': putchar(PAR); goto pnext;
  case 'W': printf("%s", *PAR); goto pnext; // PARAM!
  case '.': printf("%d", acc); goto next;
  case '"': do putchar(*ipc++); while(*ipc); goto pnext;

  default:
    // hibit? => SUBROUTINE/INTERPRETATION
    if (op & 0x80) {
      if (op & 0x40) {
        // SUBROUTINE
      } else {
        // 64 RELJUMP
      }
    } else if (op < 32) {
      // GLOBAL READ ACCESS
      acc= MEM(op & 0xf);
    } else {
      // UNKNOW
    }
  }
}
