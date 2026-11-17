// Sound Blaster 16: ymfm's YMF262 (OPL3), 18 two-operator voices.
#include "music/backend.h"

#include "ymfm_opl.h"

#include <algorithm>
#include <cmath>

namespace music {

namespace {

constexpr uint32_t kClock = 14318180;
constexpr uint8_t kOpOffset[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};

class Opl3 : public Backend, ymfm::ymfm_interface {
public:
    Opl3() { init(); }
    double rate() const override { return double(kClock) / 288.0; }

    void reset(const Song&) override { allOff(); }

    void noteOn(int id, const Note& n, const Instrument& inst) override {
        if (inst.kind != Kind::Opl) return;
        const OplPatch& p = inst.opl;
        int best = -1;
        for (int ch = 0; ch < 18; ++ch)
            if (!voice_[ch].keyed && (best < 0 || voice_[ch].since < voice_[best].since)) best = ch;
        if (best < 0)
            for (int ch = 0; ch < 18; ++ch)
                if (best < 0 || voice_[ch].since < voice_[best].since) best = ch;
        const int bank = best / 9, c = best % 9;
        write(bank, uint8_t(0xB0 + c), 0);

        const float v = std::clamp(n.velocity * inst.gain, 0.01f, 1.0f);
        const int atten = int(std::lround(-20.0 * std::log10(v) / 0.75));
        auto op = [&](const OplOperator& o, uint8_t off, bool heard) {
            const int tl = std::min(63, o.tl + (heard ? atten : 0));
            write(bank, uint8_t(0x20 + off), uint8_t((o.am << 7) | (o.vib << 6) | (o.sustain << 5) | (o.ksr << 4) | (o.mult & 15)));
            write(bank, uint8_t(0x40 + off), uint8_t(((o.ksl & 3) << 6) | tl));
            write(bank, uint8_t(0x60 + off), uint8_t(((o.ar & 15) << 4) | (o.dr & 15)));
            write(bank, uint8_t(0x80 + off), uint8_t(((o.sl & 15) << 4) | (o.rr & 15)));
            write(bank, uint8_t(0xE0 + off), uint8_t(o.ws & 7));
        };
        op(p.mod, kOpOffset[c], p.additive);
        op(p.car, uint8_t(kOpOffset[c] + 3), true);
        uint8_t out = 0x30;                            // both speakers
        if (inst.pan < -0.5f) out = 0x10; else if (inst.pan > 0.5f) out = 0x20;
        write(bank, uint8_t(0xC0 + c), uint8_t(out | ((p.feedback & 7) << 1) | (p.additive ? 1 : 0)));

        const int key = inst.fixedKey >= 0 ? inst.fixedKey : n.key;
        int block = 0;
        double fnum = keyHz(key) * double(1 << 20) / rate();
        while (fnum >= 1024 && block < 7) { ++block; fnum /= 2; }
        const int f = std::clamp(int(std::lround(fnum)), 0, 1023);
        const uint8_t b0 = uint8_t((block << 2) | (f >> 8));
        write(bank, uint8_t(0xA0 + c), uint8_t(f & 0xFF));
        write(bank, uint8_t(0xB0 + c), uint8_t(0x20 | b0));
        voice_[best] = {id, ++clock_, true, b0};
    }

    void noteOff(int id) override {
        for (int ch = 0; ch < 18; ++ch) {
            if (voice_[ch].id != id || !voice_[ch].keyed) continue;
            write(ch / 9, uint8_t(0xB0 + ch % 9), voice_[ch].b0);
            voice_[ch].keyed = false;
            voice_[ch].since = ++clock_;
        }
    }

    void allOff() override {
        for (int ch = 0; ch < 18; ++ch) {
            write(ch / 9, uint8_t(0xB0 + ch % 9), 0);
            voice_[ch] = Voice{};
        }
    }

    void frame(float& l, float& r) override {
        ymfm::ymf262::output_data o;
        opl_.generate(&o, 1);
        l = float(o.data[0]);
        r = float(o.data[1]);
    }

private:
    void init() {
        opl_.reset();
        write(1, 0x05, 0x01);   // OPL3 mode
        write(0, 0x01, 0x20);
        write(0, 0xBD, 0x00);
    }
    void write(int bank, uint8_t reg, uint8_t val) {
        opl_.write(bank ? 2 : 0, reg);
        opl_.write(bank ? 3 : 1, val);
    }

    ymfm::ymf262 opl_{*this};
    struct Voice { int id = -1; uint64_t since = 0; bool keyed = false; uint8_t b0 = 0; };
    Voice voice_[18];
    uint64_t clock_ = 0;
};

}  // namespace

std::unique_ptr<Backend> makeOpl3() { return std::make_unique<Opl3>(); }

}  // namespace music
