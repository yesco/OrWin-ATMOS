#ifndef GLOAT16_H
#define GLOAT16_H

#include <stdint.h>
#include <stdbool.h>

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
void    gtoa(gloat16 a,    char* buf);

#endif

#include <stdio.h>
#include <string.h>

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
    while (1) {
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

  if (val == 0) {
    res.bytes.meta = 0x80 | 0;
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
  
  frac16 = (uint16_t)(base_frac + ((uint32_t)rem * gap) / rem_divisor);
  res.bytes.fraction = (uint8_t)(frac16 / 256);
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
  next_frac = (digit < 9) ? log_thresholds[digit] : 65535;
  rem = full_frac - base_frac;
  gap = next_frac - base_frac;
  interp = 0;
  if (gap > 0) {
    interp = (rem * 100) / gap;
  }
  
  val = ((int32_t)digit * 100) + interp;
  
  if (true_exp == 0) {
    val = (val + 50) / 100; /* Round to nearest integer */
  } else if (true_exp == 1) {
    val = (val + 5) / 10;   /* Round to nearest integer */
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
  uint8_t digit, dec1, dec2;
  int8_t exp_val;
  uint8_t digit_idx;
  uint32_t base_frac, next_frac, gap, interpolation;
  uint16_t final_frac16;

  res.bytes.meta = 0x80;
  if (*str == '-') {
    res.bytes.meta |= 0x40;
    str++;
  } else if (*str == '+') {
    str++;
  }
  digit = *str - '0';
  str++;
  if (*str == '.') {
    str++;
  }
  dec1 = 0, dec2 = 0;
  if (*str >= '0' && *str <= '9') {
    dec1 = *str - '0';
    str++;
  }
  if (*str >= '0' && *str <= '9') {
    dec2 = *str - '0';
    str++;
  }
  exp_val = 0;
  if (*str == 'e' || *str == 'E') {
    int8_t exp_sign = 1;

    str++;
    if (*str == '-') {
      exp_sign = -1;
      str++;
    } else if (*str == '+') {
      str++;
    }
    while (*str >= '0' && *str <= '9') {
      exp_val = exp_val * 10 + (*str - '0');
      str++;
    }
    exp_val *= exp_sign;
  } else {
    char prefix = *str;

    if (prefix == 'k') exp_val = 3;
    else if (prefix == 'M') exp_val = 6;
    else if (prefix == 'G') exp_val = 9;
    else if (prefix == 'T') exp_val = 12;
    else if (prefix == 'P') exp_val = 15;
    else if (prefix == 'm') exp_val = -3;
    else if (prefix == 'u') exp_val = -6;
    else if (prefix == 'n') exp_val = -9;
    else if (prefix == 'p') exp_val = -10; 
  }
  res.bytes.meta |= (exp_val + 32) & 0x3F;
  digit_idx = (digit > 0 && digit <= 9) ? (digit - 1) : 0;
  base_frac = log_thresholds[digit_idx];
  next_frac = (digit < 9) ? log_thresholds[digit_idx + 1] : 65536;
  gap = next_frac - base_frac;
  interpolation = (dec1 * 10 + dec2) * gap / 100;
  final_frac16 = (uint16_t)(base_frac + interpolation);
  res.bytes.fraction = (uint8_t)(final_frac16 / 256);
  return res.raw;
}

void gtoa(gloat16 a, char* buf) {
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
    interp = (rem * 100) / gap;
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
  else if (true_exp == -10) { prefix = 'p'; true_exp = 0; }
  
  if (prefix != ' ') {
    sprintf(buf, "%d%c%02d", digit, prefix, (int)interp);
  } else {
    if (true_exp == 0) {
      sprintf(buf, "%d.%02d", digit, (int)interp);
    } else {
      sprintf(buf, "%d.%02de%d", digit, (int)interp, true_exp);
    }
  }
}

int main(void) {
  char buf[32];
  gloat16 n1, n2, n3, n4, n5, n6, n7, n8, i1, n9;
  int16_t r1;
  int i;

  n1 = atog("3.14e6");
  gtoa(n1, buf);
  printf("atog/gtoa Test 1 (3.14e6): %s\n", buf);

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

  printf("\nDEC\tBIN:g =>a     =>   i\tASC:g =>a     =>   i\n");
  printf("----------------------------------------------------\n");
  for(i=0; i<=256; ++i) {
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
      "\t%04x %-8s => %3d %c"
      "\t%04x %-8s => %3d %c"
      "\n"
      , i
      , g, gstr, (int)gi, i==gi?' ':'~'
      , h, hstr, (int)hi, i==hi?' ':'~'
    );
  }
  return 0;
}
