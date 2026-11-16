#!/usr/bin/env python3
"""Identify a Windows 3.x executable and list or extract its resources.

  python3 -I tools/identify_exe.py TETRIS.EXE            # header summary
  python3 -I tools/identify_exe.py TETRIS.EXE --list     # every resource
  python3 -I tools/identify_exe.py TETRIS.EXE --extract re/output/resources
        # BITMAP resources as .bmp (with a file header added), STRING tables
        # as .txt, everything else as .bin. Output stays local: re/output/ is
        # git-ignored, because it is the original's content.

Tells you how Ghidra should load it:
  * NE (16-bit Windows)  -> Ghidra's "New Executable (NE)" loader,
                            x86:LE:16:Protected Mode (re/ghidra_headless.sh)
  * PE (32-bit Windows)  -> a later re-release; Ghidra's PE loader, x86:LE:32
  * plain MZ             -> a DOS program, x86:LE:16:Real Mode
"""
import os
import struct
import sys

TYPES = {1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
         7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 12: "GROUP_CURSOR",
         14: "GROUP_ICON", 16: "VERSION"}


def pstring(data, off):
    n = data[off]
    return data[off + 1:off + 1 + n].decode("latin-1")


def ne_resources(data, ne):
    tab = ne + struct.unpack_from("<H", data, ne + 0x24)[0]
    end = ne + struct.unpack_from("<H", data, ne + 0x26)[0]
    if tab == end:
        return []
    shift = struct.unpack_from("<H", data, tab)[0]
    pos, out = tab + 2, []
    while True:
        rtype = struct.unpack_from("<H", data, pos)[0]
        if rtype == 0:
            break
        count = struct.unpack_from("<H", data, pos + 2)[0]
        pos += 8
        tname = TYPES.get(rtype & 0x7FFF, f"#{rtype & 0x7FFF}") if rtype & 0x8000 else pstring(data, tab + rtype)
        for _ in range(count):
            off, length, flags, rid = struct.unpack_from("<HHHH", data, pos)
            pos += 12
            name = f"#{rid & 0x7FFF}" if rid & 0x8000 else pstring(data, tab + rid).upper()
            out.append((tname, name, off << shift, length << shift, flags))
    return out


def dib_info(blob):
    hdr = struct.unpack_from("<I", blob, 0)[0]
    if hdr == 12:
        w, h, _, bpp = struct.unpack_from("<HhHH", blob, 4)
        return w, h, bpp, 0, 3
    w, h, _, bpp, comp = struct.unpack_from("<iiHHI", blob, 4)
    used = struct.unpack_from("<I", blob, 32)[0]
    return w, h, bpp, comp, (used or (1 << bpp if bpp <= 8 else 0)) * 4


def as_bmp(blob):
    hdr = struct.unpack_from("<I", blob, 0)[0]
    w, h, bpp, comp, pal = dib_info(blob)
    if hdr == 12 and bpp <= 8:
        pal = (1 << bpp) * 3
    bits = hdr + pal
    return b"BM" + struct.pack("<IHHI", 14 + len(blob), 0, 0, 14 + bits) + blob


def strings(blob, block):
    out, pos = [], 0
    for i in range(16):
        if pos >= len(blob):
            break
        n = blob[pos]
        s = blob[pos + 1:pos + 1 + n].decode("latin-1")
        pos += 1 + n
        if s:
            out.append(f"{(block - 1) * 16 + i}\t{s}")
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    data = open(path, "rb").read()
    print(f"file      : {path} ({len(data)} bytes)")
    if data[:2] != b"MZ":
        print("format    : not an MZ executable")
        return 1
    lfanew = struct.unpack_from("<I", data, 0x3C)[0] if len(data) >= 0x40 else 0
    sig = data[lfanew:lfanew + 2] if 0 < lfanew < len(data) - 2 else b""
    if sig == b"PE":
        print("format    : PE (32-bit Windows); a later re-release, not the 1990 original")
        return 0
    if sig != b"NE":
        print("format    : plain MZ (DOS program)")
        return 0
    ne = lfanew
    ver = struct.unpack_from("<H", data, ne + 0x3E)[0]
    resnames = ne + struct.unpack_from("<H", data, ne + 0x26)[0]
    segs = struct.unpack_from("<H", data, ne + 0x1C)[0]
    print("format    : NE (16-bit Windows)")
    print(f"module    : {pstring(data, resnames)}")
    print(f"windows   : {ver >> 8}.{ver & 0xFF}")
    print(f"segments  : {segs}")
    res = ne_resources(data, ne)
    print(f"resources : {len(res)}")

    if "--list" in argv:
        for tname, name, off, length, flags in res:
            extra = ""
            if tname == "BITMAP":
                w, h, bpp, comp, _ = dib_info(data[off:off + length])
                extra = f"  {w}x{abs(h)} {bpp}bpp" + (f" compression {comp}" if comp else "")
            print(f"  {tname:<12} {name:<12} offset {off:#08x} size {length:6}{extra}")

    if "--extract" in argv:
        dest = argv[argv.index("--extract") + 1] if len(argv) > argv.index("--extract") + 1 else "re/output/resources"
        os.makedirs(dest, exist_ok=True)
        table = []
        for tname, name, off, length, _ in res:
            blob = data[off:off + length]
            base = os.path.join(dest, f"{tname}_{name.lstrip('#')}")
            if tname == "BITMAP":
                open(base + ".bmp", "wb").write(as_bmp(blob))
            elif tname == "STRING":
                table += strings(blob, int(name.lstrip("#")))
            else:
                open(base + ".bin", "wb").write(blob)
        if table:
            open(os.path.join(dest, "strings.txt"), "w").write("\n".join(table) + "\n")
        print(f"extracted to {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
