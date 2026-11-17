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
// "snes-it" writes the SNES style as Impulse Tracker modules (song<n>.it)
// for the SNES build's SNESMOD driver, and "sfx-it [rate]" the sound effects
// as one module of samples (sfx.it, default 8000 Hz); see consoles/snes.

#include "music/player.h"
#include "music/song_lua.h"
#include "music/soundfont.h"
#include "music/itwriter.h"
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

static const char* const kSfxNames[] = {"rotate", "place", "clear", "clear_big"};

static std::vector<int16_t> renderSfx(const char* name, int rate) {
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
    return mono;
}

static bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); return false; }
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

// The sound effects as one module of samples for SNESMOD, trimmed to fit the
// SNES's sound RAM next to the music (each faded out over its last 40 ms).
static int writeSfxIt(const std::string& out, int rate) {
    const double keep[] = {0.3, 0.45, 0.6, 0.8};   // seconds
    std::vector<std::vector<int16_t>> sounds;
    std::vector<std::string> names;
    for (size_t i = 0; i < 4; ++i) {
        auto pcm = renderSfx(kSfxNames[i], rate);
        pcm.resize(std::min(pcm.size(), size_t(keep[i] * rate)));
        const size_t fade = std::min(pcm.size(), size_t(0.04 * rate));
        for (size_t k = 0; k < fade; ++k) {
            int16_t& v = pcm[pcm.size() - 1 - k];
            v = int16_t(v * double(k) / double(fade));
        }
        std::printf("%s: %.2f s\n", kSfxNames[i], double(pcm.size()) / rate);
        sounds.push_back(std::move(pcm));
        names.push_back(kSfxNames[i]);
    }
    return writeFile(out + "/sfx.it", music::soundsToIt(sounds, names, rate, "Tetrispack sfx")) ? 0 : 1;
}

// Each sound effect on its own, mono.
static int writeSfx(const std::string& out, int rate) {
    for (const char* name : kSfxNames) {
        const std::vector<int16_t> mono = renderSfx(name, rate);
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
    if (only == "sfx-it") return writeSfxIt(out, argc > 4 ? std::atoi(argv[4]) : 8000);
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
    // The SNES cartridge's sound RAM (64 KB) can't hold the PC's long form;
    // TETRISPACK_SNES_FORM picks a shorter one for snes-it: "once" (one pass)
    // or "twice" (a pass, then again a whole step up).
    const char* snesForm = std::getenv("TETRISPACK_SNES_FORM");
    const std::string formSetup = (only == "snes-it" && snesForm)
        ? std::string("local A = require('music.arrange')\n"
                      "local full = A.form\n"
                      "A.form = function(bars)\n"
                      "  local f = { { name = 'A', from = 1, to = bars } }\n"
                      "  if '") + snesForm + "' == 'twice' then f[2] = { name = 'A3', from = 1, to = bars, transpose = 2 } end\n"
                      "  return f\n"
                      "end\n"
        : std::string();
    const std::string setup = "package.path = '" + scripts + "/?.lua;' .. package.path\n" + formSetup +
        "local j = require('music.jukebox')\n"
        "local out = {}\n"
        "for _, style in ipairs(j.styles) do\n"
        "  for i, s in ipairs(j.songs) do\n"
        "    out[#out + 1] = { name = i .. '_' .. s.name .. '_' .. style.id, style = style.id, song = j.build(s, style.id),\n"
        "                      tempo = j.tempo(s, style.id) }\n"
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
        const bool vgmOut = only == "genesis-vgm", itOut = only == "snes-it";
        if (!only.empty() && only != style && !(vgmOut && style == "genesis") && !(itOut && style == "snes")) {
            lua_pop(L, 1);
            continue;
        }
        lua_getfield(L, -1, "tempo");
        const double tempo = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "song");
        auto song = music::songFromLua(L, -1);
        lua_pop(L, 2);

        if (itOut) {
            music::ItSettings st;
            st.title = name.substr(name.find('_') + 1);
            st.title = st.title.substr(0, st.title.rfind('_'));
            st.bpm = tempo;
            music::ItReport rep;
            const auto it = music::songToIt(*song, st, &rep);
            const std::string path = out + "/song" + name.substr(0, name.find('_')) + ".it";
            if (!writeFile(path, it)) return 1;
            std::printf("%s: %s, %d rows/beat, %d patterns, %d notes (%d dropped), %zu sample bytes, %zu bytes\n",
                        path.c_str(), st.title.c_str(), rep.rowsPerBeat, rep.patterns, rep.notes, rep.dropped,
                        rep.sampleBytes, it.size());
            continue;
        }
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
