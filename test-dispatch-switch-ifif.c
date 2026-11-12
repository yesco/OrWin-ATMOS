// A simple switch/case test for parse.c compiler (Gemini)
// - rewritten using if and simple conditions only

// Used for benchmarking switch/case optimizations of

char* code= (char*)0x500;

int len;
int mode;

// TODO: unsigned
void decode_low(unsigned char low) {
    int hit;

    hit = 0;
    if (low == 0x00) hit = 1;
    if (low == 0x02) hit = 1;
    if (low == 0x09) hit = 1;
    if (low == 0x0B) hit = 1;
    if (low == 0x10) hit = 1;
    if (low == 0x12) hit = 1;
    if (low == 0x19) hit = 1;
    if (low == 0x1B) hit = 1;
    if (hit) { len = 2; mode = 1; return; }

    hit = 0;
    if (low == 0x01) hit = 1;
    if (low == 0x04) hit = 1;
    if (low == 0x05) hit = 1;
    if (low == 0x06) hit = 1;
    if (low == 0x07) hit = 1;
    if (low == 0x11) hit = 1;
    if (low == 0x14) hit = 1;
    if (low == 0x15) hit = 1;
    if (low == 0x16) hit = 1;
    if (low == 0x17) hit = 1;
    if (hit) { len = 2; mode = 2; return; }

    hit = 0;
    if (low == 0x0C) hit = 1;
    if (low == 0x0D) hit = 1;
    if (low == 0x0E) hit = 1;
    if (low == 0x0F) hit = 1;
    if (low == 0x1C) hit = 1;
    if (low == 0x1D) hit = 1;
    if (low == 0x1E) hit = 1;
    if (low == 0x1F) hit = 1;
    if (hit) { len = 3; mode = 3; return; }

    hit = 0;
    if (low == 0x08) hit = 1;
    if (low == 0x0A) hit = 1;
    if (low == 0x18) hit = 1;
    if (low == 0x1A) hit = 1;
    if (hit) { len = 1; mode = 0; return; }

    len = 1;
    mode = 4;
}

void decode_op(unsigned char op) {
    int hit;

    hit = 0;
    if (op == 0x00) hit = 1;
    if (op == 0x40) hit = 1;
    if (op == 0x60) hit = 1;
    if (hit) { len = 1; mode = 0; return; }

    hit = 0;
    if (op == 0x20) hit = 1;
    if (op == 0x4C) hit = 1;
    if (op == 0x6C) hit = 1;
    if (hit) { len = 3; mode = 3; }
}

// TODO: have to have a main! otherwise crash on Q or X or ;

//int disasm_bench() {
int main() {
  int sum = 0;
  int pc = 0;
  char op;

  while (pc < 256) {
    op = *code++;
    putn(op);

    // Loop too big (>256) so broken up
    decode_low(op & 0x1F);
    decode_op(op);

    putn(pc); putn(len);

    sum = (sum + op + (mode << 4) + len) & 0x7FFF;
    putnl(sum);

    pc = pc + len;
  }
  return sum;
}
