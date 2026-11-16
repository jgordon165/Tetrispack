# What the port takes from TETRIS.EXE 2.00

Notes from reading the Microsoft Entertainment Pack's `TETRIS.EXE`
("Tetris 2.00", Windows version by Dave Edson, 61,936 bytes, NE, Windows
3.0) with `tools/identify_exe.py` and a disassembly. Addresses are
segment:offset in the code segment (1) and data segment (2). None of the
original's code or data is in this repository; the values below are what the
port's own Lua uses.

## Resources

| Resource | What it is |
|---|---|
| BITMAP 1-10 | 32x32, 16 colours: the default background for levels 1-10, tiled over the window (bitmap 5 is the blue-ring pattern) |
| MENU 1 | Game (New F2, Pause F3, High Scores, Exit), Skill (Starting Level 1-10, Starting Rows 0-9), Options (Sound, 1/2 Player, Penalize Other Player, Piece Preview, Track High Scores), Help |
| DIALOG 1 | "Tetris Experts" high-score table |
| DIALOG 2 | "Tetris Background Settings": per level, solid colour / default bitmap / custom bitmap, tiled |
| DIALOG 3 | Hall of Fame entry ("Your Cool Name", "Your Cool Quote") |
| `ABOUTTET.DLL` BITMAP 666/999 | The About box's "Microsoft Windows Entertainment Package" logo and its mask |

## Layout (WM_SIZE, 1:10cc)

Relative to the client area (W x H), one player:

| | x | y | w | h |
|---|---|---|---|---|
| Playfield | 0.325 W | 0.066 H | 0.35 W | 0.868 H |

Cells are the playfield's width / 10 by height / 20: 22 x 19 at 640 x 440,
a maximized window on a 640 x 480 screen. The gray 3-D frames (white 4 px,
gray 6, a sunken line, then the gray margin) and the score panel's contents
were measured from screenshots of the original.

## Blocks (1:635e, palette at 1:1f32)

Colours 1-7 come from a table of 0-10 values (`DS:060a`, entries 20-27)
scaled by 19.2 (base), 25.5 (light) and 12.8 (dark):

| Colour | Piece | Base |
|---|---|---|
| 1 | I | red |
| 2 | Z | green |
| 3 | S | blue |
| 4 | O | cyan |
| 5 | J | magenta |
| 6 | L | yellow |
| 7 | T | gray |

DrawBlock: three light lines on the top and left, three dark lines on the
right and bottom, a filled rectangle inside, black on the cell's last row and
column. On the reference screens the dark lines show in the base colour, so
the port draws them that way (`skin.dark_bevel`).

## Pieces (1:56a4)

`((rand() % 5039) + 2) % 7 + 1` picks the type. Spawn cells (row, column):
I 0,3-6; Z 0,3-4 1,4-5; S 0,5-6 1,4-5; O 0,4-5 1,4-5; J 0,4-6 1,6;
L 0,3-5 1,3; T 0,3-5 1,4.

## Rules

- **Gravity timer** (SetTimer), ms per row, level 1-10: 600, 600, 520, 450,
  381, 306, 230, 155, 84, 50.
- **Level up** (1:5260) at 10, 16, 24, 34, 36, 50, 64, 78, 92 lines; it
  never drops below the starting level.
- **Score**: 6 x level for every piece that lands; 100 / 200 / 400 / 800 for
  1 / 2 / 3 / 4 rows at once (1:4b44), no level factor.
- **Line clear** (1:4a3e, paint at 1:619c): full rows are marked, then
  painted and inverted (InvertRgn over the row) on 10 passes in one player,
  5 flashes, before they are removed.
- **Keys** (1:0d3e): Left / Right move, Up or keypad 5 rotate, Down or
  Insert drop, End and Page Down slide to the wall (keypad 1 and 3).

## Still to confirm

- The rotation rule (which cell is the pivot, any wall kick).
- The 16-colour path (1:22fc) and the score panel's own drawing code.
- Starting rows, two-player mode, sound, high scores (`TETRIS.HST`).
