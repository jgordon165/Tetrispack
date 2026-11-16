// A song as the music player takes it: instruments for one sound engine,
// and the notes. Built in Lua (scripts/music/jukebox.lua) and handed over
// through music/song_lua.h.
#pragma once

#include <cstdint>
#include <vector>

namespace music {

// The three sound engines a song can be played on.
enum class Engine {
    Opl3,     // Sound Blaster 16 FM (ymfm YMF262): 18 two-operator voices
    Genesis,  // Mega Drive: YM2612 (ymfm) six four-operator FM voices + SN76489 PSG
    Snes,     // Super NES style: eight sampled voices, Gaussian-ish interpolation, echo
};

// OPL two-operator patch (OPL register fields).
struct OplOperator {
    uint8_t mult = 1, tl = 0, ar = 15, dr = 0, sl = 0, rr = 7, ws = 0, ksl = 0;
    bool vib = false, am = false, sustain = true, ksr = false;
};
struct OplPatch {
    OplOperator mod, car;
    uint8_t feedback = 0;
    bool additive = false;
};

// YM2612 four-operator patch (OPN register fields), operators 1-4.
struct Fm4Operator {
    uint8_t dt = 0, mul = 1, tl = 0, ks = 0, ar = 31, d1r = 0, d2r = 0, d1l = 0, rr = 15;
    bool am = false;
};
struct Fm4Patch {
    Fm4Operator op[4];
    uint8_t alg = 0, fb = 0, ams = 0, fms = 0;
};

// Software envelope (PSG and samples): attack, decay to sustain, release.
struct Envelope {
    float attack = 0.002f, decay = 0.3f, sustain = 1.0f, release = 0.08f;  // seconds; sustain 0-1
};

// The SNES-style engine's built-in waveforms (generated, not sampled).
enum class Wave {
    Pulse12, Pulse25, Pulse50, Saw, Triangle, Sine, Organ, Strings, EPiano, Bell, Brass,
    Kick, Snare, Hat, Crash, Pluck, Tom, Count
};

enum class Kind { Opl, Fm4, PsgTone, PsgNoise, Sample };

struct Instrument {
    Kind kind = Kind::Opl;
    OplPatch opl;
    Fm4Patch fm4;
    Envelope env;              // PSG and samples
    Wave wave = Wave::Pulse25; // samples
    float noiseHz = 8000;      // PSG noise: shift rate
    float vibrato = 0;         // semitones (PSG and samples; FM uses its LFO)
    float gain = 1;            // 0-1
    float pan = 0;             // -1 left .. 1 right (Genesis: hard left/centre/right)
    float echo = 0;            // SNES: send to the echo, 0-1
    int fixedKey = -1;         // drums: always this MIDI key
};

struct Note {
    double start = 0, length = 0;  // seconds
    int key = 60;                  // MIDI key
    float velocity = 1;            // 0-1
    int instrument = 0;            // index into Song::instruments
};

struct Song {
    Engine engine = Engine::Opl3;
    std::vector<Instrument> instruments;
    std::vector<Note> notes;
    double length = 0;             // seconds; the song loops here
    // SNES echo: delay, feedback, volume.
    float echoDelay = 0.24f, echoFeedback = 0.45f, echoVolume = 0.35f;
};

}  // namespace music
