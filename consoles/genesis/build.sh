#!/bin/sh
# Builds the Genesis ROM with SGDK.
#
#   consoles/genesis/build.sh <work dir> [--data <folder with TETRIS.EXE>]
#
# Needs GDK (an SGDK checkout with its library built), an m68k-elf- gcc on
# PATH (or PREFIX set), Java (rescomp), Python 3, and the PC build's
# tetrispack_render_music (RENDER_MUSIC, default build/tetrispack_render_music)
# to turn the soundtrack into VGM and the sound effects into WAV.
#
# The ROM lands in <work dir>/tetrispack-genesis.bin. With --data its
# backgrounds are your TETRIS.EXE's: that ROM is for you only, don't share it.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:?usage: build.sh <work dir> [--data <TETRIS folder>]}
shift
data=""
if [ "${1:-}" = "--data" ]; then data=${2:?--data needs a folder}; fi
: "${GDK:?set GDK to your SGDK folder}"
render=${RENDER_MUSIC:-$root/build/tetrispack_render_music}
render=$(cd "$(dirname "$render")" && pwd)/$(basename "$render")   # absolute: it runs from res/

rm -rf "$out/src" "$out/res" "$out/out"
mkdir -p "$out/src" "$out/res"
out=$(cd "$out" && pwd)
cp "$here"/src/*.c "$out/src/"
cp "$root"/consoles/common/tetris_core.c "$root"/consoles/common/tetris_core.h "$out/src/"
if [ -n "$data" ]; then
    python3 -I "$root/tools/console_art.py" --console genesis --out "$out/src" --data "$data"
else
    python3 -I "$root/tools/console_art.py" --console genesis --out "$out/src"
fi
cp "$here/res/resources.res" "$out/res/"
# The music tool writes into the current folder.
(cd "$out/res" && "$render" "$root/scripts" genesis-vgm && "$render" "$root/scripts" sfx-wav 13300)

cd "$out"
# SGDK's own libgcc: a distro m68k libgcc is built for the 68020.
PATH="$GDK/bin:$PATH" make -f "$GDK/makefile.gen" GDK="$GDK" LIBGCC="$GDK/lib/libgcc.a" release
cp out/rom.bin tetrispack-genesis.bin
echo "ROM: $out/tetrispack-genesis.bin ($(wc -c < tetrispack-genesis.bin) bytes)"
