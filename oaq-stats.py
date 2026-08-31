import os
import struct

def check_oaq_size(filename):
    file_size = os.path.getsize(filename)
    num_integers = file_size // 4
    
    total_bytes = 0
    with open(filename, "rb") as f:
        for _ in range(num_integers):
            raw_bytes = f.read(4)
            if not raw_bytes: break
            val = struct.unpack("<I", raw_bytes)[0]
            
            # Apply your exact OAQ layout boundaries
            if val <= 127:
                total_bytes += 1  # OAQ-7
            elif val <= 30719:
                total_bytes += 2  # OAQ-15 (14.9-bit range)
            elif val <= 65535:
                total_bytes += 3  # OAQ-16 bit Escape Path
            else:
                total_bytes += 5  # OAQ-32 bit Escape Path

    reduction = (1 - (total_bytes / file_size)) * 100
    print(f"Dataset: {filename}")
    print(f"  -> Raw Data Size: {file_size // 1024} KB")
    print(f"  -> OAQ Wire Size: {total_bytes // 1024} KB")
    print(f"  -> Space Savings: {reduction:.2f}%\n")

check_oaq_size("uniform.bin")
check_oaq_size("clustered.bin")
check_oaq_size("zipfian.bin")
