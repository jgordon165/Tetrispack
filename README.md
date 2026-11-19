# Tetrispack

[![CI-build](https://github.com/jgordon165/Tetrispack/actions/workflows/ci-build.yml/badge.svg)](https://github.com/jgordon165/Tetrispack/actions/workflows/ci-build.yml)
[![code-analysis](https://github.com/jgordon165/Tetrispack/actions/workflows/code-analysis.yml/badge.svg)](https://github.com/jgordon165/Tetrispack/actions/workflows/code-analysis.yml)

A modern reimplementation host for *Tetris* from the **Microsoft
Entertainment Pack for Windows** (1990, Windows 3.x): a C++ engine (SDL2)
running game logic written in Lua.

**No original game code or data is included.** The engine reads the art and
text at runtime from the player's own `TETRIS.EXE` (its BITMAP and STRING
resources), the same way ScummVM and OpenRCT2 work. Original files are
git-ignored as a safety net, and release packages are checked for them.

## Build

Requires CMake 3.20+ and a C++20 compiler. SDL2 and Lua 5.4 are downloaded
and built automatically.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

Windows (Visual Studio 2022): the same commands work from a Developer prompt,
or open the folder in Visual Studio.

## Run

Point the engine at the folder holding your `TETRIS.EXE` (an Entertainment
Pack install folder works, as does the file itself):

```
build\Release\tetrispack.exe --data C:\WEP
```

It opens laid out like the original maximized on a 640x480 screen, shown at
1.5x (960x660; `--scale` changes it): the level's background bitmap tiled
over the window, the score panel, the playfield in its gray frame, the
original's block colours and bevel, and full rows blinking before they go.
Without `TETRIS.EXE` it still runs, on a plain background.

**Intro.** It starts, as the original's Help > About does, with the About
box over the waiting game: the Microsoft Entertainment Pack logo and the
credits, read from `ABOUTTET.DLL` (the Entertainment Pack keeps it beside
`TETRIS.EXE`), with the version line from `TETRIS.EXE`. OK closes it: click
it, or press Enter, Space or Esc (A or B on a controller). It's in the menu
too, as Help > About Tetris. Without the DLL the box shows the port's own
credits.

| Key | |
|---|---|
| F2 (or Enter) | New game |
| 1-9, 0 | Starting level 1-10 (before a game) |
| Left / Right (keypad 4 / 6) | Move |
| Up (keypad 8 / 5) | Rotate |
| Down, Insert, Space (keypad 2 / 0) | Drop |
| End / Page Down (keypad 1 / 3) | Slide to the wall |
| F3 | Pause |
| F5 | Piece preview on / off |
| Esc or F12 | Menu |

**Menu.** Esc, F12, a right click or Start on a controller pauses the game
and opens the menu: a popup in the game's own style (black, in the
playfield's gray frame) with one scrollable list. Game: New Game, Pause /
Resume, High Scores, Exit. Skill: Starting Level, Starting Rows. Options:
Piece Preview, Music, Music Style, Sound Effects (7 to start with), Music
Volume (1 to start with; Players is grayed out until the port has
two-player). Enhanced, new features beyond the original's, all off until
chosen: Background Gradient (the background darkens towards the bottom,
under the panel and playfield). Help: How to Play, Controls, About. Up / Down (or the mouse wheel) choose,
Left / Right change a setting, Enter (or A) picks, Esc (or B) goes back.
Messages (Exit?, Game Over) use the same popup. The options are saved as
you change them and come back next time.

**High scores.** As in the original, the top ten each keep a name and a
"cool quote". A game that makes the table asks for them, in the original's
own words ("Congrats! You have made the TETRIS Hall of Fame!"), then shows
"Tetris Experts" with the new entry highlighted. Type on the keyboard, or
on a controller change the last letter with Up / Down, add one with Right,
remove one with Left or B, and go on with A. Game > High Scores shows the
table (and can clear it). The first time, the table starts from the
original's own `TETRIS.HST` if it's beside your TETRIS.EXE (read, never
written).

**Where things are saved.** The options and the high scores are small
text files in your user folder for apps: `%APPDATA%\jgordon165\Tetrispack`
on Windows, `~/.local/share/jgordon165/Tetrispack` on Linux,
`~/Library/Application Support/jgordon165/Tetrispack` on macOS.

**Music.** Five pieces of 1.5 to 2.5 minutes, one per pair of levels so the music
changes with the background: *Korobeiniki* (the public-domain folk song behind Tetris,
arranged for the port), *Program Manager*, *Samovar*, *Midnight Desktop* and
*Steppe Express* (originals). Each is arranged game-style: the melody with a
harmony line, chip arpeggios, a moving bass line, drums with fills and
crashes. Options > Music Style plays them as a 1990s PC - or a console - would:

- **Windows FM MIDI** (the default): the songs as Windows 3.1/95/98's own
  .MID files sounded on a Sound Blaster 16 - a General MIDI band (clarinet,
  synth brass, accordion, vibraphone or distorted-guitar leads, strings,
  piano chords, fingered bass, a standard kit) as dry two-operator FM on
  the OPL3, panned hard left, centre and right. The FM patches are the
  port's own, in the spirit of the SB16 driver's bank.

- **Windows MIDI**: General MIDI on a GS
  SoundFont, the Roland Sound Canvas sound of Windows' MIDI playback -
  square lead, clarinet, fingered bass, strings, vibraphone, the GS drum
  kit, room reverb. The build downloads S. Christian Collins' GeneralUser
  GS (free to redistribute, pinned by hash) into `soundfont/`; any GS or
  GM .sf2 named `GeneralUser-GS.sf2` there works. TinySoundFont plays it.

- **Super NES**: eight sampled voices at 32 kHz with soft interpolation and
  the S-DSP's echo (the samples are synthesized by the port, not taken from
  any game).
- **Mega Drive**: a YM2612 (ymfm) for six four-operator FM voices, and an
  SN76489 PSG for square-wave arpeggios and noise drums.
- **Sound Blaster**: an OPL3 (ymfm), 1990s PC FM music.

Music picks By Level, one song, or Off; Music Volume sets the level. The
melodies are text in `scripts/music/songs.lua` (a small MML,
`scripts/music/mml.lua`), the accompaniment is generated from each song's
chords (`scripts/music/arrange.lua`), and every engine's instruments are in
`scripts/music/instruments.lua`. To hear them outside the game,
`tetrispack_render_music <scripts dir> [winfm|midi|snes|genesis|opl3]` writes a .wav
per song into the current folder.

**Sound effects.** A short mechanical "chk" with a small high bell when a
piece rotates, and a heavy layered thump with a fuller bell when it lands
(dropped or by gravity). Clearing rows chimes - three bells rising over a
crunchy whoosh, or for three rows or more a low boom under a five-bell run
and a shimmering top note. Losing plays the stack giving way: a crunchy
falling sweep under a slow minor bell fall to a long low C. The thumps are
bit-crushed for a chunky 16-bit texture, the bells left clean. Synthesized by the port
(`src/music/sfx.cpp`) and mixed over whichever music style is playing.
Options > Sound Effects sets their volume.

**Controller** (SDL game controllers; the layout the Xbox 360 build will
use): D-pad moves, Up or A rotates, Down or B drops, the shoulder buttons
slide to the wall, Start opens the menu, Back pauses.

Controls live in one table, `scripts/ui/input.lua`.

What comes from the original executable (layout, colours, timer per level,
scoring, the blink) is written up in `docs/original.md`.

## Consoles

The game also builds as console ROMs for the Genesis and the SNES, with the
rules rewritten in C (`consoles/common/`) because the Lua game can't run on
those machines. The ROMs use lookalike backgrounds of our own, so they can
be shared. A local build can bake in your TETRIS.EXE's art for your own use.

- **Sega Genesis / Mega Drive** (SGDK): see [consoles/genesis](consoles/genesis/README.md).
- **Super Nintendo** (PVSnesLib): see [consoles/snes](consoles/snes/README.md).
- **Xbox 360** (xsdk, `tetrispack.xex`): the same Lua game as the PC, on a
  small C++03 host. It's packaged as a folder with an empty `data/` for your
  own TETRIS.EXE. See [consoles/xbox360](consoles/xbox360/README.md).

## Layout

| Path | |
|---|---|
| `src/` | C++ host: window and drawing (`video.cpp`), input, the Lua API (`lua_api.cpp`) |
| `src/formats/` | The NE executable and its resources (`ne.cpp`), bitmaps (`dib.cpp`) |
| `scripts/` | The game in Lua: rules (`game/board.lua`), pieces, drawing (`game/skin.lua`), attract mode |
| `consoles/` | Console builds: the rules in C (`common/`), Genesis (`genesis/`), SNES (`snes/`), Xbox 360 (`xbox360/`), smoke tests (`tests/`) |
| `tests/` | Format tests on executables built in memory, and Lua rule tests |
| `re/`, `tools/` | Reverse-engineering workflow: identify, list and extract resources, decompile with Ghidra |
| `docs/formats.md` | NE and resource formats as the port reads them |
| `docs/original.md` | What the port takes from TETRIS.EXE 2.00: layout, colours, rules |

Screenshots for comparing with the original (not part of ctest):

```
TETRISPACK_SCRIPTS=scripts TETRISPACK_LEVEL=5 tetrispack --scripts tests/screens \
    --data <your TETRIS folder> --frames 600 --screenshot level5.bmp
```

## CI and releases

Same pipelines as Meguarace:

- **CI-build**: every push (and pull requests from forks) builds and tests
  on Linux (GCC and Clang), macOS (Intel and Apple silicon) and Windows
  (x64 and x86). Run it by hand from the Actions tab to pick platforms.
- **genesis** and **snes** (in CI-build): build the console ROMs (SGDK,
  PVSnesLib) and run them headless (MAME, the snes9x libretro core) to
  check that the attract demo plays and clears lines.
- **xbox360** (in CI-build): builds `tetrispack.xex` with xsdk
  (openpak/x360-sdk) on a hosted Linux runner, as Meguarace does. Its
  toolchain is cached after the first build. The package is the title in a
  folder with the scripts, music and sound effects, and an empty `data/`
  for your TETRIS.EXE. Its host also builds and runs headless in every CI
  job (the `xbox360_host` and `mixer360` tests).
- **code-analysis**: CodeQL (security-and-quality) on `master`.
- **release-linux / release-windows / release-mac-osx / release-genesis /
  release-snes / release-xbox360**,
  and **release-all** to run them all: build, test, package (no game data) and upload the
  archives as workflow artifacts, plus Windows `.pdb` symbols. macOS builds
  are universal and are signed and notarized when the `PROD_MACOS_*`
  secrets are set on the repository.

## Legal

Tetris is a trademark of The Tetris Company. This project is not affiliated
with The Tetris Company or Microsoft, and you need your own copy of the game.
