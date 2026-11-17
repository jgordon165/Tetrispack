// The Xbox 360 host's Lua side: the `tp` table the game scripts call (the
// same API as the PC's src/lua_api.cpp), and loading the scripts.
#include "host.h"

// Lua is compiled as C, on the console as on the PC.
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <cstdio>

namespace x360 {

namespace {

Game* game(lua_State* L) { return static_cast<Game*>(lua_touserdata(L, lua_upvalueindex(1))); }

int iarg(lua_State* L, int i) { return int(luaL_checkinteger(L, i)); }

uint8_t u8arg(lua_State* L, int i, lua_Integer def) {
    const lua_Integer v = luaL_optinteger(L, i, def);
    return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v);
}

Canvas::Font fontArg(lua_State* L, int i) {
    return std::string(luaL_optstring(L, i, "small")) == "system" ? Canvas::System : Canvas::Small;
}

std::string str(lua_Integer v) {
    char b[32];
    std::sprintf(b, "%d", int(v));
    return b;
}

// ---- the original's files ---------------------------------------------------

int l_has_data(lua_State* L) {
    lua_pushboolean(L, game(L)->module("exe").loaded());
    lua_pushstring(L, game(L)->dataError().c_str());
    return 2;
}

int l_module(lua_State* L) {
    NeFile& m = game(L)->module(luaL_optstring(L, 1, "exe"));
    if (!m.loaded()) return 0;
    lua_pushstring(L, m.moduleName().c_str());
    lua_pushstring(L, (str(m.windowsMajor()) + "." + str(m.windowsMinor())).c_str());
    return 2;
}

int l_resources(lua_State* L) {
    lua_newtable(L);
    const std::vector<Resource>& res = game(L)->module("exe").resources();
    for (size_t i = 0; i < res.size(); ++i) {
        lua_newtable(L);
        lua_pushstring(L, res[i].typeName.empty() ? resTypeName(res[i].typeId) : res[i].typeName.c_str());
        lua_setfield(L, -2, "type");
        lua_pushstring(L, res[i].name.empty() ? ("#" + str(res[i].id)).c_str() : res[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, res[i].length);
        lua_setfield(L, -2, "size");
        lua_rawseti(L, -2, lua_Integer(i + 1));
    }
    return 1;
}

int l_bitmap(lua_State* L) {
    NeFile& m = game(L)->module(luaL_optstring(L, 2, "exe"));
    if (!m.loaded()) { lua_pushnil(L); lua_pushstring(L, "no game data"); return 2; }
    const Resource* r = lua_isinteger(L, 1) ? m.find(2, uint16_t(lua_tointeger(L, 1))) : m.find(2, luaL_checkstring(L, 1));
    std::vector<uint8_t> bytes;
    Image img;
    std::string err;
    if (!r || !m.read(*r, bytes)) { lua_pushnil(L); lua_pushstring(L, "no such bitmap"); return 2; }
    if (!decodeDib(bytes, img, err)) { lua_pushnil(L); lua_pushstring(L, err.c_str()); return 2; }
    lua_pushinteger(L, game(L)->canvas().addImage(img));
    lua_pushinteger(L, img.width);
    lua_pushinteger(L, img.height);
    return 3;
}

int l_string(lua_State* L) {
    std::string s;
    if (!game(L)->module("exe").string(uint16_t(luaL_checkinteger(L, 1)), s)) return 0;
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}

int l_dialog(lua_State* L) {
    NeFile& m = game(L)->module(luaL_optstring(L, 2, "exe"));
    const Resource* r = m.loaded() ? m.find(5, uint16_t(luaL_checkinteger(L, 1))) : 0;
    std::vector<uint8_t> bytes;
    Dialog d;
    if (!r || !m.read(*r, bytes) || !decodeDialog(bytes, d)) { lua_pushnil(L); lua_pushstring(L, "no such dialog"); return 2; }
    lua_newtable(L);
    lua_pushinteger(L, d.x); lua_setfield(L, -2, "x");
    lua_pushinteger(L, d.y); lua_setfield(L, -2, "y");
    lua_pushinteger(L, d.w); lua_setfield(L, -2, "w");
    lua_pushinteger(L, d.h); lua_setfield(L, -2, "h");
    lua_pushinteger(L, lua_Integer(d.style)); lua_setfield(L, -2, "style");
    lua_pushstring(L, d.caption.c_str()); lua_setfield(L, -2, "caption");
    lua_newtable(L);
    for (size_t i = 0; i < d.controls.size(); ++i) {
        const DialogControl& c = d.controls[i];
        lua_newtable(L);
        lua_pushinteger(L, c.x); lua_setfield(L, -2, "x");
        lua_pushinteger(L, c.y); lua_setfield(L, -2, "y");
        lua_pushinteger(L, c.w); lua_setfield(L, -2, "w");
        lua_pushinteger(L, c.h); lua_setfield(L, -2, "h");
        lua_pushinteger(L, c.id); lua_setfield(L, -2, "id");
        lua_pushinteger(L, lua_Integer(c.style)); lua_setfield(L, -2, "style");
        lua_pushstring(L, c.cls.c_str()); lua_setfield(L, -2, "class");
        lua_pushstring(L, c.text.c_str()); lua_setfield(L, -2, "text");
        lua_rawseti(L, -2, lua_Integer(i + 1));
    }
    lua_setfield(L, -2, "controls");
    return 1;
}

int l_exe_string(lua_State* L) {
    std::string s;
    NeFile& m = game(L)->module(luaL_optstring(L, 2, "exe"));
    if (!m.loaded() || !m.cstringStartingWith(luaL_checkstring(L, 1), s)) return 0;
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}

// ---- drawing ----------------------------------------------------------------

int l_canvas(lua_State* L) {
    Canvas& c = game(L)->canvas();
    if (lua_gettop(L) >= 2) c.resize(iarg(L, 1), iarg(L, 2));
    lua_pushinteger(L, c.width());
    lua_pushinteger(L, c.height());
    return 2;
}

int l_clear(lua_State* L) { game(L)->canvas().clear(u8arg(L, 1, 0), u8arg(L, 2, 0), u8arg(L, 3, 0)); return 0; }

int l_fill(lua_State* L) {
    game(L)->canvas().fill(iarg(L, 1), iarg(L, 2), iarg(L, 3), iarg(L, 4),
                           u8arg(L, 5, 0), u8arg(L, 6, 0), u8arg(L, 7, 0), u8arg(L, 8, 255));
    return 0;
}

int l_text(lua_State* L) {
    game(L)->canvas().text(iarg(L, 1), iarg(L, 2), luaL_checkstring(L, 3), int(luaL_optinteger(L, 4, 1)),
                           u8arg(L, 5, 255), u8arg(L, 6, 255), u8arg(L, 7, 255), fontArg(L, 8));
    return 0;
}

int l_text_width(lua_State* L) {
    lua_pushinteger(L, Canvas::textWidth(luaL_checkstring(L, 1), int(luaL_optinteger(L, 2, 1)), fontArg(L, 3)));
    return 1;
}

int l_draw(lua_State* L) {
    const int sw = iarg(L, 4), sh = iarg(L, 5);
    game(L)->canvas().draw(iarg(L, 1), iarg(L, 2), iarg(L, 3), sw, sh, iarg(L, 6), iarg(L, 7),
                           int(luaL_optinteger(L, 8, sw)), int(luaL_optinteger(L, 9, sh)));
    return 0;
}

// ---- the rest -----------------------------------------------------------------

int l_key_down(lua_State* L) { lua_pushboolean(L, 0); return 1; }
int l_ticks(lua_State* L) { lua_pushinteger(L, lua_Integer(game(L)->platform().ticks())); return 1; }
int l_title(lua_State*) { return 0; }
int l_quit(lua_State* L) { game(L)->requestQuit(); return 0; }
int l_log(lua_State* L) { game(L)->platform().log(luaL_checkstring(L, 1)); return 0; }
int l_platform(lua_State* L) { lua_pushstring(L, game(L)->platform().name()); return 1; }
int l_base_path(lua_State* L) { lua_pushstring(L, game(L)->platform().root().c_str()); return 1; }
int l_has_audio(lua_State* L) { lua_pushboolean(L, game(L)->platform().hasAudio()); return 1; }

std::string musicPath(Platform& p, int n, const std::string& style) {
    return std::string("music") + p.separator() + style + "_" + str(n) + ".wav";
}

// tp.music_styles() -> the styles whose songs were rendered for this host.
int l_music_styles(lua_State* L) {
    static const char* const ids[] = { "winfm", "midi", "snes", "genesis", "opl3" };
    Platform& p = game(L)->platform();
    lua_newtable(L);
    int n = 0;
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
        if (p.fileExists(p.root() + musicPath(p, 1, ids[i]))) {
            lua_pushstring(L, ids[i]);
            lua_rawseti(L, -2, ++n);
        }
    if (n == 0) {   // none: keep one, silent, so the menu has a choice
        lua_pushstring(L, "winfm");
        lua_rawseti(L, -2, 1);
    }
    return 1;
}

// tp.music_track(n, style [, fade]): song n as rendered for `style`.
int l_music_track(lua_State* L) {
    Platform& p = game(L)->platform();
    p.playMusic(p.root() + musicPath(p, iarg(L, 1), luaL_checkstring(L, 2)), luaL_optnumber(L, 3, 0.6));
    return 0;
}

int l_music_play(lua_State*) { return 0; }   // no synthesis here: jukebox uses music_track
int l_music_stop(lua_State* L) { game(L)->platform().stopMusic(luaL_optnumber(L, 1, 0.6)); return 0; }
int l_music_pause(lua_State* L) { game(L)->platform().pauseMusic(lua_toboolean(L, 1) != 0); return 0; }
int l_music_volume(lua_State* L) { game(L)->platform().setMusicVolume(float(luaL_checknumber(L, 1))); return 0; }
int l_music_position(lua_State* L) { lua_pushnumber(L, game(L)->platform().musicPosition()); return 1; }
int l_music_soundfont(lua_State* L) { lua_pushboolean(L, 0); return 1; }
int l_sfx(lua_State* L) { game(L)->platform().playSfx(luaL_checkstring(L, 1), float(luaL_optnumber(L, 2, 1.0))); return 0; }
int l_sfx_volume(lua_State* L) { game(L)->platform().setSfxVolume(float(luaL_checknumber(L, 1))); return 0; }

// require("game.board") -> <scripts>game\board.lua, read through the platform
// (the console's paths don't suit Lua's own searcher).
int searcher(lua_State* L) {
    Game* g = game(L);
    std::string name = luaL_checkstring(L, 1);
    const char sep = g->platform().separator();
    for (size_t i = 0; i < name.size(); ++i) if (name[i] == '.') name[i] = sep;
    const std::string path = g->platform().root() + g->scriptsDir() + name + ".lua";
    std::vector<uint8_t> src;
    if (!g->platform().readFile(path, src)) {
        lua_pushfstring(L, "\n\tno file '%s'", path.c_str());
        return 1;
    }
    if (luaL_loadbuffer(L, src.empty() ? "" : reinterpret_cast<const char*>(&src[0]), src.size(),
                        ("@" + path).c_str()) != LUA_OK)
        return lua_error(L);
    lua_pushstring(L, path.c_str());
    return 2;
}

}  // namespace

Game::Game(Platform& p) : p_(p), L_(0), quit_(false) { canvas_.resize(640, 440); }

Game::~Game() { if (L_) lua_close(L_); }

bool Game::loadModule(NeFile& into, const std::string& dir, const char* name) {
    // The file's name in any case: TETRIS.EXE, tetris.exe, Tetris.exe.
    std::string lower = name;
    for (size_t i = 0; i < lower.size(); ++i) lower[i] = char(lower[i] >= 'A' && lower[i] <= 'Z' ? lower[i] + 32 : lower[i]);
    std::string title = lower;
    title[0] = name[0];
    const std::string tries[3] = { name, lower, title };
    std::vector<uint8_t> bytes;
    for (int i = 0; i < 3; ++i)
        if (p_.readFile(p_.root() + dir + tries[i], bytes)) {
            if (into.load(bytes)) return true;
            dataError_ = std::string(name) + ": " + into.error();
            return false;
        }
    dataError_ = std::string(name) + " not found in " + p_.root() + dir;
    return false;
}

bool Game::boot(const std::string& data, const std::string& scripts) {
    scripts_ = scripts;
    if (loadModule(exe_, data, "TETRIS.EXE")) {
        loadModule(about_, data, "ABOUTTET.DLL");   // optional: the About box art
        dataError_.clear();
    }
    L_ = luaL_newstate();
    // The libraries the game uses (no io, os or debug; the console build leaves them out).
    static const luaL_Reg libs[] = {
        { LUA_GNAME, luaopen_base }, { LUA_LOADLIBNAME, luaopen_package }, { LUA_COLIBNAME, luaopen_coroutine },
        { LUA_TABLIBNAME, luaopen_table }, { LUA_STRLIBNAME, luaopen_string }, { LUA_MATHLIBNAME, luaopen_math },
        { LUA_UTF8LIBNAME, luaopen_utf8 }, { 0, 0 },
    };
    for (const luaL_Reg* lib = libs; lib->func; ++lib) {
        luaL_requiref(L_, lib->name, lib->func, 1);
        lua_pop(L_, 1);
    }
    static const luaL_Reg fns[] = {
        { "has_data", l_has_data }, { "module", l_module }, { "resources", l_resources },
        { "bitmap", l_bitmap }, { "string", l_string }, { "dialog", l_dialog }, { "exe_string", l_exe_string },
        { "canvas", l_canvas }, { "clear", l_clear }, { "fill", l_fill }, { "text", l_text },
        { "text_width", l_text_width }, { "draw", l_draw },
        { "key_down", l_key_down }, { "ticks", l_ticks }, { "title", l_title }, { "quit", l_quit }, { "log", l_log },
        { "music_play", l_music_play }, { "music_track", l_music_track }, { "music_styles", l_music_styles },
        { "music_stop", l_music_stop }, { "music_pause", l_music_pause }, { "music_volume", l_music_volume },
        { "music_position", l_music_position }, { "music_soundfont", l_music_soundfont },
        { "sfx", l_sfx }, { "sfx_volume", l_sfx_volume },
        { "platform", l_platform }, { "base_path", l_base_path }, { "has_audio", l_has_audio },
        { 0, 0 },
    };
    lua_newtable(L_);
    lua_pushlightuserdata(L_, this);
    luaL_setfuncs(L_, fns, 1);
    lua_setglobal(L_, "tp");

    // Our searcher ahead of Lua's file searchers.
    lua_getglobal(L_, "package");
    lua_getfield(L_, -1, "searchers");
    const lua_Integer n = luaL_len(L_, -1);
    for (lua_Integer i = n; i >= 2; --i) {
        lua_rawgeti(L_, -1, i);
        lua_rawseti(L_, -2, i + 1);
    }
    lua_pushlightuserdata(L_, this);
    lua_pushcclosure(L_, searcher, 1);
    lua_rawseti(L_, -2, 2);
    lua_pop(L_, 2);

    std::vector<uint8_t> src;
    const std::string mainPath = p_.root() + scripts + "main.lua";
    if (!p_.readFile(mainPath, src)) { error_ = mainPath + " not found"; return false; }
    if (luaL_loadbuffer(L_, src.empty() ? "" : reinterpret_cast<const char*>(&src[0]), src.size(),
                        ("@" + mainPath).c_str()) != LUA_OK ||
        lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        error_ = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool Game::call(const char* fn, int nargs) {
    lua_getglobal(L_, "game");
    if (!lua_istable(L_, -1)) { lua_pop(L_, 1 + nargs); return true; }
    lua_getfield(L_, -1, fn);
    lua_remove(L_, -2);
    if (!lua_isfunction(L_, -1)) { lua_pop(L_, 1 + nargs); return true; }
    lua_insert(L_, -1 - nargs);
    if (lua_pcall(L_, nargs, 0, 0) != LUA_OK) {
        error_ = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool Game::frame(double dt) {
    lua_pushnumber(L_, dt);
    if (!call("update", 1)) return false;
    return call("draw", 0);
}

bool Game::key(const std::string& name) {
    lua_pushstring(L_, name.c_str());
    lua_pushboolean(L_, 0);
    return call("keypressed", 2);
}

}  // namespace x360
