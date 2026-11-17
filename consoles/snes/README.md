# Tetrispack for the Super Nintendo

Tetris for Windows 2.00 as a SNES cartridge ROM, built with
[PVSnesLib](https://github.com/alekmaul/pvsneslib) (zlib licence). It uses
the same C rules as the Genesis build (`consoles/common/tetris_core.c`): the
original's pieces, gravity timer, level-up line counts, scoring and row
blink.

- **Screen**: 256x224, mode 1. The level's background is tiled behind
  everything, with the score panel on the left. The playfield is 10x20
  cells of 8x8 in the original's gray 3-D frame. Full rows blink inverted
  before they go.
- **Music**: the PC game's SNES style, written as Impulse Tracker modules
  (`tetrispack_render_music <scripts> <dir> snes-it`) for PVSnesLib's
  SNESMOD driver.
  - The synthesized instruments become short samples, their envelopes IT
    volume envelopes, and the echo SNESMOD's.
  - The S-DSP has 64 KB of sound RAM for the driver, one song, its echo and
    the sound effects, so each song plays once, then again a whole step up,
    instead of the PC's longer form.
  - A few of the chord stabs drop out when all eight voices are busy, as on
    the PC's SNES style.
- **Sound effects**: the PC game's, as 8 kHz samples (SNESMOD's effects).
- **Title screen**: an About-style box with Starting Level, Starting Rows,
  Piece Preview and Music. Leave it alone for 10 seconds and a demo plays
  by itself.

| Pad | |
|---|---|
| Left / Right | Move (hold to repeat) |
| Up, A, X | Rotate |
| Down, B | Drop |
| L / R | Slide to the left / right wall |
| Start | Pause; on the title screen, play |

## Art and your TETRIS.EXE

As on the Genesis, the art is built into the ROM. By default the level
backgrounds are our own lookalike patterns (`tools/console_art.py`), so CI
builds that ROM and releases share it.

`--data` builds with your TETRIS.EXE's backgrounds. That ROM is for you only:
don't share it.

## Building

On Linux x64, with the PC build done first (for
`build/tetrispack_render_music`):

```
eval "$(consoles/snes/setup-pvsneslib.sh .pvsneslib)"    # PVSnesLib 4.7.1, checksum-verified
consoles/snes/build.sh build-snes                         # -> build-snes/tetrispack-snes.sfc
consoles/snes/build.sh build-snes-mine --data ~/WEP       # your own art, private
```

Run the ROM in any SNES emulator (bsnes, Snes9x, Mesen), or on the console
from a flash cartridge.

## Tests

- **Smoke test**: CI runs the ROM headless in the snes9x libretro core
  (`apt install libretro-snes9x`) through `consoles/tests/libretro_run.py`.
  MAME's SNES needs Sony's SPC700 boot ROM, which can't be shipped; the core
  has its own.
- **What it checks**: with no input the attract demo plays.
  `consoles/tests/snes_smoke.sh` reads the game's work RAM and fails unless
  the demo has placed pieces and cleared lines.
- **Artifacts**: the job keeps the ROM, the snapshots and the result.
