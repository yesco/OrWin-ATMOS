import math

# 1. Generate the target 4-digit logarithmic sequence
targets = [round(1000 * (10 ** (x / 256))) for x in range(256)]

# 2. Extract step sizes
deltas = [targets[x] - targets[x-1] for x in range(1, 256)]

# 3. Derive second layer modifications
current_delta = 9
delta_stream = []
for d in deltas:
    change = d - current_delta
    delta_stream.append(change)
    current_delta += change

# 4. Convert modifications to 2-bit symbols
bit_pairs = []
for c in delta_stream:
    if c == 0: bit_pairs.append(0)    # %00
    elif c == 1: bit_pairs.append(1)  # %01
    elif c == -1: bit_pairs.append(2) # %10
    elif c == 2: bit_pairs.append(3)  # %11

while len(bit_pairs) % 4 != 0:
    bit_pairs.append(0)

# 5. Pack 4 bit-pairs per byte (MSB to LSB)
bytes_out = []
for i in range(0, len(bit_pairs), 4):
    b = (bit_pairs[i] << 6) | (bit_pairs[i+1] << 4) | (bit_pairs[i+2] << 2) | bit_pairs[i+3]
    bytes_out.append(b)

# 6. Output ca65 formatting
formatted = [f"${x:02X}" for x in bytes_out]
for k in range(0, len(formatted), 16):
    print("\t.byte " + ", ".join(formatted[k:k+16]))
