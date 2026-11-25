# MeteoriC-65 C Manual

<!-- toc -->

- [Introduction](#introduction)
  - [Context and features](#context-and-features)
  - [Goals](#goals)
  - [Non-goals](#non-goals)
- [What MeteoriC-65 supports](#what-meteoric-65-supports)
- [The compiler](#the-compiler)
- [Program structure](#program-structure)
  - [Preprocessor](#preprocessor)
- [Types](#types)
- [Variables and declarations](#variables-and-declarations)
- [Expressions](#expressions)
  - [Operators](#operators)
  - [Precedence: left to right, never "up"](#precedence-left-to-right-never-up)
  - [Assignment forms](#assignment-forms)
- [Statements](#statements)
- [Functions](#functions)
- [Arrays](#arrays)
- [Pointers and strings](#pointers-and-strings)
- [Structs](#structs)
  - [Definition and variables](#definition-and-variables)
  - [Fields](#fields)
  - [Arrays of structs](#arrays-of-structs)
  - [`__striped` and `-S`](#__striped-and--s)
- [Input and output](#input-and-output)
- [Errors](#errors)
- [Efficiency](#efficiency)
  - [Increment, assign](#increment-assign)
  - [Multiply, divide](#multiply-divide)
  - [Arrays, pointers, structs (reads; writes cost about the same)](#arrays-pointers-structs-reads-writes-cost-about-the-same)
  - [Branches](#branches)
  - [Loops](#loops)
  - [memset loops](#memset-loops)
  - [Calls](#calls)
- [Optimizations](#optimizations)
- [Not supported: a summary](#not-supported-a-summary)

<!-- /toc -->

## Introduction

MeteoriC-65 is a limited C compiler (subset) for the OrWin-ATMOS
project.  Its used to run several cooperative programs at the same
time using a virtual machine (VM). This is similar to the
`UCSD-Pascal` system, however creating smaller byte-code programs, and
with a faster virtual machine that interprets them.  It mainly
focusses on a small virtual machhine (IVM) for the 6502 (sim65, Oric
Atmos): a bytecode VM, a 6502 JIT, with compilers for C and Pascal.
These are small enough to run on the 6502 itself.

Currently, it probably generates the smallest binaries for simple
programs, with minimal overhead. The code generated for the supported
C-subset is more efficient than `CC65` nearing `oschar64` in
performance. It performs limited peep-hole optimizations on byte-code
and has optimized fused operators for fast loops. The JIT outputs
variants of that optimizes the generated 6502 code both in size and
speed.

The way it functions: `parse` with the grammar `c.bnf` turns a C file
into an `.ivm` bytecode image. The design, from the `parse.c` header:
a tiny declarative grammar both *recognises* the source and *emits*
the bytecode in the same rules, matched by an iterative backtracking
engine. It's written iteratively, thus isn't a call-recursive descent
parser implementation but its process is basiclly intepretes BNF
rules. It coiuld be said to be "a direct descendant of META II": no
AST, no separate IR, no register allocator, just "match this pattern,
drop these bytes". The grammar *is* the compiler: a new idiom (a loop
form, a fused op) is one more line in `c.bnf`. There is also a smaller
`p.bnf` implementing a similar, but smaller pascal compiler.

The bytecode (`ivm.c`, `ivm-vm.md` implementation notes) is for an
ACCumulator machine with ASCII readable opcodes (`L` load, `S` store,
`+`, `I` increment, ...), a register for every variable (up to 254
slots: globals, frames, constants), a static compressed stack (FORTRAN
style: each function's frame is fixed memory, frames of functions
never active together share it), call vectors, and loop ops for the
improving interpretation speed and minimizing code of/for common
counting loops.

| backend   | what                                | command                    |
|-----------|-------------------------------------|----------------------------|
| `ivm`     | the VM in C (cc65 build, sim65)     | `./ivm prog`               |
| `ivm-asm` | the VM in 6502 assembler            | `./ivm-asm prog`           |
| `ivm-jit` | a 6502 JIT: bytecode to native code | `./ivm-jit prog [-z]`      |
| `jit`     | gen native code and run on a 6502   | `./jit prog ...            |
|`ivm-count`| virtual machine with "profiling"    | `./ivm-count prog ...      |
|`ivm-trace`| virtual machine with tracing of ops | `./ivm-trace prog ...      |

The same `.ivm` file runs on all of them. The JIT variants generate
actual binary 6502 machine code that is executed directly, or can be
saved as a stand-alone program, effectively making it a native
compiler. It's now, working for generating .sim files for `sim65`.


### Context and features

- The compiler either runs on the host (gcc / clang), or on the 6502
  (compiled by `cc65`). 
- Small: the cc65 build of `parse` is about 38 KB; programs are bytecode
  (a statement is a few bytes), the JIT makes 6502 code at load.
- Several Errors are reported and skipped, for the whole file in one run.

- Actual execution speed comes from the specialized optimizing
  grammar: common idioms (counting loops, `memset` loops, `x++`,
  compare-and-branch, array indexing, struct fields) have their own
  ops; the JIT turns them into tight 6502 code (`LDY i / LDA a,Y`).


### Goals

- Small code first (the compiler, the VM, the programs), then speed.
- Rules, templates, the grammar (data) rather than C code.
- A useful C subset for small 6502 programs: integers, chars, arrays,
  pointers, structs, functions, the usual statements.
- Standard compliant as much as possible. Restricted to be safe.
- Errors, not silent surprises: what does not compile is reported.
- Simple optimizations as early as possible. Get's you far!


### Non-goals

- NO: Full ANSI C, or compiling existing C programs unchanged.
- NO: Dynamic stack: recursion, reentrant functions.
- NO: `long`, floating point, signed arithmetic.
- NO: Full standard library.
- NO: Classic optimizing compiler (AST, IR, register allocation).

It is not a complete C compiler: a statement that matches no rule is
reported as an error (file, line, a caret), parsing goes on with the
next statement, and no `.ivm` is written. So most things that are not
supported simply do not compile. The dangerous cases are the few that
compile but do something other than C would: they are marked **NOTE
(silent)** below. In general it is intended that they would be
detected and disallowed/give error. Report any differences of
incorrect code execution.

This manual describes the language from a programmer's view: what
works, what does not (but that with a rewrite can can work), and what
is cheap or expensive on each backend. Every example is checked by a
file in [`Tests/Compiler/`](Tests/Compiler/) (`man-*.c`, run
[`./run.sh`](Tests/Compiler/run.sh) there): what works compiles and
gives what gcc gives, what does not is a reported error.

---


## What MeteoriC-65 supports

| area | |
|------|---|
| types        | `word` `unsigned int` (16 bit), `char` `unsigned char` (8 bit), `void`, pointers, `struct`, for convenience `int` but comparisions may not agree, TODO: warnings |
| declarations | globals, locals, several per line, initializers, `const`, `static`, `register` |
| expressions  | `+ - * / % & ^ \|` `<< >>`, compares (as values too), `!`, casts, calls anywhere, `sizeof` |
| assignment   | `=` `+=` `-=` `++` `--` |
| statements   | `if` / `else`, `while`, counting `for` loops (up, down, `<` `<=`, a variable limit or step), blocks, `return` |
| functions    | parameters, `int` / `char` / pointer results, calls in expressions and as arguments |
| arrays       | `char` and `int` arrays, initializers (`{...}`, `"..."`, size from the initializer), `sizeof`, any index expression |
| pointers     | `char*` `word*` `struct *` `int*` `*p` `*p++` `p[i]` `p++` `p += n`, strings, `malloc` / `free` / `memset` |
| structs      | fields `s.f` `p->f`, initializers, pointers, chains (`a.next->v`, `ps[i]->x`, `mk()->x`), arrays of pointers, arrays of structs (SoA, `__striped`) |
| output       | `printf` (text, one number, a string), `putchar` `puts` `fputs` `getchar` `putz` `putn` |
| preprocessor | `#define` (numbers, strings), simple: `#ifdef` `#ifndef` `#if` `#else` `#endif`, `__MC65__` |
| tools        | error recovery, `-O` (packing, peephole), `-S` (striping), `-v` (bytecode per statement) |

---



## The compiler

On host (does "more" optimizations, and more verbose output):

    ./pc prog          builds parse.x and runs parse.x
                       `(clang -DRUN -DVERBOSE -DSYMS)` and runs
                       `parse.x -O -v c.bnf prog.c`

    parse.x [-v ...] [-O] [-S] c.bnf prog.c

On simulated 6502:

    ./parse prog       builds parse.sim (cc65) and runs it in sim65:
                       `parse.sim -v c.bnf prog.c`

Both write `prog.ivm`; a `-DRUN` build also runs it (on the VM in C).


| flag | effect |
|------|--------|
| `-O` | optimize: packs variables (registers 0..31, globals, frames by call graph), peephole |
| `-S` | stripe all arrays of structs (faster on the JIT, slower on the VMs: see [`__striped` and `-S`](#__striped-and--s)) |
| `-v` | `-v -v` shows each statement's source and bytecode (`GEN:`), `-v -v -v` traces the rules |

The compiler predefines `__MC65__` (the `.ivm` version) and
`__TARGET_6502__` (1), so a file can be compiled with gcc too:

```c
#ifndef __MC65__
#define __striped          /* our keyword, gcc does not know it */
#endif
```

All the `test-*.c` files and [`Tests/Compiler/man-*.c`](Tests/Compiler/) are written that
way: gcc's output is the reference (except where `int` size shows, see [Types](#types)).

---



## Program structure

- Functions and global declarations at the top, `main` is the entry.
- Comments: `/* */` and `//`.
- `#include` lines are skipped (there is a small fixed library, see [Input and output](#input-and-output)).
- A function must be **defined before it is called**: there are no
  prototypes.

**NOTE:** no prototypes; order the functions callee first.

```c
int f(int a);                 /* error: cannot parse */
int main(void) { return f(1); }
int f(int a) { return a + 1; }
```

### Preprocessor

```c
#define SIZE 8                 /* numbers */
#define MASK 0x0f
#define GREETING "hello"       /* strings */

#ifdef __MC65__                /* our compiler, or gcc */
  #define TARGET 6502
#else
  #define TARGET 0
#endif

#if 0
  anything, also nested #if ... #endif
#endif

char buf[SIZE];

int main(void) {
  int x;
  x = 255 & MASK;              /* 15 */
  puts(GREETING);
  printf("%d\n", TARGET);      /* 6502 */
  printf("%d\n", __TARGET_6502__);
  return 0;
}
```

| | |
|---|---|
| `#define NAME number` / `"string"`  | a constant (a `const`, no storage) |
| `#ifdef` `#ifndef` `#else` `#endif` | nested ones too |
| `#if 0` `#if 1`                     | a number |
| `__MC65__` `__TARGET_6502__`        | predefined |
| `#pragma timed "name"`              | cycle marks (the JIT on the host simulator) |
| other `#` lines (`#include`)        | skipped |

Examples: [`man-pp.c`](Tests/Compiler/man-pp.c).

**NOTE:** `#define` takes one number or string (no expressions, no
arguments: use a `const int`, a function).

**NOTE (silent):** `#if NAME` is always true (only a number is
evaluated): use `#ifdef`.  *TODO: evaluate a name.*

---



## Types

| type | size | notes |
|------|------|-------|
| `word`, `unsigned int`, `int` | 16 bit | all arithmetic is **unsigned** 16 bit |
| `char`, `unsigned char`       | 8 bit  | **unsigned**: 0..255, wraps |
| `void`                        |        | function results / params |
| `char *`, `int *` (any `T *`) | 16 bit |                           |
| `struct NAME`, `struct NAME *`| slots  | see [Structs](#structs) |

`sizeof(char)` is 1, `sizeof(int)` and any pointer 2. `sizeof(struct
pt)`, `sizeof(p)` of a struct variable and `sizeof(a)` of an array of
structs (plain or `__striped`) are their size in bytes: 2 per field.

**NOTE (silent):** `int` is 16 bits and unsigned. Compares are unsigned,
`%d` prints unsigned, and there are no negative literals:

```c
int x;

x = 0 - 1;               /* -1 is not a literal: write 0 - 1 */
if (x < 0) ...           /* never true: x is 65535 */
printf("%d\n", x);       /* prints 65535 */
x = 300 * 300;           /* 24464 (90000 & 0xffff) */
```

Test the top bit when you need a sign: `if (x & 0x8000)`.

**NOTE:** not supported: `long`, `short`, `float`/`double`, `signed`,
`typedef`, `enum`, `union`, 2D arrays (`int m[3][3]`).

Examples: [`man-types.c`](Tests/Compiler/man-types.c), errors: [`man-decl-err.c`](Tests/Compiler/man-decl-err.c).

---



## Variables and declarations

```c
int a, b;                 /* several in one */
int c = 5, d = 6;         /* with initializers */
const int K = 7;          /* a named constant: no storage */
register int r;           /* a global with -O: one of the 32 register slots
                             (gcc does not allow register at the top) */
static int s;             /* accepted */
```

- Globals start at 0.
- Locals live in their function's frame. Frames are fixed memory (no
  stack, see [Functions](#functions)), so a local keeps its value between calls; do not rely
  on it, but `static` locals work for that reason.
- Locals: an initializer may be any expression: `int x = f(2) + 1;`.
- Declarations may come after statements in a block.

**NOTE:** a global initializer must be a constant (a number, a string,
a `#define` / `const`, another global's name):

```c
int g = 5;                /* ok */
int h = 2 + 3;            /* error: global initializer: a constant only */
char *code = (char *)0x500; /* error too: the cast is an expression */
char *code = 0x500;         /* ok (gcc wants the cast here) */
```

**NOTE:** writing to a constant (variable) is an error, also the name
of an array or a static struct (it is the address), as well as a
#defined name:

```c
#define N 3
const int two = 2;
int w[4], v[4];
N = 4;     two = 3;     two++;     w = v;     /* error: const modified */
```

Examples: [`man-decl.c`](Tests/Compiler/man-decl.c), errors: [`man-decl-err.c`](Tests/Compiler/man-decl-err.c).

---



## Expressions


### Operators

| works | |
|-------|---|
| `+ - * / % & ^ \|` `<< >>`         | binary |
| `< <= > >= == !=`                  | give 0 / 1, also as values: `x = a < b;` |
| `!x`                               | binds tight: `!a + 1` is `(!a) + 1` |
| `(char)x` `(char *)p` `(int *)p`   | casts |
| `f(a, b)`                          | calls, also in expressions and as arguments: `add(f(1), f(2))` |
| `sizeof(T)` `sizeof(a)`            | see [Types](#types), [Arrays](#arrays) |
| `0x1f` `0b101` `017` `0o17` `'a'` `'\n'` | literals (`017` and `0o17`: octal, as in C) |

**NOTE:** not supported in expressions: unary minus (`-x`, `-5`: write
`0 - x`), `~x` (write `x ^ 0xffff`), `&&` `||` (nest the `if`s), `?:`,
assignments (`b = (a = 3)`, `a = b = 3`), `x++` / `--x` inside an
expression, `*(p + 1)` (write `p[1]`), `&x` of a plain variable.


### Precedence: left to right, never "up"

The precedence levels are strictly adhered to, but restricted to what
is only very efficient in a `train of operators` manipulating the
transformed value.  An expression is read **left to right**, and after
an operator only an operator of the **same or a lower** level may
follow. A tighter one after a looser one is an error; it never allows
to express a semanticially errorious calculation. So result is always
correct. This optimizes efficiency, and exceptions can be made using
parenthesized expressions, or using an explicitly defined temp
variable (equivalant).

The levels, tightest first:

| level | operators |
|-------|-----------|
| 0 | the operands: `!x` `*p` `(char)x` `sizeof(T)` `f(a)` `a[i]` `p->f` `( ... )` |
| 1 | `* / %` |
| 2 | `+ -` |
| 3 | `<< >>` |
| 4 | `< <= > >=` |
| 5 | `== !=` |
| 6 | `&` |
| 7 | `^` |
| 8 | `\|` |

Level 0 always binds tightest: `!a + 1` is `(!a) + 1`, `*p * 2` is
`(*p) * 2`. `++` and `--` are not operators here but statements ([Assignment forms](#assignment-forms)).

So C's precedence holds wherever the expression parses:

| expression | |
|------------|---|
| `a * b + c` | ok: `+` is looser than `*` |
| `a - b - c` | ok: same level, left to right |
| `a + 1 < b` | ok |
| `(a & b) == c` | ok |
| `a + b * c` | **error**: `*` after `+` |
| `a < b + 1` | **error**: `+` after `<` |
| `a & b == c` | **error** (in C this is `a & (b == c)`, rarely meant) |

Rewrite with parentheses, or reorder so the tighter part comes first:

```c
x = 35 + 7 * 4;          /* error */
x = 35 + (7 * 4);        /* 63 */
x = 7 * 4 + 35;          /* 63, the same code */
if (a < b + 1) ...       /* error */
if (a + 1 <= b) ...      /* rewrite: the arithmetic first */
if (a <= b) ...          /* or simpler */
```

Parentheses make it correct, and we can sometimes reorder calculations
to avoid using extra temps: `35 + (d * a)`, `(d * a) + 35` and `d * a
+ 35` compile to the same bytecode.


### Assignment forms

| works | |
|-------|---|
| `x = E;` | |
| `x++;` `x--;` `++x;` `--x;` | statements only |
| `x += E;` `x -= E;` | |
| `a[i] = E;` `p[i] = E;` `*p = E;` `s.f = E;` `p->f = E;` | |

`x++` and `x--` are the cheapest: one op (`I x`, `J x`), 2 bytes. Use
them rather than `x += 1`: half the cycles on every backend ([Increment, assign](#increment-assign)):

| statement | bytecode | ivm | ivm-asm | JIT |
|---|---:|---:|---:|---:|
| `x++;` (also `x = x + 1;`) | 2 B | 325 | 80 | 10 |
| `x += 1;` | 6 B | 536 | 196 | 21 |
| `x += 2;` | 2 B | 255 | 83 | 15 |
| `x += d;` | 3 B | 479 | 112 | 22 |

**NOTE:** no `*= /= %= &= |= ^= <<= >>=`: write `x = x * 2;`.
**NOTE:** on array elements only `=`: `a[i]++` and `a[i] += 2` are
errors, write `a[i] = a[i] + 2;` (struct fields: see [Structs](#structs)).

Examples: [`man-expr.c`](Tests/Compiler/man-expr.c), errors: [`man-expr-err.c`](Tests/Compiler/man-expr-err.c).

---


## Statements

| works | |
|-------|---|
| `if (E) S` `if (E) S else S` `else if` | |
| `while (E) S` | |
| `for (i = A; i < B; i++) S` | also `<=`, a var or const limit |
| `for (i = N; i; i--)` `for (i = N; i > 0; i--)` `for (i = N; i-- > 0; )` | counting down |
| `for (i = A; i < B; i += d)` | step a variable |
| `{ ... }` | blocks, nested |
| `return E;` `return;` | |

**NOTE:** not supported: `do ... while`, `break`, `continue`,
`switch`, `goto` and labels, `for (;;)`, `for (int i = 0; ...)`, `for
(...; i += 2)` (a constant step: put it in a variable).
*TODO: `break` / `continue` (cheaply: a jump per loop to patch), or
`label:` / `goto` as a first step.*

Without `break`: put the loop in a function and `return`, or fold the
exit into the condition:

```c
for (i = 0; i < n; i++) if (a[i] == x) break;     /* error */

i = 0;                                            /* ok */
while (i < n) { if (a[i] == x) n = i; else i++; }
```

Examples: [`man-stmt.c`](Tests/Compiler/man-stmt.c), errors: [`man-stmt-err.c`](Tests/Compiler/man-stmt-err.c).

---


## Functions

```c
int add(int a, int b) { return a + b; }
void hello(void) { puts("hi"); }
struct pt *mk(int x) { ... return p; }      /* a pointer result */
```

- Parameters and results are `int`, `char` or pointers.
- Calls work as statements, in expressions and as arguments.
- Each function has a fixed frame: its parameters and locals at fixed
  slots (with `-O` frames of functions never active at the same time
  share memory).

**NOTE (silent): no recursion.** A function calling itself (directly or
not) overwrites its own frame and does not come back:

```c
int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }
/* fact(5) runs forever */
```

Write it as a loop ([`man-func.c`](Tests/Compiler/man-func.c): `fact` with a `while`).
*TODO: report a call of a function to itself (directly or not) as an
error; later: tail calls, recursion with frames on a stack.*

Examples: [`man-func.c`](Tests/Compiler/man-func.c), errors: [`man-stmt-err.c`](Tests/Compiler/man-stmt-err.c).

---


## Arrays

```c
char buf[80];                 /* zeroed */
int w[10];
int t[] = {1, 2, 3};          /* the size from the initializer */
int z[4] = {1, 2};            /* the rest 0 */
char msg[] = "hello";
#define N 16
int q[N];                     /* a #define or const size */
```

- Arrays are **static**, also inside a function: zeroed (or initialized)
  once, at load, not on each call.
- Initializers: constants only.
- `sizeof(a)` is the size in bytes.
- An array name is its address: pass it to an `int *` / `char *`
  parameter, assign it to a pointer.
- Indexing: `a[i]`, `a[i + 1]`, `a[3]`, both reading and writing.

**NOTE (silent):** a local array keeps its contents between calls; its
initializer is not run again:

```c
int f(void) { int w[2] = {5, 0}; w[0] = w[0] + 1; return w[0]; }
/* 6, 7, 8, ...: C would give 6 every time */
```

*TODO: require `static` on a local array with an initializer (then it is
standard C).*

**NOTE:** 2D arrays are not supported; index by hand: `m[r * COLS + c]`.

Examples: [`man-array.c`](Tests/Compiler/man-array.c), errors: [`man-decl-err.c`](Tests/Compiler/man-decl-err.c).

---


## Pointers and strings

```c
char *s; int *p; int n;
s = "hello";                 /* a string literal */
p = w;                       /* an array */
n = *p;  *p = 5;             /* int * reads / writes words, char * bytes */
n = p[3];  p[i] = 7;
p++;  p += 2;                /* int *: moves 2 bytes per element */
while (*s) s++;              /* a string walk */
c = *s++;                    /* char * */
p = malloc(10);  free(p);    /* the size in bytes */
memset(buf, 0, 80);
if (p == 0) ...
```

**NOTE (silent):** pointer arithmetic in an *expression* is not scaled:
`w + 1` is one **byte** after `w`, also for an `int *`. Only the
statements `p++` `p--` `p += n` `p = p + 1` scale:

```c
sw(w, w + 1);       /* passes &w[0] + 1 byte: wrong for int */
q = w; q++;         /* ok: q is &w[1] */
sw(w, q);
```

*TODO: report `ptr + n` on an `int *` (or a word struct pointer) as an
error, until it is scaled.*

**NOTE:** `&` works only on arrays and static structs (where it is the
name itself): `&x` of a variable is an error. `*(p + i)`: write `p[i]`.

Examples: [`man-pointer.c`](Tests/Compiler/man-pointer.c), errors: [`man-decl-err.c`](Tests/Compiler/man-decl-err.c).

---


## Structs


### Definition and variables

```c
struct pt { int x; int y; };
struct node { int v; struct node *next; };   /* a pointer to itself: ok */

struct pt p;              /* static, zeroed */
struct pt g = {5, 6};     /* constants, the rest 0 */
struct pt *q;
```

- A struct is declared at the top (not inside a function).
- Every field takes a 16 bit cell. A `char` field is a byte as in C
  (`v.c = 300;` reads back 44): the low byte of its cell, read and written
  with the byte ops `[ ]`, in every kind of struct (a variable, a pointer,
  an array of structs plain or `__striped`).
- A struct variable is static (like an array): its name is its address,
  so `q = &p;` `q = p;` and `move(&p, 3)` all work.
- Different structs may have fields with the same name.

**NOTE:** not supported: `struct s x, y;` (one variable per declaration),
`struct s { ... } x;` (declare the struct, then the variable), struct
definitions inside a function, struct assignment `a = b` (an error),
structs by value as parameters / results (pass pointers), nested struct
values (a pointer field is fine), `union`.


### Fields

```c
p.x = 3;          n = p.y;         p.x++;     p.x += 5;
q->x = 3;         n = q->y;        q->x++;    q->x += 5;
```

Chains work for loads and stores, after any pointer: a field, an array
element, a call:

```c
a.next->v = 7;
n = q->next->next->v;
ps[i]->x = 5;            /* struct pt *ps[4]: an array of pointers */
n = mk(6)->x;            /* a function returning struct pt * */
```


### Arrays of structs

```c
struct pt a[16];
a[i].x = 5;   n = a[i].y;   a[i].x += 2;   a[2].y++;
```

An array of structs is stored **SoA** (structure of arrays): all the `x`
of the elements in a row, then all the `y`. `a[i].x` is then just
"element i of the x array": exactly as fast as an int array.

- Static only (bss, zeroed), at most 254 elements, no initializer.
- The index: a variable or a constant for everything above; an expression
  (`a[i + 1].x`) for loads and `=` only (`a[i + 1].x += 2` is an error:
  as for plain arrays).

**NOTE:** because of SoA, `a` alone has no meaning: no pointer to the
array or to an element, `a[i]` without a field, `a.x` (no index), passing
`a` to a `struct pt *`: all errors (in C layout they would be wrong
silently).

```c
q = a;  q = &a[1];  n = a.x;  n = a[1];  a[2] = n;    /* errors */
```

For a list of structs to pass around, use an array of pointers
(`struct pt *ps[N]`, each one `malloc`ed): 10.2.


### `__striped` and `-S`

```c
__striped struct pt a[32];
```

A striped array also splits each field into its low bytes and its high
bytes. On the JIT an element is then
`LDY i / LDA lo,Y / LDX hi,Y`. `parse -S` stripes every array of structs.
It is not the default, because the interpreters get slower:

[`test-bench-striped-struct.c`](test-bench-striped-struct.c) (32 particles, 200 rounds), sim65 cycles:

| | plain | striped |
|---|---|---|
| JIT `-z` | 2.90 M | 1.43 M |
| `ivm-asm` | 12.78 M | 13.67 M |
| `ivm` | 32.14 M | 42.39 M |
| bytecode | 204 B | 214 B |

Use `-S` / `__striped` when the program runs on the JIT.

Examples: [`man-struct.c`](Tests/Compiler/man-struct.c), [`test-struct.c`](test-struct.c), errors:
[`test-struct-err.c`](test-struct-err.c).

---


## Input and output

| works | |
|-------|---|
| `putchar(c)` `putchar('\n')` | |
| `puts("text")` `puts(s)` | with a newline |
| `fputs("text", stdout)` `fputs(s, stdout)` `fflush(stdout)` | |
| `printf("text")` | any text, escapes, no `%` |
| `printf("%d", E)` `printf("%u", E)` | also `"%d "` `"%d\n"` (the same for `%u`) |
| `printf("%s", s)` | |
| `printf("%u%u%u", a, b, c)` | three numbers |
| `c = getchar();` | |
| `putz(s)` `putn(E)` `putnl(E)` `nl()` | non C: a string, a number and a space, a number and a newline, a newline |

`%d` and `%u` both print unsigned (3).

**NOTE (silent):** any other `printf` prints its format text literally,
then the number:

```c
printf("a=%d\n", a);      /* prints  a=%d\n5  */
printf("%c", c);          /* prints  %c65     */
printf("%d %d\n", a, b);  /* error */
```

Split it: `printf("a="); printf("%d\n", a);` and use `putchar(c)` for a
character. There is no `%x`, `%c`, no widths.

Examples: [`man-io.c`](Tests/Compiler/man-io.c). The silent cases of this manual:
[`man-quirks.c`](Tests/Compiler/man-quirks.c).

---


## Errors

- An error in a statement, declaration or function is reported
  (`file:line: error: ...`, the line, a caret) and skipped (to its `;`,
  or over its `{ }` block); parsing goes on, all errors of a file in one
  run, no `.ivm`.
- `cannot parse (identifier not defined: x)`: an undeclared name, or a
  construct not in the grammar. The name shown is sometimes a keyword
  (`if`, `for`, `printf`): the statement as a whole did not match any
  rule; check it against the tables above.
- `const modified`: a write to a `#define`, `const`, an array's or
  static struct's name (4).
- `global initializer: a constant only` (4).

---


## Efficiency

Measured with sim65: each statement in a loop of 1000 rounds, the
difference to the empty loop, per execution. `BC` the bytecode bytes, `JIT`
the 6502 bytes the JIT makes (with `-z`: variables in zero page), then the
cycles on `ivm` (the C VM, cc65), `ivm-asm` and the JIT. Variables are
ints, locals.

The loop itself: an empty `for (k = 0; k < 1000; k++)` costs 695 / 132 /
22 cycles per round (`ivm` / `ivm-asm` / JIT).


### Increment, assign

| statement | BC | JIT | ivm | asm | jit |
|---|---:|---:|---:|---:|---:|
| `x++;` (= `x = x + 1;`) | 2 | 6 | 325 | 80 | 10 |
| `x += 1;` | 6 | 14 | 536 | 196 | 21 |
| `x += 2;` | 2 | 11 | 255 | 83 | 15 |
| `x += d;` | 3 | 13 | 479 | 112 | 22 |
| `x = x + d;` | 6 | 14 | 536 | 196 | 24 |
| `c++;` (`unsigned char`) | 2 | 2 | 244 | 74 | 6 |
| `x = 0;` | 2 | 6 | 163 | 57 | 8 |
| `x = 5;` / `x = a;` | 5 / 3 | 8 | 464 | 95 | 10 / 12 |

- Write `x++`, not `x += 1` (half the cycles everywhere).
- `x += d` beats `x = x + d`.
- `x = 0` is the cheapest store.
- A `char` (`unsigned char`) counter is the cheapest on the JIT.


### Multiply, divide

| statement | BC | JIT | ivm | asm | jit |
|---|---:|---:|---:|---:|---:|
| `x = a * 2;` | 5 | 13 | 713 | 471 | 23 |
| `x = a << 1;` | 5 | 13 | 512 | 188 | 23 |
| `x = a * 8;` | 7 | 21 | 713 | 472 | 40 |
| `x = a << 3;` | 5 | 21 | 536 | 218 | 40 |
| `x = a * 3;` | 5 | 19 | 717 | 475 | 156 |
| `x = a / 4;` | 7 | 18 | 1068 | 771 | 33 |
| `x = a >> 2;` | 5 | 18 | 521 | 202 | 33 |
| `x = a % 8;` | 7 | 10 | 1054 | 764 | 13 |
| `x = a & 7;` | 5 | 12 | 558 | 183 | 17 |

- The JIT turns `*` `/` `%` by a power of 2 into shifts / ANDs itself; the
  VMs do not: write `<<` `>>` `&` and all backends are fast.
- Any other multiply is a library call on the JIT (156 cycles).


### Arrays, pointers, structs (reads; writes cost about the same)

| statement | BC | JIT | ivm | asm | jit |
|---|---:|---:|---:|---:|---:|
| `n = b[i];` (char array) | 5 | 19 | 530 | 169 | 27 |
| `n = w[i];` (int array) | 5 | 33 | 461 | 181 | 56 |
| `n = w[3];` (const index) | 5 | 10 | 461 | 181 | 14 |
| `n = p[i];` (int *) | 5 | 33 | 461 | 181 | 58 |
| `n = p[3];` | 5 | 12 | 461 | 181 | 23 |
| `n = *p;` | 4 | 12 | 369 | 142 | 23 |
| `n = s.x;` (static struct) | 5 | 10 | 461 | 181 | 14 |
| `n = q->x;` | 5 | 12 | 461 | 181 | 23 |
| `n = sa[i].x;` (array of structs) | 5 | 33 | 461 | 181 | 56 |
| `n = st[i].x;` (`__striped`) | 9 | 12 | 692 | 202 | 18 |
| `sa[i].x += a;` | 8 | 48 | 745 | 313 | 91 |
| `st[i].x += a;` (`__striped`) | 12 | 28 | 1087 | 351 | 50 |

- On the JIT a `char` array with a variable index is twice as fast as an
  `int` array: use `char` where the values fit.
- A constant index, a static struct's field: an absolute address, the
  cheapest.
- `q->x` through a pointer costs about the same as a const index on the
  VMs and 23 cycles on the JIT.
- `__striped` makes `a[i].f` three times faster on the JIT and slower on
  `ivm` ([`__striped` and `-S`](#__striped-and--s)).


### Branches

| statement | BC | JIT | ivm | asm | jit |
|---|---:|---:|---:|---:|---:|
| `if (a < d) x++;` | 6 | 16 | 400 | 144 | 16 |
| `if (a > d) x++;` | 6 | 16 | 681 | 220 | 24 |
| `if (0 < a) x++;` | 6 | 16 | 681 | 220 | 22 |
| `if (a) x++;` | 6 | 24 | 692 | 212 | 35 |
| `if (!a) x++;` | 7 | 32 | 543 | 226 | 37 |
| `if (a == d) x++;` | 8 | 36 | 727 | 247 | 31 |
| `if (a != 0) x++;` | 9 | 44 | 1151 | 318 | 49 |
| `if (a & 1) x++;` | 8 | 28 | 979 | 281 | 39 |

(the cycles include the `x++` when it runs: it does for `>`, `0 <`,
`a`, `!=`, `& 1` here)

- `<` `<=` `>` `>=` between two simple values (variables, constants) are
  one compare-and-branch op: the cheapest test (`a < d` and `a > d` cost
  the same; the `>` row includes its `x++`).
- `if (a)` (35 with its `x++`) and `if (!a)` (37 without) cost more than
  `if (0 < a)` (22 with its `x++`) on the JIT.
- Avoid `!= 0`: write `if (0 < a)` (unsigned, so it is the same test):
  about half the cost on every backend.


### Loops

| loop (body `n++`) | BC | JIT | ivm | asm | jit |
|---|---:|---:|---:|---:|---:|
| `for (k = 0; k < 1000; k++)` | +0 | +0 | +0 | +0 | +0 |
| `for (k = 0; k <= 999; k++)` | +0 | +0 | +0 | +0 | +0 |
| `for (k = 0; k < lim; k++)` (var limit) | +3 | +0 | +1 | +0 | +2 |
| `while (k < 1000) { ...; k++; }` | +2 | +4 | +139 | +127 | -1 |
| `for (k = 1000; k; k--)` | +5 | +10 | +242 | +156 | +17 |
| `for (k = 1000; k-- > 0; )` | +10 | +35 | +146 | +137 | +12 |
| `while (k + 1 <= 1000) { ...; k++; }` (no loop op; body `s = s + k`) | +5 | +12 | +509 | +252 | +16 |

(per round, compared to the first form)

- Without the fused loop ops (a test the loop ops don't know, last
  row) the plain `for` is 1.4x (ivm) to 1.8x (asm) faster.
- On the JIT the `while` form can be faster: its `n` costs up to 6
  cycles more than `I` `B` (body `s = s + i`). To improve.

- The counting `for (i = A; i < B; i++)` / `<=` is one fused loop op:
  `u` for `<`, `t` for `<=` (the same op twice: test, body, `n`: i++ and
  back), so `<` and `<=` cost the same, a constant or a variable limit:

      for (k = 0; k < 1000; k++) n++;     Z k  u k 1000 end  I n  n back
      for (k = 0; k <= 999; k++) n++;     Z k  t k 999 end   I n  n back

  The cheapest loop on every backend: prefer it to `while`, and to
  counting down.


### memset loops

A loop that sets a whole array is recognized and becomes one `memset` op:

```c
for (i = 0; i < n; i++) b[i] = v;    /* char array: any v */
for (i = 0; i < n; i++) w[i] = 0;    /* int array: 0 (or -1) */
memset(b, v, n);
```

On the JIT a constant size of 1..256 is an inline `STA (p),Y` loop, a
larger one calls the library. The cycles for the whole loop:

| loop | ivm | ivm-asm | JIT |
|---|---:|---:|---:|
| `b[i] = 0`, 200 bytes (memset) | 4.9 k | 5.7 k | 4.5 k |
| `b[i] = i`, 200 bytes (a loop) | 232 k | 61 k | 12 k |
| `b[i] = 7`, 1000 bytes (memset) | 11.3 k | 14.5 k | 11.3 k |
| `b[i] = i`, 1000 bytes (a loop) | 1144 k | 289 k | 50 k |
| `w[i] = 0`, 200 ints (memset) | 7.0 k | 8.3 k | 5.3 k |
| `w[i] = 5`, 200 ints (a loop: not 0) | 234 k | 67 k | 19 k |

Up to 100 times faster on `ivm`: write clearing / filling loops exactly in
this form.


### Calls

| statement | BC | JIT | ivm | asm | jit |
|---|---:|---:|---:|---:|---:|
| `n = f(a);` (`f` returns `a + 1`) | 4 | 11 | 1372 | 484 | 38 |

A call is cheap in bytecode; on the VMs it costs about two loop rounds.

---


## Optimizations

**Byte code generation:**

- Peephole: minimize jump sizes from 2-3 bytes to 1 byte
- Peephole: one byte loading op for registers/most frequent vars
- Peephole: advanced (LOL) reuse of remaining ACC value of previous
  operation; may use associativey to reuse.
- Const eliminiation if they not used
- Minimize allocated static memory area
- Call Tree analyzis reuses stack-slots
- CSR: Common Subexpression Reuse (from temps); subexpressions inside the
  same function calculating the same thing are reused if they are stored
  in ACC or in a temp variable slot.

**What each one does, where it runs, what it saves:**

Measured on the 43 test programs (all .ivm together: 18697 bytes with
none of them, 17514 with all) and on `test-opts.c` (cycles on
`./ivm-asm`, the loops of its cases).

- Value numbering (ivm-gen.c `vn_temp`, while emitting; always on,
  `-DIVM_NO_VN`: off). The same chain computed again into a temp
  (`a[j + 1]` twice) becomes a load of the first temp. -40 bytes
  (0.2%), test-opts 0.3% faster.
- `&x` and value numbering. A store through a pointer may then hit a
  variable's slot. The grammar warns (`\"%Warning..."` in rule L); if
  value numbering kept a slot over such a store, parse.c compiles
  again without that.
- CSE and array swap (`IVM_CSE`: `gen_deadload`, `gen_swap`, bytecode
  passes; host only). A dead load is dropped; `t = a[i]; a[i] = a[j];
  a[j] = t` becomes one `A` op. -35 bytes (0.2%), 0.1% faster.
- `-O` (`IVM_OPT`: host only, `./pc`; `./parse` on the 6502 has no -O).
  Together:
  - layout: `register` vars in slots 0..31, globals 32..95, frames
    from 96;
  - frames packed by the call tree (`gen_pack`): functions that are
    never active at the same time share cells;
  - peephole (`gen_peephole`, then `gen_squeeze` removes the DEL
    fillers and fixes the jumps): `L xx` -> `xx` (one byte load),
    `? G rr` -> zjp (one byte jump), `S x L x` -> `S x`, `0 X` -> `Q`,
    dead code after a return / an always taken jump, constant branches.

  -1108 bytes (5.9%); byte-sieve-array 5.8% smaller and 4.7% faster,
  test-ifeq 14% smaller; test-opts 0.5% faster. Layout and packing
  alone make the code bigger (globals move above 31): the peephole's
  one byte loads are what pays.
- Not optimizations, but in the same pass: unused constants are not
  in the .ivm, and a DEL (FIXIPC) every 124 bytes of straight code
  for `ivm-fix`.

The sizes of this in the 6502 parse.sim (cc65): -O 3.6 KB (not
built, `-DIVM_OPT`), value numbering 2.1 KB, CSE 3.5 KB (not built,
`-DIVM_CSE`).

**JIT compilation:**

- Const propagation; all constants are put in code in place instead
  of loading
- Const reordering of operations.
- Loops compile highly efficient

- Array lookup/setting; addopts to size of indexing variable, size of
  array, constant index, constant pointer/array, setting constant value.


## Not supported: a summary

| | instead |
|---|---|
| recursion | loops (silent: hangs) |
| prototypes | define callee first |
| `long` `short` `float` `signed` `typedef` `enum` `union` | |
| negative literals, unary `-`, `~` | `0 - x`, `x ^ 0xffff` |
| signed compares / printing | test `x & 0x8000` |
| `&&` `\|\|` `?:` | nested `if` |
| `a + b * c` (tighter after looser) | `a + (b * c)` or `b * c + a` |
| assignment / `++` inside expressions | separate statements |
| `*= /= %= &= \|= ^= <<= >>=` | `x = x * 2;` |
| `a[i]++` `a[i] += n` | `a[i] = a[i] + n;` |
| `*(p + i)` | `p[i]` |
| `ptr + n` in expressions (not scaled: silent) | `p += n;` then use `p` |
| `&x` of a variable | an array of 1 |
| `do` `break` `continue` `switch` `goto` `for (;;)` | `while`, a function and `return` |
| `for (int i ...)`, `for (...; i += 2)` | declare `i` before, step in a variable |
| 2D arrays | `m[r * COLS + c]` |
| global initializers with expressions | a constant, or assign in `main` |
| `#define` expressions / macros | `const int`, functions |
| `#if NAME` (always true: silent) | `#ifdef NAME` |
| `printf` beyond the listed forms (silent) | split, `putchar` |
| `struct s x, y;` `struct s {..} x;` struct copies, by-value | one per line, pointers |
