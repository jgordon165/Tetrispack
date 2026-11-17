// Sound effects: short sounds synthesized once at the output rate and mixed
// over the music. No SDL, so the tests render them directly; Audio
// (audio.cpp) mixes them in the audio thread.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace music {

class Sfx {
public:
    // Builds every effect for output at `rate` Hz.
    void prepare(int rate);
    // Starts effect `name` ("rotate", "place", "clear", "clear_big"); false
    // if there's no such effect.
    bool play(const std::string& name, float gain = 1.0f);
    void setVolume(float v) { volume_ = v < 0 ? 0 : v > 1 ? 1 : v; }
    bool busy() const { return !voices_.empty(); }

    // Adds `frames` stereo frames to `out` (interleaved, saturating).
    void render(int16_t* out, int frames);

private:
    struct Effect { std::string name; std::vector<float> pcm; };
    struct Voice { const Effect* fx; size_t pos; float gain; };
    std::vector<Effect> effects_;
    std::vector<Voice> voices_;
    float volume_ = 0.7f;
    int rate_ = 0;
};

}  // namespace music
