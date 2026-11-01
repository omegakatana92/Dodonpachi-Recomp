"""Generate the project's own AppImage icon (no game art). Run from the repository root."""
import struct
import zlib
from pathlib import Path

W = H = 256
pixels = bytearray()
for y in range(H):
    for x in range(W):
        dx, dy = x - 128, y - 128
        r = (dx * dx + dy * dy) ** 0.5
        if 68 <= r <= 82:
            color = (210, 59, 46)
        elif r < 24:
            color = (242, 209, 107)
        elif r < 96:
            color = (18, 19, 32)
        else:
            color = (11, 11, 18)
        pixels += bytes(color)

raw = bytearray()
for y in range(H):
    raw.append(0)
    raw += pixels[y * W * 3:(y + 1) * W * 3]

def chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

png = (b"\x89PNG\r\n\x1a\n" +
       chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) +
       chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
       chunk(b"IEND", b""))
Path("packaging/dodonpachi.png").write_bytes(png)
print(f"wrote packaging/dodonpachi.png ({len(png)} bytes)")
