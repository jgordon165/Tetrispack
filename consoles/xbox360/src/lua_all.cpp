// Lua 5.4 in one translation unit, compiled as C++ for the Xbox 360 host
// (the XDK's compiler is C89 for C files; Lua compiles cleanly as C++).
// Laid out like Lua's own onelua.c, but without the io, os and debug
// libraries (the game doesn't use them, and the console's C library lacks
// some of what they call) and without loading DLLs. game.cpp opens the
// libraries itself.
#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf   // Visual C++ 2010 has only the underscored one
#endif

#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#include "luaconf.h"
#undef LUA_DL_DLL            // no DLLs: require() only finds the scripts

#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"
#include "lauxlib.c"
#include "lbaselib.c"
#include "lcorolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
