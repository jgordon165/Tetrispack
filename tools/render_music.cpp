// Renders the soundtrack to .wav files, to listen to the songs (or try an
// instrument) without playing the game:
//   tetrispack_render_music <scripts dir> <out dir> [style] [seconds]
// writes one file per song (scripts/music/songs.lua) for each style
// (midi, snes, genesis, opl3; default all), each one pass of the song unless
// `seconds` is given. Windows MIDI needs soundfont/GeneralUser-GS.sf2 next to
// this tool (the build puts it there).

#include "music/player.h"
#include "music/song_lua.h"
#include "music/soundfont.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static std::string g_base;   // the folder this tool runs from

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
    const std::string only = argc > 3 ? argv[3] : "";
    const double seconds = argc > 4 ? std::atof(argv[4]) : 0;
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    // The little of the game's tp table the jukebox needs to find the SoundFont.
    lua_newtable(L);
    lua_pushcfunction(L, [](lua_State* S) -> int {
        lua_pushboolean(S, music::loadSoundFont(luaL_checkstring(S, 1)));
        return 1;
    });
    lua_setfield(L, -2, "music_soundfont");
    {
        const std::string exe = argv[0];
        const size_t slash = exe.find_last_of("/\\");
        g_base = slash == std::string::npos ? "./" : exe.substr(0, slash + 1);
    }
    lua_pushcfunction(L, [](lua_State* S) -> int { lua_pushstring(S, g_base.c_str()); return 1; });
    lua_setfield(L, -2, "base_path");
    lua_setglobal(L, "tp");
    const std::string setup = "package.path = '" + scripts + "/?.lua;' .. package.path\n"
        "local j = require('music.jukebox')\n"
        "local out = {}\n"
        "for _, style in ipairs(j.styles) do\n"
        "  for i, s in ipairs(j.songs) do\n"
        "    out[#out + 1] = { name = i .. '_' .. s.name .. '_' .. style.id, style = style.id, song = j.build(s, style.id) }\n"
        "  end\n"
        "end\n"
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
        lua_getfield(L, -1, "style");
        const std::string style = lua_tostring(L, -1);
        lua_pop(L, 1);
        if (!only.empty() && only != style) { lua_pop(L, 1); continue; }
        lua_getfield(L, -1, "song");
        auto song = music::songFromLua(L, -1);
        lua_pop(L, 2);

        music::Player synth;
        synth.setVolume(1.0f);
        synth.play(song, 0);
        const double len = (seconds > 0 ? seconds : song->length) + 1.5;  // + the last notes' release
        std::vector<int16_t> pcm(size_t(len * rate) * 2, 0);
        synth.render(pcm.data(), int(pcm.size() / 2), rate);
        int peak = 0;
        for (int16_t v : pcm) peak = std::max(peak, std::abs(int(v)));
        for (auto& c : name) if (c == ' ') c = '_';
        const std::string path = out + "/" + name + ".wav";
        writeWav(path, pcm, rate);
        std::printf("%s: %.1f s, %zu notes, peak %d\n", path.c_str(), song->length, song->notes.size(), peak);
    }
    lua_close(L);
    return 0;
}
