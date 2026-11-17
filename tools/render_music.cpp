// Renders the soundtrack to .wav files, to listen to the songs (or try an
// instrument) without playing the game:
//   tetrispack_render_music <scripts dir> [style] [seconds]
// writes one file per song (scripts/music/songs.lua) for each style
// (midi, snes, genesis, opl3; default all) into the current folder, each one
// pass of the song unless `seconds` is given. Windows MIDI needs
// soundfont/GeneralUser-GS.sf2 next to this tool (the build puts it there).
// Output names are fixed by the tool (nothing from the command line goes
// into a file name), and files are created readable by others but writable
// only by you.
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

#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static std::string g_base;   // the folder this tool runs from

// A file in the current folder, created with mode 0644 (not the 0666 fopen
// would ask for). False on an error.
static bool writeFile(const std::string& name, const std::vector<uint8_t>& data) {
#ifdef _WIN32
    const int fd = _open(name.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    const int fd = open(name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
    if (fd < 0) { std::perror(name.c_str()); return false; }
#ifdef _WIN32
    FILE* f = _fdopen(fd, "wb");
#else
    FILE* f = fdopen(fd, "wb");
#endif
    if (!f) { std::perror(name.c_str()); return false; }
    const bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

// 16-bit PCM as a RIFF WAVE file.
static bool writeWav(const std::string& name, const std::vector<int16_t>& pcm, int rate, int channels) {
    std::vector<uint8_t> b;
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); };
    auto u16 = [&](uint32_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); };
    auto tag = [&](const char* t) { b.insert(b.end(), t, t + 4); };
    const uint32_t bytes = uint32_t(pcm.size() * 2);
    tag("RIFF"); u32(36 + bytes); tag("WAVE"); tag("fmt ");
    u32(16); u16(1); u16(uint32_t(channels)); u32(uint32_t(rate)); u32(uint32_t(rate * channels * 2));
    u16(uint32_t(channels * 2)); u16(16);
    tag("data"); u32(bytes);
    for (int16_t v : pcm) u16(uint16_t(v));
    return writeFile(name, b);
}

// Each sound effect on its own, mono.
static int writeSfx(int rate) {
    for (const char* name : {"rotate", "place", "clear", "clear_big", "game_over"}) {
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
        const std::string file = std::string(name) + ".wav";
        if (!writeWav(file, mono, rate, 1)) return 1;
        std::printf("%s: %.2f s\n", file.c_str(), double(mono.size()) / rate);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <scripts dir> [style | genesis-vgm | sfx-wav] [seconds | rate]\n"
                             "(writes into the current folder)\n", argv[0]);
        return 2;
    }
    const std::string scripts = argv[1];
    const std::string only = argc > 2 ? argv[2] : "";
    if (only == "sfx-wav") return writeSfx(argc > 3 ? std::atoi(argv[3]) : 13300);
    const double seconds = argc > 3 ? std::atof(argv[3]) : 0;
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
            music::VgmWriter& vgm = music::beginGenesisVgm();
            const double len = song->length;
            {   // the engine logs while it plays; it's gone before the log ends
                music::Player synth;
                synth.setVolume(1.0f);
                synth.play(song, 0);
                vgm.loopHere(0);
                std::vector<int16_t> pcm(size_t(len * rate) * 2, 0);
                synth.render(pcm.data(), int(pcm.size() / 2), rate);
            }
            const size_t writes = vgm.commands();
            const std::vector<uint8_t> file = vgm.finish(uint64_t(len * 44100.0 + 0.5));
            music::endGenesisVgm();
            const std::string out = "song" + name.substr(0, name.find('_')) + ".vgm";
            if (!writeFile(out, file)) return 1;
            std::printf("%s: %s, %.1f s, %zu chip writes\n", out.c_str(), name.c_str(), len, writes);
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
        const std::string file = name + ".wav";
        if (!writeWav(file, pcm, rate, 2)) return 1;
        std::printf("%s: %.1f s, %zu notes, peak %d\n", file.c_str(), song->length, song->notes.size(), peak);
    }
    lua_close(L);
    return 0;
}
