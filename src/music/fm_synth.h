// The music: songs of FM notes played on an emulated OPL3 (ymfm's YMF262,
// BSD-3), the Sound Blaster sound of Windows 3.x MIDI. The songs and their
// instrument patches are written in Lua (scripts/music/) and handed over as
// a list of notes; this plays them, looping, with fades.
//
// No SDL, so the tests render it directly; Audio (audio.cpp) calls render()
// from the audio thread.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace music {

// One operator of a two-operator FM voice (OPL register fields).
struct Operator {
    uint8_t mult = 1, tl = 0, ar = 15, dr = 0, sl = 0, rr = 7, ws = 0, ksl = 0;
    bool vib = false, am = false, sustain = true, ksr = false;
};

struct Patch {
    Operator mod, car;
    uint8_t feedback = 0;   // 0-7
    bool additive = false;  // connection: false = FM (mod -> car), true = both heard
    int fixedKey = -1;      // drums: always this MIDI key, whatever the note says
};

struct Note {
    double start = 0, length = 0;  // seconds
    int key = 60;                  // MIDI key
    float velocity = 1;            // 0-1
    int patch = 0;                 // index into Song::patches
};

struct Song {
    std::vector<Patch> patches;
    std::vector<Note> notes;
    double length = 0;             // seconds; the song loops here
};

class FmSynth {
public:
    FmSynth();
    ~FmSynth();

    // Starts `song` (looping). If one is playing it fades out over `fade`
    // seconds first.
    void play(std::shared_ptr<const Song> song, double fade = 0.6);
    void stop(double fade = 0.6);
    void setPaused(bool p) { paused_ = p; }
    void setVolume(float v) { volume_ = v < 0 ? 0 : v > 1 ? 1 : v; }
    bool playing() const { return song_ != nullptr; }
    double position() const;       // seconds into the current song

    // Adds `frames` stereo frames at `rate` Hz to `out` (interleaved, saturating).
    void render(int16_t* out, int frames, int rate);

    static double chipRate();      // the OPL3's own rate, 49716 Hz

private:
    struct Chip;
    struct Event { uint64_t at; bool on; uint32_t note; };
    void start(std::shared_ptr<const Song> song);
    void allOff();
    void noteOn(uint32_t index);
    void noteOff(uint32_t index);
    void writeReg(int bank, uint8_t reg, uint8_t val);
    void chipFrame(float& l, float& r);

    std::unique_ptr<Chip> chip_;
    std::shared_ptr<const Song> song_, next_;
    std::vector<Event> events_;
    size_t nextEvent_ = 0;
    uint64_t pos_ = 0, length_ = 0;      // chip samples
    struct Voice { int32_t note = -1; uint64_t since = 0; bool keyed = false; };
    Voice voices_[18];
    bool paused_ = false;
    float volume_ = 0.7f;
    float gain_ = 1, fadeStep_ = 0;      // per chip sample
    bool stopping_ = false;
    double frac_ = 0;                    // resampler read position
    float prevL_ = 0, prevR_ = 0, curL_ = 0, curR_ = 0;
};

}  // namespace music
