#!/usr/bin/env python3
# @author: Ahmad Odeh
# @date: 7th Nov, 2026
#
# build.py: builds Photon Mono (fonts/PhotonMono-Regular.ttf) from the
# pixel drawings in tools/font/glyphs.txt.
#
#   python3 tools/font/build.py [--style smooth|square] [--out FILE]
#
# Every pixel becomes a vector square (128 font units), so the font stays
# sharp at any size and on Retina screens. The "smooth" style adds half-pixel
# triangles where two pixels touch only at a corner, which turns staircases
# into diagonal lines; "square" keeps pure pixels.
#
# Built here rather than drawn: accented Latin-1 letters (from Unicode's own
# decompositions: base letter + accent), box drawing (from the characters'
# Unicode names: light / heavy / double / dashed / rounded / diagonal) and
# the block and shade characters.
#
# Needs: pip install fonttools skia-pathops   (see requirements.txt)

import argparse
import os
import re
import sys
import unicodedata

try:
    from fontTools.fontBuilder import FontBuilder
    from fontTools.pens.ttGlyphPen import TTGlyphPen
    from fontTools.ttLib.removeOverlaps import removeOverlaps
    from fontTools.ttLib.tables.O_S_2f_2 import Panose
except ImportError:
    sys.exit("build.py needs fontTools and skia-pathops:  pip install -r tools/font/requirements.txt")

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

FAMILY = "Photon Mono"
VERSION = "1.000"
COPYRIGHT = "Copyright (c) 2026 Ahmad Odeh (https://github.com/ahmadodeh9000/Photon)"

# --- Grid ---------------------------------------------------------------
PX = 128              # font units per pixel
ROWS = 16             # rows 0..15, top to bottom
BASELINE_ROW = 12     # the last row above the baseline
CELL = 10             # advance width in pixels (8-pixel letters + 1 each side)
UPM = ROWS * PX       # 2048: one em is exactly the 16 rows
ASCENT = (BASELINE_ROW + 1) * PX          # 1664
DESCENT = (ROWS - BASELINE_ROW - 1) * PX  # 384
X_HEIGHT = 8 * PX     # rows 5..12
CAP_HEIGHT = 10 * PX  # rows 3..12


# --- Reading glyphs.txt -------------------------------------------------
def read_glyphs(path):
    """{codepoint: set of (col, row)} with cols already in cell coordinates."""
    glyphs = {}
    lines = open(path, encoding="utf-8").read().splitlines()
    i = 0
    while i < len(lines):
        m = re.match(r"@\s*U\+([0-9A-Fa-f]{4,6})", lines[i])
        if not m:
            i += 1
            continue
        cp = int(m.group(1), 16)
        rows = lines[i + 1:i + 1 + ROWS]
        if len(rows) != ROWS:
            sys.exit(f"glyphs.txt: U+{cp:04X} needs {ROWS} rows")
        width = len(rows[0])
        offset = (CELL - width) // 2          # 8-wide letters sit at cols 1..8
        pixels = set()
        for r, row in enumerate(rows):
            if len(row) != width or set(row) - {".", "#"}:
                sys.exit(f"glyphs.txt: U+{cp:04X} row {r}: {row!r} (all rows {width} wide, only . and #)")
            pixels |= {(c + offset, r) for c, ch in enumerate(row) if ch == "#"}
        glyphs[cp] = pixels
        i += 1 + ROWS
    return glyphs


# --- Accented letters ---------------------------------------------------
MARKS = {0x0300, 0x0301, 0x0302, 0x0303, 0x0308, 0x030A, 0x0327}


def compose(glyphs):
    """Latin-1 letters with accents: base letter + the accent drawn in glyphs.txt."""
    made = {}
    for cp in range(0xC0, 0x100):
        parts = unicodedata.decomposition(chr(cp)).split()
        if len(parts) != 2 or parts[0].startswith("<"):
            continue
        base, mark = int(parts[0], 16), int(parts[1], 16)
        if mark not in MARKS or mark not in glyphs:
            continue
        if base == ord("i"):
            base = 0x0131                       # dotless i under the accent
        if base not in glyphs:
            continue
        accent = glyphs[mark]
        if mark != 0x0327:                      # accents above the letter (cedilla stays below)
            bottom = max(r for _, r in accent)
            if chr(base).isupper():             # capitals: at the very top, the ring touching
                target = 2 if mark == 0x030A else 1
            else:                               # lowercase: one empty row above the letter
                target = min(r for _, r in glyphs[base]) - 2
            accent = {(c, r + target - bottom) for c, r in accent if 0 <= r + target - bottom < ROWS}
        made[cp] = glyphs[base] | accent
    return made


# --- Box drawing (U+2500..U+257F) ---------------------------------------
# A line's thickness across the cell (pixels): light = the letters' stems
V_SPAN = {1: (4, 5), 2: (3, 6)}            # vertical line, columns
H_SPAN = {1: (8, 8), 2: (7, 9)}            # horizontal line, rows
V_DOUBLE = ((2, 3), (6, 7))                # double vertical: two lines, columns
H_DOUBLE = ((6, 6), (10, 10))              # double horizontal: two lines, rows
WEIGHTS = {"LIGHT": 1, "SINGLE": 1, "HEAVY": 2, "DOUBLE": 3}
DIRECTIONS = {"LEFT": ["left"], "RIGHT": ["right"], "UP": ["up"], "DOWN": ["down"],
              "HORIZONTAL": ["left", "right"], "VERTICAL": ["up", "down"]}


def box_arms(name):
    """'BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE' -> {'down': 1, 'right': 3}"""
    arms, pending, current = {}, [], None
    for word in name.replace("BOX DRAWINGS", "").split():
        if word in WEIGHTS:
            if pending:                         # "DOWN SINGLE": directions, then their weight
                for d in pending:
                    arms[d] = WEIGHTS[word]
                pending = []
            else:                               # "LIGHT DOWN": weight, then directions
                current = WEIGHTS[word]
        elif word in DIRECTIONS:
            for d in DIRECTIONS[word]:
                if current is not None:
                    arms[d] = current
                else:
                    pending.append(d)
    return arms


def rect(pixels, c0, c1, r0, r1):
    pixels |= {(c, r) for c in range(c0, c1 + 1) for r in range(r0, r1 + 1)}


def box_glyph(name):
    """Pixels (and a dash count) for one box-drawing character, or None."""
    if "DIAGONAL" in name:
        pixels = set()
        for r in range(ROWS):
            c = r * CELL // ROWS
            if "UPPER RIGHT TO LOWER LEFT" in name or "CROSS" in name:
                rect(pixels, CELL - 2 - c, CELL - 1 - c, r, r)
            if "UPPER LEFT TO LOWER RIGHT" in name or "CROSS" in name:
                rect(pixels, c, c + 1, r, r)
        return pixels
    arms = box_arms(name.replace(" ARC", ""))
    if not arms:
        return None
    pixels = set()
    vw = max([arms.get("up", 0), arms.get("down", 0)])
    hw = max([arms.get("left", 0), arms.get("right", 0)])

    # Where the perpendicular lines are, to join up with them
    vx = V_DOUBLE if vw == 3 else (V_SPAN.get(vw, (4, 5)),) * 2
    hy = H_DOUBLE if hw == 3 else (H_SPAN.get(hw, (8, 8)),) * 2

    for side, weight in arms.items():
        horizontal = side in ("left", "right")
        if weight == 3:
            # Two lines; each runs to the outer line of the crossing pair,
            # unless an arm on its own side is there (then to the inner one)
            for k, span in enumerate(H_DOUBLE if horizontal else V_DOUBLE):
                if horizontal:
                    beside = "up" if k == 0 else "down"
                    if beside in arms:
                        stop = vx[1][0] if side == "right" else vx[0][1]
                    else:
                        stop = vx[0][0] if side == "right" else vx[1][1]
                    c0, c1 = (stop, CELL - 1) if side == "right" else (0, stop)
                    rect(pixels, c0, c1, span[0], span[1])
                else:
                    beside = "left" if k == 0 else "right"
                    if beside in arms:
                        stop = hy[1][0] if side == "down" else hy[0][1]
                    else:
                        stop = hy[0][0] if side == "down" else hy[1][1]
                    r0, r1 = (stop, ROWS - 1) if side == "down" else (0, stop)
                    rect(pixels, span[0], span[1], r0, r1)
        else:
            if horizontal:
                r0, r1 = H_SPAN[weight]
                c0, c1 = (vx[0][0], CELL - 1) if side == "right" else (0, vx[1][1])
                if vw == 0:
                    c0, c1 = (4, CELL - 1) if side == "right" else (0, 5)
                rect(pixels, c0, c1, r0, r1)
            else:
                c0, c1 = V_SPAN[weight]
                r0, r1 = (hy[0][0], ROWS - 1) if side == "down" else (0, hy[1][1])
                if hw == 0:
                    r0, r1 = (8, ROWS - 1) if side == "down" else (0, 8)
                rect(pixels, c0, c1, r0, r1)

    # Dashed lines: cut gaps
    dashes = {"DOUBLE DASH": 2, "TRIPLE DASH": 3, "QUADRUPLE DASH": 4}
    for words, n in dashes.items():
        if words in name:
            along = CELL if "HORIZONTAL" in name else ROWS
            seg = along / n
            keep = set()
            for (c, r) in pixels:
                pos = c if "HORIZONTAL" in name else r
                if (pos % seg) < seg - (2 if along == ROWS else 1):
                    keep.add((c, r))
            pixels = keep
    # Rounded corners: drop the corner pixel of the bend
    if "ARC" in name:
        corner = {"DOWN AND RIGHT": (4, 8), "DOWN AND LEFT": (5, 8), "UP AND LEFT": (5, 8), "UP AND RIGHT": (4, 8)}
        for words, (c, r) in corner.items():
            if words in name:
                pixels.discard((c, r))
    return pixels


# --- Blocks and shades (U+2580..U+259F) ---------------------------------
def block_glyph(cp):
    full = {(c, r) for c in range(CELL) for r in range(ROWS)}
    eighth_rows = lambda n: {(c, r) for (c, r) in full if r >= ROWS - n * 2}       # lower n/8
    eighth_cols = lambda n: {(c, r) for (c, r) in full if c < round(n * CELL / 8)}   # left n/8
    half = CELL // 2
    q = {"ul": {(c, r) for (c, r) in full if c < half and r < 8},
         "ur": {(c, r) for (c, r) in full if c >= half and r < 8},
         "ll": {(c, r) for (c, r) in full if c < half and r >= 8},
         "lr": {(c, r) for (c, r) in full if c >= half and r >= 8}}
    table = {
        0x2580: {(c, r) for (c, r) in full if r < 8},                 # upper half
        0x2588: full,
        0x2590: {(c, r) for (c, r) in full if c >= half},             # right half
        0x2591: {(c, r) for (c, r) in full if c % 2 == 0 and r % 2 == 0},             # light shade
        0x2592: {(c, r) for (c, r) in full if (c + r) % 2 == 0},                      # medium shade
        0x2593: full - {(c, r) for (c, r) in full if c % 2 == 1 and r % 2 == 1},      # dark shade
        0x2594: {(c, r) for (c, r) in full if r < 2},                 # upper 1/8
        0x2595: {(c, r) for (c, r) in full if c >= CELL - 1},         # right 1/8
        0x2596: q["ll"], 0x2597: q["lr"], 0x2598: q["ul"], 0x259D: q["ur"],
        0x2599: q["ul"] | q["ll"] | q["lr"], 0x259A: q["ul"] | q["lr"],
        0x259B: q["ul"] | q["ur"] | q["ll"], 0x259C: q["ul"] | q["ur"] | q["lr"],
        0x259E: q["ur"] | q["ll"], 0x259F: q["ur"] | q["ll"] | q["lr"],
    }
    for n in range(1, 8):
        table[0x2580 + n] = eighth_rows(n)            # lower 1/8 .. 7/8
        table[0x2590 - n] = eighth_cols(n)            # left 1/8 .. 7/8 (258F..2589)
    return table.get(cp)


# --- Outlines -----------------------------------------------------------
def to_units(c, r):
    """Pixel corner (col, row-from-top) -> font units (x, y up)."""
    return c * PX, (BASELINE_ROW + 1 - r) * PX


def bridges(pixels):
    """Half-pixel triangles where two pixels touch only diagonally (smooth style)."""
    tris = []
    for (c, r) in pixels:
        if (c + 1, r + 1) in pixels and (c + 1, r) not in pixels and (c, r + 1) not in pixels:
            tris.append([(c + 1, r), (c + 2, r + 1), (c + 1, r + 1)])       # right of a, above b
            tris.append([(c, r + 1), (c + 1, r + 1), (c + 1, r + 2)])       # below a, left of b
        if (c - 1, r + 1) in pixels and (c - 1, r) not in pixels and (c, r + 1) not in pixels:
            tris.append([(c, r), (c, r + 1), (c - 1, r + 1)])               # left of a, above b
            tris.append([(c, r + 1), (c + 1, r + 1), (c, r + 2)])           # below a, right of b
    return tris


def draw(pixels, smooth):
    pen = TTGlyphPen(None)
    # Rows of pixels as rectangles (fewer, larger contours); overlaps removed later
    for r in range(ROWS):
        cols = sorted(c for (c, rr) in pixels if rr == r)
        start = None
        for i, c in enumerate(cols):
            if start is None:
                start = c
            if i + 1 == len(cols) or cols[i + 1] != c + 1:
                x0, y1 = to_units(start, r)
                x1, y0 = to_units(c + 1, r + 1)
                pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
                start = None
    if smooth:
        for tri in bridges(pixels):
            pts = [to_units(c, r) for c, r in tri]
            area = sum(pts[i][0] * pts[(i + 1) % 3][1] - pts[(i + 1) % 3][0] * pts[i][1] for i in range(3))
            if area > 0:                         # TrueType outer contours run clockwise
                pts.reverse()
            pen.moveTo(pts[0]); pen.lineTo(pts[1]); pen.lineTo(pts[2]); pen.closePath()
    return pen.glyph()


def notdef():
    pen = TTGlyphPen(None)
    x0, x1, y0, y1 = PX, (CELL - 1) * PX, 0, CAP_HEIGHT
    t = PX
    pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
    pen.moveTo((x0 + t, y0 + t)); pen.lineTo((x1 - t, y0 + t)); pen.lineTo((x1 - t, y1 - t)); pen.lineTo((x0 + t, y1 - t)); pen.closePath()
    return pen.glyph()


# --- The font -----------------------------------------------------------
def build(style, out):
    glyphs = read_glyphs(os.path.join(HERE, "glyphs.txt"))
    glyphs.update({cp: px for cp, px in compose(glyphs).items() if cp not in glyphs})
    for cp in range(0x2500, 0x2580):
        if cp not in glyphs:
            px = box_glyph(unicodedata.name(chr(cp), ""))
            if px is not None:
                glyphs[cp] = px
    for cp in range(0x2580, 0x25A0):
        px = block_glyph(cp)
        if px is not None and cp not in glyphs:
            glyphs[cp] = px
    glyphs[0x00A0] = set()                                   # no-break space

    # Combining accents are drawn only to build letters: no cmap entry
    mapped = {cp: px for cp, px in glyphs.items() if cp not in MARKS}
    smooth = style == "smooth"
    keep_square = lambda cp: 0x2500 <= cp < 0x25A0          # box/blocks stay crisp

    order = [".notdef"] + [f"uni{cp:04X}" for cp in sorted(mapped)]
    outlines = {".notdef": notdef()}
    for cp, px in mapped.items():
        outlines[f"uni{cp:04X}"] = draw(px, smooth and not keep_square(cp))

    fb = FontBuilder(UPM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap({cp: f"uni{cp:04X}" for cp in mapped})
    fb.setupGlyf(outlines)
    metrics = {}
    for name in order:
        g = outlines[name]
        g.recalcBounds(fb.font["glyf"])
        metrics[name] = (CELL * PX, getattr(g, "xMin", 0))
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=-DESCENT, lineGap=0)
    fb.setupNameTable({
        "copyright": COPYRIGHT,
        "familyName": FAMILY,
        "styleName": "Regular",
        "uniqueFontIdentifier": f"{FAMILY} Regular {VERSION}",
        "fullName": f"{FAMILY} Regular",
        "version": f"Version {VERSION}",
        "psName": "PhotonMono-Regular",
        "manufacturer": "Ahmad Odeh",
        "designer": "Ahmad Odeh",
        "description": "A retro monospace font for coding, drawn on a 10x16 pixel grid.",
        "licenseDescription": "This Font Software is licensed under the SIL Open Font License, Version 1.1.",
        "licenseInfoURL": "https://openfontlicense.org",
    })
    panose = Panose()
    for field in ("bFamilyType", "bSerifStyle", "bWeight", "bProportion", "bContrast", "bStrokeVariation",
                  "bArmStyle", "bLetterForm", "bMidline", "bXHeight"):
        setattr(panose, field, 0)
    panose.bFamilyType, panose.bProportion = 2, 9      # Latin text, monospaced
    fb.setupOS2(
        sTypoAscender=ASCENT, sTypoDescender=-DESCENT, sTypoLineGap=0,
        usWinAscent=ASCENT, usWinDescent=DESCENT,
        sxHeight=X_HEIGHT, sCapHeight=CAP_HEIGHT, xAvgCharWidth=CELL * PX,
        achVendID="PHTN", fsType=0,
        fsSelection=(1 << 6) | (1 << 7),          # REGULAR, USE_TYPO_METRICS
        version=4, panose=panose,
        ulUnicodeRange1=(1 << 0) | (1 << 1),        # Basic Latin, Latin-1
    )
    fb.setupPost(isFixedPitch=1, underlinePosition=-2 * PX, underlineThickness=PX)
    # Fixed dates (1 Jan 2026) so the same glyphs.txt always gives the same file
    fb.setupHead(unitsPerEm=UPM, fontRevision=float(VERSION), created=3849984000, modified=3849984000)
    fb.font.recalcTimestamp = False
    removeOverlaps(fb.font)                        # one clean outline per glyph
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fb.save(out)
    print(f"{out}: {len(mapped)} characters, style {style}")


def main():
    parser = argparse.ArgumentParser(description="Build Photon Mono from tools/font/glyphs.txt")
    parser.add_argument("--style", choices=["smooth", "square"], default="smooth")
    parser.add_argument("--out", default=os.path.join(REPO, "fonts", "PhotonMono-Regular.ttf"))
    args = parser.parse_args()
    build(args.style, args.out)


if __name__ == "__main__":
    main()
