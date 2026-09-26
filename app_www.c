// www - Simplistic web-browser for OrWIN-ATMOS
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)

// A byte code virtual machine VM that renders a binary
// token encoded webpage.
// 
// Goals:
// - ease and speed of display
// - one token - one function
// - limited "CSS", <p> </p> are assigned display byte strings!
// - 
//
// It is generated from parsing html by a separate program:
//
//   ./wparse
//

// BYTE CODE
// =========
// 
//
//
// 0x00       Zero termination
//
//(0x00       INK        = black MOVED to 0x80)
// 0x0?-0x07  INK        = black...white
//
// 0x08-0x0f  CTRL       = normal control codes
//
// 0x1?-0x17  BACKGROUND = black...white
//
// 0x18-0x1f  CTRL       = normal control codes? (arrows?)
//
// 0x20-0x7f  ASCII

// -- Hibit byte code starting at 0x80; code= 0x80 + ascii - '@'
// @: INK black; 0x80 moved from 0x00
// A: <a href="..."      </a>
// B: <bold> <br/> <button> ???                    or (<a name=...       </a><
// C: <code> ... </code> ... <center> ... <caption>...?
//(D: <div - maybe not needed? ... </div>) 
// E: <em> ... </em>
// F: <form name='..."    </form>
//(G: <h1>                </h1>)
// H n: <h1> - more common? </h2>
// I: <i> .. </i> <iframe>
// J: <input name=... type=...  text,checkbox,radio,number,range,date,time,submit
// K: 
// L: <li> .. </li>
// M: <iMg> ... 
// N: <nobr> ... </nobr>
// O: <ol> .. </ol>
// P: <p> ... </p> or as </p> ?
// Q: <quote> ... </quote>
// R: <tr> ... </tr> (well formed)
// S: <select> ... <?option> ... </select> <optgroup???> <span> <strong>
// T: <table> ...  </table>
// U: <ul> ... </ul>
// V: <label> ... </label>
// W: <textarea name=...>... </textarea>
// X: <hr/>
// Y:
// Z:
// 5b:
// 5c:
// 5d:
// 5e:
// 5f:
//
// 0xa0-0xfe  ASCII, hi bit set on "breakable? chars foo-bar
//
// 0xff       END-eleemnt

#include "orwin.h"

// Minimial 8-bit "CSS":
//
// PRE:   string to print before tag content (at push time)
// POST:  string to print after tag content (at pop time)
const char* prepost[]= {
  0, 0
  // A = blue
  , BLUE
  , 0
  // Bold = red
  , RED
  , 0
  // Code = BG+black green
  , BGBLACK GREEN
  , 0
  // Div ...
  , 0
  , 0
  // Em
  , GREEN
  , 0
  // Form
  , 0
  , 0
  // G
  , 0
  , 0
  // H -- see specific 31+n
  , FLSHLN
  , FLSHLN
  // I = green
  , YELLOW
  , 0
  // J
  , 0
  , 0
  // K
  , 0
  , 0
  // Li
  , FLSHLN
  , 0
  // M
  , "[image: " // description
  , "]"
  // N
  , 0
  , 0
  // Ol = indent++
  , FLSHLN
  , FLSHLN
  // P == 
  , FLSHLN "  "
  , FLSHLN
  // Q == 
  , FLSHLN
  , FLSHLN
  // tR =
  , FLSHLN
  , FLSHLN
  // Select - color?
  , "|" // lol
  , 0
  // Table
  , FLSHLN
  , FLSHLN
  // U"""
  , 0
  // W <textarea>
  , FLSHLN
  , FLSHLN
  // X <hr>
  , FLSHLN "- - -" NL
  , FLSHLN
  // Y
  , 0
  , 0
  // Z
  , 0
  , 0
  // - special control
  // case 0x5b:
  , 0
  , 0
  // case 0x5c:
  , 0
  , 0
  // case 0x5d:
  , 0
  , 0
  // case 0x5e:
  , 0
  , 0
  // case 0x5f:
  , 0
  , 0 
  
  // H1:
  , FLSHLN // TODO: DOUBLE EVEN
  , FLSHLN
  // H2:
  , FLSHLN BGBLACK WHITE
  , FLSHLN
  // H3:
  , FLSHLN BGGREEN
  , FLSHLN
  // H4:
  , FLSHLN BGCYAN
  , FLSHLN
  // H5: foobar ....
  , FLSHLN BGYELLOW
  , 0
  // H6: fiefum ...
  , FLSHLN BGRED
  , 0
};


#define MAX_STACK 32

char stack[MAX_STACK]= {0};
char nstack= 0;

void display(char* s) {
  char c,a;
  // We use 'X to indicate X with hi-bit set
  // <n> is a single byte parameter H1...H6
  // ... is text
  // $ff is end marker that pops
  // $ff is

  goto next;

 push:
  if (nstack >= MAX_STACK) {
    // TODO: error?
  } else {
    stack[nstack++]= a;
  }
    
 next:
  a= (c= *s) & 0x7f;
  if (!c) return;
  ++s;

  if (c == 0xff) {

    // TAG END/POP
    if (!nstack) {
      // TODO: error?
    } else {
      // end formatting for TAG
      putz(prepost[stack[--nstack]*2 + 1]);
    }

    goto next;

  } else if (a >= ' ' || c < ' ') {

    // ASCII printable/control
    putchar(a);

    // TODO: breakable
    goto next;

  } else {
    // a: 0..31

    // TAG ENTER: formatting
    putz(prepost[a*2]);

    #define TAG(c) case (0x80+(c)-'@')

    // This is a list of TAGs that have special action/data
    // ... or don't require push/pop
    switch(a | '@') { 
    case 0: putchar(0); goto next;
    case 'A': // <a href="URL">...</a> 'A URL $ff ... $ff
      goto push;
    case 'H': // <h1>: 'H n ... $ff
      a= 31 + *s++; // get "n" char 1--6
      putz(prepost[a*2]); // pre
      goto push;
    case 'J': // <input>: 'J name $ff default $ff // <input>
      s= skipTill(s, 0xff); // skip "name"
      s= skipTill(s, 0xff); // skip "defalt"
      goto push;
    case 'M': // <img>: 'M' url $ff desc $ff
      s= skipTill(s, 0xff); // skip "url"
      s= skipTill(s, 0xff); // skip "descr"
      goto push;
    case 'S': // delimiter <select> & <option> & <optgroup
      // 'S ... 'S ... 'S .... $ff
      s= skipTill(s, 0xff); // skip all <options> (delimited by 'S)

      // If already inside <select> it means <option> = no push!
      if (stack[nstack-1]=='S') goto next;
      goto push;
    case 'X': // <hr/>: 'X
      goto next;

    // - tags can use
    // case 'D':
    // case 'K':
    // case 'Y':
    // case 'Z':
    // - special control
    // case 0x5b:
    // case 0x5c:
    // case 0x5d:
    // case 0x5e:
    // case 0x5f:
    default:
      // All others are structured and require END
      goto push;
    }
  }
}

void* app_www(void* state, char* line) {
  return NULL;
  (void)state; (void)line;
}
