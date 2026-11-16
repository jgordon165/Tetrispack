#include "music/song_lua.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
}

namespace music {

namespace {

double numField(lua_State* L, int t, const char* k, double def) {
    lua_getfield(L, t, k);
    const double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

Operator opField(lua_State* L, int t, const char* k) {
    Operator o;
    lua_getfield(L, t, k);
    if (lua_istable(L, -1)) {
        const int i = lua_gettop(L);
        auto u = [&](const char* f, double d) { return uint8_t(numField(L, i, f, d)); };
        o.mult = u("mult", 1); o.tl = u("tl", 0); o.ar = u("ar", 15); o.dr = u("dr", 0);
        o.sl = u("sl", 0); o.rr = u("rr", 7); o.ws = u("ws", 0); o.ksl = u("ksl", 0);
        o.vib = numField(L, i, "vib", 0) != 0;
        o.am = numField(L, i, "am", 0) != 0;
        o.sustain = numField(L, i, "sustain", 1) != 0;
        o.ksr = numField(L, i, "ksr", 0) != 0;
    }
    lua_pop(L, 1);
    return o;
}

}  // namespace

std::shared_ptr<Song> songFromLua(lua_State* L, int index) {
    index = lua_absindex(L, index);
    auto song = std::make_shared<Song>();
    song->length = numField(L, index, "length", 0);

    lua_getfield(L, index, "patches");
    luaL_checktype(L, -1, LUA_TTABLE);
    const lua_Integer np = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= np; ++i) {
        lua_rawgeti(L, -1, i);
        const int t = lua_gettop(L);
        Patch p;
        p.mod = opField(L, t, "mod");
        p.car = opField(L, t, "car");
        p.feedback = uint8_t(numField(L, t, "feedback", 0));
        p.additive = numField(L, t, "additive", 0) != 0;
        p.fixedKey = int(numField(L, t, "key", -1));
        song->patches.push_back(p);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "notes");
    luaL_checktype(L, -1, LUA_TTABLE);
    const lua_Integer nn = luaL_len(L, -1);
    song->notes.reserve(size_t(nn));
    for (lua_Integer i = 1; i <= nn; ++i) {
        lua_rawgeti(L, -1, i);
        const int t = lua_gettop(L);
        Note n;
        lua_rawgeti(L, t, 1); n.start = lua_tonumber(L, -1);
        lua_rawgeti(L, t, 2); n.length = lua_tonumber(L, -1);
        lua_rawgeti(L, t, 3); n.key = int(lua_tointeger(L, -1));
        lua_rawgeti(L, t, 4); n.velocity = float(lua_tonumber(L, -1));
        lua_rawgeti(L, t, 5); n.patch = int(lua_tointeger(L, -1)) - 1;
        lua_pop(L, 6);
        song->notes.push_back(n);
    }
    lua_pop(L, 1);
    return song;
}

}  // namespace music
