#!/usr/bin/env python3
"""Draws the Aurora dashboard's art for the Xbox 360 build, in the style of
the app icon (tools/make_icon.py):

    consoles/xbox360/aurora/icon.png     64 x 64   (Aurora's game icon)
    consoles/xbox360/aurora/banner.png   420 x 96  (Aurora's banner)

The banner: the icon's well on the left, TETRISPACK spelled in glossy
blocks the colors of the pieces, a tagline, on deep blue lit at the top and
dark at the bottom like the game's Background Gradient. The sizes are the
ones Aurora Asset Editor asks for.

    python3 tools/make_aurora.py [out_dir]      (needs Pillow)
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_icon  # noqa: E402

SS = make_icon.SS
NAVY_TOP, NAVY_BOTTOM = (34, 52, 128), (4, 6, 22)

# A 5-row block font: '#' is a block.
FONT = {
    "T": ["#####", "..#..", "..#..", "..#..", "..#.."],
    "E": ["####", "#...", "###.", "#...", "####"],
    "R": ["###.", "#..#", "###.", "#.#.", "#..#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "S": [".###", "#...", ".##.", "...#", "###."],
    "P": ["###.", "#..#", "###.", "#...", "#..."],
    "A": [".##.", "#..#", "####", "#..#", "#..#"],
    "C": [".###", "#...", "#...", "#...", ".###"],
    "K": ["#..#", "#.#.", "##..", "#.#.", "#..#"],
}
# Each letter's color, the pieces' in turn.
LETTER_COLORS = ["R", "Y", "G", "C", "B", "M", "R", "Y", "G", "C"]


def backdrop(w, h, radius=0):
    """Deep blue, lit at the top and dark at the bottom, with a faint grid."""
    img = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = make_icon.lerp(NAVY_TOP, NAVY_BOTTOM, t ** 0.8)
        d.line([(0, y), (w - 1, y)], fill=c + (255,))
    # the light band at the top
    light = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ld = ImageDraw.Draw(light)
    for y in range(int(h * 0.35)):
        a = int(70 * (1 - y / (h * 0.35)) ** 2)
        ld.line([(0, y), (w - 1, y)], fill=(255, 255, 255, a))
    img = Image.alpha_composite(img, light)
    if radius:
        mask = Image.new("L", (w, h), 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)
        out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        out.paste(img, (0, 0), mask)
        img = out
    return img


def grid(img, cell, alpha=14):
    """A faint grid of cells, blended over the image (returned)."""
    w, h = img.size
    over = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    for x in range(0, w, cell):
        d.line([(x, 0), (x, h - 1)], fill=(255, 255, 255, alpha))
    for y in range(0, h, cell):
        d.line([(0, y), (w - 1, y)], fill=(255, 255, 255, alpha))
    return Image.alpha_composite(img, over)


def icon():
    """64 x 64: the icon's 48 px drawing on the blue backdrop."""
    img = backdrop(64, 64)
    art = make_icon.draw(48)
    img.alpha_composite(art, (8, 7))
    return img


def letter_block(d, x, y, c, color):
    """A block of a letter: solid, so the strokes read as one, lit on top."""
    d.rectangle([x, y, x + c - 1, y + c - 1], fill=make_icon.lerp(color, (0, 0, 0), 0.25) + (255,))
    d.rectangle([x, y, x + c - 1, y + c - SS - 1], fill=color + (255,))
    d.rectangle([x, y, x + c - 1, y + c // 3], fill=make_icon.lerp(color, (255, 255, 255), 0.45) + (255,))


def wordmark(text, cell):
    """The name in blocks, at `cell` final pixels a block (drawn 8x larger)."""
    c = cell * SS
    gap = c  # between letters
    width = sum(len(FONT[ch][0]) * c for ch in text) + gap * (len(text) - 1)
    img = Image.new("RGBA", (width + 2 * c, 5 * c + 2 * c), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x = c
    for i, ch in enumerate(text):
        rows = FONT[ch]
        color = make_icon.COLORS[LETTER_COLORS[i % len(LETTER_COLORS)]]
        for r, line in enumerate(rows):
            for k, bit in enumerate(line):
                if bit == "#":
                    letter_block(d, x + k * c, c + r * c, c, color)
        x += len(rows[0]) * c + gap
    return img


def banner():
    W, H = 420, 96
    img = backdrop(W, H)
    img = grid(img, 8)

    # the well, from the icon, on the left
    art = make_icon.draw(64)
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([4, 8, 84, 88], fill=(80, 170, 255, 70))
    img = Image.alpha_composite(img, glow.filter(ImageFilter.GaussianBlur(10)))
    img.alpha_composite(art, (14, 16))

    # TETRISPACK in blocks, with a soft shadow
    cell = 6
    word = wordmark("TETRISPACK", cell)
    word = word.resize((word.width // SS, word.height // SS), Image.LANCZOS)
    x, y = 92, 10
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    alpha = word.split()[3].point(lambda a: a * 0.6)
    shadow.paste((0, 0, 0, 255), (x + 2, y + 3), alpha)
    img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(2)))
    img.alpha_composite(word, (x, y))

    # the tagline
    d = ImageDraw.Draw(img)
    font = None
    for path in ("/usr/share/fonts/truetype/noto/NotoSans-SemiBold.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"):
        if os.path.exists(path):
            font = ImageFont.truetype(path, 13)
            break
    if font is None:
        font = ImageFont.load_default()
    tag = "THE ENTERTAINMENT PACK CLASSIC  •  XBOX 360"
    tx = x + cell
    d.text((tx + 1, 69), tag, font=font, fill=(0, 0, 0, 160))
    d.text((tx, 68), tag, font=font, fill=(200, 222, 255, 255))
    # a thin silver rule under the name
    d.line([(tx, 64), (W - 14, 64)], fill=(200, 205, 220, 120))

    # a silver frame round the banner
    d.rectangle([0, 0, W - 1, H - 1], outline=(150, 156, 172, 255))
    d.rectangle([1, 1, W - 2, H - 2], outline=(40, 44, 60, 255))
    return img.convert("RGB")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "consoles", "xbox360", "aurora")
    os.makedirs(out, exist_ok=True)
    icon().convert("RGB").save(os.path.join(out, "icon.png"), optimize=True)
    banner().save(os.path.join(out, "banner.png"), optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
