// bnf2bin.c - the grammar: its format, rule[] comments[], loading it
// ready-built (c.bnf.bin; every target: parse.c includes this file), and
// (BNF_TEXT: the host parse, alone) reading its text (c.bnf), writing the
// .bin: no text to parse on the device, one block the rules point into
//
//   ./bnf2bin.x c.bnf        writes c.bnf.bin
//   ./parse.x -B c.bnf       the same (parse.c includes this file, MAIN
//                            defined: no main; its xrealloc FAIL eprintf)
//   ./parse.x c.bnf x.c      the host reads the text too (load_bnf_text)
//   parse.sim c.bnf.bin x.c  the 6502 builds only know the .bin
//
//   clang -O2 bnf2bin.c -o bnf2bin.x
//
// The .bin, words little endian, offsets from the file's start:
//
//   0   "BNF" 1        magic, version
//   4   len.w          the file's bytes
//   6   flags          0 (reserved)
//   7   ncomments      the #= comment styles, then each as:
//   8   start[4] end[4]  NUL padded (end "": a line comment)
//   ..  vec.w[52]      $A..$Z $a..$z: its alt table, 0: not defined (the
//                      parse then fails "rule X (called from Y): rule
//                      not defined", the stack shows which)
//   ..  per rule:      alt.w[n] 0.w: its alts, then the alts' bytes
//                      (each 0 terminated, as build_one_rule made them)
//   ..  0.w            end
//
// Loading (bnf_use): 2 byte pointers (6502): each alt table
// is relocated in place (+ the block's address): rule[r] points into it.
// The host builds the pointer arrays.

#ifndef BNF2BIN_C
#define BNF2BIN_C

// alone: what parse.c otherwise has (MAIN: included by it)
#ifndef MAIN
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <ctype.h>
  #include <stdint.h>

  typedef unsigned char uchar;
  typedef uint16_t word;

  #define BNF_TEXT                 // the text: what this is for

  static const char *fail_file;

  #define eprintf(...) fprintf(stderr, __VA_ARGS__)

  static void fail(const char *msg) {
    fprintf(stderr, "%%ERROR: %s%s%s\n", msg,
            fail_file ? ": " : "", fail_file ? fail_file : "");
    exit(1);
  }
  #define FAIL(msg) fail(msg)

  static void *xrealloc(void *p, word n) {
    if (!(p = realloc(p, n))) FAIL("out of memory");
    return p;
  }
#endif


// the host: 8 byte pointers, the .bin's alt tables become malloced
// pointer arrays (6502: relocated in place). As ivm.c
#if !defined(__CC65__) && !defined(OSCAR64) && !defined(HOST)
  #define HOST
#endif

// ---- the format: rule bytes, comment styles, the .bin ----

// A-Z
#define NRULES      ('z' - 'A' + 1)   // $A..$Z $a..$z (index: letter - 'A')

#define MAX_ALTS    64


#define HIBIT(c) ((c) | 0x80)

// Rule bytes: plain ASCII is a literal; with the hibit:
//
//   $81-$9A  letter directives \a..\z: HIDIR (so $A..$Z $a..$z are free)
//   $A0-$C0  ws gap, punctuation / digit directives \+ \# \< ..: HIBIT
//   $C1-$DA  rule calls $A..$Z, $E1-$FA $a..$z: HIBIT(letter)
//   $DB $DD  production [ ], $FB $FD \{ \}
//
// \< is 2 bytes: HIBIT('<'), ALT_JUMP + alt (1-based; 0: the rule again)
//
// A rule's index: its letter - 'A' (a..z: 32.., a few unused slots)

#define HIDIR(c)    (0x80 | ((c) & 31))  // a..z only (a case label: no ?:)
#define IS_RULE(c)  ((uchar)(((c) | 32) - HIBIT('a')) < 26)   // c: hibit
#define ALT_JUMP    0x20         // printable, not | [ ] \ \n: scans keep working
#define MAX_JUMP    31           // \< to alt 1..31


// ---- comment styles ----

// Comment styles loaded from grammar:
//   #= { }        block comment  start="{", end="}"
//   #= /* */      block comment  start="/*", end="*/"
//   #= //         line  comment  start="//", runs to newline
// Multiple #= lines are allowed (C needs both /* */ and //).
// Default (no #=): Pascal-style { } only.

#define MAX_COMMENTS 4
#define COMM_START_MAX 4
#define COMM_END_MAX   4

typedef struct {
  char start[COMM_START_MAX]; // NUL-terminated, length 1..3
  char end[COMM_END_MAX];     // empty => line comment (to \n)
  uchar start_len;
  uchar end_len;              // 0 = line comment
} CommentStyle;


// ---- the .bin (bnf2bin.c) ----

#define BNF_MAGIC   "BNF\001"
#define BNF_HDR     8                          // magic len.w flags ncomments
#define BNF_NVEC    52                         // $A..$Z $a..$z
#define BNF_RULE(k) ((k) < 26 ? (k) : (k) + 6) // vector -> rule index
#define BNF_W(p)    ((word)((p)[0] | ((p)[1] << 8)))


// ---- the grammar ----

// rule['X'-'A'] = NULL-terminated array of uchar* into one malloc blob
static uchar **rule[NRULES];

// a ready-built grammar (bnf2bin.c writes it): one block, the rules point
// into it (2 byte pointers: its alt tables too, relocated in place)
static uchar *bnf_bin;

static CommentStyle comments[MAX_COMMENTS];
static uchar ncomments;

#ifdef BNF_TEXT

// blobs[r] holds the contiguous text for rule r
static uchar *blobs[NRULES];

// skip spaces and tabs of a text line
static char *skipsp(char *p) {
  char c;

  while ((c = *p) == ' ' || c == '\t') p++;
  return p;
}

// copy a non-space run (max 3 chars) from p to dst, NUL-terminated; its length
static uchar copy_tok(char *dst, char *p) {
  uchar i = 0;
  char c;

  while ((c = *p++) && c != ' ' && c != '\t' && i + 1 < COMM_START_MAX)
    dst[i++] = c;
  dst[i] = 0;
  return i;
}

// Parse one #= directive body (after the "#="). Returns 1 on success.
// Forms:  "{ }" | "/* */" | "//"   (whitespace separates start/end)
static uchar comments_add_from_line(char *p) {
  CommentStyle *cs;

  if (ncomments >= MAX_COMMENTS) return 0;
  p = skipsp(p);

  cs = &comments[ncomments];

  // start delimiter: non-space run
  if (!(cs->start_len = copy_tok(cs->start, p))) return 0;

  // optional end delimiter => block comment; none => line comment
  cs->end_len = copy_tok(cs->end, skipsp(p + cs->start_len));

  ncomments++;
  return 1;
}

// Default: { } block (Pascal). Cleared/replaced when BNF has #= lines.
static void comments_set_default(void) {
  ncomments = 0;
  comments_add_from_line((char*)"{ }");
}

// \X is a directive (stored with the hibit) or a quoted literal char e
// note: '>' is load-time only (not stored)
//       '<' becomes HIBIT('<') + ALT_JUMP + alt (1..31, 0: restart)
static uchar is_dir(uchar e) {
  return isdigit(e) || strchr("insfdprlxywmzkcahvgetoubjq+{}#='?*@$:,.!%&^()-/~;_\"", e) != NULL;
}

static void build_one_rule(uchar r, char *rhs) {
  char *p;
  uchar *blob, **altp, e, inprod, verbatim, quoted;
  word a, bi, na, alt_starts[MAX_ALTS];
  uchar taildest;   // 1-based alt index for \> / \< (0 = none yet)

  // directives (\i \n \s \o \< \d \+ \{ \}, rule calls, bare space, [ ]) get
  // the hibit
//
  // quoted \| \c (not specials) become the plain char
  //
  // \> is not stored; it only updates taildest
  // \< becomes HIBIT('<'), ALT_JUMP + x: jump to alt x-1, x = 0 if no \> yet
  // build contiguous string with 0 separators between alts.

  blob = xrealloc(NULL, strlen(rhs) + 2);

  free(blobs[r]);
  free(rule[r]);
  blobs[r] = blob;

  bi = 0;
  na = 0;
  inprod = 0;
  verbatim = 0;
  quoted = 0;
  taildest = 0;
  alt_starts[na++] = 0;

  for (p = rhs; (e = *p); p++) {

    if (verbatim) {
      
      // Generic copy next "hex" bytes - rename
      // A: renamed to verbatim (only \# uses it so far)

      // \#xx: the two hex digits are data (may be A-F),
      // stored as they are
      verbatim--;

    } else if (quoted) {

      // \"text": the text as it is, to its " (stored: run() ends there)
      if (e == '"') quoted = 0;

    } else if (e == '\\' && p[1]) {

      e = *++p;
      if (e == '>') {
        // \> : set jump target to current alt (1-based); do not store
        taildest = (uchar)na;   // na is already the current alt count
        continue;
      }

      if (e == '<') {

        // \< : HIBIT('<'), then the \> target alt (1..31), else 0: the
        //   whole rule again
        if (taildest > MAX_JUMP) {
          eprintf("\nrule %c: ", 'A' + r);
          FAIL("\\< to an alt past 31");
        }
        blob[bi++] = HIBIT('<');
        e = (uchar)(ALT_JUMP + taildest);

      } else {

        if (is_dir(e)) e = isalpha(e) ? HIDIR(e) : HIBIT(e);  // (a..z)
      }

    } else if (e == '|' && !inprod) {

      // '|' separates alts only outside a bare [ ... ] production
      // skip empty alts from || or line-join artifacts
      if (na > 0 && alt_starts[na - 1] == bi)
        continue;

      blob[bi++] = 0;

      if (na >= MAX_ALTS) {
        // TODO: don't repeat message
        // A: done: the rule letter here, the rest in the FAIL message
        eprintf("\nrule %c: ", 'A' + r);
        FAIL("too many alts (MAX_ALTS)");
      }
      alt_starts[na++] = bi;
      continue;

    } else if (e == '[') {

      // bare [ ] = production (no nesting); | inside is a literal (\[ \] are handled above)
      inprod = 1;
      e = HIBIT('[');

    } else if (e == ']' && inprod) {

      inprod = 0;
      e = HIBIT(']');

    } else {

      // rule calls $R $r and the ws-gap are directives, not in a production
      if (!inprod && e == '$' && isalpha(p[1])) e = HIBIT(*++p);
      else if (!inprod && e == ' ') e = HIBIT(e);

    }
    
    blob[bi++] = e;

    // \"text": the text up to its " as it is
    if (e == HIBIT('"')) quoted = 1;

    // \#xx \xHH => copy next two chars verbatim
    if (e == HIBIT('#') ||
        (e == HIDIR('x') && isxdigit(p[1]) && isxdigit(p[2]))) verbatim = 2;
    
  }

  blob[bi++] = 0;

  altp = xrealloc(NULL, sizeof(uchar *) * (na + 1));

  // create and fill an array of pointers to ALTs
  for (a = 0; a < na; a++)
    altp[a] = blob + alt_starts[a];

  // end with 0 -- TODO: just make all allocs clean/zero?
  // A: calloc would clear all of it for this one store: no gain

  // TODO: I mean if xrealloc always made new bytes allocated
  //   0 we wouldn't need to null things out later... like here!
  altp[na] = NULL;
  
  rule[r] = altp;
}


// ------------------------------------------------------------

#define MAX_LINE 1024
#define MAX_ACCUM 4096

// a grammar's text as rule[] (each rule's alts in its blob) and the #=
// comment styles (none: Pascal's { })
static void load_bnf_text(const char *path) {
  FILE *f;
  char *line, *acc;
  uchar cr;
  char *nl, *eq, *p, c;
  word acc_len, have_cur;

  fail_file = path;
  f = fopen(path, "r");
  if (!f) FAIL("cannot open grammar");
  fail_file = NULL;

  line = xrealloc(NULL, MAX_LINE);
  acc = xrealloc(NULL, MAX_ACCUM);

  have_cur = 0;
  cr = 0;
  acc_len = 0;
  acc[0] = 0;

  // comment styles from #= lines; default Pascal { } if none given
  ncomments = 0;

  while (fgets(line, MAX_LINE, f)) {

    nl = strchr(line, '\n');
    if (nl) *nl = 0;

    // split a new line - use the rest
    c = *line;
    if (c == '.') break;
    if (c == 0) continue;

    // #= comment-style directive (must be before bare # skip)
    if (c == '#' && line[1] == '=') {
      comments_add_from_line(line + 2);
      continue;
    }
    if (c == '#') continue;

    p = skipsp(line);

    // continuation: line starts with |  (p stays, text is appended below)
    if (!(*p == '|' && have_cur)) {

      // new rule: flush previous
      if (have_cur)
        build_one_rule(cr, acc);
      have_cur = 0;

      if (!isalpha(c)) continue;

      eq = strchr(line, '=');
      if (!eq) continue;

      cr = c - 'A';
      have_cur = 1;
      p = eq + 1;
      acc_len = 0;
    }

    // TODO: why" it's just copying?
    // A: yes: append this line (or its part after "X=") to acc, because
    //   fgets reuses line for the next line
    while ((c = *p) && acc_len + 1 < MAX_ACCUM) {
      acc[acc_len++] = c;
      p++;
    }
    acc[acc_len] = 0;
  }

  if (have_cur)
    build_one_rule(cr, acc);

  if (ncomments == 0)
    comments_set_default();

  fclose(f);
  free(line);
  free(acc);
}

// the text's rule blobs (rule[]: parse.c's free_rules)
static void bnf_text_free(void) {
  uchar r;

  for (r = 0; r < NRULES; r++) {
    free(blobs[r]);
    blobs[r] = NULL;
  }
}


// ---- the .bin ----

// little endian word out
static void bnf_w(FILE *f, word w) {
  fputc(w & 0xff, f);
  fputc(w >> 8, f);
}

// the alt strings of rule r: bytes from its first alt to its last 0
static word bnf_blob_len(uchar **alts) {
  uchar **a = alts;

  while (a[1]) a++;
  return (word)(*a + strlen((char *)*a) + 1 - *alts);
}

// the rules (load_bnf() made them) as path; its bytes
static word bnf_save(const char *path) {
  FILE *f;
  word at, n, len, vec[BNF_NVEC];
  uchar k, r, c, i;
  uchar **a;

  // where: header, comments, vectors, then the rules
  at = (word)(BNF_HDR + ncomments * 2 * COMM_START_MAX + 2 * BNF_NVEC);
  for (k = 0; k < BNF_NVEC; k++) {
    r = BNF_RULE(k);
    vec[k] = 0;
    if (!rule[r]) continue;
    vec[k] = at;
    for (n = 0; rule[r][n]; n++) ;
    at += (word)(2 * (n + 1) + bnf_blob_len(rule[r]));
  }
  len = (word)(at + 2);

  fail_file = path;
  f = fopen(path, "wb");
  if (!f) FAIL("cannot write grammar .bin");
  fail_file = NULL;

  // header
  fputs(BNF_MAGIC, f);
  bnf_w(f, len);
  fputc(0, f);
  fputc(ncomments, f);

  // comment styles
  for (i = 0; i < ncomments; i++) {
    for (c = 0; c < COMM_START_MAX; c++) fputc(comments[i].start[c], f);
    for (c = 0; c < COMM_END_MAX; c++)   fputc(comments[i].end[c], f);
  }

  // vectors
  for (k = 0; k < BNF_NVEC; k++) bnf_w(f, vec[k]);

  // rules: the alt table (offsets), then the alts' bytes
  for (k = 0; k < BNF_NVEC; k++) {
    if (!vec[k]) continue;
    a = rule[BNF_RULE(k)];
    for (n = 0; a[n]; n++) ;
    at = (word)(vec[k] + 2 * (n + 1));
    for (n = 0; a[n]; n++) bnf_w(f, (word)(at + (a[n] - a[0])));
    bnf_w(f, 0);
    fwrite(a[0], 1, bnf_blob_len(a), f);
  }

  // end
  bnf_w(f, 0);
  fclose(f);
  return len;
}

// "c.bnf" -> "c.bnf.bin" (malloced)
static char *bnf_bin_name(const char *bnf) {
  char *s = xrealloc(NULL, (word)(strlen(bnf) + 5));

  strcpy(s, bnf);
  strcat(s, ".bin");
  return s;
}

#endif // BNF_TEXT


// ---- loading it ready-built ----

// a grammar built into the program (oscar64 #embed): no file to load
#ifdef BNF_EMBED
static uchar bnf_embedded[] = {
  #embed "c.bnf.bin"
};
#endif

// frees the alt tables (HOST: malloced; 6502: in bnf_bin), the .bin's
// block, the text's blobs
static void free_rules(void) {
  uchar r;

  for (r = 0; r < NRULES; r++) {
#ifdef HOST
    free(rule[r]);
#endif
    rule[r] = NULL;
  }
#ifdef BNF_EMBED
  if (bnf_bin != bnf_embedded)
#endif
    free(bnf_bin);
  bnf_bin = NULL;
#ifdef BNF_TEXT
  bnf_text_free();
#endif
}

// a ready-built grammar (bnf2bin.c: the layout) in memory, b: rule[] into
// it. A 0 vector: rule[] stays NULL (push(): "rule not defined", with its
// letter)
static void bnf_use(uchar *b) {
  uchar *p, *t, k, i;
  word j;
#ifdef HOST
  uchar **altp;
  word n;
#endif

  bnf_bin = b;

  // comment styles
  ncomments = b[7];
  if (ncomments > MAX_COMMENTS) FAIL("bad grammar .bin");
  p = b + BNF_HDR;
  for (i = 0; i < ncomments; i++, p += 2 * COMM_START_MAX) {
    memcpy(comments[i].start, p, COMM_START_MAX);
    memcpy(comments[i].end, p + COMM_START_MAX, COMM_END_MAX);
    comments[i].start_len = (uchar)strlen(comments[i].start);
    comments[i].end_len = (uchar)strlen(comments[i].end);
  }

  // rules: offsets -> pointers
  for (k = 0; k < BNF_NVEC; k++, p += 2) {
    if (!(j = BNF_W(p))) continue;
    t = b + j;

#ifdef HOST
    // a pointer array
    for (n = 0; BNF_W(t + 2 * n); n++) ;
    altp = xrealloc(NULL, (word)(sizeof(uchar *) * (n + 1)));
    for (j = 0; j < n; j++) altp[j] = b + BNF_W(t + 2 * j);
    altp[n] = NULL;
    rule[BNF_RULE(k)] = altp;
#else
    // in place: + the block's address (its 0 end stays NULL)
    rule[BNF_RULE(k)] = (uchar **)t;
    for (; *(word *)t; t += 2)
      *(word *)t += (word)b;
#endif
  }
}

// the .bin from f (after its magic): one block (its first 6 bytes unused)
static void load_bnf_bin(FILE *f) {
  uchar h[2], *b;
  word len;

  if (fread(h, 1, 2, f) != 2) FAIL("bad grammar .bin");
  len = BNF_W(h);
  b = xrealloc(NULL, len);
  if (fread(b + 6, 1, len - 6, f) != len - 6) FAIL("bad grammar .bin");
  bnf_use(b);
}

// the grammar: ready-built (its magic), else its text (host), else the
// one built in (BNF_EMBED: path unused)
static void load_bnf(const char *path) {
  FILE *f;
  uchar magic[4];

  free_rules();

  #ifdef BNF_EMBED
    bnf_use(bnf_embedded);
    return;
  #endif

  f = fopen(path, "rb");
  if (f && fread(magic, 1, 4, f) == 4 && !memcmp(magic, BNF_MAGIC, 4)) {
    load_bnf_bin(f);
    fclose(f);
    return;
  }
  if (f) fclose(f);                      // (no fseek on sim65)

  #ifdef BNF_TEXT
    load_bnf_text(path);
  #else
    fail_file = path;
    FAIL(f ? "a grammar's text: needs its .bin (./bnf2bin.x c.bnf)"
         : "cannot open grammar");
  #endif
}


#ifndef MAIN
int main(int argc, char **argv) {
  char *out;

  if (argc != 2) {
    fprintf(stderr, "usage: %s c.bnf    (writes c.bnf.bin)\n", argv[0]);
    return 1;
  }
  load_bnf_text(argv[1]);
  out = bnf_bin_name(argv[1]);
  printf("%s: %u bytes\n", out, (unsigned)bnf_save(out));
  free(out);
  return 0;
}
#endif // MAIN

#endif // BNF2BIN_C
