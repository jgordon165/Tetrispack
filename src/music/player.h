// Plays songs (music/song.h) on their sound engine (music/backend.h):
// sequencing, looping, fades between songs, pause, volume, and resampling
// to the output rate. No SDL, so the tests render it directly; Audio
// (audio.cpp) calls render() from the audio thread.
#pragma once

#include "music/backend.h"
#include "music/song.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace music {

class Player {
public:
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

private:
    struct Event { uint64_t at; bool on; uint32_t note; };
    void start(std::shared_ptr<const Song> song);
    void engineFrame(float& l, float& r);

    std::unique_ptr<Backend> backend_;
    Engine engine_ = Engine::Opl3;
    std::shared_ptr<const Song> song_, next_;
    std::vector<Event> events_;
    size_t nextEvent_ = 0;
    uint64_t pos_ = 0, length_ = 0;      // engine samples
    double rate_ = 1;
    bool paused_ = false;
    float volume_ = 0.7f;
    float gain_ = 1, fadeStep_ = 0;
    bool stopping_ = false;
    double frac_ = 1;                    // resampler read position
    float prevL_ = 0, prevR_ = 0, curL_ = 0, curR_ = 0;
};

}  // namespace music
