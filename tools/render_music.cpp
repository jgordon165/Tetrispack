// Renders the soundtrack to .wav files, to listen to the songs (or try a
// patch) without playing the game:
//   tetrispack_render_music <scripts dir> <out dir> [seconds]
// writes one file per song (scripts/music/songs.lua), each looped for
// `seconds` (default: one pass of the song).

#include "music/fm_synth.h"
#include "music/song_lua.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static void writeWav(const std::string& path, const std::vector<int16_t>& pcm, int rate) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); return; }
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    const uint32_t bytes = uint32_t(pcm.size() * 2);
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32(uint32_t(rate)); u32(uint32_t(rate) * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(pcm.data(), 2, pcm.size(), f);
    std::fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <scripts dir> <out dir> [seconds]\n", argv[0]);
        return 2;
    }
    const std::string scripts = argv[1], out = argv[2];
    const double seconds = argc > 3 ? std::atof(argv[3]) : 0;
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    const std::string setup = "package.path = '" + scripts + "/?.lua;' .. package.path\n"
        "local j = require('music.jukebox')\n"
        "local out = {}\n"
        "for i, s in ipairs(j.songs) do out[i] = { name = s.name, song = j.build(s) } end\n"
        "return out\n";
    if (luaL_dostring(L, setup.c_str()) != LUA_OK) {
        std::fprintf(stderr, "lua: %s\n", lua_tostring(L, -1));
        return 1;
    }
    const int rate = 44100;
    const lua_Integer n = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= n; ++i) {
        lua_rawgeti(L, -1, i);
        lua_getfield(L, -1, "name");
        std::string name = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "song");
        auto song = music::songFromLua(L, -1);
        lua_pop(L, 2);

        music::FmSynth synth;
        synth.setVolume(1.0f);
        synth.play(song, 0);
        const double len = (seconds > 0 ? seconds : song->length) + 1.5;  // + the last notes' release
        std::vector<int16_t> pcm(size_t(len * rate) * 2, 0);
        synth.render(pcm.data(), int(pcm.size() / 2), rate);
        int peak = 0;
        for (int16_t v : pcm) peak = std::max(peak, std::abs(int(v)));
        for (auto& c : name) if (c == ' ') c = '_';
        const std::string path = out + "/" + std::to_string(i) + "_" + name + ".wav";
        writeWav(path, pcm, rate);
        std::printf("%s: %.1f s, %zu notes, peak %d\n", path.c_str(), song->length, song->notes.size(), peak);
    }
    lua_close(L);
    return 0;
}
