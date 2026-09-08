int fputqsnw(unsigned char* s, int len, FILE* f, int width) {
  int n= 0; unsigned char c;

  printf("fputqsnw: %04X %d %04X %d\n", s,  len, f, width);
  
  if (!s)  return fputs("(NULL)", f);
  if (!*s) return fputs("\"\"", f);
  
  n += fputs("\"", f);
  printf("fputqsnw: 11111\n");
 next:
  printf("fputqsnw: 22\n");
  --len;
  if (width > 0 && width-n <= 3) { n+= fprintf(f, "..."); goto spaces; }
  printf("fputqsnw: 33\n");
  switch((c= *s++)) {
  case '\n': n+= fputs("\\n", f);  goto next;
  case '\t': n+= fputs("\\t", f);  goto next;
  case '"' : n+= fputs("\\\"", f); goto next;
  default  :
  printf("fputqsnw: 44\n");
    if (c==0 && len < 0) goto done;
    printf("fputqsnw: 44 aa\n");
//    if (c<32 || c>126 || c&0x80) // nah, doesn't catch it
    if (c<32 || c&0x80)
{
    printf("fputqsnw: 44 bb\n");
      n+= fprintf(f, "\\x%02x", c);
    printf("fputqsnw: 44 cc\n");
}
    else
{
    printf("fputqsnw: 44 dd f=%04x c=%d\n", f, c);
//    if (c<128)  // doesn't do anything!
//      n+= fprintf(f, "%c", c&0x7f);// also nothing
//    n+= fprintf(f, "%c", 42); // crashes too!
//    ++n; fputc(42, f);
    ++n; fputc(c, f);
    
    printf("fputqsnw: 44 ee\n");
}
    printf("fputqsnw: 44 ff\n");
    if (len>0 && len) goto next;
    printf("fputqsnw: 44 gg\n");
  }
  printf("fputqsnw: 55\n");
 done:
  printf("fputqsnw: 66\n");
  n+= fputs("\"", f);
  printf("fputqsnw: 77\n");

 spaces:
  while (n++ < width) putchar(' ');
  printf("fputqsnw: 88\n");

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
