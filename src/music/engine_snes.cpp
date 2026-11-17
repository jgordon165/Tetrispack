// Super NES style: eight sampled voices like the S-DSP — 32 kHz, a soft
// four-point interpolation, per-voice envelopes and pan, and the echo
// (a delay with feedback through an eight-tap low-pass filter) that gives
// SNES music its space. The instrument samples are generated here
// (waveforms and synthesized drums), not taken from any game.
#include "music/backend.h"
#include "music/snes_wave.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace music {

namespace {

constexpr double kRate = 32000.0;
constexpr int kCycle = 256;             // looped waveforms: one cycle
constexpr float kVoiceGain = 10500.0f;
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
        if (best < 0)   // all busy: the oldest of the lowest priority gives way
            for (int i = 0; i < 8; ++i)
                if (best < 0 || voice_[i].priority < voice_[best].priority ||
                    (voice_[i].priority == voice_[best].priority && voice_[i].since < voice_[best].since)) best = i;
        // Portamento: slide from the last note this instrument played.
        double from = 0;
        Last* last = nullptr;
        for (auto& l : last_) if (l.inst == &inst) last = &l;
        if (!last) { last_[lastNext_] = Last{&inst, 0}; last = &last_[lastNext_]; lastNext_ = (lastNext_ + 1) % last_.size(); }
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
        v.priority = inst.priority;
        if (inst.glide > 0 && last->step > 0) from = last->step;
        last->step = v.step;
        v.glideRatio = from > 0 ? float(from / v.step) : 1.0f;
        v.glideRate = inst.glide > 0 ? float(1.0 / (inst.glide * kRate)) : 1.0f;
        v.env.start();
    }

    void noteOff(int id) override {
        for (auto& v : voice_)
            if (v.id == id) v.env.release();
    }

    void allOff() override {
        for (auto& v : voice_) v = Voice{};
        last_.fill(Last{});
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
            if (v.glideRatio != 1.0f) {   // glide in log-frequency toward the note
                const float logRatio = std::log(v.glideRatio) * (1.0f - std::min(1.0f, v.glideRate * 6.0f));
                v.glideRatio = std::fabs(logRatio) < 0.0005f ? 1.0f : std::exp(logRatio);
                step *= v.glideRatio;
            }
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

    // For the SNES build's exporter (snesWave below).
    const Sample& sample(Wave w) const { return samples_[size_t(w)]; }

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
        // House organ: drawbars 16', 8', 5 1/3', 4', 2 2/3', 2' (as harmonics of the 16').
        cycle(Wave::M1Organ, [](int n) {
            const double a[] = {0, 1.0, 0.85, 0.6, 0.5, 0, 0.32, 0, 0.25};
            return n < 9 ? a[n] : 0.0;
        }, 8);
        // A round synth/finger bass.
        cycle(Wave::M1Bass, [](int n) { return n <= 12 ? std::pow(double(n), -1.6) * (n % 2 ? 1.0 : 0.6) : 0.0; }, 12);
        // A soft flute / whistle: nearly a sine.
        cycle(Wave::Flute, [](int n) { const double a[] = {0, 1, 0.12, 0.05}; return n < 4 ? a[n] : 0.0; }, 3);

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
            const double hz = 48 + 150 * std::exp(-t / 0.025);
            ph += 2 * kPi * hz / kRate;
            const double click = t < 0.004 ? noise() * (1 - t / 0.004) * 0.6 : 0.0;
            return float(std::tanh(1.6 * std::sin(ph) * std::exp(-t / 0.12)) + click);
        });
        float lp = 0;
        ph = 0;
        oneShot(Wave::Snare, 0.35, 60, [&](double t) {
            lp += 0.62f * (noise() - lp);
            const double hz = 175 + 60 * std::exp(-t / 0.01);
            ph += 2 * kPi * hz / kRate;
            return float(lp * (0.75 * std::exp(-t / 0.09) + 0.2 * std::exp(-t / 0.02)) +
                         std::sin(ph) * std::exp(-t / 0.035) * 0.7);
        });
        float prev = 0;
        oneShot(Wave::Hat, 0.08, 60, [&](double t) {
            const float x = noise();
            const float hp = x - prev;
            prev = x;
            return float(hp * 0.6 * std::exp(-t / 0.018));
        });
        prev = 0;
        oneShot(Wave::OpenHat, 0.30, 60, [&](double t) {
            const float x = noise();
            const float hp = x - prev;
            prev = x;
            return float(hp * 0.55 * std::exp(-t / 0.09));
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
        // An early-90s workstation piano (the bright, chorused house piano):
        // stretched partials that die away faster the higher they are, a
        // hammer click, and a second copy a few cents sharp for the chorus.
        oneShot(Wave::M1Piano, 2.2, 60, [&](double t) {
            const double f0 = 261.63, B = 0.00035;
            double s = 0;
            for (int n = 1; n <= 14; ++n) {
                const double fn = f0 * n * std::sqrt(1 + B * n * n);
                const double a = std::pow(double(n), -1.05) * std::exp(-t * (0.9 + 0.55 * n));
                s += a * (std::sin(2 * kPi * fn * t) + 0.7 * std::sin(2 * kPi * fn * 1.0035 * t + 0.6));
            }
            const double hammer = t < 0.006 ? noise() * (1 - t / 0.006) * 0.4 : 0.0;
            return float(0.42 * s * (1 - std::exp(-t / 0.0015)) + hammer);
        });
        // Marimba: the fundamental, its fourth partial, a short knock.
        oneShot(Wave::Marimba, 1.0, 60, [&](double t) {
            const double f0 = 261.63;
            return float(std::sin(2 * kPi * f0 * t) * std::exp(-t / 0.32) +
                         0.35 * std::sin(2 * kPi * f0 * 4.0 * t) * std::exp(-t / 0.05) +
                         0.12 * std::sin(2 * kPi * f0 * 9.9 * t) * std::exp(-t / 0.015)) * 0.8f;
        });
        ph = 0;
        oneShot(Wave::Conga, 0.4, 60, [&](double t) {
            const double hz = 210 + 50 * std::exp(-t / 0.02);
            ph += 2 * kPi * hz / kRate;
            const double slap = t < 0.01 ? noise() * (1 - t / 0.01) * 0.3 : 0.0;
            return float(std::sin(ph) * std::exp(-t / 0.16) + slap);
        });
        ph = 0;
        oneShot(Wave::Bongo, 0.25, 60, [&](double t) {
            const double hz = 400 + 80 * std::exp(-t / 0.01);
            ph += 2 * kPi * hz / kRate;
            return float(std::sin(ph) * std::exp(-t / 0.07));
        });
        lp = 0;
        oneShot(Wave::Clap, 0.3, 60, [&](double t) {
            lp += 0.5f * (noise() - lp);
            const float band = noise() - lp;                       // keep the middle
            double env = std::exp(-t / 0.09);
            for (double burst : {0.0, 0.011, 0.022})               // three hands, slightly apart
                if (t >= burst && t < burst + 0.009) env += std::exp(-(t - burst) / 0.003);
            return float(band * env * 0.5);
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
        float glideRatio = 1, glideRate = 1;
        int priority = 1;
    };
    struct Last { const Instrument* inst = nullptr; double step = 0; };
    std::array<Last, 16> last_{};
    size_t lastNext_ = 0;
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

SnesWave snesWave(Wave w) {
    static const Snes engine;
    const Sample& s = engine.sample(w);
    return SnesWave{s.data, s.loop, s.baseKey, kRate, kCycle};
}

}  // namespace music
