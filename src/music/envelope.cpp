// What every engine shares (music/backend.h): note pitches and the
// software envelope. Apart from backend.cpp's Backend::create, so a host
// with fewer engines (the Xbox 360's backend_snes.cpp) links only these.
#include "music/backend.h"

#include <cmath>

namespace music {

double keyHz(int key) { return 440.0 * std::pow(2.0, (key - 69) / 12.0); }

float EnvState::step(const Envelope& e, float dt) {
    switch (stage) {
        case Off: level = 0; break;
        case Attack:
            level += e.attack > 0 ? dt / e.attack : 1.0f;
            if (level >= 1) { level = 1; stage = Decay; }
            break;
        case Decay: {
            // Exponential approach to the sustain level over about `decay` seconds.
            const float k = e.decay > 0 ? std::exp(-dt * 5.0f / e.decay) : 0.0f;
            level = e.sustain + (level - e.sustain) * k;
            if (std::fabs(level - e.sustain) < 0.001f) { level = e.sustain; stage = Sustain; }
            break;
        }
        case Sustain: level = e.sustain; break;
        case Release: {
            const float k = e.release > 0 ? std::exp(-dt * 5.0f / e.release) : 0.0f;
            level *= k;
            if (level < 0.0005f) { level = 0; stage = Off; }
            break;
        }
    }
    if (stage == Sustain && e.sustain <= 0.0005f) { level = 0; stage = Off; }
    return level;
}

}  // namespace music
