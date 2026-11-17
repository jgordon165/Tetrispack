#!/bin/sh
# Fetches and builds SGDK (the Mega Drive / Genesis devkit, MIT licence) for
# consoles/genesis/build.sh, on Linux, with the distribution's m68k cross
# compiler (Ubuntu / Debian: apt install gcc-m68k-linux-gnu; also Java for
# SGDK's resource compiler).
#
#   consoles/genesis/setup-sgdk.sh <dir>
#
# Leaves SGDK in <dir>/SGDK (library built) and m68k-elf-* wrappers in
# <dir>/bin, then prints the two lines to set up a shell for build.sh:
#   export GDK=<dir>/SGDK
#   export PATH=<dir>/bin:$PATH
set -eu

SGDK_COMMIT=ef9292c03fe33a2f8af3a2589ab856a53dcef35c   # v2.11
dir=${1:?usage: setup-sgdk.sh <dir>}
mkdir -p "$dir/bin"
dir=$(cd "$dir" && pwd)

# m68k-elf-* names for the m68k-linux-gnu tools, which SGDK's makefiles
# expect. The linker mustn't add a build-id note: it would land at address 0,
# over the 68000's vectors.
for t in ar as ld nm objcopy objdump; do
    ln -sf "$(command -v m68k-linux-gnu-$t)" "$dir/bin/m68k-elf-$t"
done
cat > "$dir/bin/m68k-elf-gcc" <<'WRAP'
#!/bin/sh
exec m68k-linux-gnu-gcc -Wl,--build-id=none -Wl,-z,noexecstack "$@"
WRAP
chmod +x "$dir/bin/m68k-elf-gcc"

# Build output goes to stderr: stdout is only the two export lines.
if [ ! -f "$dir/SGDK/.built-$SGDK_COMMIT" ]; then {
    rm -rf "$dir/SGDK"
    git init -q "$dir/SGDK"
    git -C "$dir/SGDK" fetch -q --depth 1 https://github.com/Stephane-D/SGDK.git "$SGDK_COMMIT"
    git -C "$dir/SGDK" checkout -q FETCH_HEAD
    # The two native helpers the library build uses (Z80 assembler, bin to
    # asm); the rest of SGDK's tools are Java.
    make -s -C "$dir/SGDK/tools/sjasm/src"
    cp "$dir/SGDK/tools/sjasm/src/sjasm" "$dir/SGDK/bin/"
    cc -O2 "$dir/SGDK/tools/bintos/src/bintos.c" -o "$dir/SGDK/bin/bintos"
    rm -f "$dir/SGDK/lib/libmd.a"
    PATH="$dir/bin:$dir/SGDK/bin:$PATH" make -s -f "$dir/SGDK/makelib.gen" GDK="$dir/SGDK" release
    touch "$dir/SGDK/.built-$SGDK_COMMIT"
} >&2; fi

echo "export GDK=$dir/SGDK"
echo "export PATH=$dir/bin:\$PATH"
