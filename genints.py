import random
import struct
import math

NUM_INTEGERS = 1000000

# -----------------------------------------------------------------------------
# 0. FULL WINDOW (16-bit Window Simulation)
# -----------------------------------------------------------------------------
print("Generating u16-all.bin...")
with open("u16.bin", "wb") as f:
    for i in range(NUM_INTEGERS):
        val = i % 0xffff;
        f.write(struct.pack("<I", val))

# -----------------------------------------------------------------------------
# 1. UNIFORM DISTRIBUTION (16-bit Window Simulation)
# -----------------------------------------------------------------------------
print("Generating uniform.bin...")
with open("uniform.bin", "wb") as f:
    for _ in range(NUM_INTEGERS):
        val = random.randint(0, 65535)
#        val = random.randint(0, 0xffffffff)
        f.write(struct.pack("<I", val))

# -----------------------------------------------------------------------------
# 2. CLUSTERED DISTRIBUTION (Models sparse array structures & deltas)
# -----------------------------------------------------------------------------
print("Generating clustered.bin...")
with open("clustered.bin", "wb") as f:
    current_val = 10
    count = 0
    while count < NUM_INTEGERS:
        cluster_size = random.randint(10, 500)
        # Prevent runaway overflow past 32-bit ceiling
#        if current_val > 4000000000:
        if current_val > 65535-30:
            current_val = 10
            
        for _ in range(cluster_size):
            current_val += random.choice([1, 2, 3, 4, 5])
            f.write(struct.pack("<I", current_val))
            count += 1
            if count >= NUM_INTEGERS: 
                break
                
        current_val += random.randint(1000, 10000)

# -----------------------------------------------------------------------------
# 3. ZIPFIAN / POWER-LAW SIMULATION (Models natural assets/dictionary IDs)
# -----------------------------------------------------------------------------
print("Generating zipfian.bin...")
# Bypasses numpy by using a mathematical bounded log transform approximation
with open("zipfian.bin", "wb") as f:
    for _ in range(NUM_INTEGERS):
        u = random.random()
        # Generates a heavy concentration near 0-127 with a long, sparse tail
        val = int(math.pow(1.0 - u, -1.0 / 0.5)) - 1
        if val < 0: val = 0

#        if val > 1000000: val = 1000000  # Clamp upper limit
        if val > 65535: val = 65535

        f.write(struct.pack("<I", val))

print("Done! Three raw binary source data files created with pure Python.")
