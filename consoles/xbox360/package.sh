#!/bin/sh
# Packages the Xbox 360 title: tetrispack.xex in a folder with the content
# (stage-content.sh: scripts, music, sound effects, an empty data/ for the
# player's TETRIS.EXE), zipped.
#
#   consoles/xbox360/package.sh <tetrispack.xex> <out.zip> [style ...]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
xex=${1:?usage: package.sh <tetrispack.xex> <out.zip> [style ...]}
zip=${2:?usage: package.sh <tetrispack.xex> <out.zip> [style ...]}
shift 2
test -s "$xex" || { echo "no title at $xex" >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
"$here/stage-content.sh" "$work/tetrispack" "$@"
cp "$xex" "$work/tetrispack/tetrispack.xex"
mkdir -p "$(dirname "$zip")"
zip_abs=$(cd "$(dirname "$zip")" && pwd)/$(basename "$zip")
rm -f "$zip_abs"
(cd "$work" && zip -qr "$zip_abs" tetrispack)
ls -la "$zip_abs"
