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

// Accummulator Alphabetical Virtual Machine
// =========================================
// A simple "register" virtual machine run by
// ascii encoded op-codes.
//
// Programs are "fortran" compiled assuming no
// stack, static allocation of parameters.
//
// Each function has a parameter block:
//   0: return address
//   1: return value
//   2: parameter 1...
// 1+n: parameter n
// 2+n: local var/temp...
//   m: last local
//
// $ = most ops address value in memory (0-255)
// # = inline numeric argument count (unusual as fixed)
// 0 = only constant setter!
// 9 = only second (LOL) constant setter (-1/$ffff/true)
// all other constants need to be put in the $ memory
//=  = indicates "implemented"
//
// 0-31: global memory read value
//=' ' - nop
//  !
//= " - inline print string end with " or 0
//  # - number?
//  $ - string?
//= %& - mod and
//  '
//  ( - ??? no stack: push acc == dup 
//  ) - ??? no stack: drop
//= *+ - mul/add $
//  , - half a cons?
//= - - sub $
//= . - print number
//= '/' - div $
//= 0 - acc = 0 nul/nil
//  1234 - local var read  | single byte read/write "frame"
//  5678 - local var write | can use 2 byte L/S $ instead
//= 9 - acc= -1 
//  : - define?
//  ; - return /enddef
//  <=> - cmp - do we need all?
//  ? - have value == C ! 0=>-1 any=>0
//  @ - read *$ - Load
//= A - car acc= acc[0] == @
//  B
//  C - cons(acc, $) - a variant of alloc cell and , ???
//= D - cdr acc= acc[1] == 2 + @, lol
//= E - emit char = putchar(acc)
//= F - free *$
//  G - geap ?
//  H - heap ?
//= IJ - inc/dec acc TODO: or $ (and load it in acc)
//= K - key (waits?)
//= L - load $
//= M - $= malloc #
//= N - newline 
//= O - output (emit #)
//= P - print acc as string
//  Q
//  R - recurse (save current frame, malloc?)
//= S - store $
//  T - type acc chars from $ (?)
//  U - null? === ?
//  V
//( W - write var string $)
//  XY
//  Z - zero # (fill)
//
//  [ - shift left $
//  '\'
//  ] - shift rigth $
//= ^ - xor $
//  _ - dynamic lookup(acc) => addr
//  ` - conditional non-zero loop jump backwards (0-255)
//  { - shift left #
//= | - or $
//  } - shift right #
//= ~ - inverse (oric: gray)
//  - DEL not visible (oric: full block)
//
// $80-$bf (64) conditional relative jump (forward)
// $bf-$ff (64) subroutine calls (vectorized)

// GLOBAL "OS" API
// ===============
// TODO: cd ls (first/next) R/O user lock truncate
//
// a - block in
// b - block out
// c - create window
// d - file descriptor open (can kill it!)
// e - execute/system - external programs hexdump
// f - file open/close
// g - gotoxy
//(h - height)
// i - input from file (#0 => freadline)
// j - jinfo (version, stats, file size, disk free, xywh v)
// k - kill window/file/free memory?/flush buffers/exit if 0
// l - line input (message/event read)
// m - message/event to $ or #
// n - nreceive
// o - output to file (#0 => 0 terminated, otherwise count)
// p - printf or generic small formatting printers
//(q - quit (exit(#) program) see "k")
// r - restore window
// s - save window
//(t - time, see j)
//(u - status, see j)
//(v - vvindow pointer, see j)
//(w - width, see j)
//(xy- cursor, see j)
// z - fill A over X Y
   
// PROGRAM MEMORY LAYOUT
// ---------------------
// There may be several programs at any time and any
// program could have several instances. So, each
// execution of a process will depend on a few zero
// page globals. The interpretive loop runs for at most
// 4 ms. These are restored/saved by the multi-tasker.
//  
//   // - pure byte addressing program
//   char      *program;
//   // - program as vectors (this 
//   #define   VPROGRAM ((uint16_t*)program)
//
//   // - slots of vars/constants indexed by 0-255 max
//   uint16_t  *vars; 
//   
//   // - program running info
//   char      *ipc;


// PROGRAM VECTORS
// ---------------
// This is a vector interpretation of program bytes.
// Each is 2 bytes uint16_t relative to 0==program
// Patch at load time to actual addresses, until
// a final $0000 subroutine vector.

// 0: 1 byte index C=start of constants, N=number
// 1: P=start of actual program
// 2: subroutine code vectors... (max 64)
//    ..... relocated subs
// C: constant to copy start
//    ..... relocated constants! (like str ptr)
//    $0000 = end marker, relocate till here
//    .....
// P: program start
//    ...init...
//    jmp main (M)
//    ...subs...
// M: main program
//    rts
// S: strings/data referre to by relocated constants
//    ....

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
