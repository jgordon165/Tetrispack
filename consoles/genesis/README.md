# Tetrispack for the Sega Genesis / Mega Drive

Tetris for Windows 2.00 as a Genesis cartridge ROM, built with
[SGDK](https://github.com/Stephane-D/SGDK) (MIT). The Lua game can't run on
a 68000 with 64 KB of RAM, so the rules are rewritten in plain C
(`consoles/common/tetris_core.c`, shared with the SNES build). They keep the
original's pieces, gravity timer, level-up line counts, scoring and row
blink.

- **Screen**: 320x224. The level's background is tiled behind everything,
  with the score panel on the left. The playfield is 10x20 cells of 8x8 in
  the original's gray 3-D frame. Full rows blink inverted before they go.
- **Music**: the PC game's Genesis style, the same arrangements on the real
  sound chips. The YM2612 FM and SN76489 PSG writes are logged to VGM
  (`tetrispack_render_music <scripts> <dir> genesis-vgm`) and played by
  SGDK's XGM2 driver, one track per pair of levels as on the PC.
- **Sound effects**: the PC game's, as samples on XGM2's PCM channels.
- **Title screen**: an About-style box with Starting Level, Starting Rows,
  Piece Preview and Music. Leave it alone for 10 seconds and a demo plays
  by itself.

| Pad | |
|---|---|
| Left / Right | Move (hold to repeat) |
| Up, A, C | Rotate |
| Down, B | Drop |
| X / Z (six-button pad) | Slide to the left / right wall |
| Start | Pause; on the title screen, play |

## Art and your TETRIS.EXE

A cartridge can't read your `TETRIS.EXE` while it runs, so the art is built
into the ROM. By default the level backgrounds are ten patterns of our own
in the style of Windows 3.x wallpapers (`tools/console_art.py`). That ROM has
no original content, so CI builds it and releases share it.

To build with the original backgrounds, pass your Tetris folder with
`--data`. The ROM then contains Microsoft's art: keep it for yourself and
don't share it.

## Building

On Linux (Ubuntu / Debian), with the PC build done first (for
`build/tetrispack_render_music`):

```
sudo apt install gcc-m68k-linux-gnu binutils-m68k-linux-gnu default-jre-headless
eval "$(consoles/genesis/setup-sgdk.sh .sgdk)"      # fetches and builds SGDK v2.11
consoles/genesis/build.sh build-genesis              # -> build-genesis/tetrispack-genesis.bin
consoles/genesis/build.sh build-genesis-mine --data ~/WEP   # your own art, private
```

Run the ROM in any Genesis emulator (BlastEm, Genesis Plus GX, MAME), or on
the console from a flash cartridge.

## Tests

- `ctest` runs the C rules on the PC (`tests/test_console_core.c`), the VGM
  writer, and the rest of the game's tests.
- CI also boots the ROM in MAME with no input, so the attract demo plays.
  `consoles/tests/mame_smoke.sh` reads the game's RAM and fails unless the
  demo has placed pieces and cleared lines. The job's artifact holds the
  snapshots.
