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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_RULE_LEN  512
#define MAX_ALTS      32
#define MAX_ALT_LEN   256
#define MAX_STACK     256
#define MAX_SRC       65536
#define MAX_BUF       65536

static char  alts[26][MAX_ALTS][MAX_ALT_LEN];
static int   nalt[26];
static int   has_rule[26];

static char  src[MAX_SRC];
static int   src_len;
static int   pos;

static char  outbuf[MAX_STACK][MAX_BUF];
static int   outlen[MAX_STACK];

enum { ST_TRY, ST_RUN, ST_OK, ST_FAIL };

typedef struct {
  int rule, alt, ip, save, state;
} Frame;

static Frame stack[MAX_STACK];
static int sp;

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
  if (isalpha((unsigned char)want)) {
    if (tolower((unsigned char)g) != tolower((unsigned char)want))
      return 0;
  } else if (g != want)
    return 0;
  cap(g);
  pos++;
  return 1;
}

static int match_ident(void) {
  skip_ws();
  if (pos >= src_len || !isalpha((unsigned char)src[pos])) return 0;
  while (pos < src_len && (isalnum((unsigned char)src[pos]) || src[pos] == '_')) {
    cap(src[pos]);
    pos++;
  }
  return 1;
}

static int match_num(void) {
  skip_ws();
  if (pos >= src_len || !isdigit((unsigned char)src[pos])) return 0;
  while (pos < src_len && isdigit((unsigned char)src[pos])) {
    cap(src[pos]);
    pos++;
  }
  return 1;
}

static int match_str(void) {
  skip_ws();
  if (pos >= src_len || src[pos] != '"') return 0;
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
    return 1;
  }

  if (fr->state == ST_OK) {
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
      fr->state = ST_FAIL;
      return 1;
    }
    fr->ip += 2;
    return 1;
  }

  skip_ws();
  if (!match_lit(c)) {
    fr->state = ST_FAIL;
    return 1;
  }
  fr->ip++;
  return 1;
}

static int run_machine(void) {
  int rc;
  sp = 0;
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
  if (argc < 3) {
    fprintf(stderr, "usage: %s grammar.bnf source.p\n", argv[0]);
    return 1;
  }
  if (!load_bnf(argv[1])) return 1;
  if (!load_src(argv[2])) return 1;

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
  fprintf(stderr, "--- parse ---\n");

  ok = run_machine();
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
  fprintf(stderr, "--- REJECT at %d ---\n", pos);
  return 1;
}
