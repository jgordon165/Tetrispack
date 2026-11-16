#!/usr/bin/env bash
# Import TETRIS.EXE into a local Ghidra project and run auto-analysis, then
# export decompiled C for every function into re/output/.
#
#   GHIDRA_HOME=/path/to/ghidra_11.x re/ghidra_headless.sh /path/to/your/TETRIS.EXE [processor]
#
# The Entertainment Pack's Tetris is a 16-bit Windows (NE) program; Ghidra's
# built-in NE loader takes it with the 16-bit protected-mode x86 processor.
# Output stays in re/ (git-ignored): decompiled original code is reference
# material for writing the port, not something to publish.
set -euo pipefail

EXE="${1:?usage: $0 <exe> [processor]}"
PROC="${2:-x86:LE:16:Protected Mode}"
HERE="$(cd "$(dirname "$0")" && pwd)"
: "${GHIDRA_HOME:?set GHIDRA_HOME to your Ghidra install}"

mkdir -p "$HERE/ghidra_project" "$HERE/output"

"$GHIDRA_HOME/support/analyzeHeadless" \
    "$HERE/ghidra_project" tetris \
    -import "$EXE" -overwrite \
    -processor "$PROC" \
    -scriptPath "$HERE/scripts" \
    -postScript ExportDecompiled.java "$HERE/output"

echo "decompiled C written to $HERE/output/"
