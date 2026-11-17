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

## DIALOG

The 16-bit `DLGTEMPLATE`: style (32 bits), control count (8 bits), x, y,
width, height in dialog units, then the menu, class and caption (each a
NUL-terminated string, or `FF` and a 16-bit id). `DS_SETFONT` adds a point
size and a font name. Each control: x, y, width, height, id, style, then
its class (a byte `80`-`85` for button / edit / static / listbox /
scrollbar / combobox, else a name), its text (or `FF` and an id), and a
length byte for creation data. Decoded by `src/formats/dialog.cpp`.

## ABOUTTET.DLL

The About box the Entertainment Pack games share, a small NE DLL beside
`TETRIS.EXE`: BITMAP 666 (the colour logo, 259x64) and 999 (monochrome),
DIALOG 100 (colour) and 101 (monochrome). The dialog's static text holds the
credits; the game passes the version and copyright lines ("Tetris 2.00",
"(c) 1990 Microsoft Corporation."), which sit in TETRIS.EXE's data segment,
not in a resource. The port reads both at runtime (`tp.dialog`,
`tp.bitmap(666, "about")`, `tp.exe_string`).

## TETRIS.HST

The original's high score table, beside TETRIS.EXE (660 bytes): ten
16-bit scores (little-endian, highest first), then ten records of a
32-byte name and a 32-byte "cool quote", each NUL-terminated (the rest of
the field is whatever was there before, often spaces). An entry with an
empty name is still a score. The port reads it once to start its own
table (`scripts/game/scores.lua`) and never writes it.

## The game itself

What the port takes from the 2.00 executable (block colours, layout, timer,
scoring, the line-clear blink) is in `original.md`.
