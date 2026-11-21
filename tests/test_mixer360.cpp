// The Xbox 360 host's mixer (consoles/xbox360/src/mixer360.cpp): WAVE
// decoding, resampling, looping, fades and sound effects.
#include "mixer360.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

void put16(std::vector<uint8_t>& v, uint32_t x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }
void put32(std::vector<uint8_t>& v, uint32_t x) { put16(v, x & 0xFFFF); put16(v, x >> 16); }

std::vector<uint8_t> wave(int rate, int channels, const std::vector<int16_t>& pcm) {
    std::vector<uint8_t> v;
    v.insert(v.end(), "RIFF", "RIFF" + 4);
    put32(v, uint32_t(36 + pcm.size() * 2));
    v.insert(v.end(), "WAVE", "WAVE" + 4);
    v.insert(v.end(), "fmt ", "fmt " + 4);
    put32(v, 16);
    put16(v, 1);
    put16(v, uint32_t(channels));
    put32(v, uint32_t(rate));
    put32(v, uint32_t(rate * channels * 2));
    put16(v, uint32_t(channels * 2));
    put16(v, 16);
    v.insert(v.end(), "data", "data" + 4);
    put32(v, uint32_t(pcm.size() * 2));
    for (size_t i = 0; i < pcm.size(); ++i) put16(v, uint16_t(pcm[i]));
    return v;
}

}  // namespace

int main() {
    using x360::Mixer;
    using x360::Sound;

    // Decoding: little-endian samples, either channel count; junk refused.
    std::vector<int16_t> pcm;
    pcm.push_back(1000);
    pcm.push_back(-1000);
    pcm.push_back(-32768);
    pcm.push_back(32767);
    Sound s;
    check(x360::decodeWave(wave(32000, 2, pcm), s), "stereo wave decodes");
    check(s.rate == 32000 && s.channels == 2 && s.frames() == 2, "stereo wave's format");
    check(s.samples[1] == -1000 && s.samples[2] == -32768, "samples kept their sign");
    std::vector<uint8_t> junk(40, 0);
    check(!x360::decodeWave(junk, s), "junk is refused");
    std::vector<uint8_t> cut = wave(44100, 1, pcm);
    cut[40] = 0xFF;   // a data chunk longer than the file
    cut[41] = 0xFF;
    check(!x360::decodeWave(cut, s), "a truncated file is refused");

    // A constant song at half the output rate fills every frame, loops, and
    // its position counts in its own frames.
    Mixer m(48000);
    Sound song;
    song.rate = 24000;
    song.channels = 2;
    song.samples.assign(2 * 2400, 10000);   // 0.1 s
    m.setMusicVolume(1);
    m.setMusic(song, 0);
    check(song.samples.empty(), "setMusic takes the samples");
    std::vector<int16_t> out(2 * 9600);
    m.mix(&out[0], 9600);   // 0.2 s: twice round
    bool steady = true;
    for (size_t i = 0; i < out.size(); ++i) steady = steady && out[i] == 10000;
    check(steady, "a looping song plays without gaps");
    check(std::fabs(m.musicPosition()) < 1e-6, "two loops end back at the start");

    // A fade out lowers the level, then drops the song.
    m.fadeOutMusic(0.1);
    m.mix(&out[0], 2400);
    check(out[2 * 2399] < 5500 && out[2 * 2399] > 4500, "halfway through a fade is about half as loud");
    m.mix(&out[0], 2400);
    check(!m.hasMusic(), "the song is dropped after its fade");
    m.mix(&out[0], 10);
    check(out[0] == 0 && out[19] == 0, "and silence follows");

    // A fade in starts silent.
    Sound again;
    again.rate = 48000;
    again.channels = 1;
    again.samples.assign(4800, 8000);
    m.setMusic(again, 0.05);
    m.mix(&out[0], 4800);
    check(out[0] < 100 && out[2 * 4000] == 8000, "a fade in rises to full");

    // A slow fade in (the game's are 0.8 s) rises too, all the way: its
    // first steps are tiny, and must not be taken for the end of a fade out.
    Sound slow;
    slow.rate = 48000;
    slow.channels = 2;
    slow.samples.assign(2 * 48000, 8000);
    m.setMusic(slow, 0.8);
    std::vector<int16_t> second(2 * 48000);
    m.mix(&second[0], 48000);
    check(second[2 * 100] > 0, "a slow fade in is heard from the start");
    check(second[2 * 19200] > 3500 && second[2 * 19200] < 4500, "halfway through a slow fade in is about half as loud");
    check(second[2 * 47999] == 8000, "a slow fade in ends at full volume");
    m.fadeOutMusic(0.8);
    m.mix(&second[0], 48000);
    check(!m.hasMusic(), "a slow fade out drops the song");

    // Sound effects: mono to both sides, once, scaled, restarted not doubled.
    Mixer fx(48000);
    Sound beep;
    beep.rate = 48000;
    beep.channels = 1;
    beep.samples.assign(480, 20000);
    fx.setSfxVolume(0.5f);
    fx.playSfx(&beep, 1.0f);
    fx.playSfx(&beep, 1.0f);   // the same sound again: restarts it
    fx.mix(&out[0], 960);
    check(out[0] == 10000 && out[1] == 10000, "a mono effect plays on both sides at its volume");
    check(out[2 * 600] == 0, "an effect plays once");
    fx.playSfx(&beep, 1.0f);
    fx.setSfxVolume(4);
    fx.mix(&out[0], 1);
    check(out[0] == 32767, "loud mixes are clipped");

    if (failures) return EXIT_FAILURE;
    std::printf("mixer360: all checks passed\n");
    return EXIT_SUCCESS;
}
