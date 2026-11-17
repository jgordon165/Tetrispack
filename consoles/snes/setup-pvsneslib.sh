#!/bin/sh
# Fetches PVSnesLib (the SNES devkit: 816-tcc, WLA-DX, SNESMOD; zlib licence),
# the Linux x64 release, checked against its SHA-256.
#
#   consoles/snes/setup-pvsneslib.sh <dir>
#
# Prints the line to set up a shell for consoles/snes/build.sh:
#   export PVSNESLIB_HOME=<dir>/pvsneslib
set -eu

VERSION=4.7.1
ZIP=pvsneslib_471_64b_linux_release.zip
SHA256=10c40a79322c3c93a9d38effada23117c040b5285cd8460954105f777cb2db33

dir=${1:?usage: setup-pvsneslib.sh <dir>}
mkdir -p "$dir"
dir=$(cd "$dir" && pwd)
if [ ! -f "$dir/.done-$VERSION" ]; then {
    curl -sSfL -o "$dir/$ZIP" "https://github.com/alekmaul/pvsneslib/releases/download/$VERSION/$ZIP"
    echo "$SHA256  $dir/$ZIP" | sha256sum -c -
    rm -rf "$dir/pvsneslib"
    (cd "$dir" && unzip -q "$ZIP" && rm "$ZIP")
    touch "$dir/.done-$VERSION"
} >&2; fi
echo "export PVSNESLIB_HOME=$dir/pvsneslib"
