// The FM synth on its own: notes sound, loops wrap, fades and stops end in
// silence, pause holds still.

#include "music/fm_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static double rms(music::FmSynth& s, double seconds, int rate = 48000) {
    std::vector<int16_t> buf(size_t(seconds * rate) * 2, 0);
    s.render(buf.data(), int(buf.size() / 2), rate);
    double sum = 0;
    for (int16_t v : buf) sum += double(v) * v;
    return std::sqrt(sum / double(buf.size()));
}

static std::shared_ptr<music::Song> testSong() {
    auto song = std::make_shared<music::Song>();
    music::Patch p;          // a plain sine-ish organ
    p.mod.tl = 40;
    p.car.tl = 0;
    song->patches.push_back(p);
    for (int i = 0; i < 4; ++i) song->notes.push_back({i * 0.25, 0.2, 60 + i * 2, 1.0f, 0});
    song->notes.push_back({0, 1.0, 48, 0.5f, 0});
    song->length = 1.0;
    return song;
}

// A pure tone (modulator silent) at MIDI key `key`: count rising zero crossings.
static double measuredHz(int key) {
    auto song = std::make_shared<music::Song>();
    music::Patch p;
    p.mod.tl = 63;
    song->patches.push_back(p);
    song->notes.push_back({0, 2.0, key, 1.0f, 0});
    song->length = 3.0;
    music::FmSynth s;
    s.play(song, 0);
    const int rate = 48000;
    std::vector<int16_t> buf(size_t(rate) * 2, 0);
    s.render(buf.data(), rate, rate);
    std::fill(buf.begin(), buf.end(), int16_t(0));
    s.render(buf.data(), rate, rate);   // the second second: past the attack
    int crossings = 0;
    for (int i = 1; i < rate; ++i)
        if (buf[2 * (i - 1)] < 0 && buf[2 * i] >= 0) ++crossings;
    return crossings;
}

int main() {
    CHECK(std::abs(measuredHz(69) - 440) <= 2);    // A4
    CHECK(std::abs(measuredHz(45) - 110) <= 1);    // A2
    CHECK(std::abs(measuredHz(81) - 880) <= 3);    // A5

    music::FmSynth s;
    CHECK(std::abs(music::FmSynth::chipRate() - 49715.9) < 1);
    CHECK(rms(s, 0.1) == 0);                   // nothing playing: silence

    s.play(testSong(), 0);
    CHECK(s.playing());
    const double loud = rms(s, 0.5);
    CHECK(loud > 200);
    rms(s, 0.7);                               // past the 1 s loop point
    CHECK(s.position() < 0.5);
    CHECK(rms(s, 0.3) > 200);                  // still playing after the loop

    s.setVolume(0.2f);
    const double quiet = rms(s, 0.5);
    CHECK(quiet < loud * 0.5);
    s.setVolume(1.0f);

    s.setPaused(true);
    const double pos = s.position();
    CHECK(rms(s, 0.2) == 0);
    CHECK(s.position() == pos);
    s.setPaused(false);

    s.play(testSong(), 0.2);                   // switch with a fade
    rms(s, 0.3);
    CHECK(s.playing());
    CHECK(s.position() < 0.2);                 // the new song started over

    s.stop(0.1);
    rms(s, 0.2);
    CHECK(!s.playing());
    rms(s, 0.5);                               // let the release tails die away
    CHECK(rms(s, 0.2) == 0);

    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::printf("music: all passed (rms %.0f, quiet %.0f)\n", loud, quiet);
    return 0;
}
