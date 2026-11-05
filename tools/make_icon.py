#!/usr/bin/env python3
# @author: Ahmad Odeh
# @date: 5th Nov, 2026
#
# make_icon: draws Photon's icon, a 32x32 pixel-art 90s CRT monitor with a
# yellow gamma (the photon symbol) on a Turbo-blue screen, and writes it
# at every size the installers need:
#
#   icons/photon-<N>.png   N = 16 32 48 64 128 256 512
#   icons/photon.png       1024 px
#   icons/photon.icns      macOS app icon (only where `iconutil` exists)
#
# Only the Python 3 standard library is needed. Run it from the repo root
# after changing the drawing:   python3 tools/make_icon.py

import os
import shutil
import struct
import subprocess
import tempfile
import zlib

# One character per pixel. Edit the art here.
ART = [
    "................................",
    "................................",
    "..KKKKKKKKKKKKKKKKKKKKKKKKKKKK..",
    "..KWWWWWWWWWWWWWWWWWWWWWWWWWWSK.",
    "..KWBBBBBBBBBBBBBBBBBBBBBBBBBSK.",
    "..KWBKKKKKKKKKKKKKKKKKKKKKKBBSK.",
    "..KWBKlllbbbbbbbbbbbbbbbbbKBBSK.",
    "..KWBKlbbbbbbbbbbbbbbbbbbbKBBSK.",
    "..KWBKbbbbYYbbbbbYbbbbbbbbKBBSK.",
    "..KWBKbbbYbbYbbbbYbbbbbbbbKBBSK.",
    "..KWBKbbbbbbYbbbYbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbbYbbYbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbbYbYbbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbbbYYbbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbbbYbbbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbbYYbbbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbYbbYbbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbYbbYbbbbbbbbbbKBBSK.",
    "..KWBKbbbbbbbYYbbbbbYYYbbbKBBSK.",
    "..KWBKbbbbbbbbbbbbbbbbbbbbKBBSK.",
    "..KWBKKKKKKKKKKKKKKKKKKKKKKBBSK.",
    "..KWBBBBBBBBBBBBBBBBBBBBBBBBBSK.",
    "..KWBBBBBBBBBBBBBBBBBBBBBGGBBSK.",
    "..KWSSSSSSSSSSSSSSSSSSSSSSSSSSK.",
    "..KKKKKKKKKKKKKKKKKKKKKKKKKKKKK.",
    "...........KWBBBBBBBSK..........",
    "...........KWBBBBBBBSK..........",
    "........KKKKKKKKKKKKKKKKK.......",
    "........KWWWWWWWWWWWWWWSK.......",
    "........KBBBBBBBBBBBBBBSK.......",
    "........KKKKKKKKKKKKKKKKK.......",
    "................................",
]

COLORS = {
    ".": (0, 0, 0, 0),          # transparent
    "K": (0, 0, 0, 255),        # outline
    "W": (248, 244, 228, 255),  # beige highlight
    "B": (214, 205, 179, 255),  # beige plastic
    "S": (140, 132, 110, 255),  # beige shadow
    "b": (0, 0, 170, 255),      # Turbo C++ blue
    "l": (85, 85, 255, 255),    # glare on the glass
    "Y": (255, 255, 85, 255),   # yellow gamma + cursor
    "G": (85, 255, 85, 255),    # power LED
}

SIZES = [16, 32, 48, 64, 128, 256, 512, 1024]


def pixels():
    assert len(ART) == 32 and all(len(row) == 32 for row in ART), "ART must be 32x32"
    return [[COLORS[c] for c in row] for row in ART]


def scale(src, size):
    """Nearest neighbour up (crisp pixels), 2x2 average down (for 16 px)."""
    n = len(src)
    if size < n:
        f = n // size
        out = []
        for y in range(size):
            row = []
            for x in range(size):
                block = [src[y * f + j][x * f + i] for j in range(f) for i in range(f)]
                alpha = sum(p[3] for p in block)
                if alpha == 0:
                    row.append((0, 0, 0, 0))
                    continue
                rgb = [sum(p[k] * p[3] for p in block) // alpha for k in range(3)]
                row.append((*rgb, alpha // len(block)))
            out.append(row)
        return out
    return [[src[y * n // size][x * n // size] for x in range(size)] for y in range(size)]


def write_png(path, image):
    height, width = len(image), len(image[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in image)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def write_icns(icons_dir):
    if shutil.which("iconutil") is None:
        print("iconutil not found (not macOS): skipping photon.icns")
        return
    with tempfile.TemporaryDirectory() as tmp:
        iconset = os.path.join(tmp, "photon.iconset")
        os.mkdir(iconset)
        for size in (16, 32, 128, 256, 512):
            shutil.copy(os.path.join(icons_dir, f"photon-{size}.png"), os.path.join(iconset, f"icon_{size}x{size}.png"))
            double = os.path.join(icons_dir, "photon.png" if size == 512 else f"photon-{size * 2}.png")
            shutil.copy(double, os.path.join(iconset, f"icon_{size}x{size}@2x.png"))
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", os.path.join(icons_dir, "photon.icns")], check=True)


def main():
    icons_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "icons")
    art = pixels()
    for size in SIZES:
        name = "photon.png" if size == 1024 else f"photon-{size}.png"
        write_png(os.path.join(icons_dir, name), scale(art, size))
    write_icns(icons_dir)
    print("icons written to", os.path.normpath(icons_dir))


if __name__ == "__main__":
    main()
