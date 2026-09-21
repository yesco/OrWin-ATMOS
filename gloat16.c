// Must compile under cc65 to 65-2 so comply with C89
// (variables can only be defined at beginning of scope)
#ifndef GLOAT16_H
#define GLOAT16_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef uint16_t gloat16;

gloat16 gmul(gloat16 a,    gloat16 b);
gloat16 gdiv(gloat16 num,  gloat16 den);
gloat16 gadd(gloat16 a,    gloat16 b);
gloat16 gsub(gloat16 a,    gloat16 b);
gloat16 glog(gloat16 a);
gloat16 gpow(gloat16 base, gloat16 exponent);

gloat16 itog(int16_t val);
int16_t gtoi(gloat16 a);
gloat16 atog(const char* str);
char*   gtoa(gloat16 a,    char* buf);

#endif

typedef union {
  gloat16 raw;
  struct {
    uint8_t fraction;
    uint8_t meta;
  } bytes;
} gloat_cast;

static const uint8_t step_widths[67] = {
  2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2,
  3, 2, 3, 2, 3, 2, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 4, 3, 3, 4, 3, 4, 4, 4, 4, 4, 4, 4,
  5, 4, 5, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9,
  9, 10, 11
};

static const uint16_t log_thresholds[9] = {
  0, 0x4d10, 0x7a25, 0x9a21, 0xb2f1, 0xc735, 0xd859, 0xe731, 0xf44b
};

gloat16 gmul(gloat16 a, gloat16 b) {
  gloat_cast ca, cb, res;
  uint8_t exp_a, exp_b;
  int16_t true_exp;
  uint16_t frac_sum;
  
  ca.raw = a;
  cb.raw = b;
  res.bytes.meta = 0x80;
  res.bytes.meta |= (ca.bytes.meta ^ cb.bytes.meta) & 0x40;
  exp_a = ca.bytes.meta & 0x3F;
  exp_b = cb.bytes.meta & 0x3F;
  true_exp = (int16_t)(exp_a - 32) + (int16_t)(exp_b - 32);
  frac_sum = (uint16_t)ca.bytes.fraction + cb.bytes.fraction;
  if (frac_sum >= 256) {
    true_exp++;
  }
  res.bytes.fraction = (uint8_t)frac_sum;
  res.bytes.meta |= (uint8_t)(true_exp + 32) & 0x3F;
  return res.raw;
}

gloat16 gdiv(gloat16 num, gloat16 den) {
  gloat_cast cnum, cden, res;
  uint8_t exp_num, exp_den;
  int16_t true_exp;
  int16_t frac_diff;

  cnum.raw = num;
  cden.raw = den;
  res.bytes.meta = 0x80;
  res.bytes.meta |= (cnum.bytes.meta ^ cden.bytes.meta) & 0x40;
  exp_num = cnum.bytes.meta & 0x3F;
  exp_den = cden.bytes.meta & 0x3F;
  true_exp = (int16_t)(exp_num - 32) - (int16_t)(exp_den - 32);
  frac_diff = (int16_t)cnum.bytes.fraction - cden.bytes.fraction;
  if (frac_diff < 0) {
    true_exp--;
    frac_diff += 256;
  }
  res.bytes.fraction = (uint8_t)frac_diff;
  res.bytes.meta |= (uint8_t)(true_exp + 32) & 0x3F;
  return res.raw;
}

gloat16 glog(gloat16 a) {
  gloat_cast ca;
  ca.raw = a;
  ca.bytes.meta &= 0x3F;
  return ca.raw;
}

gloat16 gpow(gloat16 base, gloat16 exponent) {
  return gmul(glog(base), exponent);
}

gloat16 gadd(gloat16 a, gloat16 b) {
  gloat_cast ca, cb, final_res, res;
  uint8_t exp_a, exp_b, displacement_s;
  uint16_t final_frac_sum, f_sum;
  int16_t delta_frac, delta_exp, delta;
  
  // Make A bigger than B
  if ((a & 0x7FFF) > (b & 0x7FFF)) {
    ca.raw = a; cb.raw = b;
  } else {
    ca.raw = b; cb.raw = a;
  }

  exp_a = ca.bytes.meta & 0x3F;
  exp_b = cb.bytes.meta & 0x3F;
  delta_exp = (int16_t)exp_a - exp_b;
  delta_frac = (int16_t)ca.bytes.fraction - cb.bytes.fraction;
  delta = (delta_exp * 256) + delta_frac;
  if (delta >= 524) {
    return ca.raw;
  }
  if (delta == 0) {
    res = ca;
    f_sum = (uint16_t)res.bytes.fraction + 77;
    if (f_sum >= 256) {
      res.bytes.meta = (res.bytes.meta & 0xC0) | (((res.bytes.meta & 0x3F) + 1) & 0x3F);
    }
    res.bytes.fraction = (uint8_t)f_sum;
    return res.raw;
  }
  if (delta >= 256) {
    if (delta >= 446)      displacement_s = 1;
    else if (delta >= 401) displacement_s = 2;
    else if (delta >= 368) displacement_s = 3;
    else if (delta >= 343) displacement_s = 4;
    else if (delta >= 322) displacement_s = 5;
    else if (delta >= 304) displacement_s = 6;
    else if (delta >= 289) displacement_s = 7;
    else if (delta >= 275) displacement_s = 8;
    else if (delta >= 263) displacement_s = 9;
    else                   displacement_s = 10;
  } else {
    uint8_t running_delta = (uint8_t)delta;
    uint8_t table_ptr;
    if (running_delta >= 63) {
      running_delta -= 63;
      displacement_s = 49;
      table_ptr = 28;
    } else {
      displacement_s = 77;
      table_ptr = 0;
    }
    while (table_ptr < 67) { /* Boundary protection safely inside loop condition */
      int16_t test_sub = (int16_t)running_delta - step_widths[table_ptr];
      if (test_sub < 0) break;
      running_delta = (uint8_t)test_sub;
      displacement_s--;
      table_ptr++;
    }
  }
  final_res = ca;
  final_frac_sum = (uint16_t)final_res.bytes.fraction + displacement_s;
  if (final_frac_sum >= 256) {
    final_res.bytes.meta = (final_res.bytes.meta & 0xC0) | (((final_res.bytes.meta & 0x3F) + 1) & 0x3F);
  }
  final_res.bytes.fraction = (uint8_t)final_frac_sum;
  return final_res.raw;
}

gloat16 gsub(gloat16 a, gloat16 b) {
  gloat_cast cb;
  cb.raw = b;
  cb.bytes.meta ^= 0x40;
  return gadd(a, cb.raw);
}

gloat16 itog(int16_t val) {
  gloat_cast res;
  uint8_t exp, digit_idx;
  uint16_t frac16;
  int16_t temp_val, leading_digit;
  uint32_t base_frac, next_frac, gap;
  int32_t rem;
  int16_t rem_divisor;
  uint8_t i;
  uint32_t log_interp;

  if (val == 0) {
    res.bytes.meta = 0x80;
    res.bytes.fraction = 0;
    return res.raw;
  }
  res.bytes.meta = 0x80;
  if (val < 0) {
    res.bytes.meta |= 0x40;
    val = -val;
  }
  exp = 0;
  temp_val = val;
  while (temp_val >= 10) {
    temp_val /= 10;
    exp++;
  }
  leading_digit = temp_val;
  res.bytes.meta |= (exp + 32) & 0x3F;
  
  digit_idx = (leading_digit > 0 && leading_digit <= 9) ? (leading_digit - 1) : 0;
  base_frac = log_thresholds[digit_idx];
  next_frac = (leading_digit < 9) ? log_thresholds[digit_idx + 1] : 65536;
  gap = next_frac - base_frac;
  
  rem = val;
  rem_divisor = 1;
  for (i = 0; i < exp; i++) {
    rem_divisor *= 10;
  }
  rem -= (int32_t)leading_digit * rem_divisor;
  
  if (rem == 0) {
    log_interp = 0;
  } else {
    log_interp = ((uint32_t)rem * gap) / rem_divisor;
    log_interp = (log_interp * leading_digit) / (leading_digit + (rem / rem_divisor));
    if (log_interp >= gap) log_interp = gap - 1;
  }
  
  frac16 = (uint16_t)(base_frac + log_interp);
  res.bytes.fraction = (uint8_t)((frac16 + 128) / 256);
  return res.raw;
}

int16_t gtoi(gloat16 a) {
  gloat_cast ca;
  int8_t true_exp;
  uint16_t full_frac;
  int16_t digit;
  uint16_t base_frac, next_frac;
  uint32_t rem, gap, interp;
  int32_t val;
  signed char i;

  ca.raw = a;
  true_exp = (ca.bytes.meta & 0x3F) - 32;
  if (true_exp < 0) return 0;
  
  full_frac = (uint16_t)ca.bytes.fraction * 256;
  digit = 1;
  for (i = 8; i >= 0; i--) {
    if (full_frac >= log_thresholds[i]) {
      digit = i + 1;
      break;
    }
  }
  
  base_frac = log_thresholds[digit - 1];
  next_frac = (digit < 9) ? log_thresholds[digit] : 0xffff;
  rem = full_frac - base_frac;
  gap = next_frac - base_frac;
  interp = 0;
  if (gap > 0) {
    interp = (rem * 100 + (gap / 2)) / gap;
  }
  
  val = ((int32_t)digit * 100) + interp;
  
  if (true_exp == 0) {
    val = (val + 50) / 100;
  } else if (true_exp == 1) {
    val = (val + 5) / 10;
  } else {
    true_exp -= 2;
    while (true_exp > 0) {
      val *= 10;
      true_exp--;
    }
  }
  
  if (ca.bytes.meta & 0x40) {
    val = -val;
  }
  return (int16_t)val;
}

gloat16 atog(const char* str) {
  gloat_cast res;
  const char* p = str;
  const char* end_digits;
  const char* dec_ptr = NULL;
  int8_t exp_val = 0;
  bool is_negative = false;
  signed char post_dec_digits = 0;
  const char* scan;
  int16_t final_exp;
  int32_t linear_mantissa;
  int32_t multiplier;
  const char* r;

  if (*p == '-') {
    is_negative = true;
    p++;
  } else if (*p == '+') {
    p++;
  }

  scan = p;
  while (*scan) {
    if (*scan == '.') {
      dec_ptr = scan;
    } else if (*scan == 'e' || *scan == 'E' || 
               *scan == 'k' || *scan == 'M' || *scan == 'G' || *scan == 'T' || *scan == 'P' ||
               *scan == 'm' || *scan == 'u' || *scan == 'n' || *scan == 'p') {
      break;
    }
    scan++;
  }
  end_digits = scan;

  if (*end_digits == 'e' || *end_digits == 'E') {
    int8_t exp_sign = 1;
    int8_t parsed_exp = 0;
    const char* e_scan = end_digits + 1;
    if (*e_scan == '-') { exp_sign = -1; e_scan++; }
    else if (*e_scan == '+') { e_scan++; }
    while (*e_scan >= '0' && *e_scan <= '9') {
      parsed_exp = parsed_exp * 10 + (*e_scan - '0');
      e_scan++;
    }
    exp_val = parsed_exp * exp_sign;
  } else if (*end_digits != '\0') {
    char prefix = *end_digits;
    if (prefix == 'k') exp_val = 3;
    else if (prefix == 'M') exp_val = 6;
    else if (prefix == 'G') exp_val = 9;
    else if (prefix == 'T') exp_val = 12;
    else if (prefix == 'P') exp_val = 15;
    else if (prefix == 'm') exp_val = -3;
    else if (prefix == 'u') exp_val = -6;
    else if (prefix == 'n') exp_val = -9;
    else if (prefix == 'p') exp_val = -12; /* Fixed Pico Prefix */
  }

  if (dec_ptr != NULL) {
    post_dec_digits = (signed char)(end_digits - dec_ptr - 1);
    exp_val -= post_dec_digits;
  }

  linear_mantissa = 0;
  multiplier = 1;
  r = end_digits - 1;

  while (r >= p) {
    if (*r == '.') {
      r--;
      continue;
    }
    
    if (*r >= '0' && *r <= '9') {
      linear_mantissa += (int32_t)(*r - '0') * multiplier;
      multiplier *= 10;
    }
    r--;
  }

  res.raw = itog((int16_t)linear_mantissa);
  
  if (is_negative) {
    res.bytes.meta |= 0x40;
  }
  
  if (linear_mantissa != 0) {
    final_exp = (int16_t)(res.bytes.meta & 0x3F) - 32 + exp_val;
    res.bytes.meta = (res.bytes.meta & 0xC0) | ((final_exp + 32) & 0x3F);
  }

  return res.raw;
}

#define GFLOAT_DEBUG 1

char* gtoa(gloat16 a, char* buf) {
  char* origbuf = buf;
  gloat_cast ca;
  int8_t true_exp;
  uint16_t full_frac;
  int16_t digit;
  uint16_t base_frac, next_frac;
  uint32_t rem, gap, interp;
  char prefix;
  signed char i;
  
  ca.raw = a;
  if (ca.bytes.meta & 0x40) *buf++ = '-';
  true_exp = (ca.bytes.meta & 0x3F) - 32;
  full_frac = (uint16_t)ca.bytes.fraction * 256;
  digit = 1;
  for (i = 8; i >= 0; i--) {
    if (full_frac >= log_thresholds[i]) {
      digit = i + 1;
      break;
    }
  }

  base_frac = log_thresholds[digit - 1];
  next_frac = (digit < 9) ? log_thresholds[digit] : 0xffff;
  rem = full_frac - base_frac;
  gap = next_frac - base_frac;
  interp = 0;
  if (gap > 0) {
    interp = (rem * 100 + (gap / 2)) / gap;
    if (interp >= 100) {
      interp -= 100;
      digit++;
      if (digit > 9) {
        digit = 1;
        true_exp++;
      }
    }
  }
  prefix = ' ';
  if (true_exp == 0) { prefix = '.'; }
  else if (true_exp == 3) { prefix = 'k'; true_exp = 0; }
  else if (true_exp == 6) { prefix = 'M'; true_exp = 0; }
  else if (true_exp == 9) { prefix = 'G'; true_exp = 0; }
  else if (true_exp == 12) { prefix = 'T'; true_exp = 0; }
  else if (true_exp == 15) { prefix = 'P'; true_exp = 0; }
  else if (true_exp == -3) { prefix = 'm'; true_exp = 0; }
  else if (true_exp == -6) { prefix = 'u'; true_exp = 0; }
  else if (true_exp == -9) { prefix = 'n'; true_exp = 0; }
  else if (true_exp == -12) { prefix = 'p'; true_exp = 0; } /* Fixed prefix match */

  if (prefix != ' ' && prefix != '.') {
    sprintf(buf, "%d%c%02d", digit, prefix, (int)interp);
  } else if (prefix == '.') {
    sprintf(buf, "%d.%02d", digit, (int)interp);
  } else if (true_exp == 1) {
    uint32_t aligned_val = ((uint32_t)digit * 100) + interp;
    sprintf(buf, "%d.%d", (int)(aligned_val / 10), (int)(aligned_val % 10));
  } else {
    sprintf(buf, "%d.%02de%d", digit, (int)interp, true_exp);
  }

#if GFLOAT_DEBUG
  sprintf(buf+strlen(buf), "(%u)", (unsigned int)rem);
#endif

  return origbuf;
}



#ifndef MAIN

void testatog(char* a) {
  char buf[32];
  gloat16 g= atog(a);
  printf("atog/gtoa test (%s) => $%04x == %s\n", a, g, gtoa(g, buf));
}  

int main(void) {
  char buf[32];
  gloat16 n1, n2, n3, n4, n5, n6, n7, n8, n9, n10;
  gloat16 i1;
  int16_t r1;
  int i;

  testatog("3");
  testatog("3.");
  testatog("3.1");
  testatog("3.14");
  testatog("3.145");
  testatog("3.1415");
  testatog("3.14159");
  testatog("3.141592");
  testatog("3.1415926");
  testatog("3.14159265");
  testatog("3.141592654");

  n2 = atog("2.00");
  n3 = gmul(n1, n2);
  gtoa(n3, buf);
  printf("gmul Test (3.14e6 * 2.00): %s\n", buf);

  n4 = gdiv(n3, n2);
  gtoa(n4, buf);
  printf("gdiv Test (Result / 2.00): %s\n", buf);

  n5 = atog("1.00e3");
  n6 = atog("5.00e2");
  n7 = gadd(n5, n6);
  gtoa(n7, buf);
  printf("gadd Test (1.00e3 + 5.00e2): %s\n", buf);

  n8 = gsub(n7, n6);
  gtoa(n8, buf);
  printf("gsub Test (Result - 5.00e2): %s\n", buf);

  i1 = itog(42);
  gtoa(i1, buf);
  printf("itog Test (42): %s\n", buf);

  r1 = gtoi(i1);
  printf("gtoi Test (Converted back): %d\n", r1);

  n9 = atog("-3G14");
  gtoa(n9, buf);
  printf("Schematic Input Test (-3G14): %s\n", buf);

  printf("\nDEC\tBIN:g =>a         =>   i\tASC:g =>a         =>   i\n");
  printf("-----------------------------------------------------------------------\n");
  for(i=-16; i<=256; ++i) {
    char str[16], gstr[16], hstr[16];
    gloat16 g, h;
    int16_t gi, hi;

    g = itog((int16_t)i);
    gtoa(g, gstr);
    gi = gtoi(g);
    sprintf(str, "%d", i);

    h = atog(str);
    gtoa(h, hstr);
    hi = gtoi(h);
    printf("%3d"
      "\t%04x %-12s => %3d %c"
      "\t%04x %-12s => %3d %c"
      "   %c\n"
      , i
      , g, gstr, (int)gi, i==gi?' ':'~'
      , h, hstr, (int)hi, i==hi?' ':'~'
      , gi==hi?' ':'/'
    );
  }
  return 0;
}

#endif // !MAIN
