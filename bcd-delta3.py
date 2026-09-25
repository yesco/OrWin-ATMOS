import math

# 1. Generate the exact 4-digit rounded log targets matching your left column
targets = [round(1000 * (10 ** (x / 256))) for x in range(256)]

# 2. Extract step sizes starting from Step 1 (Bypassing Step 0 offset slip)
deltas = [targets[x] - targets[x-1] for x in range(1, 256)]

# 3. Simulate the exact sequential step tracking of the 6502 engine
current_delta = 9
delta_stream = []

for d in deltas:
    change = d - current_delta
    delta_stream.append(change)
    current_delta += change

# 4. Correct Flag Bug Map matching ASL processing (%00=0, %01=+1, %10=-1, %11=+2)
bit_pairs = []
for c in delta_stream:
    if c == 0: bit_pairs.append(0)     # %00 (Unchanged)
    elif c == 1: bit_pairs.append(1)   # %01 (Increment +1)
    elif c == -1: bit_pairs.append(2)  # %10 (Decrement -1)
    elif c == 2: bit_pairs.append(3)   # %11 (Add Two +2)

# Pad trailing bits to finalize last byte alignment cleanly
while len(bit_pairs) % 4 != 0:
    bit_pairs.append(0)

# 5. Pack 4 pairs per byte from MSB to LSB matching ASL direction
bytes_out = []
for i in range(0, len(bit_pairs), 4):
    b = (bit_pairs[i] << 6) | (bit_pairs[i+1] << 4) | (bit_pairs[i+2] << 2) | bit_pairs[i+3]
    bytes_out.append(b)

# 6. Format cleanly as ca65 source lines
formatted = [f"${x:02X}" for x in bytes_out]
for k in range(0, len(formatted), 16):
    print("\t.byte " + ", ".join(formatted[k:k+16]))
