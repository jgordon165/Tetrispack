#!/usr/bin/env python3
"""Tiles and palettes for the console builds (consoles/genesis, consoles/snes).

Draws everything in 8x8 tiles of 16 colours: the playfield blocks (and their
inverted look for the line-clear blink), the Windows 3.x gray frames, the
next-piece box, the About-style box of the title screen, the text font, and
ten level backgrounds of 32x32 pixels.

The backgrounds are our own lookalike patterns, so the output can be
committed to CI artifacts and released. With --data, the ten backgrounds come
from your TETRIS.EXE instead (BITMAP 1-10: 32x32, 16 colours, which is what
a 4x4 block of console tiles holds). A ROM built that way contains the
original's art: keep it to yourself.

  python3 -I tools/console_art.py --console genesis --out build-md/gen
  python3 -I tools/console_art.py --console snes --out build-snes/gen --data <TETRIS folder>

Writes art.h plus art.c (genesis) or art.asm and .bin files (snes).
"""

import argparse
import importlib.util
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def load_sibling(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, name + ".py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ---- Colours (the Lua game's skin.lua) ----------------------------------------

BLACK, WHITE = (0, 0, 0), (255, 255, 255)
GRAY, SHADOW = (192, 192, 192), (128, 128, 128)
FRAME_BLUE, VALUE_BLUE = (0, 0, 192), (0, 0, 255)

RGB10 = [(10, 0, 0), (0, 10, 0), (0, 0, 10), (0, 10, 10), (10, 0, 10), (10, 10, 0), (6, 6, 6)]
BASE = [tuple(int(c * 19.2) for c in t) for t in RGB10]
LIGHT = [tuple(int(c * 25.5) for c in t) for t in RGB10]

# Palette 1: blocks. 0 transparent, 1 black, 2-8 base colours, 9-15 light.
PAL_BLOCK = [BLACK, BLACK] + BASE + LIGHT
# Palette 3: the same, inverted (the blink).
PAL_INVERT = [BLACK] + [tuple(255 - c for c in rgb) for rgb in PAL_BLOCK[1:]]
# Palette 2: frames, boxes and text.
UI = {"clear": 0, "black": 1, "white": 2, "gray": 3, "shadow": 4, "frame": 5, "value": 6}
PAL_UI = [BLACK, BLACK, WHITE, GRAY, SHADOW, FRAME_BLUE, VALUE_BLUE] + [BLACK] * 9


# ---- Drawing on small index canvases ------------------------------------------

class Canvas:
    def __init__(self, w, h, fill=0):
        self.w, self.h = w, h
        self.px = [[fill] * w for _ in range(h)]

    def rect(self, x, y, w, h, c):
        for yy in range(max(0, y), min(self.h, y + h)):
            for xx in range(max(0, x), min(self.w, x + w)):
                self.px[yy][xx] = c

    def tiles(self):
        """8x8 tiles left to right, top to bottom; each a list of 64 indices."""
        out = []
        for ty in range(0, self.h, 8):
            for tx in range(0, self.w, 8):
                out.append([self.px[ty + y][tx + x] for y in range(8) for x in range(8)])
        return out


def block_tile(color):
    """One cell, as the original's DrawBlock at 8x8: a light line top and
    left, the colour inside, the last row and column black (the grid)."""
    c = Canvas(8, 8, 1)
    c.rect(0, 0, 7, 7, 1 + color)            # base: index 2-8
    c.rect(0, 0, 7, 1, 8 + color)            # light top: index 9-15
    c.rect(0, 0, 1, 7, 8 + color)            # light left
    return c.tiles()[0]


def frame_tiles(center):
    """The 3-D gray frame (skin.frame at half scale) as 3x3 tiles: 2 px raised
    edge, 3 px gray, the sunken line, 2 px gray margin; `center` fills the
    middle tile (black for the playfield, gray for the score panel)."""
    u = UI
    c = Canvas(24, 24)
    c.rect(0, 0, 24, 24, u["white"])                                   # D: raised edge
    c.rect(22, 0, 2, 24, u["shadow"])
    c.rect(0, 22, 24, 2, u["shadow"])
    c.rect(2, 2, 20, 20, u["gray"])                                    # C: gray band
    c.rect(5, 5, 14, 14, u["white"])                                   # B: sunken line
    c.rect(5, 5, 13, 1, u["black"])
    c.rect(5, 5, 1, 13, u["black"])
    c.rect(6, 6, 12, 12, u["gray"])                                    # A: margin
    c.rect(8, 8, 8, 8, u[center])
    return c.tiles()


def sunken_tiles():
    """The next-piece box: black top-left line, white bottom-right, gray in."""
    u = UI
    c = Canvas(24, 24, u["gray"])
    c.rect(0, 0, 24, 24, u["white"])
    c.rect(0, 0, 23, 1, u["black"])
    c.rect(0, 0, 1, 23, u["black"])
    c.rect(1, 1, 22, 22, u["gray"])
    return c.tiles()


def about_tiles():
    """The About box's frame: 1 px black, 4 px blue band, white inside."""
    u = UI
    c = Canvas(24, 24, u["black"])
    c.rect(1, 1, 22, 22, u["frame"])
    c.rect(5, 5, 14, 14, u["white"])
    return c.tiles()


def font_tiles(color):
    glyphs = load_sibling("console_font").GLYPHS
    out = []
    for g in glyphs:
        out.append([color if (g[y] >> (7 - x)) & 1 else 0 for y in range(8) for x in range(8)])
    return out


# ---- Backgrounds -----------------------------------------------------------------
# Ten original patterns of our own in the spirit of Windows 3.x wallpapers:
# geometric, 32x32, a handful of colours each. Each returns (palette, pixels).

def _pattern(palette, fn):
    px = [[fn(x, y) for x in range(32)] for y in range(32)]
    pal = list(palette) + [BLACK] * (16 - len(palette))
    return pal, px


def lookalike_backgrounds():
    import math
    bgs = []
    # 1: red and rose diagonal bands.
    bgs.append(_pattern([(128, 0, 0), (255, 0, 0), (255, 96, 160), (192, 0, 64)],
                        lambda x, y: ((x + y) // 4) % 4))
    # 2: green bricks with dark mortar.
    def bricks(x, y):
        row = y // 8
        if y % 8 == 7: return 0
        if (x + (8 if row % 2 else 0)) % 16 == 15: return 0
        return 1 if (y % 8) else 2
    bgs.append(_pattern([(0, 64, 0), (0, 160, 0), (96, 224, 96)], bricks))
    # 3: yellow diamonds on brown.
    bgs.append(_pattern([(96, 64, 0), (224, 192, 0), (255, 255, 128)],
                        lambda x, y: 2 if abs(x % 16 - 8) + abs(y % 16 - 8) < 3 else
                        1 if abs(x % 16 - 8) + abs(y % 16 - 8) < 7 else 0))
    # 4: cyan bevelled squares.
    def bevel(x, y):
        a, b = x % 16, y % 16
        if a == 0 or b == 0: return 2
        if a == 15 or b == 15: return 0
        return 1
    bgs.append(_pattern([(0, 64, 96), (0, 160, 192), (160, 255, 255)], bevel))
    # 5: concentric rings, blue on black.
    def rings(x, y):
        d = math.hypot(x % 16 - 7.5, y % 16 - 7.5)
        return 3 if d < 2 else 0 if d < 3 else 2 if d < 5 else 1 if d < 7 else 0
    bgs.append(_pattern([(0, 0, 0), (0, 0, 192), (64, 128, 255), (255, 255, 0)], rings))
    # 6: purple waves.
    bgs.append(_pattern([(48, 0, 64), (128, 0, 160), (224, 128, 255)],
                        lambda x, y: (int((y + 3 * math.sin(x * math.pi / 8)) // 4)) % 3))
    # 7: tartan in red, green and gold.
    def plaid(x, y):
        a, b = (x % 16) < 4, (y % 16) < 4
        if a and b: return 3
        if a or b: return 2 if (x + y) % 2 else 1
        return 0
    bgs.append(_pattern([(96, 0, 0), (0, 96, 32), (160, 128, 0), (224, 224, 96)], plaid))
    # 8: gray dots on teal.
    bgs.append(_pattern([(0, 96, 96), (192, 192, 192), (255, 255, 255)],
                        lambda x, y: 2 if math.hypot(x % 8 - 3.5, y % 8 - 3.5) < 1.2 else
                        1 if math.hypot(x % 8 - 3.5, y % 8 - 3.5) < 2.6 else 0))
    # 9: orange zigzag.
    bgs.append(_pattern([(64, 16, 0), (255, 128, 0), (255, 224, 160)],
                        lambda x, y: 1 + ((y + abs(x % 8 - 4)) // 4) % 2 if (y + abs(x % 8 - 4)) % 4 else 0))
    # 10: white crosses on navy.
    def cross(x, y):
        a, b = x % 16, y % 16
        if (6 <= a <= 9 and 3 <= b <= 12) or (6 <= b <= 9 and 3 <= a <= 12):
            return 2 if a in (6, 7) or b in (6, 7) else 1
        return 0
    bgs.append(_pattern([(0, 0, 96), (160, 160, 224), (255, 255, 255)], cross))
    return bgs


def original_backgrounds(data_dir):
    """BITMAP 1-10 of the player's TETRIS.EXE: 32x32, 4 bits per pixel."""
    ide = load_sibling("identify_exe")
    exe = None
    for root in (data_dir, *[os.path.join(data_dir, d) for d in sorted(os.listdir(data_dir))
                             if os.path.isdir(os.path.join(data_dir, d))]):
        for n in os.listdir(root):
            if n.upper() == "TETRIS.EXE":
                exe = os.path.join(root, n)
        if exe:
            break
    if not exe:
        sys.exit(f"TETRIS.EXE not found in {data_dir}")
    data = open(exe, "rb").read()
    ne = struct.unpack_from("<I", data, 0x3C)[0]
    res = {(t, name): (off, length) for t, name, off, length, _ in ide.ne_resources(data, ne)}
    bgs = []
    for n in range(1, 11):
        if ("BITMAP", "#%d" % n) not in res:
            sys.exit(f"{exe}: no BITMAP {n}")
        off, length = res[("BITMAP", "#%d" % n)]
        blob = data[off:off + length]
        hsize, w, h, planes, bpp = struct.unpack_from("<IiiHH", blob, 0)
        if hsize != 40 or bpp != 4 or w != 32 or abs(h) != 32:
            sys.exit(f"{exe}: BITMAP {n} is {w}x{h} {bpp}bpp, expected 32x32 4bpp")
        ncol = struct.unpack_from("<I", blob, 32)[0] or 16
        pal = []
        for i in range(16):
            if i < ncol:
                b, g, rr, _ = blob[40 + 4 * i:44 + 4 * i]
                pal.append((rr, g, b))
            else:
                pal.append(BLACK)
        bits = 40 + 4 * ncol
        px = []
        for row in range(32):
            src = bits + (31 - row if h > 0 else row) * 16
            line = []
            for i in range(16):
                v = blob[src + i]
                line += [v >> 4, v & 15]
            px.append(line)
        bgs.append((pal, px))
    return bgs


def background_tiles(px):
    c = Canvas(32, 32)
    c.px = [row[:] for row in px]
    return c.tiles()


# ---- The tile set ----------------------------------------------------------------

def build(data_dir):
    tiles, names = [], {}

    def add(name, ts):
        names[name] = len(tiles)
        tiles.extend(ts)

    add("BLOCK", [block_tile(c) for c in range(1, 8)])        # 7, palette 1 (or 3 inverted)
    add("FRAME_BLACK", frame_tiles("black"))                  # 9: TL T TR L C R BL B BR
    add("FRAME_GRAY", frame_tiles("gray")[4:5])               # 1: the panel's middle
    add("SUNKEN", sunken_tiles())                             # 9
    add("ABOUT", about_tiles())                               # 9
    add("FONT", font_tiles(UI["black"]))                      # 96, ASCII 32-127
    add("FONT_BLUE", font_tiles(UI["value"]))                 # 96
    add("FONT_WHITE", font_tiles(UI["white"]))                # 96
    bgs = original_backgrounds(data_dir) if data_dir else lookalike_backgrounds()
    return tiles, names, bgs


# ---- Output ----------------------------------------------------------------------

def md_color(rgb):
    r, g, b = rgb
    return ((b >> 5) << 9) | ((g >> 5) << 5) | ((r >> 5) << 1)


def snes_color(rgb):
    r, g, b = rgb
    return (b >> 3) << 10 | (g >> 3) << 5 | (r >> 3)


def md_tile(t):
    return [sum(t[y * 8 + x] << (28 - 4 * x) for x in range(8)) for y in range(8)]


def snes_tile(t):
    out = bytearray(32)
    for y in range(8):
        for plane in range(4):
            v = 0
            for x in range(8):
                v |= ((t[y * 8 + x] >> plane) & 1) << (7 - x)
            base = 0 if plane < 2 else 16
            out[base + y * 2 + (plane & 1)] = v
    return bytes(out)


def header(names, ntiles, source):
    lines = ["/* Generated by tools/console_art.py (%s). Do not edit. */" % source,
             "#ifndef TETRISPACK_ART_H", "#define TETRISPACK_ART_H", ""]
    for k, v in names.items():
        lines.append("#define ART_%s %d" % (k, v))
    lines += ["#define ART_TILES %d" % ntiles, "#define ART_BG_TILES 16", ""]
    return lines


def emit_genesis(out, tiles, names, bgs, source):
    h = header(names, len(tiles), source) + [
        '#include <genesis.h>', "",
        "extern const u32 art_tiles[ART_TILES * 8];",
        "extern const u32 art_bg_tiles[10][ART_BG_TILES * 8];",
        "extern const u16 art_bg_pal[10][16];",
        "extern const u16 art_pal_block[16], art_pal_invert[16], art_pal_ui[16];",
        "", "#endif", ""]
    open(os.path.join(out, "art.h"), "w").write("\n".join(h))

    def u32s(vals):
        return ",\n".join("    " + ", ".join("0x%08X" % v for v in vals[i:i + 8]) for i in range(0, len(vals), 8))

    def pal(p):
        return "{ " + ", ".join("0x%03X" % md_color(c) for c in p) + " }"

    c = ['/* Generated by tools/console_art.py (%s). Do not edit. */' % source, '#include "art.h"', ""]
    c.append("const u32 art_tiles[ART_TILES * 8] = {\n%s\n};" % u32s([w for t in tiles for w in md_tile(t)]))
    c.append("const u32 art_bg_tiles[10][ART_BG_TILES * 8] = {")
    for pal_, px in bgs:
        c.append("  {\n%s\n  }," % u32s([w for t in background_tiles(px) for w in md_tile(t)]))
    c.append("};")
    c.append("const u16 art_bg_pal[10][16] = {\n" + ",\n".join("    " + pal(p) for p, _ in bgs) + "\n};")
    c.append("const u16 art_pal_block[16] = %s;" % pal(PAL_BLOCK))
    c.append("const u16 art_pal_invert[16] = %s;" % pal(PAL_INVERT))
    c.append("const u16 art_pal_ui[16] = %s;" % pal(PAL_UI))
    open(os.path.join(out, "art.c"), "w").write("\n".join(c) + "\n")


def emit_snes(out, tiles, names, bgs, source):
    h = header(names, len(tiles), source) + [
        "extern char art_tiles, art_tiles_end;",
        "extern char art_bg_tiles, art_bg_tiles_end;   /* 10 x 16 tiles */",
        "extern char art_bg_pal, art_bg_pal_end;       /* 10 x 16 colours */",
        "extern char art_pals, art_pals_end;           /* block, invert, ui: 3 x 16 colours */",
        "", "#endif", ""]
    open(os.path.join(out, "art.h"), "w").write("\n".join(h))
    open(os.path.join(out, "art_tiles.bin"), "wb").write(b"".join(snes_tile(t) for t in tiles))
    open(os.path.join(out, "art_bg_tiles.bin"), "wb").write(
        b"".join(snes_tile(t) for _, px in bgs for t in background_tiles(px)))
    open(os.path.join(out, "art_bg_pal.bin"), "wb").write(
        b"".join(struct.pack("<H", snes_color(c)) for p, _ in bgs for c in p))
    open(os.path.join(out, "art_pals.bin"), "wb").write(
        b"".join(struct.pack("<H", snes_color(c)) for p in (PAL_BLOCK, PAL_INVERT, PAL_UI) for c in p))
    asm = ["; Generated by tools/console_art.py (%s). Do not edit." % source, ""]
    for i, name in enumerate(("art_tiles", "art_bg_tiles", "art_bg_pal", "art_pals")):
        asm += ['.section ".rodata_art%d" superfree' % i, "%s:" % name,
                '.incbin "%s.bin"' % name, "%s_end:" % name, ".ends", ""]
    open(os.path.join(out, "art.asm"), "w").write("\n".join(asm))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--console", choices=("genesis", "snes"), required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--data", help="folder with your TETRIS.EXE: use its backgrounds (private builds only)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    tiles, names, bgs = build(a.data)
    source = "backgrounds from TETRIS.EXE - do not distribute" if a.data else "lookalike backgrounds"
    (emit_genesis if a.console == "genesis" else emit_snes)(a.out, tiles, names, bgs, source)
    print(f"{a.console}: {len(tiles)} tiles + 10 backgrounds ({source}) -> {a.out}")


if __name__ == "__main__":
    main()
