#!/bin/sh
# Stages the Xbox 360 build's content folder (what goes next to tetrispack.xex):
# the game's scripts, the sound effects, and an empty data\ folder for the
# player's own TETRIS.EXE. No songs: the title synthesizes them live, Super
# NES style. Needs the PC build's tetrispack_render_music for the effects
# (RENDER_MUSIC, default build/tetrispack_render_music).
#
#   consoles/xbox360/stage-content.sh <out dir>
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:?usage: stage-content.sh <out dir>}
render=${RENDER_MUSIC:-$root/build/tetrispack_render_music}
render=$(cd "$(dirname "$render")" && pwd)/$(basename "$render")   # absolute: it runs from the content folders

rm -rf "$out"
mkdir -p "$out/sfx" "$out/data"
out=$(cd "$out" && pwd)
cp -R "$root/scripts" "$out/scripts"
# The music tool writes into the current folder.
(cd "$out/sfx" && "$render" "$root/scripts" sfx-wav 44100)
cp "$here/README-360.txt" "$out/README.txt"
# Where Menu > Exit goes (the dashboard unless changed)
cp "$here/tetrispack.ini" "$out/tetrispack.ini"
# Art for the Aurora dashboard (tools/make_aurora.py)
mkdir -p "$out/aurora"
cp "$here/aurora/icon.png" "$here/aurora/banner.png" "$out/aurora/"
cp "$here/README-360.txt" "$out/data/PUT-TETRIS-EXE-HERE.txt"
echo "content: $out ($(du -sh "$out" | cut -f1))"
