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

Without it the game still runs, with flat-coloured blocks, so the engine
and the rules can be worked on before the original's art is wired in.

| Key | |
|---|---|
| Enter / F2 | New game |
| Left / Right | Move |
| Up | Rotate |
| Down | Soft drop (hold) |
| Space | Drop |
| P / F3 | Pause |
| Esc | Back to the title (quits from the title) |

## Layout

| Path | |
|---|---|
| `src/` | C++ host: window and drawing (`video.cpp`), input, the Lua API (`lua_api.cpp`) |
| `src/formats/` | The NE executable and its resources (`ne.cpp`), bitmaps (`dib.cpp`) |
| `scripts/` | The game in Lua: rules (`game/board.lua`), pieces, drawing (`game/skin.lua`), attract mode |
| `tests/` | Format tests on executables built in memory, and Lua rule tests |
| `re/`, `tools/` | Reverse-engineering workflow: identify, list and extract resources, decompile with Ghidra |
| `docs/formats.md` | NE and resource formats as the port reads them |

Values in `scripts/game/board.lua` marked *verify* (scoring, gravity per
level, level-up rule) are placeholders until confirmed from the original;
`re/README.md` describes how.

## CI and releases

Same pipelines as Meguarace:

- **CI-build**: every push (and pull requests from forks) builds and tests
  on Linux (GCC and Clang), macOS (Intel and Apple silicon) and Windows
  (x64 and x86). Run it by hand from the Actions tab to pick platforms.
- **code-analysis**: CodeQL (security-and-quality) on `master`.
- **release-linux / release-windows / release-mac-osx**, and **release-all**
  to run all three: build, test, package (no game data) and upload the
  archives as workflow artifacts, plus Windows `.pdb` symbols. macOS builds
  are universal and are signed and notarized when the `PROD_MACOS_*`
  secrets are set on the repository.

## Legal

Tetris is a trademark of The Tetris Company. This project is not affiliated
with The Tetris Company or Microsoft, and you need your own copy of the game.
