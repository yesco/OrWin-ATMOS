import os
import struct

def verify_dataset(filename):
    file_size = os.path.getsize(filename)
    num_ints = file_size // 4
    
    # Baseline comparison metrics
    leb128_bytes = 0
    oaq_bytes = 0
    sqlite_bytes = 0
    
    with open(filename, "rb") as f:
        for _ in range(num_ints):
            raw = f.read(4)
            if not raw: break
            val = struct.unpack("<I", raw)[0]
            
            # --- LEB128 Baseline Math ---
            if   val <= 0x7F:      leb128_bytes += 1
            elif val <= 0x3FFF:    leb128_bytes += 2
            elif val <= 0x1FFFFF:  leb128_bytes += 3
            elif val <= 0xFFFFFFF: leb128_bytes += 4
            else:                  leb128_bytes += 5  # Spills over 28-bit to hit full 32-bit uint32
            
            # --- SQLite Baseline Math ---
            if   val <= 240:       sqlite_bytes += 1
            elif val <= 2287:      sqlite_bytes += 2
            elif val <= 67823:     sqlite_bytes += 3
            elif val <= 16777215:  sqlite_bytes += 4
            else:                  sqlite_bytes += 5  # Spills over 24-bit to hit full 32-bit uint32
            
            # --- Your OAQ Layout ---
            if   val <= 127:       oaq_bytes += 1
            elif val <= 30719:     oaq_bytes += 2
            elif val <= 65535-256: oaq_bytes += 3
            elif val <= 65535:     oaq_bytes += 2
            elif val <= 0xffffff:  oaq_bytes += 4
            else:                  oaq_bytes += 5

    print(f"");
    print(f"--- Verification Report: {filename} ---")
    print(f"Total Integers Profiled: {num_ints:,}")
    print(f"    EB128      {leb128_bytes / 1024:.1f} KB")
    print(f"    SQLite     {sqlite_bytes / 1024:.1f} KB")
    print(f"    OAQ        {oaq_bytes / 1024:.1f} KB")
    
verify_dataset("uniform.bin")
verify_dataset("clustered.bin")
verify_dataset("zipfian.bin")

#verify_dataset("wiki_sorted.bin")
