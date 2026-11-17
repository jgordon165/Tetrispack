# Tetrispack for the Xbox 360

Tetris for Windows 2.00 as an Xbox 360 executable (`default.xex`). It is the
same game as on the PC: the Lua scripts, with the art read at runtime from
your own `TETRIS.EXE`. They run on a small C++03 host built with the Xbox
360 XDK.

- **Video**: the 640x440 canvas is drawn in software, then shown with
  Direct3D 9 at 1280x720, letterboxed, with crisp pixels.
- **Pad** (XInput): the controls are the PC game's controller mapping
  (`scripts/ui/input.lua`).
  - D-pad or left stick moves; holding left or right repeats.
  - A rotates, B drops, Back pauses, the shoulders slide to the wall.
  - Start opens the menu.
- **Music**: the songs are rendered ahead of time and looped (XAudio2): the
  default Windows FM MIDI style, at 32 kHz. Other styles can be staged too
  (`stage-content.sh <dir> winfm snes`, about 80 MB each).
- **Sound effects**: the PC game's.
- **Intro**: the About box from your `ABOUTTET.DLL`, as on the PC.

## Running it

The package is a folder to copy to the console (Xbox 360 homebrew needs a
console that runs unsigned code):

```
Tetrispack360\
  default.xex
  data\        <- copy your TETRIS.EXE (and ABOUTTET.DLL) here
  scripts\
  music\
  sfx\
```

Without `TETRIS.EXE` it still plays, on a plain background.

## Building

`default.xex` needs Visual Studio 2010 and the Xbox 360 XDK. Neither is part
of this repository, and they can't run on GitHub's hosted runners.

- **Your own runner**: register a self-hosted Windows runner with the labels
  `self-hosted`, `windows` and `xdk`, with the XDK installed (`XEDK` set).
- **Turn on the jobs**: set the repository variable `XBOX360_RUNNER` to
  `true`. CI-build's `xbox360` job then builds the executable, and
  release-xbox360 packages it.
- **Content**: the scripts, music and sound effects are staged on Linux by
  `consoles/xbox360/stage-content.sh`.

By hand, on the XDK machine (after the PC build has fetched Lua into
`.deps/lua-src`, or with `LuaDir` pointing at Lua 5.4.7):

```
MSBuild consoles\xbox360\Tetrispack360.vcxproj /p:Configuration=Release "/p:Platform=Xbox 360"
```

## How it's put together

| File | |
|---|---|
| `src/host.h`, `src/game.cpp` | The Lua game and the `tp` API (as `src/lua_api.cpp` on the PC) |
| `src/canvas.*` | Software drawing: fills, the two fonts, bitmaps |
| `src/formats360.*` | TETRIS.EXE / ABOUTTET.DLL: NE resources, bitmaps, dialogs |
| `src/lua_all.cpp` | Lua 5.4 as one C++ unit, without the io / os / debug libraries |
| `src/platform_x360.cpp` | Direct3D 9, XInput, XAudio2 (XDK only) |
| `src/platform_headless.cpp` | The same host on a PC, no window or sound, for tests |

Everything but `platform_x360.cpp` and `lua_all.cpp` builds on a PC as
C++98/03, linking the PC build's Lua (`tetrispack360_headless`, run from a content folder or the repository's
root). The `xbox360_host` test runs it: it starts a game, turns and drops
pieces, and checks the sound effects fire.
