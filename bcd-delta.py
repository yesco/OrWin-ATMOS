// TODO: This is gemini generated code,
//   it couldn't implement the other opitmized version,
//   and it keeps failing on this one1 Possibly wrong
//   code as well as data genereated by bcd-*.py
	 
import math

end= 256+2
# 1. Generate the absolute rounded 4-digit BCD targets
ideal_vals = [round(1000 * (10 ** (x / 256))) for x in range(end)]

# 2. Extract first-layer step sizes
deltas = [ideal_vals[x] - ideal_vals[x-1] for x in range(1, end)]

# 3. Model the 6502 loop logic (MSB-first bit-pair streaming)
current_delta = deltas[0]  # First step size defaults to 9
encoded_bits = []

run= 1000
i= 0
for d in deltas[1:]:
    change = d - current_delta
    run= run + d
    print(i, ideal_vals[i], deltas[i], d, change)
    i= i + 1
    if change == 0:
        encoded_bits.extend([0, 0]) # %00 = Unchanged
    elif change == 1:
        encoded_bits.extend([0, 1]) # %01 = Increment +1
        current_delta += 1
    elif change == -1:
        encoded_bits.extend([1, 0]) # %10 = Decrement -1
        current_delta -= 1
    elif change == 2:
        encoded_bits.extend([1, 1]) # %11 = Increment +2
        current_delta += 2

print("RUN", run)

# Pad out trailing bits to finalize last byte alignment
while len(encoded_bits) < 512:
    encoded_bits.append(0)

# 4. Pack sequential bit pairs into 64 clean bytes
bytes_out = []
for i in range(0, len(encoded_bits), 8):
    byte_val = 0
    for b in range(8):
        byte_val = (byte_val << 1) | encoded_bits[i+b]
    bytes_out.append(byte_val)

# 5. Format cleanly for ca65 segment output
hex_lines = [f"${b:02X}" for b in bytes_out]
for chunk in [hex_lines[i:i+16] for i in range(0, 64, 16)]:
    print("\t.byte " + ", ".join(chunk))

    
# First Delta:
#
# 9 9 9 10 9 9 10 10 9 10 10 10 10 10 10 11
# 10 11 10 11 11 11 11 11 11 11 12 11 12 12 12 12
# 12 12 12 12 13 12 13 13 13 13 13 14 13 13 14 14
# 14 14 14 14 15 14 15 15 15 15 15 15 16 16 15 16
# 16 17 16 16 17 17 17 17 17 18 17 18 18 18 18 19
# 18 19 19 19 19 19 20 20 20 20 20 21 20 21 21 21
# 22 21 22 22 22 23 22 23 23 24 23 24 24 24 24 24
# 25 25 25 26 25 26 26 27 26 27 27 28 27 28 28 28
# 29 29 29 29 30 30 30 30 31 31 31 32 32 32 32 33
# 33 33 34 34 34 34 35 35 36 35 37 36 37 37 37 38
# 38 39 38 39 40 40 40 41 41 41 42 42 42 43 43 44
# 44 44 45 45 46 46 46 47 47 48 48 49 49 49 50 50
# 51 51 52 52 53 53 54 54 55 55 55 56 57 57 58 58
# 58 60 59 61 61 61 62 62 63 64 64 65 65 66 67 67
# 68 68 69 70 70 71 71 72 73 74 74 75 75 76 77 78
# 78 79 79 81 81 82 82 84 84 85 85 87 87 88 88 0

# First Delta:
#
# 0 0 1 -1 0 1 0 -1 1 0 0 0 0 0 1 -1
# 1 -1 1 0 0 0 0 0 0 1 -1 1 0 0 0 0
# 0 0 0 1 -1 1 0 0 0 0 1 -1 0 1 0 0
# 0 0 0 1 -1 1 0 0 0 0 0 1 0 -1 1 0
# 1 -1 0 1 0 0 0 0 1 -1 1 0 0 0 1 -1
# 1 0 0 0 0 1 0 0 0 0 1 -1 1 0 0 1
# -1 1 0 0 1 -1 1 0 1 -1 1 0 0 0 0 1
# 0 0 1 -1 1 0 1 -1 1 0 1 -1 1 0 0 1
# 0 0 0 1 0 0 0 1 0 0 1 0 0 0 1 0
# 0 1 0 0 0 1 0 1 -1 2 -1 1 0 0 1 0
# 1 -1 1 1 0 0 1 0 0 1 0 0 1 0 1 0
# 0 1 0 1 0 0 1 0 1 0 1 0 0 1 0 1
# 0 1 0 1 0 1 0 1 0 0 1 1 0 1 0 0
# 2 -1 2 0 0 1 0 1 1 0 1 0 1 1 0 1
# 0 1 1 0 1 0 1 1 1 0 1 0 1 1 1 0
# 1 0 2 0 1 0 2 0 1 0 2 0 1 0 0 0
 
