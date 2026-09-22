// GLOAT16 - Graphics Logarithmic Oric Atmos Topspeed 16-bit Math
//
// Designed by Jonas S Karlsson (jsk@yesco.org)
// Gemini helped write implementations, bad, buggy. lol
//
// Most of the test functions and framework by jsk.

// Must compile under cc65 to 65-2 so comply with C89
// (variables can only be defined at beginning of scope)


#ifndef GLOAT16_H
#define GLOAT16_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// TODO: we need to remove the BIAS implementation

 #define NOBIAS

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



static const uint16_t log_thresholds[9] = {
  0, 0x4d10, 0x7a25, 0x9a21, 0xb2f1, 0xc735, 0xd859, 0xe731, 0xf44b
};

#ifdef NOBIAS
gloat16 gmul(gloat16 a, gloat16 b) {
  gloat16 r = a + b;
  // Tricky: If raw addition leaves Bit 15 set, an upper overflow
  // occurred.  We selectively apply XOR 64 (0x4000) to flip the
  // linear sign slot.
  if (r & 0x8000) r ^= 0x4000;
  return r | 0x8000; // Permanently secure Gloat Tag
}
#else
gloat16 gmul(gloat16 a, gloat16 b) {
  // XOR the sign bits together, extract the combined raw value minus
  // double bias, and clear out the overlapping upper flags so we can
  // slam on the 0x8000 marker cleanly
  return (((a + b - (32 << 8)) & 0x3FFF) | 0x8000) ^ ((a ^ b) & 0x4000);
}
#endif

#ifdef NOBIAS
gloat16 gdiv(gloat16 num, gloat16 den) {
  gloat16 r = num - den;
  /* Tricky: In subtraction, if Bit 15 remains 0, no borrow flipped the upper bits.
     We selectively apply XOR 64 (0x4000) only when Bit 15 is NOT set. */
  if (!(r & 0x8000)) {
    r ^= 0x4000;
  }
  return r | 0x8000; /* Permanently secure Gloat Tag */
}
#else
gloat16 gdiv(gloat16 num, gloat16 den) {
  
  // Subtract den from num, add the bias back since we subtracted it
  // twice, mask out upper trash, restore the 0x8000 flag, and XOR the
  // sign bit.
  return (((num - den + (32 << 8)) & 0x3FFF) | 0x8000) ^ ((num ^ den) & 0x4000);
}
#endif

gloat16 glog(gloat16 a) {
  gloat_cast ca;
  int16_t true_exp;
  uint16_t frac;
  int32_t total_frac;
  int16_t linear_val;

  ca.raw = a;
  true_exp = (ca.bytes.meta & 0x3F) - 32;
  frac = ca.bytes.fraction;

  // Combine true exponent and 8-bit fraction into a single 24-bit
  // fixed-point value 
  total_frac = (true_exp * 256) + frac;
  total_frac = ((int32_t)true_exp << 8) + frac;

  // Round to the nearest linear integer to pass into itog
  linear_val = (int16_t)((total_frac + 128) >> 8);

  // Convert the linear integer back into a fresh gloat16 structure layout
  return itog(linear_val);
}

// Gemini wrote it and that wouldn't work
//
//gloat16 gpow(gloat16 base, gloat16 exponent) {
//  return gmul(glog(base), exponent);
//}

gloat16 gpow(gloat16 base, gloat16 exponent) {
  gloat_cast cbase, res;
  int16_t linear_exp;
  int16_t base_log;
  int32_t final_log;
  int16_t final_exp;

  // . Extract the linear integer value of the exponent
  linear_exp = gtoi(exponent);
  if (linear_exp == 0) {
    return 0xa000; // 1.0 in gloat16 layout
  }

  cbase.raw = base;
  // 2. Compute the base's internal fixed-point log value (biased_exp . fraction)
  base_log = ((int16_t)(cbase.bytes.meta & 0x3F) - 32) * 256 + cbase.bytes.fraction;

  // 3. Scale the log value linearly by the exponent
  final_log = (int32_t)base_log * linear_exp;

  // 4. Pack the resulting linear fixed-point log directly back into the fields
  final_exp = (int16_t)(final_log / 256);
  res.bytes.fraction = (uint8_t)(final_log & 0xFF);
  res.bytes.meta = 0x80 | ((final_exp + 32) & 0x3F);

  return res.raw;
}

// Steps subs displacment; each subtracts 1 from 77
static const uint8_t step_widths_add[77] = {
  2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2,
  3, 2, 3, 2, 3, 2, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 4, 3, 3, 4, 3, 4, 4, 4, 4, 4, 4, 4,
  5, 4, 5, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9,
  9,10,11,12,13,15,17,19,22,26,33,44,76
};

// Direct sub displacement lookup values for Delta 1 to 36
static const uint8_t sub_displacement[36] = {
  255, 192, 147, 116, 91,  72,  55,  41,  28,  17,  7,   254,
  247, 241, 235, 230, 225, 220, 216, 211, 207, 203, 199, 196,
  192, 189, 186, 183, 180, 177, 174, 171, 169, 166, 164, 161
};

gloat16 gadd(gloat16 a, gloat16 b) {
  gloat_cast ca, cb, final_res, res;
  uint8_t exp_a, exp_b, displacement_s;
  uint16_t final_frac_sum, f_sum;
  int16_t delta_frac, delta_exp, delta;
  int16_t running_delta; // Fixed: Must be 16-bit to preserve deltas > 255
  unsigned int i;
  
  // Guard: Mask strictly by 0x3FFF to sort by absolute log magnitude, ignoring sign bit
  if ((a & 0x3FFF) > (b & 0x3FFF)) {
    ca.raw = a; cb.raw = b;
  } else {
    ca.raw = b; cb.raw = a;
  }

  // Tricky: If linear signs differ and magnitudes match, they cancel to zero
  if (((a ^ b) == 0x4000)) {
    return 0x8000; // Biased representation of true 0
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

  // Determine branch path: Subtraction (Mixed Signs) vs Addition (Same Signs)
  if ((a ^ b) & 0x4000) {
    // SUBTRACTION PATH (Linear signs differ)
    if (delta >= 446)      displacement_s = 255;
    else if (delta >= 347) displacement_s = 254;
    else if (delta >= 256) displacement_s = 244;
    else if (delta >= 143) displacement_s = 214;
    else if (delta >= 93)  displacement_s = 163;
    else if (delta >= 63)  displacement_s = 131;
    else if (delta >= 37)  displacement_s = 114;
    else {
      displacement_s = sub_displacement[delta - 1];
    }
    
    final_res = ca;
    f_sum = (uint16_t)final_res.bytes.fraction - (256 - displacement_s);
    if (f_sum >= 256) { // Underflow borrow tracking across 8-bit boundary
      final_res.bytes.meta = (final_res.bytes.meta & 0xC0) | (((final_res.bytes.meta & 0x3F) - 1) & 0x3F);
    }
    final_res.bytes.fraction = (uint8_t)f_sum;
    return final_res.raw;

  } else {
    // ADDITION PATH (Linear signs match)
    running_delta = delta; // Preserves 16-bit precision layout
    displacement_s = 77;
    i = 0;

    while (i < 77) { 
      int16_t test_sub = running_delta - step_widths_add[i];
      if (test_sub < 0) break;
      running_delta = test_sub; // Fixed: Removed 8-bit truncation cast
      displacement_s--;
      ++i;
    }

    final_res = ca;
    final_frac_sum = (uint16_t)final_res.bytes.fraction + displacement_s;
    if (final_frac_sum >= 256) {
      final_res.bytes.meta = (final_res.bytes.meta & 0xC0) | (((final_res.bytes.meta & 0x3F) + 1) & 0x3F);
    }
    final_res.bytes.fraction = (uint8_t)final_frac_sum;
    return final_res.raw;
  }
}

#define GNEG(a) ((a) ^ 0x4000)

#define GSUB(a, b) gadd((a), ((b) ^ 0x4000))

gloat16 gsub(gloat16 a, gloat16 b) {
  return gadd(a, GNEG(b));
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

/* Helper to compare string outputs minus the debug suffix like (1234) */
static bool match_base_string(const char* actual, const char* expected) {
  const char * origexpected= expected, * origactual= actual;
  while (*expected) {
    if (*actual != *expected) return false;
    actual++;
    expected++;
  }
  /* Ensure the remaining string is just the debug info or empty */
  if (*actual == '\0' || *actual == '(') return 1;
  // report error
  printf("\n\tGOT     : \"%s\"\n", origactual);
  printf("\n\tEXPECTED: \"%s\"\n", origexpected);
  return 0;
}

void testatog(const char* input, const char* expected_base, uint16_t expected_hex) {
  char buf[32];
  gloat16 g = atog(input);
  gtoa(g, buf);
  
  if (g != expected_hex || !match_base_string(buf, expected_base)) {
    printf("FAIL: atog(\"%s\") -> Got hex $%04x, str \"%s\" (Expected hex $%04x, str \"%s\")\n", 
           input, g, buf, expected_hex, expected_base);
  } else {
    printf("PASS: atog(\"%s\") -> %s\n", input, expected_base);
  }
}  


typedef gloat16 (*g_op)(gloat16, gloat16);

#define STEST(op,     sa,     sb,     sres) \
  xtest(#op, op, 0,   sa, 0,  sb, 0,  sres, 0)
#define XTEST(op,     sa, ha,  sb, hb,  sres, hres)  \
  xtest(#op, op, 1,   sa, ha,  sb, hb,  sres, hres)

// Extended test validating the exact binary register layouts
gloat16 xtest(char* name, g_op op, char x,
  const char* sa,   uint16_t ha,
  const char* sb,   uint16_t hb,
  const char* sres, uint16_t hres) {

  gloat16 a = sa? atog(sa): ha;
  gloat16 b = sb? atog(sb): hb;
  gloat16 r = op(a, b);

  char buf[32], ba[32], bb[32];
  char err= 0;

  gtoa(r, buf);

  if (!sa)   sa   = gtoa(ha, ba);
  if (!sb)   sb   = gtoa(hb, bb);
  if (!sres) sres = buf;
             
  /* Verify both string representations and hardware bit values match */
  if (sres) {
    if (err= !match_base_string(buf, sres)) {
      printf("FAIL.1: %-13s %s   %-13s -> Got %s, Exp %s\n", sa, name, sb, buf, sres);
    } else {
      printf("PASS.1: %-13s %s   %-13s -> %s\n", sa, name, sb, sres);
    }
  }
    
  if (err | (x && (ha && a != ha || hb && b != hb || hres && r != hres))) {
    printf("FAIL.2 Hex:\n\tA:   %-13s Got $%04x, Exp $%04x  %c\n"
                        "\tB:   %-13s Got $%04x, Exp $%04x  %c\n"
                        "\tRes: %-13s Got $%04x, Exp $%04x  %c\n"
      , sa,  a, ha,   ha  ? a==ha  ?'=':'?' : ' '
      , sb,  b, hb,   hb  ? b==hb  ?'=':'?' : ' '
      , buf, r, hres, hres? r==hres?'=':'?' : ' '
    );
  } else if (x || !sres) {
    // TODO: maybe not print
    //printf("PASS.2 Hex: %s ($%04x) %s %s ($%04x) -> %s ($%04x)\n", 
    //sa, ha, name, sb, hb, sres, hres);
  }

  putchar('\n');
    
  return r;
}

int autotests(void) {
  char buf[32];
  gloat16 n1, n2, n3, n4, n5, n6, n7, n8, n9;
  gloat16 i1;
  int16_t r1;
  int i;
  int fail_count = 0;

  printf("\n\n============ AUTO TESTS\n");
  
  // 1. Verify parsing behavior on critical precision values
  testatog("3",         "3.00", 0xa07a);
  testatog("3.",        "3.00", 0xa07a);
  testatog("3.1",       "3.09", 0xa07d);
  testatog("3.14",      "3.15", 0xa07f);
  testatog("3.145",     "3.15", 0xa07f);
  testatog("3.1415",    "3.15", 0xa07f);
  testatog("3.14159",   "3.15", 0xa07f);
  testatog("3.141592",  "3.15", 0xa07f);
  testatog("3.1415926", "3.15", 0xa07f);

  // 2. Target operational math regressions 
  n1 = atog("3.14e6");
  n2 = atog("2.00");
  n3 = gmul(n1, n2);
  gtoa(n3, buf);
  if (!match_base_string(buf, "6.28e6")) {
    printf("FAIL: gmul Test (3.14e6 * 2.00) -> Got: %s\n", buf);
    fail_count++;
  }

  n4 = gdiv(n3, n2);
  gtoa(n4, buf);
  if (!match_base_string(buf, "3.14e6")) {
    printf("FAIL: gdiv Test (Result / 2.00) -> Got: %s\n", buf);
    fail_count++;
  }

  n5 = atog("1.00e3");
  n6 = atog("5.00e2");
  n7 = gadd(n5, n6);
  gtoa(n7, buf);
  if (!match_base_string(buf, "1k50")) {
    printf("FAIL: gadd Test (1.00e3 + 5.00e2) -> Got: %s\n", buf);
    fail_count++;
  }

  n8 = gsub(n7, n6);
  gtoa(n8, buf);
  if (!match_base_string(buf, "1.00e3")) {
    printf("FAIL: gsub Test (Result - 5.00e2) -> Got: %s\n", buf);
    fail_count++;
  }

  // 3. Integer roundtrip validations
  i1 = itog(42);
  r1 = gtoi(i1);
  if (r1 != 42) {
    printf("FAIL: itog/gtoi Test (42) -> Converted back to: %d\n", r1);
    fail_count++;
  }

  n9 = atog("-3G14");
  gtoa(n9, buf);
  if (!match_base_string(buf, "-3.14G")) {
    printf("FAIL: Schematic Input Test (-3G14) -> Got: %s\n", buf);
    fail_count++;
  }

  // 4. Complete dynamic range roundtrip integrity verification

  // NOTE: -101..101 is encode "exact"
  for (i = -16; i <= 101; ++i) {
    gloat16 g = itog((int16_t)i);
    int16_t gi = gtoi(g);
    if (i != gi) {
      printf("FAIL: Roundtrip mismatch at integer %d -> Got %d via gtoi\n", i, gi);
      fail_count++;
    }
  }

  if (fail_count == 0) {
    printf("\nALL CORE REGRESSION TESTS PASSED CLEANLY.\n");
  } else {
    printf("\nTEST SUITE FAILED WITH %d TOTAL ERRORS.\n", fail_count);
  }

  return fail_count;
}

int misctests() {
  char buf[32];
  gloat16 n9;
  gloat16 i1;
  int16_t r1;

  int fails= 0; // TODO: make global?
  gloat16 g;
  
  printf("\n============ MISC TESTS\n");

  g= STEST(gmul,   "3.14",     "2.00",      "6.28"    );
  g= XTEST(gdiv,   NULL, g,    "2.00", 0,   "3.15",  0);
  
  g= STEST(gadd,   "1.00e3",   "5.00e2",    "1k58"    );
  g= XTEST(gsub,   NULL, g,    "5.00e2",0,  "1.00e3",0);

  g= STEST(gadd,   "42",       "42",        "84"       );
//g= STEST(gadd,   "42",       "-42",       "1e-32"    );
  g= STEST(gadd,   "42",       "-42",       "1.00e-32"    );


  // TODO: extend STEST/XTEST with single arg op(a) ?

  // TODO: redundant with listing test
  i1 = itog(42);
  gtoa(i1, buf);
  printf("itog Test (42): %s\n", buf);

  r1 = gtoi(i1);
  printf("gtoi Test (Converted back): %d\n", r1);

  n9 = atog("-3G14");
  gtoa(n9, buf);
  printf("Schematic Input Test (-3G14): %s\n", buf);

  return fails;
}

// TODO: pick out some random numbers
int numbertests() {
  int16_t i;
  
  printf("\n============ NUMBER TESTS\n");
  printf("\nDEC\tBIN:g =>a         =>   i\tASC:g =>a         =>   i\n");
  printf("-----------------------------------------------------------------------\n");
  for(i=-16; i<=256; ++i) {
    char str[16], gstr[16], hstr[16];
    gloat16 g, h;
    int16_t gi, hi;

    g = itog(i);
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
  putchar('\n');

  return 0;
}

int addsubtests(int base) {
  int16_t i;
  gloat16 gbase_val = itog(base);
  char gb[16];

  gtoa(gbase_val, gb);

  printf("\n============ ADDSUB TESTS\n");
  printf("base= %d  gbase=%-16s", base, gb);
  printf("\nDEC\t             | ADD:g =>a             => got want CAN             | SUB:g =>a         =>    got want CAN\n");
  printf("----------------------------------------------------------------------------------------------------------------------------\n");
  for(i=base; i>=0; --i) {
    char gstr[16], astr[16], sstr[16], pastr[16], psstr[16];
    gloat16 g, a, s;
    gloat16 pa, ps;
    int16_t ai, si;

    g = itog(i);
    gtoa(g, gstr);

    // ADD
    a = gadd(gbase_val, g);
    gtoa(a, astr);
    ai = gtoi(a);
    pa = itog(base+i);
    gtoa(pa, pastr);
    
    // SUB
    s = gsub(gbase_val, g);
    gtoa(s, sstr);
    si = gtoi(s);
    ps = itog(base-i);
    gtoa(ps, psstr);

    printf("%4d => %-13s"
      " | %04x = %-13s =>%4d %4d %-13s %c"
      " | %04x = %-13s =>%4d %4d %-13s %c"
      "\n"
      , i, gstr
      , a, astr, ai, base+i, pastr, ai==(base+i)?' ':'?'
      , s, sstr, si, base-i, psstr, si==(base-i)?' ':'?'
    );
  }
  putchar('\n');

  return 0;
}

int main(void) {
  return 0 
//    + numbertests()
//    + autotests()
//    + misctests()
    + addsubtests(1000)
    ;
}

#endif // !MAIN
