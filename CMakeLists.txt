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
    src/formats/dib.cpp)
target_include_directories(tetrispack_formats PUBLIC src)
tetrispack_warnings(tetrispack_formats)

# ---- Engine ----------------------------------------------------------------
add_executable(tetrispack
    src/main.cpp
    src/video.cpp
    src/input.cpp
    src/lua_api.cpp)
target_link_libraries(tetrispack PRIVATE tetrispack_formats SDL2::SDL2-static SDL2::SDL2main lua_static)
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
