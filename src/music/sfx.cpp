#include "music/sfx.h"

#include <algorithm>
#include <cmath>

namespace music {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr float kGain = 16000.0f;
}  // namespace

void Sfx::prepare(int rate) {
    rate_ = rate;
    effects_.clear();
    voices_.clear();
    effects_.reserve(8);
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

    // Both run through a bit-crusher: held at about 11 kHz and quantized to
    // a few bits, the chunky texture of early-90s console effects.
    auto crush = [&](std::vector<float>& pcm, double holdHz, int bits) {
        const int hold = std::max(1, int(rate / holdHz));
        const float steps = float(1 << (bits - 1));
        float held = 0;
        for (size_t i = 0; i < pcm.size(); ++i) {
            if (i % size_t(hold) == 0) held = std::round(pcm[i] * steps) / steps;
            pcm[i] = held;
        }
    };
    auto sat = [](double x, double drive) { return std::tanh(x * drive) / std::tanh(drive); };

    // A struck bell, mixed in clean after the crush: a bell's partials (the
    // hum and the fundamental, the minor third, the fifth, the nominal...)
    // at their inharmonic ratios, the higher ones dying away faster.
    auto bell = [&](std::vector<float>& pcm, double hz, double seconds, float gain, double at = 0) {
        static const double ratio[] = {0.5, 1.0, 1.183, 1.506, 2.0, 2.514, 2.662, 3.011};
        static const double amp[] = {0.35, 1.0, 0.45, 0.3, 0.5, 0.18, 0.15, 0.1};
        const size_t first = size_t(at * rate), n = size_t(seconds * rate);
        if (pcm.size() < first + n) pcm.resize(first + n, 0.0f);
        for (size_t j = 0; j < n; ++j) {
            const size_t i = first + j;
            const double t = double(j) / rate;
            double v = 0;
            for (int k = 0; k < 8; ++k) {
                const double decay = seconds * 0.45 / (0.6 + ratio[k]);
                v += amp[k] * std::sin(2 * kPi * hz * ratio[k] * t) * std::exp(-t / decay);
            }
            pcm[i] += float(gain * v * std::min(1.0, t / 0.001) / 2.2);
        }
    };

    // Rotate: a short mechanical "chk" - a low, driven square that drops a
    // little, under a burst of filtered noise.
    double ph = 0;
    float lp = 0;
    make("rotate", 0.085, [&](double t) {
        const double hz = 180 + 140 * std::exp(-t / 0.012);
        ph += 2 * kPi * hz / rate;
        lp += 0.45f * (noise() - lp);
        const double body = sat(std::sin(ph), 3.0) * std::exp(-t / 0.03);
        const double click = lp * std::exp(-t / 0.008);
        const double env = std::min(1.0, t / 0.0015);
        return float(env * (0.55 * body + 0.6 * click));
    });
    crush(effects_.back().pcm, 11025, 6);
    bell(effects_.back().pcm, 1318.5, 0.35, 0.35);   // a small high ting (E6)

    // Place: a heavy landing in layers - a sub thump falling from 110 to
    // 38 Hz, a saturated mid body, a crunchy noise hit, and a smaller
    // knock 45 ms later as the piece settles.
    double ph1 = 0, ph2 = 0;
    lp = 0;
    float lp2 = 0;
    make("place", 0.30, [&](double t) {
        const double subHz = 38 + 72 * std::exp(-t / 0.035);
        const double midHz = 95 + 120 * std::exp(-t / 0.015);
        ph1 += 2 * kPi * subHz / rate;
        ph2 += 2 * kPi * midHz / rate;
        lp += 0.3f * (noise() - lp);           // darker noise for the hit
        lp2 += 0.6f * (noise() - lp2);
        const double sub = std::sin(ph1) * std::exp(-t / 0.11);
        const double mid = sat(std::sin(ph2), 4.0) * std::exp(-t / 0.045);
        const double hit = lp * std::exp(-t / 0.02);
        const double t2 = t - 0.045;
        const double settle = t2 > 0 ? lp2 * std::exp(-t2 / 0.012) * 0.5 + std::sin(2 * kPi * 160 * t2) * std::exp(-t2 / 0.02) * 0.35 : 0.0;
        const double env = std::min(1.0, t / 0.001);
        return float(sat(env * (0.75 * sub + 0.45 * mid + 0.7 * hit + settle), 1.6) * 0.95);
    });
    crush(effects_.back().pcm, 11025, 7);
    bell(effects_.back().pcm, 784.0, 0.7, 0.4);      // a fuller dong (G5)

    // Clear (one or two rows): a crunchy rising whoosh under a quick bell
    // chime, G5 C6 E6.
    lp = 0;
    make("clear", 0.25, [&](double t) {
        lp += float(0.1 + 0.5 * t / 0.25) * (noise() - lp);       // opening up as it rises
        const double env = std::min(1.0, t / 0.05) * std::exp(-t / 0.09);
        return float(lp * env * 0.8);
    });
    crush(effects_.back().pcm, 11025, 6);
    {
        auto& pcm = effects_.back().pcm;
        const double notes[] = {784.0, 1046.5, 1318.5};
        for (int k = 0; k < 3; ++k) bell(pcm, notes[k], k == 2 ? 0.9 : 0.5, 0.55f, 0.06 * k);
    }

    // Big clear (three rows or more): a low crunchy boom, a five-note bell
    // run up to G6, and the top note again an octave up, shimmering out.
    ph = 0;
    lp = 0;
    make("clear_big", 0.6, [&](double t) {
        const double hz = 45 + 90 * std::exp(-t / 0.06);
        ph += 2 * kPi * hz / rate;
        lp += 0.25f * (noise() - lp);
        const double boom = sat(std::sin(ph), 3.0) * std::exp(-t / 0.2);
        const double rush = lp * std::min(1.0, t / 0.02) * std::exp(-t / 0.15);
        return float(sat(0.8 * boom + 0.7 * rush, 1.5) * 0.9);
    });
    crush(effects_.back().pcm, 11025, 6);
    {
        auto& pcm = effects_.back().pcm;
        const double notes[] = {784.0, 1046.5, 1318.5, 1568.0, 2093.0};
        for (int k = 0; k < 5; ++k) bell(pcm, notes[k], k == 4 ? 1.6 : 0.6, 0.4f, 0.055 * k);
        bell(pcm, 3136.0, 1.4, 0.18f, 0.33);                       // sparkle, an octave above the top
        bell(pcm, 523.25, 1.2, 0.3f, 0.0);                          // and a low C under it all
    }
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
