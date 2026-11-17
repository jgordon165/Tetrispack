cmake_minimum_required(VERSION 3.20)
project(Tetrispack VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TETRISPACK_TESTS "Build the test suite" ON)

# One static MSVC runtime for everything (SDL, Lua and the engine), so
# releases need no Visual C++ redistributable and nothing mismatches at link.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# ---- SDL2 (static) ---------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(SDL2)

# ---- Lua 5.4 (built from source; upstream has no CMake) ---------------------
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/(lua|luac|onelua|ltests)\\.c$")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})
set_target_properties(lua_static PROPERTIES LINKER_LANGUAGE C)
target_compile_options(lua_static PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/W0,-w>)   # third-party code
if(UNIX AND NOT APPLE)
    target_compile_definitions(lua_static PRIVATE LUA_USE_LINUX)
    target_link_libraries(lua_static PUBLIC m dl)
elseif(APPLE)
    target_compile_definitions(lua_static PRIVATE LUA_USE_MACOSX)
endif()

# ---- ymfm (Yamaha FM chip emulators, BSD-3; we use its OPL3 and YM2612) -----
# Pinned to a commit: the project has no release tags. SOURCE_SUBDIR points
# at a folder with no CMakeLists.txt, so only the sources are fetched (ymfm's
# own CMake would build every chip it has).
FetchContent_Declare(ymfm
    GIT_REPOSITORY https://github.com/aaronsgiles/ymfm.git
    GIT_TAG        81aec25ccbb98f4873a255f7551ac4dadac59b4a
    SOURCE_SUBDIR  sources-only)
FetchContent_MakeAvailable(ymfm)
add_library(ymfm_opl STATIC
    ${ymfm_SOURCE_DIR}/src/ymfm_opl.cpp
    ${ymfm_SOURCE_DIR}/src/ymfm_opn.cpp
    ${ymfm_SOURCE_DIR}/src/ymfm_ssg.cpp
    ${ymfm_SOURCE_DIR}/src/ymfm_adpcm.cpp
    ${ymfm_SOURCE_DIR}/src/ymfm_pcm.cpp)
target_include_directories(ymfm_opl SYSTEM PUBLIC ${ymfm_SOURCE_DIR}/src)
target_compile_options(ymfm_opl PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)   # third-party code

# ---- TinySoundFont (SoundFont synth, MIT; header-only) -----------------------
# The Windows MIDI music style. Pinned to a commit (no release tags).
FetchContent_Declare(tinysoundfont
    GIT_REPOSITORY https://github.com/schellingb/TinySoundFont.git
    GIT_TAG        853a0a171759f1ddba0de1442133a75912bbeffa)
FetchContent_GetProperties(tinysoundfont)
if(NOT tinysoundfont_POPULATED)
    FetchContent_Populate(tinysoundfont)
endif()
add_library(tinysoundfont STATIC src/music/tsf_impl.cpp)
target_include_directories(tinysoundfont SYSTEM PUBLIC ${tinysoundfont_SOURCE_DIR})
target_compile_options(tinysoundfont PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)   # third-party code

# ---- GeneralUser GS (the bundled SoundFont) ---------------------------------
# S. Christian Collins' GS / General MIDI bank (free to use and redistribute,
# see its LICENSE.txt), copied next to the game as soundfont/. Pinned to a
# commit and checked by hash, as in Meguarace. Off: the Windows MIDI style
# needs a .sf2 put in soundfont/ by hand.
option(TETRISPACK_FETCH_SOUNDFONT "Download the GeneralUser GS SoundFont next to the game" ON)
if(TETRISPACK_FETCH_SOUNDFONT)
    set(gugs_commit 684543d5e5efaef08d02be50dcda8d552478fa60)
    set(gugs_dir ${CMAKE_BINARY_DIR}/_soundfont)
    if(NOT EXISTS ${gugs_dir}/GeneralUser-GS.sf2)
        message(STATUS "Downloading the GeneralUser GS SoundFont (32 MB)")
        file(DOWNLOAD https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/${gugs_commit}/GeneralUser-GS.sf2
             ${gugs_dir}/GeneralUser-GS.sf2.part
             EXPECTED_HASH SHA256=9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe
             STATUS gugs_status)
        list(GET gugs_status 0 gugs_code)
        if(gugs_code EQUAL 0)
            file(RENAME ${gugs_dir}/GeneralUser-GS.sf2.part ${gugs_dir}/GeneralUser-GS.sf2)
        else()
            message(WARNING "GeneralUser GS download failed (${gugs_status}); the Windows MIDI style needs a .sf2 added by hand")
        endif()
    endif()
    if(NOT EXISTS ${gugs_dir}/LICENSE.txt)
        file(DOWNLOAD https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/${gugs_commit}/documentation/LICENSE.txt
             ${gugs_dir}/LICENSE.txt STATUS gugs_status)
    endif()
endif()

# ---- Warnings ----------------------------------------------------------------
function(tetrispack_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# ---- File formats (no SDL; shared by the engine and the tests) ---------------
# The 16-bit Windows (NE) executable and the resources inside it: bitmaps,
# icons, string tables, menus and dialogs, read from the player's TETRIS.EXE.
add_library(tetrispack_formats STATIC
    src/formats/binreader.cpp
    src/formats/ne.cpp
    src/formats/dib.cpp
    src/formats/dialog.cpp)
target_include_directories(tetrispack_formats PUBLIC src)
tetrispack_warnings(tetrispack_formats)

# ---- Music (no SDL; shared by the engine and the tests) ---------------------
add_library(tetrispack_music STATIC
    src/music/player.cpp
    src/music/backend.cpp
    src/music/engine_opl3.cpp
    src/music/engine_genesis.cpp
    src/music/engine_snes.cpp
    src/music/engine_midi.cpp
    src/music/sfx.cpp
    src/music/song_lua.cpp
    src/music/vgm.cpp
    src/music/itwriter.cpp)
target_include_directories(tetrispack_music PUBLIC src)
target_link_libraries(tetrispack_music PUBLIC ymfm_opl tinysoundfont lua_static)
tetrispack_warnings(tetrispack_music)

# Renders the songs to .wav, to listen to them outside the game.
add_executable(tetrispack_render_music tools/render_music.cpp)
target_link_libraries(tetrispack_render_music PRIVATE tetrispack_music)
tetrispack_warnings(tetrispack_render_music)

# ---- Engine ----------------------------------------------------------------
add_executable(tetrispack
    src/main.cpp
    src/video.cpp
    src/input.cpp
    src/audio.cpp
    src/lua_api.cpp)
target_link_libraries(tetrispack PRIVATE tetrispack_formats tetrispack_music SDL2::SDL2-static SDL2::SDL2main lua_static)
tetrispack_warnings(tetrispack)
if(MSVC)
    # Debugging symbols for Release too (uploaded separately by the release workflow).
    target_compile_options(tetrispack PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(tetrispack PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()

# Copy Lua scripts next to the binary on every build (not only when the C++
# changes), so script edits are picked up by a plain rebuild.
add_custom_target(copy_scripts ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:tetrispack>/scripts
    COMMENT "Copying Lua scripts")
add_dependencies(copy_scripts tetrispack)

# The bundled SoundFont next to the binary, as the package has it.
if(TETRISPACK_FETCH_SOUNDFONT AND EXISTS ${gugs_dir}/GeneralUser-GS.sf2)
    add_custom_target(copy_soundfont ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:tetrispack>/soundfont
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gugs_dir}/GeneralUser-GS.sf2 ${gugs_dir}/LICENSE.txt
                $<TARGET_FILE_DIR:tetrispack>/soundfont
        COMMENT "GeneralUser GS next to the game")
    add_dependencies(copy_soundfont tetrispack)
    install(FILES ${gugs_dir}/GeneralUser-GS.sf2 ${gugs_dir}/LICENSE.txt DESTINATION soundfont COMPONENT game)
endif()

# ---- The Xbox 360 host, headless ---------------------------------------------
# consoles/xbox360 runs the Lua game on a C++03 host for the XDK's Visual
# C++ 2010; everything but its Direct3D / XInput / XAudio2 layer also builds
# here, held to C++98/03 on GCC and Clang, and runs in the tests.
add_executable(tetrispack360_headless
    consoles/xbox360/src/canvas.cpp
    consoles/xbox360/src/formats360.cpp
    consoles/xbox360/src/game.cpp
    consoles/xbox360/src/lua_all.cpp
    consoles/xbox360/src/platform_headless.cpp)
target_include_directories(tetrispack360_headless PRIVATE src ${lua_SOURCE_DIR})
if(NOT MSVC)
    set_target_properties(tetrispack360_headless PROPERTIES CXX_STANDARD 98 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    set_source_files_properties(consoles/xbox360/src/lua_all.cpp PROPERTIES COMPILE_OPTIONS "-w")
else()
    set_source_files_properties(consoles/xbox360/src/lua_all.cpp PROPERTIES COMPILE_OPTIONS "/W0")
endif()
tetrispack_warnings(tetrispack360_headless)

# ---- Tests -------------------------------------------------------------------
# No game data is in the repository, so CI runs only the data-free tests.
# Configure with TETRISPACK_DATA=<folder with TETRIS.EXE> in the environment
# to also run the tests against the real game file.
if(TETRISPACK_TESTS)
    enable_testing()

    add_executable(tetrispack_tests tests/test_formats.cpp)
    target_link_libraries(tetrispack_tests PRIVATE tetrispack_formats)
    tetrispack_warnings(tetrispack_tests)
    add_test(NAME formats COMMAND tetrispack_tests)

    add_executable(tetrispack_music_tests tests/test_music.cpp)
    target_link_libraries(tetrispack_music_tests PRIVATE tetrispack_music)
    tetrispack_warnings(tetrispack_music_tests)
    add_test(NAME music COMMAND tetrispack_music_tests)
    # The console builds' C rules (consoles/), on the host.
    add_executable(tetrispack_console_core_tests tests/test_console_core.c consoles/common/tetris_core.c)
    target_include_directories(tetrispack_console_core_tests PRIVATE consoles/common)
    tetrispack_warnings(tetrispack_console_core_tests)
    add_test(NAME console_core COMMAND tetrispack_console_core_tests)

    if(DEFINED ENV{TETRISPACK_DATA})
        set_tests_properties(formats PROPERTIES ENVIRONMENT "TETRISPACK_DATA=$ENV{TETRISPACK_DATA}")
    endif()

    set(lua_test_args --scripts ${CMAKE_SOURCE_DIR}/tests/lua --frames 1)
    if(DEFINED ENV{TETRISPACK_DATA})
        list(APPEND lua_test_args --data $ENV{TETRISPACK_DATA})
    endif()
    add_test(NAME lua COMMAND tetrispack ${lua_test_args})
    set_tests_properties(lua PROPERTIES ENVIRONMENT
        "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;TETRISPACK_SCRIPTS=${CMAKE_SOURCE_DIR}/scripts")

    # The Xbox 360 host plays: past the About box, a game at level 5, a turn
    # and a drop (no game data here, so the plain background).
    add_test(NAME xbox360_host COMMAND tetrispack360_headless --frames 600
             --keys "30:pad a,40:5,50:f2,200:pad dpup,300:pad dpdown,420:pad dpdown"
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(xbox360_host PROPERTIES PASS_REGULAR_EXPRESSION "sfx place: [1-9]")

    # The shipped game must at least boot (and play its attract mode) without data.
    add_test(NAME boot COMMAND tetrispack --data ${CMAKE_BINARY_DIR}/no-such-folder --frames 120)
    set_tests_properties(boot PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
endif()

# ---- Install / packaging -----------------------------------------------------
# tetrispack(.exe), scripts/, README.md and docs/. No game data, ever.
install(TARGETS tetrispack RUNTIME DESTINATION . COMPONENT game)
install(DIRECTORY scripts DESTINATION . COMPONENT game)
install(FILES README.md DESTINATION . COMPONENT game)
install(DIRECTORY docs DESTINATION . COMPONENT game)
