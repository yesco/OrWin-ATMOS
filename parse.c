/* parse.c — iterative BNF bytecode interpreter
 *
 *   ./parse tpi.bnf test.p
 *
 * BNF (tpi.bnf):
 *   X=body          rule X
 *   a-z / punct     match char (letters case-insensitive)
 *   space           skip whitespace + {comments}
 *   A-Z             call rule
 *   \i \n \s        ident / number / string
 *   \|  \c          literal | or literal c (loader expands)
 *   |               alternative (epsilon ok)
 *   line starting . ends rule section
 *
 * Explicit stack — no recursive match.
 * Capture commits only when an alternative succeeds.
 */

// EDING INSTRUCTION (keep)
// 1. use // comments ONLY
// 2. prefer dynamic allocations ONLY except for static data
// 3. design to have an init(filename) method
// 4. assume there is "unbounded input" read using fgetc()
//    (buffer a programming line/statement, can reset beginnin of it)
//  4a. since we're doing backtracking, we may insert explicit \C
//      commit characters at the end of key rules. This resets the
//      buffer to beginning. For example once a statement has been
//      parsed there is no ambiguity; we're committed to accept it.
// 5. I think we could just read the bnf input, until a line
//    starts with a '.', there just stop reading rules.
// 6. use typedef unsigned char uchar; and remove ALL typecasts by using uchar everywhere
// 7. typedef uint16_t word; use "word" instead of int unless you need signed values,
//    prefer uchar if variable fits in it.
// 8. newline between defs and code
// 9. No line constants like 63? what is 63?
// 10. What does "cap" mean, maybe use better name or at least 
//     one // cap == capture text to print once committing
// 11. c <= 32 is whitespace
// 12. we want a run(n) method that does a chunk of work, possible
//     n steps. It should be bounded in time, assume a 6502 1MHz
//     so add comment for estimated run time in us/ms for n=1
//     to that method.

// How rules are managed and stored:
// a) Read a single rule to a continugous malloced space, including all it's alts.
//    Note offsets/make an dynamic array of pointers, terminated by 0,
//    that are pointed to from rule[letter].
// b) the uchar* pointers for alt point into the read single rule.
//    the existsence of rule['X'-'A'] value means A rule is defined




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define MAX_RULE_LEN  512
#define MAX_ALTS      32
#define MAX_ALT_LEN   256
#define MAX_STACK     256
#define MAX_SRC       65536
#define MAX_BUF       65536

static int   verbose;   /* 0=off 1=compact 2+=detail */
static int   ignore_case = 1; /* -I default on; -C case-sensitive */


static char  alts[26][MAX_ALTS][MAX_ALT_LEN];
static char  nalt[26];
static char  has_rule[26]; // TODO: goal to remove.

static char  src[MAX_SRC];
static int   src_len;
static int   pos;
static int   max_pos;
static char  far_stack[64];
static int   far_sp;
static char  far_alt[64];
static const char *src_name = "source";

static char  outbuf[MAX_STACK][MAX_BUF];
static int   outlen[MAX_STACK];

enum { ST_TRY, ST_RUN, ST_OK, ST_FAIL };

typedef struct {
  int rule, alt, ip, save, state;
} Frame;

static Frame stack[MAX_STACK];
static int sp;

/* detail log (verbose >= 2) */
static void vlog(const char *fmt, ...) {
  va_list ap;
  if (verbose < 2) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

/* compact understanding log (verbose >= 1) */
static void v1(const char *fmt, ...) {
  va_list ap;
  if (verbose < 1) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

static void v1_indent(void) {
  int i;
  if (verbose < 1) return;
  for (i = 0; i < sp; i++)
    fputc(' ', stderr);
}

static void note_progress(void) {
  int i;
  if (pos <= max_pos) return;
  max_pos = pos;
  far_sp = sp < 63 ? sp : 63;
  for (i = 0; i < far_sp; i++) {
    far_stack[i] = (char)('A' + stack[i].rule);
    far_alt[i] = (char)stack[i].alt;
  }
}

static void push(int rule, int state) {
  if (sp >= MAX_STACK) {
    fprintf(stderr, "stack overflow\n");
    exit(1);
  }
  stack[sp].rule  = rule;
  stack[sp].alt   = 0;
  stack[sp].ip    = 0;
  stack[sp].save  = pos;
  stack[sp].state = state;
  outlen[sp] = 0;
  sp++;
}

static void cap(char c) {
  int i = sp - 1;
  if (i >= 0 && outlen[i] < MAX_BUF - 1)
    outbuf[i][outlen[i]++] = c;
}

static void flush_cap(int frame) {
  int i;
  for (i = 0; i < outlen[frame]; i++)
    putchar(outbuf[frame][i]);
  fflush(stdout);
  outlen[frame] = 0;
}

static void commit_cap(int frame) {
  int p = frame - 1, i;
  if (p < 0) {
    flush_cap(frame);
    return;
  }
  for (i = 0; i < outlen[frame]; i++)
    if (outlen[p] < MAX_BUF - 1)
      outbuf[p][outlen[p]++] = outbuf[frame][i];
  outlen[frame] = 0;
}

static int is_ws(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* whitespace and { pascal comments } */
static void skip_ws(void) {
  for (;;) {
    while (pos < src_len && is_ws(src[pos])) {
      cap(src[pos]);
      pos++;
    }
    if (pos < src_len && src[pos] == '{') {
      cap(src[pos++]);
      while (pos < src_len && src[pos] != '}') {
        cap(src[pos]);
        pos++;
      }
      if (pos < src_len && src[pos] == '}') {
        cap(src[pos]);
        pos++;
      }
      continue;
    }
    break;
  }
}

static int match_lit(char want) {
  char g;
  if (pos >= src_len) return 0;
  g = src[pos];
  if (isalpha((unsigned char)want) && ignore_case) {
    if (tolower((unsigned char)g) != tolower((unsigned char)want))
      return 0;
  } else if (g != want)
    return 0;
  cap(g);
  pos++;
  note_progress();
  return 1;
}

static int match_ident(void) {
  int start;
  skip_ws();
  if (pos >= src_len || !isalpha((unsigned char)src[pos])) return 0;
  start = pos;
  while (pos < src_len && (isalnum((unsigned char)src[pos]) || src[pos] == '_')) {
    cap(src[pos]);
    pos++;
  }
  (void)start;
  note_progress();
  return 1;
}

static int match_num(void) {
  int start;
  skip_ws();
  if (pos >= src_len || !isdigit((unsigned char)src[pos])) return 0;
  start = pos;
  while (pos < src_len && isdigit((unsigned char)src[pos])) {
    cap(src[pos]);
    pos++;
  }
  (void)start;
  note_progress();
  return 1;
}

static int match_str(void) {
  int start;
  skip_ws();
  if (pos >= src_len || src[pos] != '"') return 0;
  start = pos;
  cap(src[pos++]);
  while (pos < src_len && src[pos] != '"') {
    if (src[pos] == '\\' && pos + 1 < src_len) {
      cap(src[pos++]);
      cap(src[pos++]);
    } else
      cap(src[pos++]);
  }
  if (pos >= src_len || src[pos] != '"') return 0;
  cap(src[pos++]);
  (void)start;
  note_progress();
  return 1;
}

/*
 * Split RHS on | into alternatives.
 * \| or \c (when c not i/n/s) → single literal char in alt text.
 * \i \n \s stay as two-char escapes for the matcher.
 */
static void split_alts(int r, const char *rhs) {
  int a = 0, j = 0;
  const char *p = rhs;
  nalt[r] = 0;
  while (1) {
    if (*p == '\0') {
      alts[r][a][j] = '\0';
      nalt[r] = a + 1;
      break;
    }
    if (*p == '\\' && p[1] != '\0') {
      char e = p[1];
      if (e == 'i' || e == 'n' || e == 's') {
        if (j < MAX_ALT_LEN - 2) {
          alts[r][a][j++] = '\\';
          alts[r][a][j++] = e;
        }
        p += 2;
        continue;
      }
      /* \| or \c → literal c */
      if (j < MAX_ALT_LEN - 1)
        alts[r][a][j++] = e;
      p += 2;
      continue;
    }
    if (*p == '|') {
      alts[r][a][j] = '\0';
      a++;
      j = 0;
      if (a >= MAX_ALTS) break;
      p++;
      continue;
    }
    if (j < MAX_ALT_LEN - 1)
      alts[r][a][j++] = *p;
    p++;
  }
}

static int load_bnf(const char *path) {
  FILE *f = fopen(path, "r");
  char line[MAX_RULE_LEN];
  int i;
  if (!f) {
    perror(path);
    return 0;
  }
  for (i = 0; i < 26; i++) {
    has_rule[i] = 0;
    nalt[i] = 0;
  }
  while (fgets(line, sizeof line, f)) {
    char *nl = strchr(line, '\n');
    char *eq;
    int r;
    if (nl) *nl = '\0';
    if (line[0] == '.') break;
    if (line[0] == '\0' || line[0] == '#') continue;
    if (!isupper((unsigned char)line[0])) continue;
    eq = strchr(line, '=');
    if (!eq) continue;
    r = line[0] - 'A';
    has_rule[r] = 1;
    split_alts(r, eq + 1);
  }
  fclose(f);
  return 1;
}

static int load_src(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    perror(path);
    return 0;
  }
  src_len = (int)fread(src, 1, MAX_SRC - 1, f);
  src[src_len] = '\0';
  fclose(f);
  pos = 0;
  if (src_len >= 3 && (unsigned char)src[0] == 0xEF
      && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF)
    pos = 3;
  return 1;
}

static int step(void) {
  Frame *fr;
  char *alt;
  char c;

  if (sp <= 0) return 0;
  fr = &stack[sp - 1];

  if (fr->state == ST_TRY) {
    if (fr->alt >= nalt[fr->rule]) {
      vlog("fail %c (no alt) @%d\n", 'A' + fr->rule, pos);
      if (verbose >= 2) {
        v1("\n");
        v1_indent();
        v1("!%c\n", 'A' + fr->rule);
      }
      sp--;
      if (sp <= 0) return -1;
      fr = &stack[sp - 1];
      pos = fr->save;
      outlen[sp - 1] = 0;
      fr->alt++;
      fr->ip = 0;
      fr->save = pos;
      fr->state = ST_TRY;
      return 1;
    }
    pos = fr->save;
    outlen[sp - 1] = 0;
    fr->ip = 0;
    fr->state = ST_RUN;
    vlog("try %c alt %d @%d \"%s\"\n",
         'A' + fr->rule, fr->alt, pos, alts[fr->rule][fr->alt]);
    /* compact labels printed on successful match */
    return 1;
  }

  if (fr->state == ST_OK) {
    vlog("ok %c alt %d → @%d\n", 'A' + fr->rule, fr->alt, pos);
    if (verbose >= 1 && outlen[sp - 1] > 0) {
      int i;
      v1("\n");
      v1_indent();
      if (nalt[fr->rule] > 1)
        v1("%c%d:", 'A' + fr->rule, fr->alt);
      else
        v1("%c:", 'A' + fr->rule);
      for (i = 0; i < outlen[sp - 1]; i++) {
        char ch = outbuf[sp - 1][i];
        if (ch == '\n')
          v1("\n");
        else
          v1("%c", ch);
      }
      v1("\n");
      v1_indent();
      v1("<%c\n", 'A' + fr->rule);
    }
    commit_cap(sp - 1);
    sp--;
    if (sp <= 0) return 0;
    fr = &stack[sp - 1];
    fr->ip++;
    fr->state = ST_RUN;
    return 1;
  }

  /* current alternative failed — try next alt of SAME rule (do not pop) */
  if (fr->state == ST_FAIL) {
    outlen[sp - 1] = 0;
    pos = fr->save;          /* rule-entry input */
    fr->alt++;
    fr->ip = 0;
    fr->state = ST_TRY;      /* ST_TRY exhausts alts → then pop to parent */
    return 1;
  }

  alt = alts[fr->rule][fr->alt];

  if (alt[fr->ip] == '\0') {
    fr->state = ST_OK;
    return 1;
  }

  c = alt[fr->ip];

  if (c == ' ') {
    skip_ws();
    fr->ip++;
    return 1;
  }

  if (c >= 'A' && c <= 'Z') {
    int r = c - 'A';
    if (!has_rule[r]) {
      fprintf(stderr, "\n[no rule %c]\n", c);
      fr->state = ST_FAIL;
      return 1;
    }
    vlog("  call %c from %c @%d\n", c, 'A' + fr->rule, pos);
    push(r, ST_TRY);
    return 1;
  }

  if (c == '\\') {
    char e = alt[fr->ip + 1];
    int ok = 0;
    if (e == 'i') ok = match_ident();
    else if (e == 'n') ok = match_num();
    else if (e == 's') ok = match_str();
    else {
      skip_ws();
      ok = match_lit(e);
    }
    if (!ok) {
      vlog("  fail \\%c @%d\n", e, pos);
      fr->state = ST_FAIL;
      return 1;
    }
    vlog("  ok \\%c @%d\n", e, pos);
    fr->ip += 2;
    return 1;
  }

  skip_ws();
  if (!match_lit(c)) {
    vlog("  fail lit '%c' @%d got '%c'\n", c, pos,
         pos < src_len ? src[pos] : '?');
    fr->state = ST_FAIL;
    return 1;
  }
  vlog("  ok lit '%c' @%d\n", c, pos - 1);
  fr->ip++;
  return 1;
}


static int line_at(int p) {
  int i, line = 1;
  for (i = 0; i < p && i < src_len; i++)
    if (src[i] == '\n') line++;
  return line;
}

static void report_reject(void) {
  int i, col, line, nlines, start, end;
  fprintf(stderr, "\n--- REJECT at %d (furthest %d) ---\n", pos, max_pos);
  fprintf(stderr, "STACK:");
  if (far_sp <= 0)
    fprintf(stderr, " (empty)");
  for (i = 0; i < far_sp; i++) {
    fprintf(stderr, " %c", far_stack[i]);
    if (far_alt[i] > 0 || 1)
      fprintf(stderr, "%d", (int)far_alt[i]);
  }
  fprintf(stderr, "\n");
  if (far_sp > 0) {
    int r = far_stack[far_sp - 1] - 'A';
    int a = (int)far_alt[far_sp - 1];
    fprintf(stderr, "EXPECTED: rule %c", far_stack[far_sp - 1]);
    if (has_rule[r] && a < nalt[r])
      fprintf(stderr, " alt %d \"%s\"", a, alts[r][a]);
    fprintf(stderr, "\n");
  }
  /* source context from max_pos */
  start = max_pos;
  while (start > 0 && src[start - 1] != '\n')
    start--;
  end = max_pos;
  nlines = 0;
  while (end < src_len && nlines < 5 && end - max_pos < 128) {
    if (src[end] == '\n') nlines++;
    end++;
  }
  line = line_at(max_pos);
  col = max_pos - start;
  fprintf(stderr, "REJECT:\n");
  {
    int lp = start, cur_line = line;
    while (lp < end && lp < src_len) {
      fprintf(stderr, "%s:%d: ", src_name, cur_line);
      while (lp < end && lp < src_len && src[lp] != '\n') {
        fputc(src[lp], stderr);
        lp++;
      }
      fputc('\n', stderr);
      if (lp < end && lp < src_len && src[lp] == '\n') {
        lp++;
        cur_line++;
      } else
        break;
    }
    /* caret under furthest column on first line */
    fprintf(stderr, "%s:%d: ", src_name, line);
    for (i = 0; i < col; i++)
      fputc(' ', stderr);
    fprintf(stderr, "^\n");
  }
}

static int run_machine(void) {
  int rc;
  sp = 0;
  max_pos = 0;
  far_sp = 0;
  if (!has_rule['P' - 'A']) {
    fprintf(stderr, "missing start rule P\n");
    return 0;
  }
  push('P' - 'A', ST_TRY);
  while ((rc = step()) == 1)
    ;
  return rc == 0;
}

int main(int argc, char **argv) {
  int i, ok;
  int ai;
  const char *bnf = NULL, *srcpath = NULL;

  // parse args
  verbose = 0;
  ignore_case = 1;
  for (ai = 1; ai < argc; ai++) {
    if (strcmp(argv[ai], "-v") == 0)
      verbose++;
    else if (strcmp(argv[ai], "-I") == 0)
      ignore_case = 1;
    else if (strcmp(argv[ai], "-C") == 0)
      ignore_case = 0;
    else if (!bnf)
      bnf = argv[ai];
    else if (!srcpath)
      srcpath = argv[ai];
  }
  if (!bnf || !srcpath) {
    fprintf(stderr, "usage: %s [-v|-vv] [-I|-C] grammar.bnf source.p\n", argv[0]);
    fprintf(stderr, "  -v   compact parse trace\n");
    fprintf(stderr, "  -vv  detailed engine trace\n");
    fprintf(stderr, "  -I   ignore case (default)\n");
    fprintf(stderr, "  -C   case-sensitive\n");
    return 1;
  }
  src_name = srcpath;
  if (!load_bnf(bnf)) return 1;
  if (!load_src(srcpath)) return 1;


  // print rules
  if (verbose >= 2) {
    fprintf(stderr, "--- rules ---\n");
    for (i = 0; i < 26; i++)
      if (has_rule[i]) {
        int a;
        fprintf(stderr, "%c =", 'A' + i);
        for (a = 0; a < nalt[i]; a++) {
          if (a) fprintf(stderr, "|");
          fprintf(stderr, "%s", alts[i][a]);
        }
        fprintf(stderr, "\n");
      }
  }
  if (verbose) fprintf(stderr, "--- parse ---\n");


  // do the parsing
  ok = run_machine();


  // report status
  putchar('\n');
  if (ok) {
    skip_ws();
    if (pos < src_len) {
      fprintf(stderr, "--- OK but trailing input at %d ---\n", pos);
      return 2;
    }
    fprintf(stderr, "--- ACCEPT ---\n");
    return 0;
  }

  // ERROR?
  report_reject();

  return 1;
}
