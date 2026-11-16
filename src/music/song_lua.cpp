#include "music/song_lua.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
}

#include <cstring>
#include <string>

namespace music {

namespace {

double num(lua_State* L, int t, const char* k, double def) {
    lua_getfield(L, t, k);
    const double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

std::string str(lua_State* L, int t, const char* k, const char* def) {
    lua_getfield(L, t, k);
    std::string v = lua_isstring(L, -1) ? lua_tostring(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

// Pushes field k if it's a table; returns its index or 0 (nothing pushed).
int sub(lua_State* L, int t, const char* k) {
    lua_getfield(L, t, k);
    if (lua_istable(L, -1)) return lua_gettop(L);
    lua_pop(L, 1);
    return 0;
}

OplOperator oplOp(lua_State* L, int t, const char* k) {
    OplOperator o;
    if (const int i = sub(L, t, k)) {
        auto u = [&](const char* f, double d) { return uint8_t(num(L, i, f, d)); };
        o.mult = u("mult", 1); o.tl = u("tl", 0); o.ar = u("ar", 15); o.dr = u("dr", 0);
        o.sl = u("sl", 0); o.rr = u("rr", 7); o.ws = u("ws", 0); o.ksl = u("ksl", 0);
        o.vib = num(L, i, "vib", 0) != 0;
        o.am = num(L, i, "am", 0) != 0;
        o.sustain = num(L, i, "sustain", 1) != 0;
        o.ksr = num(L, i, "ksr", 0) != 0;
        lua_pop(L, 1);
    }
    return o;
}

Wave waveNamed(const std::string& s) {
    static const char* names[] = {"pulse12", "pulse25", "pulse50", "saw", "triangle", "sine", "organ",
                                  "strings", "epiano", "bell", "brass", "kick", "snare", "hat", "crash",
                                  "pluck", "tom"};
    for (int i = 0; i < int(Wave::Count); ++i)
        if (s == names[i]) return Wave(i);
    return Wave::Pulse25;
}

Instrument instrument(lua_State* L, int t) {
    Instrument in;
    const std::string kind = str(L, t, "kind", "opl");
    in.kind = kind == "fm4" ? Kind::Fm4 : kind == "psg" ? Kind::PsgTone : kind == "noise" ? Kind::PsgNoise
            : kind == "sample" ? Kind::Sample : Kind::Opl;
    if (const int o = sub(L, t, "opl")) {
        in.opl.mod = oplOp(L, o, "mod");
        in.opl.car = oplOp(L, o, "car");
        in.opl.feedback = uint8_t(num(L, o, "feedback", 0));
        in.opl.additive = num(L, o, "additive", 0) != 0;
        lua_pop(L, 1);
    }
    if (const int f = sub(L, t, "fm4")) {
        in.fm4.alg = uint8_t(num(L, f, "alg", 0));
        in.fm4.fb = uint8_t(num(L, f, "fb", 0));
        in.fm4.ams = uint8_t(num(L, f, "ams", 0));
        in.fm4.fms = uint8_t(num(L, f, "fms", 0));
        if (const int ops = sub(L, f, "ops")) {
            for (int k = 0; k < 4; ++k) {
                lua_rawgeti(L, ops, k + 1);
                if (lua_istable(L, -1)) {
                    const int i = lua_gettop(L);
                    Fm4Operator& o = in.fm4.op[k];
                    auto u = [&](const char* n, double d) { return uint8_t(num(L, i, n, d)); };
                    o.dt = u("dt", 0); o.mul = u("mul", 1); o.tl = u("tl", 0); o.ks = u("ks", 0);
                    o.ar = u("ar", 31); o.d1r = u("d1r", 0); o.d2r = u("d2r", 0); o.d1l = u("d1l", 0);
                    o.rr = u("rr", 15); o.am = num(L, i, "am", 0) != 0;
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    if (const int e = sub(L, t, "env")) {
        in.env.attack = float(num(L, e, "attack", in.env.attack));
        in.env.decay = float(num(L, e, "decay", in.env.decay));
        in.env.sustain = float(num(L, e, "sustain", in.env.sustain));
        in.env.release = float(num(L, e, "release", in.env.release));
        lua_pop(L, 1);
    }
    in.wave = waveNamed(str(L, t, "wave", "pulse25"));
    in.noiseHz = float(num(L, t, "noise_hz", in.noiseHz));
    in.vibrato = float(num(L, t, "vibrato", 0));
    in.gain = float(num(L, t, "gain", 1));
    in.pan = float(num(L, t, "pan", 0));
    in.echo = float(num(L, t, "echo", 0));
    in.fixedKey = int(num(L, t, "key", -1));
    return in;
}

}  // namespace

std::shared_ptr<Song> songFromLua(lua_State* L, int index) {
    index = lua_absindex(L, index);
    auto song = std::make_shared<Song>();
    const std::string engine = str(L, index, "engine", "opl3");
    song->engine = engine == "genesis" ? Engine::Genesis : engine == "snes" ? Engine::Snes : Engine::Opl3;
    song->length = num(L, index, "length", 0);
    if (const int e = sub(L, index, "echo")) {
        song->echoDelay = float(num(L, e, "delay", song->echoDelay));
        song->echoFeedback = float(num(L, e, "feedback", song->echoFeedback));
        song->echoVolume = float(num(L, e, "volume", song->echoVolume));
        lua_pop(L, 1);
    }

    lua_getfield(L, index, "instruments");
    luaL_checktype(L, -1, LUA_TTABLE);
    const lua_Integer ni = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= ni; ++i) {
        lua_rawgeti(L, -1, i);
        song->instruments.push_back(instrument(L, lua_gettop(L)));
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
        lua_rawgeti(L, t, 5); n.instrument = int(lua_tointeger(L, -1)) - 1;
        lua_pop(L, 6);
        song->notes.push_back(n);
    }
    lua_pop(L, 1);
    return song;
}

}  // namespace music
