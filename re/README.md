# Reverse engineering workflow

Everything produced here (Ghidra project, decompiled C, extracted
resources) is **git-ignored**. It's reference material for writing the port,
not part of it. The port's own code in `src/` and `scripts/` is written fresh
from what we learn.

## 1. Identify the executable and list its resources

```
python3 -I tools/identify_exe.py /path/to/TETRIS.EXE --list
python3 -I tools/identify_exe.py /path/to/TETRIS.EXE --extract re/output/resources
```

| Result | What to do |
|---|---|
| NE (16-bit Windows) | The 1990 Entertainment Pack original: run `ghidra_headless.sh` as is. |
| PE (32-bit Windows) | A later re-release (Best of / Windows 95 era): pass `x86:LE:32:default`. |

`--extract` writes the BITMAP resources as `.bmp` files and the string
tables as `strings.txt`. Use them to fill in `scripts/game/skin.lua` (which
bitmap holds the blocks, and where each piece's block sits in it).

## 2. Decompile

```
GHIDRA_HOME=~/ghidra_11.x re/ghidra_headless.sh /path/to/TETRIS.EXE
```

Produces `re/output/all_functions.c` and `re/output/functions.tsv`.

## 3. Map subsystems, then rewrite

Work from the resources inward:

1. **Resources**: `LoadBitmap`, `LoadString`, `DialogBox` and `LoadMenu`
   imports (USER.EXE ordinals) show which resource each screen uses.
2. **Timer and gravity**: find the `SetTimer` call and the table it reads
   per level; that confirms `Board.gravity` in `scripts/game/board.lua`.
3. **Rules**: piece generation (the random number routine), rotation,
   scoring per lines cleared and per level, the level-up rule, the
   "starting height" option. Every value marked *verify* in
   `scripts/game/board.lua` should end up confirmed here.
4. **High scores**: where they are stored (`WIN.INI` via
   `WriteProfileString`, or a file) and their format.

Game logic stays in Lua under `scripts/`, written from your notes on the
decompiled behaviour rather than translated line by line.

Tools worth having alongside: Windows 3.1 in DOSBox-X or 86Box with the
original running, and a hex editor.
