#!/bin/sh
# Smoke test for the SNES ROM: runs it headless in the snes9x libretro core
# (MAME's SNES needs Sony's SPC700 boot ROM, which can't be shipped) with no
# input, so the attract demo plays; saves snapshots and the sound, and fails
# unless the demo placed pieces and cleared lines (read from work RAM).
#
#   consoles/tests/snes_smoke.sh <rom> <symbols (.sym)> <out dir>
#
# Needs the core: apt install libretro-snes9x (or set SNES9X_CORE).
set -eu
rom=$1 sym=$2 out=$3
here=$(cd "$(dirname "$0")" && pwd)
core=${SNES9X_CORE:-/usr/lib/x86_64-linux-gnu/libretro/snes9x_libretro.so}
board=$(awk '$2=="board"{print substr($1,3,2)":"substr($1,5)}' "$sym")
[ -n "$board" ] || { echo "no 'board' symbol in $sym"; exit 1; }
# TcBoard on 816-tcc: its 32-bit score takes 6 bytes, so lines, level and
# placed sit at 212, 205 and 214 (210, 205, 212 on the 68000).
python3 -I "$here/libretro_run.py" "$core" "$rom" "$out" --frames 4500 \
    --snaps 300,1500,3000,4400 --board "$board" --offsets 212,205,214
lines=$(sed -n 's/.*lines=\([0-9]*\).*/\1/p' "$out/result.txt")
placed=$(sed -n 's/.*placed=\([0-9]*\).*/\1/p' "$out/result.txt")
if [ "${lines:-0}" -lt 5 ] || [ "${placed:-0}" -lt 20 ]; then
    echo "smoke test failed: the demo should have placed pieces and cleared lines"
    exit 1
fi
echo "smoke test passed"
