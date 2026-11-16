// Tetrispack: C++ host for a port of Tetris from the Microsoft Entertainment
// Pack (1990, Windows 3.x). Owns the window and input, and runs the game
// logic written in Lua (scripts/). The original's art and text are read at
// runtime from the player's own TETRIS.EXE; nothing from it is compiled in
// or shipped.
//
// Usage: tetrispack --data <folder with TETRIS.EXE, or the file>
//                   [--scale N] [--scripts <dir>] [--frames N]   (N: headless testing)

#include "engine.h"
#include "input.h"
#include "lua_api.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

bool Engine::loadData() {
    std::error_code ec;
    fs::path p = dataArg;
    if (fs::is_directory(p, ec)) {
        // Case-insensitive: TETRIS.EXE, Tetris.exe, ... (also one level down,
        // e.g. an Entertainment Pack install folder holding WEP/).
        auto findIn = [](const fs::path& dir) -> fs::path {
            std::error_code e2;
            for (const auto& ent : fs::directory_iterator(dir, e2)) {
                std::string n = ent.path().filename().string();
                for (auto& c : n) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (n == "TETRIS.EXE" && ent.is_regular_file(e2)) return ent.path();
            }
            return {};
        };
        fs::path found = findIn(p);
        if (found.empty())
            for (const auto& ent : fs::directory_iterator(p, ec))
                if (ent.is_directory(ec) && !(found = findIn(ent.path())).empty()) break;
        p = found;
    }
    if (p.empty() || !fs::is_regular_file(p, ec)) {
        dataError = "TETRIS.EXE not found in " + dataArg;
        return false;
    }
    try {
        exe = tp::NeFile::load(p.string());
        exePath = p.string();
        return true;
    } catch (const std::exception& ex) {
        dataError = p.string() + ": " + ex.what();
        return false;
    }
}

static bool callLua(lua_State* L, const char* fn, int nargs) {
    lua_getglobal(L, "game");
    if (!lua_istable(L, -1)) { lua_pop(L, 1 + nargs); return true; }
    lua_getfield(L, -1, fn);
    lua_remove(L, -2);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1 + nargs); return true; }
    lua_insert(L, -1 - nargs);
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "lua error in game.%s: %s\n", fn, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static void addPackagePath(lua_State* L, const std::string& dir) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string path = lua_tostring(L, -1);
    lua_pop(L, 1);
    path = dir + "/?.lua;" + dir + "/?/init.lua;" + path;
    lua_pushstring(L, path.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

int main(int argc, char** argv) {
    Engine eng;
    std::string scripts;
    int scale = 2;
    long maxFrames = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if      (a == "--data")    eng.dataArg = next();
        else if (a == "--scripts") scripts = next();
        else if (a == "--scale")   scale = std::max(1, std::atoi(next().c_str()));
        else if (a == "--frames")  maxFrames = std::atol(next().c_str());
        else if (a == "--help" || a == "-h") {
            std::printf("tetrispack --data <folder with your TETRIS.EXE, or the file> [--scale N]\n");
            return 0;
        }
    }
    if (scripts.empty()) {
        char* base = SDL_GetBasePath();
        scripts = (base ? std::string(base) : std::string("./")) + "scripts";
        SDL_free(base);
    }
    if (eng.dataArg.empty()) eng.dataArg = "data";
    if (!eng.loadData()) std::fprintf(stderr, "%s (playing without the original's art)\n", eng.dataError.c_str());

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (!eng.video.open("Tetrispack", 320, 240, scale)) { SDL_Quit(); return 1; }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    registerLuaApi(L, &eng);
    if (const char* extra = std::getenv("TETRISPACK_SCRIPTS")) addPackagePath(L, extra);
    addPackagePath(L, scripts);
    const std::string mainLua = scripts + "/main.lua";
    if (luaL_dofile(L, mainLua.c_str()) != LUA_OK) {
        std::fprintf(stderr, "lua: %s\n", lua_tostring(L, -1));
        lua_close(L);
        SDL_Quit();
        return 1;
    }

    const bool headless = maxFrames >= 0;
    bool ok = true;
    Uint64 last = SDL_GetPerformanceCounter();
    for (long frame = 0; ok && !eng.quit && (!headless || frame < maxFrames); ++frame) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) eng.quit = true;
            else if (ev.type == SDL_KEYDOWN) {
                lua_pushstring(L, input::keyName(ev.key.keysym.sym).c_str());
                lua_pushboolean(L, ev.key.repeat != 0);
                ok = callLua(L, "keypressed", 2) && ok;
            }
        }
        // Headless runs step a fixed 1/60 s so tests are deterministic.
        const Uint64 now = SDL_GetPerformanceCounter();
        double dt = headless ? 1.0 / 60.0 : double(now - last) / double(SDL_GetPerformanceFrequency());
        last = now;
        dt = std::min(dt, 0.1);
        lua_pushnumber(L, dt);
        ok = callLua(L, "update", 1) && ok;
        ok = callLua(L, "draw", 0) && ok;
        eng.video.present();
    }

    lua_close(L);
    SDL_Quit();
    if (!ok) return 1;
    return eng.exitCode;
}
