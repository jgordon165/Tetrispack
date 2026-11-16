#include "lua_api.h"

#include "engine.h"
#include "formats/dib.h"
#include "input.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
}

#include <SDL.h>
#include <cstdio>

namespace {

Engine* eng(lua_State* L) {
    return static_cast<Engine*>(lua_touserdata(L, lua_upvalueindex(1)));
}

uint8_t u8arg(lua_State* L, int i, lua_Integer def = 0) {
    const lua_Integer v = luaL_optinteger(L, i, def);
    return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
}

int iarg(lua_State* L, int i) { return static_cast<int>(luaL_checkinteger(L, i)); }

// tp.has_data() -> bool, error message
int l_has_data(lua_State* L) {
    lua_pushboolean(L, eng(L)->exe.has_value());
    lua_pushstring(L, eng(L)->dataError.c_str());
    return 2;
}

// tp.module() -> the executable's module name and its expected Windows version
int l_module(lua_State* L) {
    const auto& e = eng(L)->exe;
    if (!e) return 0;
    lua_pushstring(L, e->moduleName().c_str());
    lua_pushfstring(L, "%d.%d", e->expectedWindowsMajor(), e->expectedWindowsMinor());
    return 2;
}

// tp.resources() -> { {type=, name=, size=}, ... }
int l_resources(lua_State* L) {
    lua_newtable(L);
    const auto& e = eng(L)->exe;
    if (!e) return 1;
    int n = 0;
    for (const auto& r : e->resources()) {
        lua_newtable(L);
        lua_pushstring(L, r.typeName.empty() ? tp::resTypeName(r.typeId) : r.typeName.c_str());
        lua_setfield(L, -2, "type");
        lua_pushstring(L, r.key.str().c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, r.length);
        lua_setfield(L, -2, "size");
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

// tp.bitmap("NAME" or "#12" or 12) -> handle, width, height | nil, error
int l_bitmap(lua_State* L) {
    const auto& e = eng(L)->exe;
    if (!e) { lua_pushnil(L); lua_pushstring(L, "no game data"); return 2; }
    const tp::NeResource* r = lua_isinteger(L, 1)
        ? e->find(static_cast<uint16_t>(tp::ResType::Bitmap), static_cast<uint16_t>(lua_tointeger(L, 1)))
        : e->find(static_cast<uint16_t>(tp::ResType::Bitmap), std::string(luaL_checkstring(L, 1)));
    if (!r) { lua_pushnil(L); lua_pushstring(L, "no such bitmap"); return 2; }
    try {
        const auto img = tp::decodeDib(e->read(*r));
        lua_pushinteger(L, eng(L)->video.addImage(img));
        lua_pushinteger(L, img.width);
        lua_pushinteger(L, img.height);
        return 3;
    } catch (const std::exception& ex) {
        lua_pushnil(L);
        lua_pushstring(L, ex.what());
        return 2;
    }
}

// tp.string(id) -> string | nil (the executable's string table)
int l_string(lua_State* L) {
    const auto& e = eng(L)->exe;
    if (!e) return 0;
    const auto s = e->string(static_cast<uint16_t>(luaL_checkinteger(L, 1)));
    if (!s) return 0;
    lua_pushlstring(L, s->data(), s->size());
    return 1;
}

int l_canvas(lua_State* L) {
    if (lua_gettop(L) >= 2) eng(L)->video.setCanvas(iarg(L, 1), iarg(L, 2));
    lua_pushinteger(L, eng(L)->video.canvasW());
    lua_pushinteger(L, eng(L)->video.canvasH());
    return 2;
}

int l_clear(lua_State* L) {
    eng(L)->video.clear(u8arg(L, 1), u8arg(L, 2), u8arg(L, 3));
    return 0;
}

int l_fill(lua_State* L) {
    eng(L)->video.fill(iarg(L, 1), iarg(L, 2), iarg(L, 3), iarg(L, 4),
                       u8arg(L, 5), u8arg(L, 6), u8arg(L, 7), u8arg(L, 8, 255));
    return 0;
}

// tp.text(x, y, s, scale, r, g, b)
int l_text(lua_State* L) {
    eng(L)->video.text(iarg(L, 1), iarg(L, 2), luaL_checkstring(L, 3),
                       static_cast<int>(luaL_optinteger(L, 4, 1)),
                       u8arg(L, 5, 255), u8arg(L, 6, 255), u8arg(L, 7, 255));
    return 0;
}

int l_text_width(lua_State* L) {
    lua_pushinteger(L, Video::textWidth(luaL_checkstring(L, 1), static_cast<int>(luaL_optinteger(L, 2, 1))));
    return 1;
}

// tp.draw(handle, sx, sy, sw, sh, dx, dy [, dw, dh])
int l_draw(lua_State* L) {
    const int sw = iarg(L, 4), sh = iarg(L, 5);
    eng(L)->video.draw(iarg(L, 1), iarg(L, 2), iarg(L, 3), sw, sh, iarg(L, 6), iarg(L, 7),
                       static_cast<int>(luaL_optinteger(L, 8, sw)), static_cast<int>(luaL_optinteger(L, 9, sh)));
    return 0;
}

int l_key_down(lua_State* L) {
    lua_pushboolean(L, input::isDown(luaL_checkstring(L, 1)));
    return 1;
}

int l_ticks(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(SDL_GetTicks64()));
    return 1;
}

int l_title(lua_State* L) {
    eng(L)->video.setTitle(luaL_checkstring(L, 1));
    return 0;
}

int l_quit(lua_State* L) {
    eng(L)->quit = true;
    eng(L)->exitCode = static_cast<int>(luaL_optinteger(L, 1, 0));
    return 0;
}

int l_log(lua_State* L) {
    std::printf("%s\n", luaL_checkstring(L, 1));
    std::fflush(stdout);
    return 0;
}

}  // namespace

void registerLuaApi(lua_State* L, Engine* e) {
    static const luaL_Reg fns[] = {
        {"has_data", l_has_data}, {"module", l_module}, {"resources", l_resources},
        {"bitmap", l_bitmap}, {"string", l_string},
        {"canvas", l_canvas}, {"clear", l_clear}, {"fill", l_fill}, {"text", l_text},
        {"text_width", l_text_width}, {"draw", l_draw},
        {"key_down", l_key_down}, {"ticks", l_ticks}, {"title", l_title},
        {"quit", l_quit}, {"log", l_log},
        {nullptr, nullptr},
    };
    lua_newtable(L);
    lua_pushlightuserdata(L, e);
    luaL_setfuncs(L, fns, 1);
    lua_setglobal(L, "tp");
}
