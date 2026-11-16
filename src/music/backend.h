// One sound engine: turns note-ons and note-offs into stereo frames at its
// own rate. The player (player.h) sequences the song and resamples.
#pragma once

#include "music/song.h"

#include <memory>

namespace music {

class Backend {
public:
    virtual ~Backend() = default;
    virtual double rate() const = 0;
    // A new song: forget every voice, take its instruments and settings.
    virtual void reset(const Song& song) = 0;
    // `id` names the note for its later noteOff.
    virtual void noteOn(int id, const Note& note, const Instrument& inst) = 0;
    virtual void noteOff(int id) = 0;
    virtual void allOff() = 0;
    // One frame, roughly in -32768..32767 per channel.
    virtual void frame(float& l, float& r) = 0;

    static std::unique_ptr<Backend> create(Engine e);
};

// Helpers shared by the engines.
double keyHz(int key);

// Software envelope state (PSG and samples).
struct EnvState {
    enum Stage { Off, Attack, Decay, Sustain, Release } stage = Off;
    float level = 0;
    void start() { stage = Attack; }
    void release() { if (stage != Off) stage = Release; }
    // Advances by dt seconds; returns the level (0-1).
    float step(const Envelope& e, float dt);
};

}  // namespace music
