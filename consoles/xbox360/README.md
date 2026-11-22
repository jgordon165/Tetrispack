# Tetrispack for the Xbox 360

Tetris for Windows 2.00 as an Xbox 360 title, `tetrispack.xex`. It is the
same game as on the PC: the Lua scripts, with the art read at runtime from
your own `TETRIS.EXE`. They run on a small C++20 host built with
[xsdk](https://github.com/openpak/x360-sdk), the open-source Xbox 360 SDK,
the way Meguarace builds its 360 title. No Microsoft XDK is involved.

- **Video**: the 640x440 canvas is drawn in software. It's scaled to the
  TV's 720 lines with crisp pixels and black bars at the sides, then flipped
  at vblank through xsdk's presenter.
- **Pad**: the controls are the PC game's controller mapping
  (`scripts/ui/input.lua`). Any connected pad works.
  - D-pad or left stick moves; holding left or right repeats.
  - A rotates, B drops, Back pauses, the shoulders slide to the wall.
  - Start opens the menu.
- **Music**: synthesized as it plays, in the Super NES style, the only
  style the 360 offers. It uses the PC's own music player and SNES engine
  (`src/music`: sample voices, the SNES echo) on the audio feeder thread, so
  the package has no song files and is under 1 MB besides the title. The
  songs were rendered ahead of time before, about 77 MB.
- **Sound effects**: the PC game's.
  - The mixer is `src/mixer360.cpp`. It resamples to 48 kHz on a feeder
    thread.
  - The kernel's render driver plays the result.
- **Intro**: the About box from your `ABOUTTET.DLL`, as on the PC.

## Running it

The package is a folder wrapped around the title. The game's data isn't in
it: you supply that yourself.

```
tetrispack/
  tetrispack.xex
  data/        <- copy your TETRIS.EXE (and ABOUTTET.DLL, TETRIS.HST) here
  scripts/
  sfx/
  aurora/      icon.png, banner.png for the Aurora dashboard
  tetrispack.ini   where Menu > Exit goes
  README.txt
```

1. Copy the folder to a USB stick or the console's hard drive. The console
   must run unsigned code (BadUpdate + FreeMyXe, RGH or JTAG).
2. Launch `tetrispack.xex` from Aurora, a file manager or the dashboard.
3. To leave, choose Menu > Exit to Dashboard. The title stops its sound and
   hands back the display. By default it then ends with
   `XamLoaderTerminateTitle`, and the console goes to its default dashboard,
   which is Aurora or FreeStyle if DashLaunch has made it the default.

### Where Exit goes

Exit can also go to a dashboard or app of your choosing. Set it in
`tetrispack.ini`, next to `tetrispack.xex`:

```ini
[xbox360]
exit = Usb0:\Aurora\default.xex   ; or: dashboard (the default)
; exit_name = Aurora                ; the menu's "Exit to ..."
```

- **Drives:** `Usb0:\` (also `Usb:\`), `Usb1:\` and `Usb2:\` are the USB
  drives, `Hdd:\` is the hard drive, and `game:\` is Tetrispack's own
  folder (`game:\..\` is the one above it). A `\Device\...` path is used as
  written.
- **Menu label:** it names the .xex's folder, so the item reads "Exit to
  Aurora", unless `exit_name` says otherwise.
- **Launching:** Exit stops the sound, hands back the display and GPU, then
  starts the .xex with XAM's `XamLoaderLaunchTitle`.
- **Fallback:** if the setting is wrong or the .xex isn't there, Exit goes to
  the dashboard. The log says why.

The parsing is `src/exit360.cpp`, tested by `exit360` and
`xbox360_exit_to`.

### Aurora art

The package's `aurora/` folder has an icon (64x64) and a banner (420x96),
at the sizes Aurora Asset Editor asks for. Both are drawn by
`tools/make_aurora.py` in the style of the app icon.

- The title's ID is **54500360**. xsdk gives every title 5853444B, so
  without its own ID Aurora would mix up Tetrispack's art with other xsdk
  titles'.
- To show the art, give Aurora the images for that title. You can use
  Aurora's own asset options for the game, or Aurora Asset Editor on a PC,
  loading `icon.png` as the Icon and `banner.png` as the Banner.

Without `TETRIS.EXE` it still plays, on a plain background. The title logs
to `tetrispack.log` next to the .xex (xsdk's log, named after the title); a Lua error is also shown on screen.

## Building

CI-build's `xbox360` job builds it on GitHub's Linux runners, and
release-xbox360 does the same for releases
(`.github/actions/build-xbox360`). The xsdk commit is pinned. Its
toolchain (GCC, binutils, newlib for the console's PowerPC) is built once,
in about an hour, and then cached.

By hand on Linux, with xsdk checked out in `xsdk/`:

```
JOBS=$(nproc) bash xsdk/tools/toolchain/build.sh        # once: ~/xsdk-gcc
make -C xsdk build/libxsdk.a
cmake -S consoles/xbox360 -B build-xex -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/xsdk/cmake/xsdk.cmake -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc
cmake --build build-xex                                  # build-xex/tetrispack.xex
cmake --build build --target tetrispack_render_music     # the PC build's music tool
consoles/xbox360/package.sh build-xex/tetrispack.xex dist/tetrispack-xbox360.zip
```

## How it's put together

| File | |
|---|---|
| `CMakeLists.txt` | The title's own project (xsdk's toolchain file; Lua 5.4.7 as C) |
| `src/host.h`, `src/game.cpp` | The Lua game and the `tp` API (as `src/lua_api.cpp` on the PC) |
| `src/canvas.*` | Software drawing: fills, the two fonts, bitmaps |
| `src/formats360.*` | TETRIS.EXE / ABOUTTET.DLL: NE resources, bitmaps, dialogs |
| `src/mixer360.*` | The sound effects (WAVE files) and the live music, mixed |
| `../../src/music` | The music player and Super NES engine, shared with the PC (`backend_snes.cpp` offers only that engine) |
| `src/platform_xsdk.cpp` | The console: presenter, pads, sound driver, `game:/` files |
| `src/platform_headless.cpp` | The same host on a PC, no window or sound, for tests |
| `stage-content.sh`, `package.sh` | The folder around the title, zipped |
| `aurora/` | The Aurora dashboard's icon and banner (`tools/make_aurora.py`) |

Everything but `platform_xsdk.cpp` also builds on a PC. It links the PC
build's Lua and the music library `tetrispack360_music`.

- `tetrispack360_headless` runs it from a content folder or the
  repository's root.
- The `xbox360_host` test starts a game, turns and drops pieces, and checks
  that the sound effects fire.
- The `mixer360` test checks the mixer.
- The `xbox360_music` test plays a game and checks that its synthesized
  music is heard.

Options and high scores aren't saved on the console yet.
