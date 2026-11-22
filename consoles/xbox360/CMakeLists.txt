# The Xbox 360 title, tetrispack.xex, built with xsdk (openpak/x360-sdk) as
# Meguarace does: no Microsoft XDK, just GCC for the console's PowerPC.
#
#   make -C <xsdk> build/libxsdk.a
#   cmake -S consoles/xbox360 -B build-xex -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc
#   cmake --build build-xex
#
# Then package.sh puts tetrispack.xex in a folder with the scripts, the
# music and sound effects, and an empty data/ for the player's own files.
cmake_minimum_required(VERSION 3.20)
project(Tetrispack360 VERSION 0.1.0 LANGUAGES C CXX)

if(NOT XSDK)
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake "
                        "(the PC build of this host is tetrispack360_headless, in the top-level project)")
endif()

set(repo ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# ---- Lua 5.4, as the PC build has it --------------------------------------------
include(FetchContent)
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(lua)
# Only what the game opens (game.cpp): no io, os or debug library, and no
# linit (the host opens its own list).
file(GLOB lua_sources ${lua_SOURCE_DIR}/*.c)
list(FILTER lua_sources EXCLUDE REGEX ".*/(lua|luac|onelua|ltests|liolib|loslib|ldblib|linit)\\.c$")
add_library(lua360 STATIC ${lua_sources})
target_include_directories(lua360 PUBLIC ${lua_SOURCE_DIR})
target_compile_options(lua360 PRIVATE -w)   # third-party code

# newlib's <limits.h> leaves LLONG_MAX out of C++ here, and luaconf.h then
# refuses 64-bit integers; C sees it, so give C++ the same (as Meguarace does).
add_compile_definitions(
    $<$<COMPILE_LANGUAGE:CXX>:LLONG_MAX=__LONG_LONG_MAX__>
    "$<$<COMPILE_LANGUAGE:CXX>:LLONG_MIN=(-__LONG_LONG_MAX__-1LL)>"
    "$<$<COMPILE_LANGUAGE:CXX>:ULLONG_MAX=(__LONG_LONG_MAX__*2ULL+1ULL)>")

# ---- the title -------------------------------------------------------------------
add_executable(tetrispack
    src/canvas.cpp
    src/formats360.cpp
    src/game.cpp
    src/mixer360.cpp
    src/platform_xsdk.cpp)
target_include_directories(tetrispack PRIVATE ${repo}/src)   # the fonts
target_link_libraries(tetrispack PRIVATE lua360)
target_compile_options(tetrispack PRIVATE -Wall -Wextra)
# The console's binary: xsdk links its start-up and library, then packs tetrispack.xex.
xsdk_add_xex(tetrispack)
# Packed again with a title ID of its own: xsdk gives every title 5853444B
# ("XSDK"), and dashboards keep art and stats by title ID, so Aurora would
# show one title's banner on every xsdk title. "TP" and 0360.
set(TETRISPACK_TITLE_ID 54500360 CACHE STRING "The title ID in tetrispack.xex (hex)")
add_custom_command(TARGET tetrispack POST_BUILD
    COMMAND python3 "${XSDK_ROOT}/tools/elf2xex.py" --title-id ${TETRISPACK_TITLE_ID}
            "$<TARGET_FILE:tetrispack>" "$<TARGET_FILE_DIR:tetrispack>/tetrispack.xex"
    VERBATIM)
