#!/usr/bin/env python3
"""
Build bootable Microdisc MFM_DISK for SmallTable.

Track 0 layout (256-byte sectors):
  S1  ROM signature (from SEDORIC template)
  S2  Boot payload  (prefix + our code)
  S3  Fake SYSTEMDOS directory
  S4  free
  S5  SmallTable directory   <-- yours
  S6+ SmallTable data        <-- yours

Requires SEDORIC3.DSK in the same folder (MFM template).
"""
import pathlib

HERE = pathlib.Path(__file__).parent
TEMPLATE = HERE / "SEDORIC3.DSK"
OUT = HERE / "bootdisk_mfm.dsk"

def crc16_ccitt(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

def find_marks(img, limit=17):
    marks, idx = [], 256
    while len(marks) < limit:
        pos = img.find(b"\xa1\xa1\xa1\xfb", idx)
        if pos < 0:
            break
        marks.append(pos)
        idx = pos + 4
    return marks

PREFIX = bytes([
    0x00,0x00,0xFF,0x00,0xD0,0x9F,0xD0,0x9F,
    0x02,0xB9,0x01,0x00,0xFF,0x00,0x00,0xB9,
    0xE4,0xB9,0x00,0x00,0xE6,0x12,0x00,
])

def build_payload():
    c = bytearray()
    e = c.extend
    e([0x78, 0xD8, 0xA2, 0xFF, 0x9A])
    e([0xA9, 0x00, 0x8D, 0x6A, 0x02])
    e([0xA2, 0x00, 0xA9, 0x20])
    e([0x9D, 0x80, 0xBB, 0xE8, 0xE0, 0x28, 0xD0, 0xF8])
    for i, ch in enumerate(b"SMALLTABLE BOOT OK"):
        e([0xA9, ch, 0x8D, (0x80 + i) & 0xFF, 0xBB])
    e([0x38, 0xB0, 0xFE])
    return bytes(c)

def main():
    real = TEMPLATE.read_bytes()
    marks0 = find_marks(real, 3)
    sec1 = real[marks0[0]+4 : marks0[0]+4+256]
    sec3 = real[marks0[2]+4 : marks0[2]+4+256]
    payload = build_payload()
    sec2 = PREFIX + payload + bytes(256 - 23 - len(payload))

    img = bytearray(real)
    marks = find_marks(img, 17)

    def put(s, data):
        mark = marks[s - 1]
        doff, coff = mark + 4, mark + 4 + 256
        img[doff:doff+256] = data
        crc = crc16_ccitt(bytes(img[mark:mark+4+256]))
        img[coff] = (crc >> 8) & 0xFF
        img[coff+1] = crc & 0xFF

    put(1, sec1)
    put(2, sec2)
    put(3, sec3)
    for s in [4, 5, 6] + list(range(7, 18)):
        put(s, bytes(256))

    OUT.write_bytes(img)
    print(f"Wrote {OUT} ({len(img)} bytes)")
    print("S1=signature  S2=boot  S3=SYSTEMDOS  S4=free  S5=dir  S6+=data")

if __name__ == "__main__":
    main()
