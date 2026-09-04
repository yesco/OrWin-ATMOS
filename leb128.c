// leb128 and various serialization implemenations
//
// WARNING: untested embryos



// USE: oaq.c instead1





// (C) 2026 Jonas S Karlsson (jsk@yesco.org)
// Varoious encodings:

// working I think

char* LEB128(char* s, long v) {
  while(v >= 128) {
    *s++= (v & 127) | 128;
    v>>= 7;
  }
  *s++= v;
  return s;
}

char* unLEB128(char* s, long *v) {
  char c;
  if ((c= *s) < 128) {
    *v|= c;
    return s+1;
  } else {
    *v|= c & 127;
    *v<<= 7;
    return unLEB128(s+1, v);
  }
}


// reverse LEB128 encoding (big endian)
// working?
char* BEL128(char* s, unsigned long v) {
  if (v >= 128) {
    *s= 128;
    s= BEL128(s, v >> 7);
  }

  *s^= v;
  return s+1;
}

char* unBEL128(char* s, long *v) {
  static char c;
  *v= 0;
  while((c= *s++) >= 128) {
    *v|= c & 127;
    *v<<= 7;
  }
  *v|= c;
  return s;
}


// TODO: test
char* QAOLS(char* s, int32_t *l) {
  ++s; // skip sign!
  return QAOL(s, (uint32_t*)l);
}

// TODO: test
char* SLOAQ(char* s, int32_t l) {
  *s++= l<0? OAQ_NEG: OAQ_POS;
  return LOAQ(s, (uint32_t)l);
}

#endif // OAQ_SIGNED


#endif // OAQ_U32

long unJSK128(char* *s) {
  static char c; static long v;
  if ((c= *(*s)++) < 128) return c;
  v= c ^ 128;
  while((c= *++*s) >= 128) {
    v= (v<<7) + c ^ 128;
  }
  return v;
}

