// fragment of global vars code.

// only benefit is that there is only
// "atom"string search using strstr
// result -1 is "index" number 0x8n.
// Possibly "fastests" search, but much code.
// Also, requires 2 parallell arrays (vars/vals)

// TODO: these are global for now
char* vars= NULL;
#define MAX_VARS 128
char* vals[MAX_VARS]= {0};

// possibly too much code,lol - too clever?
char vnth(char* name) {
  unsigned int vlen, len, n= (unsigned int)(intptr_t)name;
  char* found;
  if (vars) {
    if ((n^0x80) < 0x80) return n;
    // second chance, lol
    n= *name;
    if ((n^0x80) < 0x80) return n;
  }
  // defined?
  // TODO: prefix by ':'
  if (vars && (found= strstr(vars, name))) return found[-1];

  // not defined, let's add
  n= ((vars?*vars: 0) + 1) | 0x80; // assigns next 0x8n code
  len= strlen(name)+1;
  // TODO: reallocs every friggin time, lol
  vlen = vars? strlen(vars)+1: 1;
  vars= realloc(vars, vlen + len);
  memmove(vars+len, vars, vlen);
  // prefix with "id" number 0x8n
  *vars= n;
  memcpy(vars+1, name, len-1);

  return n;
}
    
char* vset(char* name, char* val) {
  char n= vnth(name), **p;
  qputs(vars);
  lfree(*(p= &vals[n & 0x80]));
  return *p= val;
}

// you don't own the value coming out
// you can make a copy
char* vget(char* name) {
  char n= vnth(name), *p;
  return vals[n & 0x80];
}

char* veval(char* expr) {
  return expr;
}

#endif
