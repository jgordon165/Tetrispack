#!/bin/sh
# Stages the Xbox 360 build's content folder (what goes next to default.xex):
# the game's scripts, the songs rendered ahead of time, the sound effects,
# and an empty data\ folder for the player's own TETRIS.EXE.
#
#   consoles/xbox360/stage-content.sh <out dir> [style ...]
#
# Styles default to winfm (Windows FM MIDI, the game's default); each one
# adds about 80 MB of 32 kHz PCM. Needs the PC build's
# tetrispack_render_music (RENDER_MUSIC, default build/tetrispack_render_music).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:?usage: stage-content.sh <out dir> [style ...]}
shift
styles=${*:-winfm}
render=${RENDER_MUSIC:-$root/build/tetrispack_render_music}

rm -rf "$out"
mkdir -p "$out/music" "$out/sfx" "$out/data"
cp -R "$root/scripts" "$out/scripts"
for s in $styles; do
    "$render" "$root/scripts" "$out/music" loop-wav "$s" 32000
done
"$render" "$root/scripts" "$out/sfx" sfx-wav 44100
cp "$here/README.md" "$out/README.md"
cat > "$out/data/PUT-TETRIS-EXE-HERE.txt" <<'TXT'
Copy your own TETRIS.EXE (and ABOUTTET.DLL, for the About box) from the
Microsoft Entertainment Pack into this folder. Without them the game
still plays, on a plain background.
TXT
echo "content: $out ($(du -sh "$out" | cut -f1))"
