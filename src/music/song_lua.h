// A song from its Lua table (what scripts/music/jukebox.lua builds):
//   { engine = "opl3" | "genesis" | "snes" | "midi", length = seconds,
//     soundfont = path,                                          -- midi (see loadSoundFont)
//     echo = { delay, feedback, volume },                         -- snes
//     instruments = { {
//         kind = "opl" | "fm4" | "psg" | "noise" | "sample" | "gm",
//         program, bank, drums,                                   -- gm
//         opl = { mod = {mult, tl, ar, dr, sl, rr, ws, ksl, vib, am, sustain, ksr},
//                 car = {...}, feedback, additive },
//         fm4 = { alg, fb, ams, fms, ops = { {dt, mul, tl, ks, ar, d1r, d2r, d1l, rr, am} x4 } },
//         env = { attack, decay, sustain, release },             -- psg, noise, sample
//         wave = "pulse25" | "saw" | ... | "kick" | "openhat",    -- sample
//         noise_hz, vibrato, gain, pan, echo, glide, priority, key }, ... },
//     notes = { { start, length, key, velocity, instrument (1-based) }, ... } }
#pragma once

#include "music/song.h"

#include <memory>

struct lua_State;

namespace music {
std::shared_ptr<Song> songFromLua(lua_State* L, int index);
}
