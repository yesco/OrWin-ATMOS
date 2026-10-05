import math

# 1. Exact 4-digit rounded BCD targets from your fil.out column
targets = [
    1000, 1009, 1018, 1027, 1037, 1046, 1056, 1066, 1075, 1084, 1094, 1104, 1114, 1124, 1134, 1144,
    1155, 1165, 1176, 1186, 1197, 1208, 1219, 1230, 1241, 1252, 1263, 1275, 1286, 1298, 1310, 1322,
    1334, 1346, 1358, 1370, 1382, 1395, 1407, 1420, 1433, 1446, 1459, 1472, 1486, 1499, 1512, 1526,
    1540, 1554, 1568, 1582, 1596, 1610, 1625, 1639, 1654, 1669, 1684, 1699, 1714, 1729, 1744, 1759,
    1775, 1791, 1807, 1823, 1839, 1855, 1872, 1888, 1905, 1922, 1939, 1956, 1973, 1990, 2008, 2025,
    2043, 2061, 2079, 2097, 2115, 2133, 2152, 2171, 2190, 2209, 2228, 2247, 2267, 2286, 2306, 2326,
    2346, 2366, 2386, 2407, 2427, 2448, 2469, 2490, 2511, 2532, 2553, 2575, 2596, 2618, 2640, 2662,
    2684, 2706, 2728, 2751, 2773, 2796, 2819, 2842, 2865, 2888, 2912, 2935, 2959, 2983, 3007, 3031,
    3056, 3080, 3105, 3130, 3155, 3180, 3206, 3231, 3257, 3283, 3309, 3335, 3361, 3388, 3414, 3441,
    3468, 3495, 3522, 3550, 3577, 3605, 3633, 3661, 3689, 3717, 3746, 3775, 3804, 3833, 3862, 3892,
    3921, 3951, 3981, 4011, 4042, 4072, 4103, 4134, 4165, 4196, 4228, 4259, 4291, 4323, 4355, 4387,
    4419, 4452, 4485, 4518, 4551, 4584, 4618, 4652, 4686, 4720, 4754, 4789, 4824, 4859, 4894, 4929,
    4965, 5001, 5037, 5073, 5110, 5146, 5183, 5220, 5257, 5295, 5333, 5371, 5409, 5447, 5486, 5525,
    5564, 5604, 5644, 5684, 5724, 5764, 5805, 5846, 5887, 5928, 5970, 6012, 6054, 6096, 6138, 6181,
    6224, 6267, 6310, 6354, 6398, 6442, 6486, 6531, 6576, 6621, 6666, 6712, 6758, 6804, 6851, 6898,
    6945, 6992, 7040, 7087, 7135, 7183, 7232, 7281, 7330, 7380, 7430, 7480, 7530, 7581, 7632, 7683,
    7735, 7787, 7839, 7891, 7944, 7997, 8050, 8104, 8158, 8212, 8267, 8322, 8377, 8432, 8488, 8544,
    8601, 8658, 8715, 8772, 8830, 8888, 8946, 9005, 9064, 9123, 9183, 9243, 9303, 9363, 9424, 9485,
    9547, 9609, 9671, 9733, 9796, 9859, 9923, 9987
]

# 2. Emulate 6502 BCD carries to verify exact runtime delta changes
def to_bcd(val):
    return int(f"{val:02d}", 16) if 0 <= val <= 99 else 0

def bcd_add(a_bcd, b_bcd):
    # Simulated 6502 SED Mode addition logic
    val = int(f"{a_bcd:02x}") + int(f"{b_bcd:02x}")
    hundreds, remainder = divmod(val, 100)
    return int(f"{remainder:02d}", 16), hundreds

# Step-by-step execution simulation matching the assembly loop
current_bcd = int("1000", 16)
current_delta = 9
encoded_tokens = []

for idx in range(1, len(targets)):
    target_val = targets[idx]
    
    # Calculate the exact target first-layer delta
    target_delta_dec = target_val - int(f"{(current_bcd >> 8):02x}{(current_bcd & 0xFF):02x}")
    
    # Trace the required second-layer adjustment token
    change = target_delta_dec - current_delta
    encoded_tokens.append(change)
    
    # Apply change to current delta tracking state
    current_delta += change
    
    # Emulate the assembly "Accumulate" step under SED constraints
    low_byte = current_bcd & 0xFF
    high_byte = current_bcd >> 8
    
    new_low, carry = bcd_add(low_byte, to_bcd(current_delta))
    new_high, _ = bcd_add(high_byte, to_bcd(carry))
    current_bcd = (new_high << 8) | new_low

# 3. Map adjustments to token bit layouts (%00=0, %01=+1, %10=-1, %11=+2)
bit_stream = []
for c in encoded_tokens:
    if c == 0: bit_stream.append(0)     # %00
    elif c == 1: bit_stream.append(1)   # %01
    elif c == -1: bit_stream.append(2)  # %10
    elif c == 2: bit_stream.append(3)   # %11

while len(bit_stream) % 4 != 0:
    bit_stream.append(0)

# 4. Serialize 4 token pairs per byte using big-endian shift rules
bytes_out = []
for i in range(0, len(bit_stream), 4):
    b = (bit_stream[i] << 6) | (bit_stream[i+1] << 4) | (bit_stream[i+2] << 2) | bit_stream[i+3]
    bytes_out.append(b)

# 5. Format cleanly into 4 source output blocks
formatted = [f"${x:02X}" for x in bytes_out]
for k in range(0, len(formatted), 16):
    print(".byte " + ", ".join(formatted[k:k+16]))
