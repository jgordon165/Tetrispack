// Super NES style: eight sampled voices like the S-DSP — 32 kHz, a soft
// four-point interpolation, per-voice envelopes and pan, and the echo
// (a delay with feedback through an eight-tap low-pass filter) that gives
// SNES music its space. The instrument samples are generated here
// (waveforms and synthesized drums), not taken from any game.
#include "music/backend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace music {

namespace {

constexpr double kRate = 32000.0;
constexpr int kCycle = 256;             // looped waveforms: one cycle
constexpr float kVoiceGain = 12000.0f;
constexpr double kPi = 3.14159265358979323846;
// Echo FIR: a gentle low-pass, as games usually set it.
constexpr std::array<float, 8> kFir = {0.05f, 0.10f, 0.16f, 0.19f, 0.19f, 0.15f, 0.10f, 0.06f};

struct Sample {
    std::vector<float> data;
    bool loop = true;
    int baseKey = 69;        // one-shots: the key that plays at the recorded pitch
    double baseHz = 0;       // loops: cycle frequency at step 1
};

class Snes : public Backend {
public:
    Snes() { build(); }
    double rate() const override { return kRate; }

    void reset(const Song& song) override {
        allOff();
        delay_ = std::clamp(int(song.echoDelay * kRate), 1, int(kRate));
        feedback_ = song.echoFeedback;
        echoVol_ = song.echoVolume;
        echoL_.assign(size_t(delay_), 0.0f);
        echoR_.assign(size_t(delay_), 0.0f);
        echoPos_ = 0;
        histL_.fill(0);
        histR_.fill(0);
    }

    void noteOn(int id, const Note& n, const Instrument& inst) override {
        if (inst.kind != Kind::Sample) return;
        int best = -1;
        for (int i = 0; i < 8; ++i)
            if (voice_[i].env.stage == EnvState::Off && (best < 0 || voice_[i].since < voice_[best].since)) best = i;
        if (best < 0)
            for (int i = 0; i < 8; ++i)
                if (voice_[i].env.stage == EnvState::Release && (best < 0 || voice_[i].since < voice_[best].since)) best = i;
        if (best < 0)
            for (int i = 0; i < 8; ++i)
                if (best < 0 || voice_[i].since < voice_[best].since) best = i;
        Voice& v = voice_[best];
        v = Voice{};
        v.id = id;
        v.since = ++clock_;
        v.sample = &samples_[size_t(inst.wave)];
        const int key = inst.fixedKey >= 0 ? inst.fixedKey : n.key;
        v.step = v.sample->loop ? keyHz(key) * kCycle / kRate
                                : std::pow(2.0, (key - v.sample->baseKey) / 12.0);
        v.vel = std::clamp(n.velocity * inst.gain, 0.0f, 1.0f);
        v.envelope = inst.env;
        v.vibrato = inst.vibrato;
        const float pan = std::clamp(inst.pan, -1.0f, 1.0f);
        v.gl = std::sqrt(0.5f * (1 - pan)) * 1.4142f;
        v.gr = std::sqrt(0.5f * (1 + pan)) * 1.4142f;
        v.echo = inst.echo;
        v.env.start();
    }

    void noteOff(int id) override {
        for (auto& v : voice_)
            if (v.id == id) v.env.release();
    }

    void allOff() override {
        for (auto& v : voice_) v = Voice{};
    }

    void frame(float& l, float& r) override {
        const float dt = float(1.0 / kRate);
        float dl = 0, dr = 0, el = 0, er = 0;
        for (auto& v : voice_) {
            if (v.env.stage == EnvState::Off || !v.sample) continue;
            const float e = v.env.step(v.envelope, dt);
            const auto& d = v.sample->data;
            const size_t n = d.size();
            if (!v.sample->loop && v.pos >= double(n - 1)) { v.env.stage = EnvState::Off; continue; }
            // Four-point B-spline interpolation: soft, like the S-DSP's Gaussian filter.
            const size_t i = size_t(v.pos);
            const float t = float(v.pos - double(i));
            auto at = [&](size_t k) {
                if (v.sample->loop) return d[k % n];
                return k < n ? d[k] : 0.0f;
            };
            const float s0 = at(i == 0 ? (v.sample->loop ? n - 1 : 0) : i - 1), s1 = at(i), s2 = at(i + 1), s3 = at(i + 2);
            const float t2 = t * t, t3 = t2 * t;
            const float s = ((1 - t) * (1 - t) * (1 - t) * s0 + (3 * t3 - 6 * t2 + 4) * s1 +
                             (-3 * t3 + 3 * t2 + 3 * t + 1) * s2 + t3 * s3) / 6.0f;
            v.t += dt;
            double step = v.step;
            if (v.vibrato > 0 && v.t > 0.15f)   // delayed vibrato, as composers liked
                step *= std::pow(2.0, v.vibrato * std::sin(2 * kPi * 5.0 * v.t) / 12.0);
            v.pos += step;
            if (v.sample->loop && v.pos >= double(n)) v.pos -= double(n) * std::floor(v.pos / double(n));
            const float a = s * e * v.vel * kVoiceGain;
            dl += a * v.gl;
            dr += a * v.gr;
            el += a * v.gl * v.echo;
            er += a * v.gr * v.echo;
        }
        // Echo: read the delayed signal through the FIR, feed back.
        const float inL = echoL_[size_t(echoPos_)], inR = echoR_[size_t(echoPos_)];
        for (int k = 7; k > 0; --k) { histL_[size_t(k)] = histL_[size_t(k - 1)]; histR_[size_t(k)] = histR_[size_t(k - 1)]; }
        histL_[0] = inL;
        histR_[0] = inR;
        float fl = 0, fr = 0;
        for (int k = 0; k < 8; ++k) { fl += histL_[size_t(k)] * kFir[size_t(k)]; fr += histR_[size_t(k)] * kFir[size_t(k)]; }
        echoL_[size_t(echoPos_)] = el + fl * feedback_;
        echoR_[size_t(echoPos_)] = er + fr * feedback_;
        echoPos_ = (echoPos_ + 1) % delay_;
        l = dl + fl * echoVol_;
        r = dr + fr * echoVol_;
    }

private:
    void build() {
        samples_.resize(size_t(Wave::Count));
        auto cycle = [&](Wave w, auto harmonic, int count) {
            Sample s;
            s.data.assign(kCycle, 0.0f);
            for (int n = 1; n <= count; ++n) {
                const double a = harmonic(n);
                if (a == 0) continue;
                for (int i = 0; i < kCycle; ++i) s.data[size_t(i)] += float(a * std::sin(2 * kPi * n * i / kCycle));
            }
            float peak = 0;
            for (float x : s.data) peak = std::max(peak, std::fabs(x));
            for (float& x : s.data) x /= peak;
            samples_[size_t(w)] = s;
        };
        auto pulse = [&](Wave w, double duty) {
            // A band-limited pulse: cosine series of a pulse train.
            Sample s;
            s.data.assign(kCycle, 0.0f);
            for (int n = 1; n <= 40; ++n) {
                const double a = std::sin(n * kPi * duty) / n;
                for (int i = 0; i < kCycle; ++i)
                    s.data[size_t(i)] += float(a * std::cos(2 * kPi * n * (double(i) / kCycle - duty / 2)));
            }
            float mean = 0;
            for (float x : s.data) mean += x;
            mean /= kCycle;
            float peak = 0;
            for (float& x : s.data) { x -= mean; peak = std::max(peak, std::fabs(x)); }
            for (float& x : s.data) x /= peak;
            samples_[size_t(w)] = s;
        };
        pulse(Wave::Pulse12, 0.125);
        pulse(Wave::Pulse25, 0.25);
        pulse(Wave::Pulse50, 0.5);
        cycle(Wave::Saw, [](int n) { return 1.0 / n; }, 40);
        cycle(Wave::Triangle, [](int n) { return n % 2 ? ((n / 2) % 2 ? -1.0 : 1.0) / (n * n) : 0.0; }, 31);
        cycle(Wave::Sine, [](int n) { return n == 1 ? 1.0 : 0.0; }, 1);
        cycle(Wave::Organ, [](int n) { const double a[] = {0, 1, 0.55, 0.35, 0.25, 0, 0.12, 0, 0.1}; return n < 9 ? a[n] : 0.0; }, 8);
        cycle(Wave::Strings, [](int n) { return std::pow(double(n), -1.35); }, 24);
        cycle(Wave::EPiano, [](int n) { const double a[] = {0, 1, 0.3, 0.12, 0.05}; return n < 5 ? a[n] : 0.0; }, 4);
        cycle(Wave::Bell, [](int n) { const double a[] = {0, 1, 0, 0.45, 0, 0, 0.3, 0, 0, 0, 0.15}; return n < 11 ? a[n] : 0.0; }, 10);
        cycle(Wave::Brass, [](int n) { return n <= 14 ? std::pow(double(n), -0.85) : 0.0; }, 14);

        // One-shots, synthesized.
        unsigned seed = 12345;
        auto noise = [&]() { seed = seed * 1103515245u + 12345u; return float((seed >> 9) & 0xFFFF) / 32768.0f - 1.0f; };
        auto oneShot = [&](Wave w, double seconds, int baseKey, auto gen) {
            Sample s;
            s.loop = false;
            s.baseKey = baseKey;
            const int n = int(seconds * kRate);
            s.data.resize(size_t(n));
            for (int i = 0; i < n; ++i) s.data[size_t(i)] = gen(double(i) / kRate);
            samples_[size_t(w)] = s;
        };
        double ph = 0;
        oneShot(Wave::Kick, 0.30, 60, [&](double t) {
            const double hz = 45 + 120 * std::exp(-t / 0.035);
            ph += 2 * kPi * hz / kRate;
            return float(std::sin(ph) * std::exp(-t / 0.13) * 1.1);
        });
        float lp = 0;
        ph = 0;
        oneShot(Wave::Snare, 0.25, 60, [&](double t) {
            lp += 0.55f * (noise() - lp);
            ph += 2 * kPi * 185 / kRate;
            return float(lp * std::exp(-t / 0.07) * 0.9 + std::sin(ph) * std::exp(-t / 0.03) * 0.6);
        });
        float prev = 0;
        oneShot(Wave::Hat, 0.08, 60, [&](double t) {
            const float x = noise();
            const float hp = x - prev;
            prev = x;
            return float(hp * 0.6 * std::exp(-t / 0.018));
        });
        prev = 0;
        oneShot(Wave::Crash, 1.4, 60, [&](double t) {
            const float x = noise();
            const float hp = x - 0.6f * prev;
            prev = x;
            return float(hp * 0.45 * std::exp(-t / 0.45));
        });
        ph = 0;
        oneShot(Wave::Tom, 0.35, 60, [&](double t) {
            const double hz = 95 + 90 * std::exp(-t / 0.06);
            ph += 2 * kPi * hz / kRate;
            return float(std::sin(ph) * std::exp(-t / 0.16));
        });
        {   // Karplus-Strong pluck at A3.
            const int period = int(kRate / 220.0);
            std::vector<float> ring(static_cast<size_t>(period), 0.0f);
            for (auto& x : ring) x = noise();
            Sample s;
            s.loop = false;
            s.baseKey = 57;
            const int n = int(1.6 * kRate);
            s.data.resize(size_t(n));
            for (int i = 0; i < n; ++i) {
                const size_t a = size_t(i % period), b = size_t((i + 1) % period);
                const float out = ring[a];
                ring[a] = 0.497f * (ring[a] + ring[b]);
                s.data[size_t(i)] = out * 0.9f;
            }
            samples_[size_t(Wave::Pluck)] = s;
        }
    }

    struct Voice {
        int id = -1;
        uint64_t since = 0;
        const Sample* sample = nullptr;
        double pos = 0, step = 1;
        Envelope envelope;
        EnvState env;
        float vel = 0, gl = 1, gr = 1, echo = 0, vibrato = 0, t = 0;
    };
    std::vector<Sample> samples_;
    Voice voice_[8];
    uint64_t clock_ = 0;
    std::vector<float> echoL_ = std::vector<float>(1, 0.0f), echoR_ = std::vector<float>(1, 0.0f);
    std::array<float, 8> histL_{}, histR_{};
    int delay_ = 1, echoPos_ = 0;
    float feedback_ = 0.4f, echoVol_ = 0.3f;
};

}  // namespace

std::unique_ptr<Backend> makeSnes() { return std::make_unique<Snes>(); }

}  // namespace music
