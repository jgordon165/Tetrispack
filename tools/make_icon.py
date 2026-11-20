#!/usr/bin/env python3
"""Draws Tetrispack's icon, assets/tetrispack.ico (and a 256 px PNG).

An original drawing in the spirit of the Entertainment Pack's: a gray-framed
black well, a T piece falling over stacks of colored blocks. The flare: a
brushed-silver frame with rounded corners and a drop shadow, a deep blue
glow inside the well, glossy beveled blocks, the falling piece's glow and
light trails, its ghost where it will land, and a sparkle on the frame.

Every size is drawn for itself, with the blocks on whole pixels, so the
small ones stay crisp: each is rendered 8x larger and scaled down.

    python3 tools/make_icon.py [out_dir]       (needs Pillow)
"""
import io
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter

SS = 8  # supersampling

# The well: 7 x 7 cells, row 0 at the top.  '.' empty.
WELL = [
    ".......",
    ".......",
    ".......",
    "Y....M.",
    "YY.G.MC",
    "RY.G.MC",
    "RB.GYYC",
]
FALLING = [(3, 0), (2, 1), (3, 1), (4, 1)]     # the T, coming down
GHOST = [(3, 2), (2, 3), (3, 3), (4, 3)]       # where it will land, drawn faintly

COLORS = {
    "Y": (250, 214, 40),
    "R": (232, 44, 60),
    "M": (220, 40, 160),
    "G": (60, 210, 70),
    "C": (40, 210, 230),
    "B": (50, 100, 245),
    "T": (70, 170, 255),
}

# Per size: the frame's width, and the cell size, in final pixels.
LAYOUT = {16: (1, 2), 24: (2, 3), 32: (2, 4), 48: (3, 6), 64: (4, 8), 128: (8, 16), 256: (16, 32)}


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def block(d, x, y, c, color, alpha=255, small=False):
    """A glossy beveled block: c is the cell size in supersampled pixels."""
    hi = lerp(color, (255, 255, 255), 0.55)
    lo = lerp(color, (0, 0, 0), 0.45)
    e = max(SS, c // 7)  # bevel
    if small:
        d.rectangle([x, y, x + c - 1, y + c - 1], fill=color + (alpha,))
        d.rectangle([x, y, x + c - 1, y + c // 2 - 1], fill=lerp(color, (255, 255, 255), 0.3) + (alpha,))
        return
    d.rectangle([x, y, x + c - 1, y + c - 1], fill=lo + (alpha,))
    d.polygon([(x, y), (x + c - 1, y), (x + c - 1 - e, y + e), (x + e, y + e), (x + e, y + c - 1 - e), (x, y + c - 1)],
              fill=hi + (alpha,))
    d.rectangle([x + e, y + e, x + c - 1 - e, y + c - 1 - e], fill=color + (alpha,))
    # gloss: a lighter top half on the face
    g = lerp(color, (255, 255, 255), 0.22)
    d.rectangle([x + e, y + e, x + c - 1 - e, y + e + (c - 2 * e) // 2 - 1], fill=g + (alpha,))


def draw(size):
    frame, cell = LAYOUT[size]
    S = size * SS
    f, c = frame * SS, cell * SS
    well = 7 * c
    ox = (S - well) // 2
    oy = (S - well) // 2
    # snap the well to whole final pixels
    ox -= ox % SS
    oy -= oy % SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

    # drop shadow (larger sizes)
    if size >= 32:
        sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle([ox - f + SS * size // 64, oy - f + SS * size // 32,
                                              ox + well + f - 1 + SS * size // 64, oy + well + f - 1 + SS * size // 32],
                                             radius=f * 2, fill=(0, 0, 0, 110))
        img = Image.alpha_composite(img, sh.filter(ImageFilter.GaussianBlur(SS * size / 40)))

    d = ImageDraw.Draw(img)
    # the frame: brushed silver, light at the top
    r = f * 2 if size >= 32 else f
    x0, y0, x1, y1 = ox - f, oy - f, ox + well + f - 1, oy + well + f - 1
    frame_img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    fd = ImageDraw.Draw(frame_img)
    for y in range(y0, y1 + 1):
        t = (y - y0) / max(1, y1 - y0)
        fd.line([(x0, y), (x1, y)], fill=lerp((240, 242, 248), (120, 124, 136), t) + (255,))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([x0, y0, x1, y1], radius=r, fill=255)
    img.paste(frame_img, (0, 0), mask)
    d = ImageDraw.Draw(img)
    if size >= 48:  # a dark outline round the frame
        d.rounded_rectangle([x0, y0, x1, y1], radius=r, outline=(40, 42, 50, 255), width=SS)

    # the well: deep blue at the top to black, the modern gradient
    for y in range(oy, oy + well):
        t = (y - oy) / well
        d.line([(ox, y), (ox + well - 1, y)], fill=lerp((22, 34, 92), (0, 0, 6), min(1, t * 1.4)) + (255,))

    small = size <= 24
    # light trails above the falling piece, and its glow
    if not small:
        glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        for (cx, cy) in FALLING:
            gd.rectangle([ox + cx * c - c // 3, oy + cy * c - c // 3, ox + (cx + 1) * c + c // 3, oy + (cy + 1) * c + c // 3],
                         fill=(80, 180, 255, 120))
        glow = glow.filter(ImageFilter.GaussianBlur(c / 2.5))
        clip = Image.new("L", (S, S), 0)
        ImageDraw.Draw(clip).rectangle([ox, oy, ox + well - 1, oy + well - 1], fill=255)
        img.paste(Image.alpha_composite(img, glow), (0, 0), clip)
        d = ImageDraw.Draw(img)
    if size >= 48:  # the ghost: outlines where the piece will land
        for (cx, cy) in GHOST:
            w = max(SS, c // 12)
            d.rectangle([ox + cx * c + w, oy + cy * c + w, ox + (cx + 1) * c - 1 - w, oy + (cy + 1) * c - 1 - w],
                        outline=(120, 200, 255, 110), width=w)

    for row, line in enumerate(WELL):
        for col, ch in enumerate(line):
            if ch != ".":
                block(d, ox + col * c, oy + row * c, c, COLORS[ch], small=small)
    for (cx, cy) in FALLING:
        block(d, ox + cx * c, oy + cy * c, c, COLORS["T"], small=small)

    # a sparkle on the frame's top-right corner
    if size >= 48:
        sp = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        sd = ImageDraw.Draw(sp)
        cx, cy, L, w = x1 - r * 1.2, y0 + r * 0.8, f * 2.0, max(SS, f // 3)
        sd.polygon([(cx, cy - L), (cx + w, cy), (cx, cy + L), (cx - w, cy)], fill=(255, 255, 255, 255))
        sd.polygon([(cx - L, cy), (cx, cy - w), (cx + L, cy), (cx, cy + w)], fill=(255, 255, 255, 255))
        sd.ellipse([cx - w * 1.5, cy - w * 1.5, cx + w * 1.5, cy + w * 1.5], fill=(255, 255, 255, 255))
        halo = sp.filter(ImageFilter.GaussianBlur(w * 1.2))
        img = Image.alpha_composite(img, halo)
        img = Image.alpha_composite(img, sp)

    return img.resize((size, size), Image.LANCZOS if size >= 48 else Image.BOX)


def write_ico(path, images):
    """An .ico of PNG entries (Windows Vista and later read them at every size)."""
    blobs = []
    for im in images:
        b = io.BytesIO()
        im.save(b, "PNG", optimize=True)
        blobs.append(b.getvalue())
    out = struct.pack("<HHH", 0, 1, len(images))
    off = 6 + 16 * len(images)
    for im, data in zip(images, blobs):
        w, h = im.size
        out += struct.pack("<BBBBHHII", w % 256, h % 256, 0, 0, 1, 32, len(data), off)
        off += len(data)
    with open(path, "wb") as fh:
        fh.write(out + b"".join(blobs))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "assets")
    os.makedirs(out, exist_ok=True)
    images = [draw(s) for s in sorted(LAYOUT)]
    write_ico(os.path.join(out, "tetrispack.ico"), images)
    images[-1].save(os.path.join(out, "tetrispack.png"))
    print("wrote", os.path.join(out, "tetrispack.ico"))


if __name__ == "__main__":
    main()
