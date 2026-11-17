// Renders the soundtrack to .wav files, to listen to the songs (or try an
// instrument) without playing the game:
//   tetrispack_render_music <scripts dir> <out dir> [style] [seconds]
// writes one file per song (scripts/music/songs.lua) for each style
// (midi, snes, genesis, opl3; default all), each one pass of the song unless
// `seconds` is given. Windows MIDI needs soundfont/GeneralUser-GS.sf2 next to
// this tool (the build puts it there).
//
// Style "genesis-vgm" instead writes the Genesis style as VGM chip logs
// (<n>.vgm, one looping pass of each song) for the Genesis build's XGM2
// music; see consoles/genesis. "sfx-wav [rate]" writes the sound effects
// as mono .wav files (default 13300 Hz, the Genesis XGM2 PCM rate).

#include "music/player.h"
#include "music/song_lua.h"
#include "music/soundfont.h"
#include "music/sfx.h"
#include "music/vgm.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstdio>
#include <iterator>
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

// Each sound effect on its own, mono.
static int writeSfx(const std::string& out, int rate) {
    for (const char* name : {"rotate", "place", "clear", "clear_big"}) {
        music::Sfx sfx;
        sfx.prepare(rate);
        sfx.setVolume(1.0f);
        sfx.play(name);
        std::vector<int16_t> mono;
        int16_t buf[2 * 256];
        while (sfx.busy()) {
            std::fill(std::begin(buf), std::end(buf), int16_t(0));
            sfx.render(buf, 256);
            for (int k = 0; k < 256; ++k) mono.push_back(buf[2 * k]);
        }
        const std::string path = out + "/" + name + ".wav";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) { std::perror(path.c_str()); return 1; }
        auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
        const uint32_t bytes = uint32_t(mono.size() * 2);
        std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
        u32(16); u16(1); u16(1); u32(uint32_t(rate)); u32(uint32_t(rate) * 2); u16(2); u16(16);
        std::fwrite("data", 1, 4, f); u32(bytes);
        std::fwrite(mono.data(), 2, mono.size(), f);
        std::fclose(f);
        std::printf("%s: %.2f s\n", path.c_str(), double(mono.size()) / rate);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <scripts dir> <out dir> [style | genesis-vgm | sfx-wav] [seconds | rate]\n", argv[0]);
        return 2;
    }
    const std::string scripts = argv[1], out = argv[2];
    const std::string only = argc > 3 ? argv[3] : "";
    if (only == "sfx-wav") return writeSfx(out, argc > 4 ? std::atoi(argv[4]) : 13300);
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
        const bool vgmOut = only == "genesis-vgm";
        if (!only.empty() && only != style && !(vgmOut && style == "genesis")) { lua_pop(L, 1); continue; }
        lua_getfield(L, -1, "song");
        auto song = music::songFromLua(L, -1);
        lua_pop(L, 2);

        if (vgmOut) {
            music::VgmWriter vgm;
            music::setGenesisVgm(&vgm);
            music::Player synth;
            synth.setVolume(1.0f);
            synth.play(song, 0);
            vgm.loopHere(0);
            const double len = song->length;
            std::vector<int16_t> pcm(size_t(len * rate) * 2, 0);
            synth.render(pcm.data(), int(pcm.size() / 2), rate);
            music::setGenesisVgm(nullptr);
            const std::string path = out + "/song" + name.substr(0, name.find('_')) + ".vgm";
            if (!vgm.save(path, uint64_t(len * 44100.0 + 0.5))) { std::perror(path.c_str()); return 1; }
            std::printf("%s: %s, %.1f s, %zu chip writes\n", path.c_str(), name.c_str(), len, vgm.commands());
            continue;
        }

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
