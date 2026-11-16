# Tetris (Microsoft Entertainment Pack) file formats

The game is a single 16-bit Windows program, `TETRIS.EXE`, from the
Microsoft Entertainment Pack for Windows (1990; also in *The Best of
Microsoft Entertainment Pack*, 1994). Its art, text, menus and dialogs are
resources inside the executable. All values little-endian.

## Executable: MZ stub + NE

```
0x00  "MZ" DOS stub ("This program requires Microsoft Windows")
0x3C  u32 e_lfanew -> NE header
```

NE header (offsets from its start) fields the port reads:

| Offset | Size | Field |
|---|---|---|
| 0x00 | 2 | `"NE"` |
| 0x24 | 2 | resource table offset (relative to the NE header) |
| 0x26 | 2 | resident-name table offset (relative); also ends the resource table |
| 0x3E | 2 | expected Windows version (major high byte, minor low byte) |

The first resident name is the module name.

## Resource table

```
u16 align_shift                       offsets and lengths are << this
repeat {
    u16 type                          0 ends the table; bit 15 set: numbered type
                                      (bit 15 clear: offset of its name, from table start)
    u16 count
    u32 reserved
    count × {
        u16 offset                    << align_shift = file offset
        u16 length                    << align_shift (rounded up to the alignment)
        u16 flags                     0x10 moveable, 0x20 pure, 0x40 preload
        u16 id                        bit 15 set: numbered; clear: offset of its name
        u32 reserved
    }
}
names: u8 length + characters (not NUL-terminated)
```

Numbered types: 1 CURSOR, 2 BITMAP, 3 ICON, 4 MENU, 5 DIALOG, 6 STRING,
9 ACCELERATOR, 10 RCDATA, 12 GROUP_CURSOR, 14 GROUP_ICON.

## BITMAP

A DIB without the 14-byte file header: `BITMAPINFOHEADER` (40 bytes) or
`BITMAPCOREHEADER` (12 bytes), then the palette (4 or 3 bytes per entry,
B, G, R), then bottom-up rows padded to 4 bytes. 1/4/8/24 bpp, uncompressed
or RLE4/RLE8. Decoded by `src/formats/dib.cpp`.

## STRING

Blocks of 16 strings; block *n* holds ids `(n-1)*16 ... (n-1)*16+15`. Each
string is a length byte then the characters (an empty entry is a 0 byte).

## The game itself

What the port takes from the 2.00 executable (block colours, layout, timer,
scoring, the line-clear blink) is in `original.md`.
