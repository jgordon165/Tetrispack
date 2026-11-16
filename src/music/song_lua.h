// A song from its Lua table (what scripts/music/jukebox.lua builds):
//   { length = seconds,
//     patches = { { mod = {mult, tl, ar, dr, sl, rr, ws, ksl, vib, am, sustain, ksr},
//                   car = {...}, feedback, additive, key }, ... },
//     notes = { { start, length, key, velocity, patch (1-based) }, ... } }
#pragma once

#include "music/fm_synth.h"

#include <memory>

struct lua_State;

namespace music {
std::shared_ptr<Song> songFromLua(lua_State* L, int index);
}
