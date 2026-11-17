#!/usr/bin/env python3
"""Drops code scanning results in third-party code from CodeQL's SARIF
before upload (.github/workflows/code-analysis.yml). C/C++ analysis covers
whatever the build compiles, so the fetched libraries (SDL2, Lua, ymfm,
TinySoundFont under .deps/) would otherwise report as ours.

  python3 -I tools/filter_sarif.py <sarif file or folder> <path prefix>...
"""
import json
import os
import sys


def third_party(result, prefixes):
    for loc in result.get("locations", []):
        uri = loc.get("physicalLocation", {}).get("artifactLocation", {}).get("uri", "")
        if uri.startswith(tuple(prefixes)):
            return True
    return False


def filter_file(path, prefixes):
    with open(path, encoding="utf-8") as f:
        sarif = json.load(f)
    dropped = kept = 0
    for run in sarif.get("runs", []):
        results = run.get("results", [])
        mine = [r for r in results if not third_party(r, prefixes)]
        dropped += len(results) - len(mine)
        kept += len(mine)
        run["results"] = mine
    with open(path, "w", encoding="utf-8") as f:
        json.dump(sarif, f)
    print(f"{path}: kept {kept}, dropped {dropped} in {', '.join(prefixes)}")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    target, prefixes = sys.argv[1], sys.argv[2:]
    files = [os.path.join(target, n) for n in sorted(os.listdir(target)) if n.endswith(".sarif")] \
        if os.path.isdir(target) else [target]
    if not files:
        sys.exit(f"no .sarif files in {target}")
    for path in files:
        filter_file(path, prefixes)


if __name__ == "__main__":
    main()
