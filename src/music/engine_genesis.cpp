// Mega Drive: ymfm's YM2612 (six four-operator FM voices) and an SN76489
// PSG (three square waves and a noise channel, software envelopes as the
// games' sound drivers did), mixed as the console does.
#include "music/backend.h"

#include "ymfm_opn.h"

#include <algorithm>
#include <cmath>

namespace music {

namespace {

constexpr uint32_t kClock = 7670453;            // NTSC Mega Drive YM2612 clock
constexpr uint8_t kSlot[4] = {0, 8, 4, 12};     // operators 1-4 -> register offsets
// Which operators are heard (carriers) for each algorithm.
constexpr uint8_t kCarriers[8] = {0x8, 0x8, 0x8, 0x8, 0xA, 0xE, 0xE, 0xF};

constexpr float kFmGain = 2.1f;                  // ymfm's 9-bit DAC levels -> our scale
constexpr float kPsgGain = 1550.0f;              // one PSG channel at full volume

// The PSG's 2 dB volume steps.
float psgLevel(float v) {
    if (v <= 0.0001f) return 0;
    const int step = std::clamp(int(std::lround(-20.0 * std::log10(v) / 2.0)), 0, 15);
    return step == 15 ? 0.0f : float(std::pow(10.0, -2.0 * step / 20.0));
}

class Genesis : public Backend, ymfm::ymfm_interface {
public:
    Genesis() {
        opn_.reset();
        write(0, 0x22, 0x08 | 3);   // LFO on (about 6 Hz) for vibrato patches
        write(0, 0x27, 0x00);
        write(0, 0x2B, 0x00);       // DAC off: six FM channels
    }
    double rate() const override { return double(kClock) / 144.0; }

    void reset(const Song&) override { allOff(); }

    void noteOn(int id, const Note& n, const Instrument& inst) override {
        const int key = inst.fixedKey >= 0 ? inst.fixedKey : n.key;
        const float vel = std::clamp(n.velocity * inst.gain, 0.0f, 1.0f);
        if (inst.kind == Kind::Fm4) fmOn(id, key, vel, inst);
        else if (inst.kind == Kind::PsgTone || inst.kind == Kind::PsgNoise) psgOn(id, key, vel, inst);
    }

    void noteOff(int id) override {
        for (int ch = 0; ch < 6; ++ch)
            if (fm_[ch].id == id && fm_[ch].keyed) {
                write(ch / 3, 0x28, uint8_t(((ch / 3) << 2) | (ch % 3)));
                fm_[ch].keyed = false;
                fm_[ch].since = ++clock_;
            }
        for (auto& v : psg_)
            if (v.id == id) v.env.release();
    }

    void allOff() override {
        for (int ch = 0; ch < 6; ++ch) {
            write(ch / 3, 0x28, uint8_t(((ch / 3) << 2) | (ch % 3)));
            fm_[ch] = FmVoice{};
        }
        for (auto& v : psg_) v = PsgVoice{};
    }

    void frame(float& l, float& r) override {
        ymfm::ym2612::output_data o;
        opn_.generate(&o, 1);
        l = float(o.data[0]) * kFmGain;
        r = float(o.data[1]) * kFmGain;

        const float dt = float(1.0 / rate());
        for (int i = 0; i < 4; ++i) {
            PsgVoice& v = psg_[i];
            const float e = v.env.step(v.envelope, dt);
            if (v.env.stage == EnvState::Off) continue;
            v.t += dt;
            float s;
            if (i < 3) {
                const float hz = v.hz * (v.vibrato > 0 ? std::pow(2.0f, v.vibrato * std::sin(6.2832f * 5.5f * v.t) / 12.0f) : 1.0f);
                v.phase += hz / float(rate());
                v.phase -= std::floor(v.phase);
                s = v.phase < 0.5f ? 1.0f : -1.0f;
            } else {
                v.phase += v.hz / float(rate());
                while (v.phase >= 1) {
                    v.phase -= 1;
                    // Sega's PSG: 16-bit LFSR, taps 0 and 3.
                    const unsigned bit = (lfsr_ ^ (lfsr_ >> 3)) & 1u;
                    lfsr_ = (lfsr_ >> 1) | (bit << 15);
                }
                s = (lfsr_ & 1u) ? 1.0f : -1.0f;
            }
            const float a = s * psgLevel(e * v.vel) * kPsgGain;
            l += a * v.gl;
            r += a * v.gr;
        }
    }

private:
    void write(int port, uint8_t reg, uint8_t val) {
        opn_.write(port ? 2 : 0, reg);
        opn_.write(port ? 3 : 1, val);
    }

    void fmOn(int id, int key, float vel, const Instrument& inst) {
        int best = -1;
        for (int ch = 0; ch < 6; ++ch)
            if (!fm_[ch].keyed && (best < 0 || fm_[ch].since < fm_[best].since)) best = ch;
        if (best < 0)
            for (int ch = 0; ch < 6; ++ch)
                if (best < 0 || fm_[ch].since < fm_[best].since) best = ch;
        const int port = best / 3, c = best % 3;
        write(0, 0x28, uint8_t((port << 2) | c));   // key off (retrigger)

        const Fm4Patch& p = inst.fm4;
        const int atten = vel > 0.001f ? int(std::lround(-20.0 * std::log10(vel) / 0.75)) : 127;
        for (int k = 0; k < 4; ++k) {
            const Fm4Operator& o = p.op[k];
            const uint8_t s = uint8_t(kSlot[k] + c);
            const bool carrier = (kCarriers[p.alg & 7] >> k) & 1;
            const int tl = std::min(127, o.tl + (carrier ? atten : 0));
            write(port, uint8_t(0x30 + s), uint8_t(((o.dt & 7) << 4) | (o.mul & 15)));
            write(port, uint8_t(0x40 + s), uint8_t(tl));
            write(port, uint8_t(0x50 + s), uint8_t(((o.ks & 3) << 6) | (o.ar & 31)));
            write(port, uint8_t(0x60 + s), uint8_t((o.am << 7) | (o.d1r & 31)));
            write(port, uint8_t(0x70 + s), uint8_t(o.d2r & 31));
            write(port, uint8_t(0x80 + s), uint8_t(((o.d1l & 15) << 4) | (o.rr & 15)));
            write(port, uint8_t(0x90 + s), 0);
        }
        write(port, uint8_t(0xB0 + c), uint8_t(((p.fb & 7) << 3) | (p.alg & 7)));
        uint8_t lr = 0xC0;
        if (inst.pan < -0.5f) lr = 0x80; else if (inst.pan > 0.5f) lr = 0x40;
        write(port, uint8_t(0xB4 + c), uint8_t(lr | ((p.ams & 3) << 4) | (p.fms & 7)));

        // Frequency: fnum = Hz * 2^21 / (rate * 2^block), fnum < 2048.
        int block = 0;
        double fnum = keyHz(key) * double(1 << 21) / rate();
        while (fnum >= 2048 && block < 7) { ++block; fnum /= 2; }
        const int f = std::clamp(int(std::lround(fnum)), 0, 2047);
        write(port, uint8_t(0xA4 + c), uint8_t((block << 3) | (f >> 8)));
        write(port, uint8_t(0xA0 + c), uint8_t(f & 0xFF));
        write(0, 0x28, uint8_t(0xF0 | (port << 2) | c));
        fm_[best] = {id, ++clock_, true};
    }

    void psgOn(int id, int key, float vel, const Instrument& inst) {
        int best;
        if (inst.kind == Kind::PsgNoise) {
            best = 3;
        } else {
            best = -1;
            for (int i = 0; i < 3; ++i)
                if (psg_[i].env.stage == EnvState::Off || psg_[i].env.stage == EnvState::Release)
                    if (best < 0 || psg_[i].since < psg_[best].since) best = i;
            if (best < 0)
                for (int i = 0; i < 3; ++i)
                    if (best < 0 || psg_[i].since < psg_[best].since) best = i;
        }
        PsgVoice& v = psg_[best];
        v = PsgVoice{};
        v.id = id;
        v.since = ++clock_;
        v.vel = vel;
        v.envelope = inst.env;
        v.vibrato = inst.vibrato;
        v.hz = inst.kind == Kind::PsgNoise ? inst.noiseHz : float(keyHz(key));
        // The PSG is mono on the console; pan a little anyway for width.
        v.gl = inst.pan > 0.5f ? 0.6f : 1.0f;
        v.gr = inst.pan < -0.5f ? 0.6f : 1.0f;
        v.env.start();
    }

    ymfm::ym2612 opn_{*this};
    struct FmVoice { int id = -1; uint64_t since = 0; bool keyed = false; };
    struct PsgVoice {
        int id = -1;
        uint64_t since = 0;
        Envelope envelope;
        EnvState env;
        float vel = 0, hz = 0, phase = 0, t = 0, vibrato = 0, gl = 1, gr = 1;
    };
    FmVoice fm_[6];
    PsgVoice psg_[4];
    unsigned lfsr_ = 0x8000;
    uint64_t clock_ = 0;
};

}  // namespace

std::unique_ptr<Backend> makeGenesis() { return std::make_unique<Genesis>(); }

}  // namespace music
