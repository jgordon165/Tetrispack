// The music player and its three engines on their own: every engine plays
// in tune, loops wrap, fades and stops end in silence, pause holds still.

#include "music/player.h"
#include "music/sfx.h"
#include "music/soundfont.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <memory>
#include <vector>

using namespace music;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static std::vector<int16_t> render(Player& p, double seconds, int rate = 48000) {
    std::vector<int16_t> buf(size_t(seconds * rate) * 2, 0);
    p.render(buf.data(), int(buf.size() / 2), rate);
    return buf;
}

static double rms(Player& p, double seconds) {
    const auto buf = render(p, seconds);
    double sum = 0;
    for (int16_t v : buf) sum += double(v) * v;
    return std::sqrt(sum / double(buf.size()));
}

// A pure tone for each engine.
static Instrument tone(Engine e) {
    Instrument in;
    switch (e) {
        case Engine::Opl3:
            in.kind = Kind::Opl;
            in.opl.mod.tl = 63;
            break;
        case Engine::Genesis:
            in.kind = Kind::Fm4;
            in.fm4.alg = 7;                 // all four heard: silence three
            for (int k = 1; k < 4; ++k) in.fm4.op[k].tl = 127;
            break;
        case Engine::Snes:
            in.kind = Kind::Sample;
            in.wave = Wave::Sine;
            break;
        case Engine::Midi:
            in.kind = Kind::Gm;
            in.program = 73;                // Flute: nearly a sine
            break;
    }
    return in;
}

static std::shared_ptr<Song> oneNote(Engine e, Instrument in, int key, double len = 2.0) {
    auto song = std::make_shared<Song>();
    song->engine = e;
    song->instruments.push_back(in);
    song->notes.push_back({0, len, key, 1.0f, 0});
    song->length = len + 1.0;
    song->echoVolume = 0;
    return song;
}

// Rising zero crossings in the second second (past the attack).
static int measuredHz(std::shared_ptr<Song> song) {
    Player p;
    p.setVolume(1);
    p.play(song, 0);
    render(p, 1.0);
    const auto buf = render(p, 1.0);
    int crossings = 0;
    for (size_t i = 1; i < buf.size() / 2; ++i)
        if (buf[2 * (i - 1)] < 0 && buf[2 * i] >= 0) ++crossings;
    return crossings;
}

// Pitch by autocorrelation (for sampled instruments rich in harmonics).
static double autocorrHz(std::shared_ptr<Song> song) {
    Player p;
    p.setVolume(1);
    p.play(song, 0);
    render(p, 0.5);
    const auto buf = render(p, 0.5);
    const int rate = 48000, n = rate / 4;
    int best = 0;
    double bestC = -1;
    for (int lag = rate / 2000; lag < rate / 60; ++lag) {
        double c = 0;
        for (int i = 0; i < n; ++i) c += double(buf[2 * i]) * buf[2 * (i + lag)];
        if (c > bestC) { bestC = c; best = lag; }
    }
    return double(rate) / best;
}

int main(int, char** argv) {
    // Windows MIDI: only with the SoundFont the build downloads next to the tests.
    std::string sf = argv[0];
    sf = sf.substr(0, sf.find_last_of("/\\") + 1) + "soundfont/GeneralUser-GS.sf2";
    const bool haveSf = loadSoundFont(sf);
    if (haveSf) {
        auto song = oneNote(Engine::Midi, tone(Engine::Midi), 69);
        song->soundfont = sf;
        const double hz = autocorrHz(song);
        std::printf("midi: A4 %.1f Hz\n", hz);
        CHECK(std::abs(hz - 440) <= 5);
    } else {
        std::printf("midi: no SoundFont at %s, skipped\n", sf.c_str());
    }

    for (Engine e : {Engine::Opl3, Engine::Genesis, Engine::Snes}) {
        const char* name = e == Engine::Opl3 ? "opl3" : e == Engine::Genesis ? "genesis" : "snes";
        const int a4 = measuredHz(oneNote(e, tone(e), 69)), a2 = measuredHz(oneNote(e, tone(e), 45));
        std::printf("%s: A4 %d Hz, A2 %d Hz\n", name, a4, a2);
        CHECK(std::abs(a4 - 440) <= 3);
        CHECK(std::abs(a2 - 110) <= 2);
    }
    {   // The PSG's square wave.
        Instrument sq;
        sq.kind = Kind::PsgTone;
        sq.env = {0.001f, 0.1f, 1.0f, 0.05f};
        const int hz = measuredHz(oneNote(Engine::Genesis, sq, 69));
        std::printf("psg: A4 %d Hz\n", hz);
        CHECK(std::abs(hz - 440) <= 3);
    }

    // Looping, volume, pause, switching and stopping (on each engine).
    for (Engine e : {Engine::Opl3, Engine::Genesis, Engine::Snes}) {
        auto song = std::make_shared<Song>();
        song->engine = e;
        song->instruments.push_back(tone(e));
        for (int i = 0; i < 4; ++i) song->notes.push_back({i * 0.25, 0.2, 60 + i * 2, 1.0f, 0});
        song->notes.push_back({0, 1.0, 48, 0.5f, 0});
        song->length = 1.0;

        Player p;
        p.setVolume(1);
        CHECK(rms(p, 0.1) == 0);
        p.play(song, 0);
        const double loud = rms(p, 0.5);
        CHECK(loud > 300);
        rms(p, 0.7);
        CHECK(p.position() < 0.5);          // looped
        CHECK(rms(p, 0.3) > 300);

        p.setVolume(0.2f);
        CHECK(rms(p, 0.5) < loud * 0.5);
        p.setVolume(1);

        p.setPaused(true);
        const double pos = p.position();
        CHECK(rms(p, 0.2) == 0);
        CHECK(p.position() == pos);
        p.setPaused(false);

        p.play(song, 0.2);
        rms(p, 0.3);
        CHECK(p.playing() && p.position() < 0.2);

        p.stop(0.1);
        rms(p, 0.3);
        CHECK(!p.playing());
        rms(p, 1.5);                         // release tails and echo die away
        CHECK(rms(p, 0.2) < 1);
    }

    {   // Sound effects: they sound, mix over what's there, and end.
        Sfx fx;
        fx.prepare(48000);
        CHECK(!fx.play("nope"));
        for (const char* name : {"rotate", "place", "clear", "clear_big"}) {
            CHECK(fx.play(name));
            std::vector<int16_t> buf(48000 * 2 * 2, 0);
            fx.render(buf.data(), int(buf.size() / 2));
            int peak = 0;
            for (int16_t v : buf) peak = std::max(peak, std::abs(int(v)));
            std::printf("sfx %s: peak %d\n", name, peak);
            CHECK(peak > 2000 && peak < 32767);
            CHECK(!fx.busy());                     // short: done within two seconds
        }
        fx.play("place");
        fx.setVolume(0);
        std::vector<int16_t> buf(4800 * 2, 100);
        fx.render(buf.data(), 4800);
        CHECK(buf[0] == 100 && buf[5000] == 100);   // volume 0: leaves the mix alone
    }

    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::printf("music: all passed\n");
    return 0;
}
