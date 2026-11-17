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
// "snes-it" writes the SNES style as Impulse Tracker modules (song<n>.it)
// for the SNES build's SNESMOD driver, and "sfx-it [rate]" the sound effects
// as one module of samples (sfx.it, default 8000 Hz); see consoles/snes.
// "loop-wav <style> [rate]" writes each song for hosts that can't synthesize
// (the Xbox 360 build) as <style>_<n>.wav: exactly one loop long, the last
// notes' release wrapped onto the start so it loops without a seam
// (default 32000 Hz); "sfx-wav 44100" writes the sound effects to go with it.

#include "music/itwriter.h"
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

// Each sound effect on its own, mono (with the game-over sound, which the
// SNES's effects module leaves out).
static int writeSfx(int rate) {
    for (const char* name : {"rotate", "place", "clear", "clear_big", "game_over"}) {
        const std::vector<int16_t> mono = renderSfx(name, rate);
        const std::string file = std::string(name) + ".wav";
        if (!writeWav(file, mono, rate, 1)) return 1;
        std::printf("%s: %.2f s\n", file.c_str(), double(mono.size()) / rate);
    }
    return 0;
}

// The sound effects as one module of samples for SNESMOD, trimmed to fit the
// SNES's sound RAM next to the music (each faded out over its last 40 ms).
static int writeSfxIt(int rate) {
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
    return writeFile("sfx.it", music::soundsToIt(sounds, names, rate, "Tetrispack sfx")) ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <scripts dir> [style | genesis-vgm | snes-it | loop-wav <style> | sfx-wav | sfx-it] [seconds | rate]\n"
                             "(writes into the current folder)\n", argv[0]);
        return 2;
    }
    const std::string scripts = argv[1];
    const std::string only = argc > 2 ? argv[2] : "";
    if (only == "sfx-wav") return writeSfx(argc > 3 ? std::atoi(argv[3]) : 13300);
    if (only == "sfx-it") return writeSfxIt(argc > 3 ? std::atoi(argv[3]) : 8000);
    // loop-wav: the style must be one of the game's (it becomes part of the file names).
    const bool loopWav = only == "loop-wav";
    std::string loopStyle = "winfm";
    if (loopWav && argc > 3) {
        static const char* const kStyles[] = {"winfm", "midi", "snes", "genesis", "opl3"};
        loopStyle.clear();
        for (const char* id : kStyles) if (std::string(argv[3]) == id) loopStyle = id;
        if (loopStyle.empty()) { std::fprintf(stderr, "no such style: %s\n", argv[3]); return 2; }
    }
    const int loopRate = loopWav && argc > 4 ? std::atoi(argv[4]) : 32000;
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
    // The SNES cartridge's sound RAM (64 KB) can't hold the PC's long form;
    // TETRISPACK_SNES_FORM picks a shorter one for snes-it: "once" (one pass)
    // or "twice" (a pass, then again a whole step up).
    const char* snesForm = std::getenv("TETRISPACK_SNES_FORM");
    const std::string formSetup = (only == "snes-it" && snesForm)
        ? std::string("local A = require('music.arrange')\n"
                      "A.form = function(bars)\n"
                      "  local f = { { name = 'A', from = 1, to = bars } }\n"
                      "  if '") + (std::string(snesForm) == "twice" ? "twice" : "once") + "' == 'twice' then f[2] = { name = 'A3', from = 1, to = bars, transpose = 2 } end\n"
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
        if (loopWav ? style != loopStyle
                    : (!only.empty() && only != style && !(vgmOut && style == "genesis") && !(itOut && style == "snes"))) {
            lua_pop(L, 1);
            continue;
        }
        lua_getfield(L, -1, "tempo");
        const double tempo = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "song");
        auto song = music::songFromLua(L, -1);
        lua_pop(L, 2);

        if (loopWav) {
            music::Player synth;
            synth.setVolume(1.0f);
            synth.play(song, 0);
            const size_t loop = size_t(song->length * loopRate), tail = size_t(2.0 * loopRate);
            std::vector<int16_t> pcm((loop + tail) * 2, 0);
            synth.render(pcm.data(), int(pcm.size() / 2), loopRate);
            for (size_t k = 0; k < tail * 2 && k < loop * 2; ++k) {
                const int v = pcm[k] + pcm[loop * 2 + k];
                pcm[k] = int16_t(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
            }
            pcm.resize(loop * 2);
            const std::string out = loopStyle + "_" + name.substr(0, name.find('_')) + ".wav";
            if (!writeWav(out, pcm, loopRate, 2)) return 1;
            std::printf("%s: %.1f s\n", out.c_str(), song->length);
            continue;
        }
        if (itOut) {
            music::ItSettings st;
            st.title = name.substr(name.find('_') + 1);
            st.title = st.title.substr(0, st.title.rfind('_'));
            st.bpm = tempo;
            music::ItReport rep;
            const auto it = music::songToIt(*song, st, &rep);
            const std::string out = "song" + name.substr(0, name.find('_')) + ".it";
            if (!writeFile(out, it)) return 1;
            std::printf("%s: %s, %d rows/beat, %d patterns, %d notes (%d dropped), %zu sample bytes, %zu bytes\n",
                        out.c_str(), st.title.c_str(), rep.rowsPerBeat, rep.patterns, rep.notes, rep.dropped,
                        rep.sampleBytes, it.size());
            continue;
        }
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
