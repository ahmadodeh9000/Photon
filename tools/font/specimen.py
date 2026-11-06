#!/usr/bin/env python3
# @author: Ahmad Odeh
# @date: 7th Nov, 2026
#
# specimen.py: renders a specimen PNG of a Photon Mono build: the
# alphabet, digits, symbols, Latin-1, box drawing, and Lunar / C / Python
# code at 11, 13 and 16 pt, at 1x and 2x (pt = px at 1x, as on macOS).
#
#   python3 tools/font/specimen.py FONT.ttf OUT.png [--title TEXT]
#
# Needs Pillow (pip install pillow). Rendering goes through FreeType.

import argparse
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("specimen.py needs Pillow:  pip install pillow")

CHARSET = [
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "abcdefghijklmnopqrstuvwxyz",
    "0123456789  !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
    "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞß",
    "àáâãäåæçèéêëìíîïðñòóôõöøùúûüýþÿ",
    "¡¢£¥§©®°±²³µ¶·¹º«»¼½¾¿×÷  ‘’‚“”„–—…•‹›†‡€™",
    "←↑→↓↔↕↵⇐⇒  ■▲▼◄►●○  ░▒▓█▀▄▌▐▖▗▘▝▙▛▜▟",
]

CONFUSABLES = "0O o 1lI| ' ` rn m {}()[] ;:., == != <= >= ** && || -> =>"

BOXES = [
    "┌──┬──┐ ╔══╦══╗ ┏━━┳━━┓ ╭──╮",
    "│  │  │ ║  ║  ║ ┃  ┃  ┃ │  │",
    "├──┼──┤ ╠══╬══╣ ┣━━╋━━┫ ╰──╯",
    "└──┴──┘ ╚══╩══╝ ┗━━┻━━┛ ╱╲╳",
]

CODE = [
    "// Lunar",
    "struct Point {",
    "    init(x, y) { self.x = x; self.y = y; }",
    "}",
    "let m = { hp: 10, name: \"Rex\" };",
    "print \"hp={m.hp} 0O 1lI rn m\";",
    "",
    "/* C */",
    "int main(void) {",
    "    if (a != b && c <= 10 || d >= 2) { x[0] = *p; }",
    "    printf(\"%d\\n\", total(items, 2));",
    "}",
    "",
    "# Python",
    "def f(n: int) -> str:",
    "    return f\"{n ** 2} {'yes' if n == 1 else `no`}\"",
]

INK, PAPER = (0, 0, 0), (255, 255, 255)
TURBO_BG, TURBO_FG = (0, 0, 170), (255, 255, 85)
LABEL = ("DejaVu Sans", "Helvetica", "Arial")


def label_font(size):
    for path in ("/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/Supplemental/Arial.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            pass
    return ImageFont.load_default()


def line_height(font):
    ascent, descent = font.getmetrics()
    return ascent + descent


def render(font_path, out, title):
    blocks = []      # (kind, payload) in drawing order, laid out below

    def text_block(lines, px, colors=(INK, PAPER), caption=None):
        blocks.append(("text", lines, px, colors, caption))

    text_block(CHARSET, 16, caption="16 px (1x) - every character group")
    text_block(CHARSET, 32, caption="32 px (2x)")
    text_block([CONFUSABLES], 32, caption="look-alikes, 2x")
    text_block(BOXES, 32, caption="box drawing, 2x")
    for scale in (1, 2):
        for pt in (11, 13, 16):
            text_block(CODE, pt * scale, (TURBO_FG, TURBO_BG), caption=f"{pt} pt at {scale}x ({pt * scale} px)")

    width = 1500
    pad, gap = 24, 18
    cap_font = label_font(15)
    title_font = label_font(26)

    # Measure
    height = pad + 40
    fonts = {}
    for _, lines, px, _, caption in blocks:
        fonts[px] = fonts.get(px) or ImageFont.truetype(font_path, px)
        height += 22 + len(lines) * line_height(fonts[px]) + 2 * 8 + gap
    height += pad

    img = Image.new("RGB", (width, height), (232, 232, 232))
    draw = ImageDraw.Draw(img)
    y = pad
    draw.text((pad, y), title, fill=INK, font=title_font)
    y += 40
    for _, lines, px, (fg, bg), caption in blocks:
        font = fonts[px]
        draw.text((pad, y), caption, fill=(80, 80, 80), font=cap_font)
        y += 22
        box_h = len(lines) * line_height(font) + 16
        draw.rectangle([pad, y, width - pad, y + box_h], fill=bg)
        ty = y + 8
        for line in lines:
            draw.text((pad + 10, ty), line, fill=fg, font=font)
            ty += line_height(font)
        y += box_h + gap
    img.save(out)
    print(f"{out}: {width}x{height}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("font")
    parser.add_argument("out")
    parser.add_argument("--title", default="Photon Mono")
    args = parser.parse_args()
    render(args.font, args.out, args.title)


if __name__ == "__main__":
    main()
