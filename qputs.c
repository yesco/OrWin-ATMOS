int fputqsnw(char* s, int len, FILE* f, int width) {
  int n= 0; char c;

  if (!s)  return fputs("(NULL)", f);
  if (!*s) return fputs("\"\"", f);
  
  n += fputs("\"", f);
 next:
  --len;
  if (width > 0 && width-n <= 3) { n+= fprintf(f, "..."); goto spaces; }
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
    if (c==0 && len < 0) goto done;
    if (c<32 || c>126)
      n+= fprintf(f, "\\x%02x", c);
    else
      n+= fprintf(f, "%c", c);
    if (len>0 && len) goto next;
  }
 done:
  n+= fputs("\"", f);

 spaces:
  while (n++ < width) putchar(' ');

  return n;
}

void fputqsn(char* s, int len, FILE* f) {
  fputqsnw(s, len, f, -1);
}

//////////////////////////////
#ifndef NL

  #define nl() { putchar('\n'); }

  #define NL

#endif // NL




#ifdef OLD
// TODO: get rid of, but first see if we have
//   any fixes to forward port!?


// TODO: replace and use the oafs: fputqsnw function instead
//   or maybe here just a byte/hex print %x lol w &
int qputsn(char* s, int len, FILE* f) {
  int n= 0; char c;

  if (!s) return fputs("(NULL)", f);
  n += fputc('"', f);

 next:
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
    if (c<32 || c>126)
      n+= fprintf(f, "\\x%02x", c);
    else
      n+= fputc(c, f);
    if (len>0 && --len) goto next;
  }

  n+= fputc('"', f);
  return n;
}

void nl() { putchar('\n'); }

#endif
