#include "music/sfx.h"

#include <algorithm>
#include <cmath>

namespace music {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr float kGain = 14000.0f;
}  // namespace

void Sfx::prepare(int rate) {
    rate_ = rate;
    effects_.clear();
    voices_.clear();
    effects_.reserve(4);
    voices_.reserve(16);   // no allocations on the audio thread later
    unsigned seed = 0x1234567u;
    auto noise = [&]() { seed = seed * 1103515245u + 12345u; return float((seed >> 9) & 0xFFFF) / 32768.0f - 1.0f; };
    auto make = [&](const char* name, double seconds, auto gen) {
        Effect e;
        e.name = name;
        const int n = int(seconds * rate);
        e.pcm.resize(size_t(n));
        for (int i = 0; i < n; ++i) e.pcm[size_t(i)] = gen(double(i) / rate);
        effects_.push_back(std::move(e));
    };

    // Rotate: a quick rising blip, a soft square with its edges rounded off.
    double ph = 0;
    make("rotate", 0.06, [&](double t) {
        const double hz = 700 + 900 * (t / 0.06);
        ph += 2 * kPi * hz / rate;
        const double sq = std::tanh(4.0 * std::sin(ph));         // rounded square
        const double env = std::min(1.0, t / 0.003) * std::exp(-t / 0.025);
        return float(0.6 * sq * env);
    });

    // Place: a low thud that drops in pitch, with a short knock on top.
    ph = 0;
    float lp = 0;
    make("place", 0.16, [&](double t) {
        const double hz = 55 + 110 * std::exp(-t / 0.02);
        ph += 2 * kPi * hz / rate;
        lp += 0.35f * (noise() - lp);
        const double thud = std::sin(ph) * std::exp(-t / 0.05);
        const double knock = lp * std::exp(-t / 0.012);
        return float(0.7 * thud + 0.45 * knock);
    });
}

bool Sfx::play(const std::string& name, float gain) {
    for (const Effect& e : effects_) {
        if (e.name != name) continue;
        if (voices_.size() >= 16) voices_.erase(voices_.begin());   // drop the oldest
        voices_.push_back({&e, 0, gain});
        return true;
    }
    return false;
}

void Sfx::render(int16_t* out, int frames) {
    for (auto& v : voices_) {
        const float g = v.gain * volume_ * kGain;
        const size_t n = std::min(v.fx->pcm.size() - v.pos, size_t(frames));
        for (size_t i = 0; i < n; ++i) {
            const int s = int(std::lround(v.fx->pcm[v.pos + i] * g));
            for (int c = 0; c < 2; ++c) {
                int16_t& d = out[2 * i + size_t(c)];
                d = int16_t(std::clamp(int(d) + s, -32768, 32767));
            }
        }
        v.pos += n;
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice& v) { return v.pos >= v.fx->pcm.size(); }),
                  voices_.end());
}

}  // namespace music
