Tetrispack for the Xbox 360
===========================

Tetris for Windows 2.00 (Microsoft Entertainment Pack), as an Xbox 360
title: tetrispack.xex. The game is the PC port's Lua game; its art comes
from your own copy of the original, which is not included.

1. Copy your TETRIS.EXE into the data folder (and ABOUTTET.DLL for the
   About box, TETRIS.HST for the original's high scores, if you have them).
   Without TETRIS.EXE the game still plays, on a plain background.

     tetrispack/
       tetrispack.xex
       data/          <- TETRIS.EXE, ABOUTTET.DLL, TETRIS.HST
       scripts/
       music/
       sfx/

2. Copy the whole tetrispack folder to a USB stick or the hard drive of a
   console that runs unsigned code (BadUpdate + FreeMyXe, RGH or JTAG).

3. Launch tetrispack.xex from Aurora, a file manager or the dashboard.

Controls: D-pad or left stick moves (left and right repeat while held),
A rotates, B drops, Back pauses, the shoulders slide to the wall, Start
opens the menu.

Leaving: Menu > Exit to Dashboard goes to the console's default dashboard
(Aurora or FreeStyle, if DashLaunch makes it the default). To go somewhere
else, edit tetrispack.ini next to tetrispack.xex, for example

  exit = Usb0:\Aurora\default.xex

for a default.xex on the USB stick (Hdd:\ for the hard drive, Usb1:\ and
Usb2:\ for other USB drives). The menu then reads "Exit to Aurora". If the
.xex isn't there, Exit goes to the dashboard. tetrispack.log, next to the
game, says what it chose.

Aurora: the aurora folder has an icon (64x64) and a banner (420x96) for
the dashboard. The title's ID is 54500360; load the images for it with
Aurora's asset options or Aurora Asset Editor.

Source and issues: https://github.com/jgordon165/Tetrispack
