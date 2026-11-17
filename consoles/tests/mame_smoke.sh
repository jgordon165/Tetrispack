#!/bin/sh
# Smoke test for a console ROM: boots it in MAME with no input, so the title
# screen's attract demo plays by itself, saves snapshots and the sound, and
# fails unless the demo cleared lines (read from the game's RAM).
#
#   consoles/tests/mame_smoke.sh <system> <rom> <out dir> <board address> <offsets> <endian>
#
#   system         MAME driver: genesis or snes
#   board address  RAM address of the game's TcBoard, hex (from the ROM's symbols)
#   offsets        lines,level,placed offsets in TcBoard
#   endian         big (68000) or little (65816)
set -eu
system=$1 rom=$2 out=$3 board=$4 offsets=$5 endian=$6
here=$(cd "$(dirname "$0")" && pwd)
mame=${MAME:-mame}
command -v "$mame" >/dev/null 2>&1 || mame=/usr/games/mame
mkdir -p "$out"
rm -rf "$out/snap" "$out/result.txt"
case $system in
    genesis) pad=":ctrl1:mdpad:PAD" ;;
    *) pad=":ctrl1:joypad:JOY" ;;
esac
SDL_AUDIODRIVER=dummy TETRISPACK_DEMO=1 TETRISPACK_SNAPS="300,1500,3000,4200" \
TETRISPACK_PAD=$pad TETRISPACK_BOARD=$board TETRISPACK_OFFSETS=$offsets TETRISPACK_ENDIAN=$endian \
TETRISPACK_RESULT="$out/result.txt" \
    "$mame" "$system" -cart "$rom" -seconds_to_run 75 -video none -nothrottle -skip_gameinfo \
    -snapshot_directory "$out/snap" -autoboot_script "$here/mame_play.lua" \
    -sound sdl -wavwrite "$out/sound.wav" > "$out/mame.log" 2>&1 || { cat "$out/mame.log"; exit 1; }
cat "$out/result.txt"
lines=$(sed -n 's/.*lines=\([0-9]*\).*/\1/p' "$out/result.txt")
placed=$(sed -n 's/.*placed=\([0-9]*\).*/\1/p' "$out/result.txt")
if [ "${lines:-0}" -lt 5 ] || [ "${placed:-0}" -lt 20 ]; then
    echo "smoke test failed: the demo should have placed pieces and cleared lines"
    exit 1
fi
echo "smoke test passed"
