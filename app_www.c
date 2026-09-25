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
// G: <h1>                </h1>
// H: <h2> - more common? </h2>
// I: <input name=... type=...  text,checkbox,radio,number,range,date,time,submit
// J: <i> .. </i> <iframe>
// K: 
// L: <li> .. </li>
// M: <iMg> ... 
// N: <nobr> ... </nobr>
// O: <ol> .. </ol>
// P: <p> ... </p> or as </p> ?
// Q: <quote> ... </quote>
// R: <tr> ... (optional? </tr>)
// S: <select> ... <option> ... </select> <optgroup???> <span> <strong>
// T: <table> ...  </table>
// U: <ul> ... </ul>
// V:  TODO: <label>name</label
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


