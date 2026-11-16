#include "music/fm_synth.h"

#include "ymfm_opl.h"

#include <algorithm>
#include <cmath>

namespace music {

namespace {
constexpr uint32_t kClock = 14318180;          // OPL3 master clock (Sound Blaster 16)
constexpr uint8_t kOpOffset[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
}  // namespace

struct FmSynth::Chip : ymfm::ymfm_interface {
    ymfm::ymf262 opl{*this};
};

double FmSynth::chipRate() { return double(kClock) / 288.0; }

FmSynth::FmSynth() : chip_(std::make_unique<Chip>()) {
    chip_->opl.reset();
    writeReg(1, 0x05, 0x01);   // OPL3 mode: all eight waveforms, stereo output bits
    writeReg(0, 0x01, 0x20);   // waveform select (OPL2 compatibility)
    writeReg(0, 0xBD, 0x00);   // melodic mode
}

FmSynth::~FmSynth() = default;

void FmSynth::writeReg(int bank, uint8_t reg, uint8_t val) {
    chip_->opl.write(bank ? 2 : 0, reg);
    chip_->opl.write(bank ? 3 : 1, val);
}

double FmSynth::position() const { return double(pos_) / chipRate(); }

void FmSynth::play(std::shared_ptr<const Song> song, double fade) {
    if (!song_ || fade <= 0) { start(std::move(song)); return; }
    next_ = std::move(song);
    stopping_ = true;
    fadeStep_ = float(1.0 / (fade * chipRate()));
}

void FmSynth::stop(double fade) {
    next_.reset();
    if (!song_) return;
    if (fade <= 0) { allOff(); song_.reset(); return; }
    stopping_ = true;
    fadeStep_ = float(1.0 / (fade * chipRate()));
}

void FmSynth::start(std::shared_ptr<const Song> song) {
    allOff();
    song_ = std::move(song);
    events_.clear();
    nextEvent_ = 0;
    pos_ = 0;
    gain_ = 1;
    stopping_ = false;
    if (!song_) return;
    const double rate = chipRate();
    length_ = std::max<uint64_t>(1, uint64_t(song_->length * rate));
    for (uint32_t i = 0; i < song_->notes.size(); ++i) {
        const Note& n = song_->notes[i];
        const uint64_t on = uint64_t(n.start * rate);
        if (on >= length_ || n.patch < 0 || n.patch >= int(song_->patches.size())) continue;
        const uint64_t off = std::min(length_ - 1, std::max(on + 1, uint64_t((n.start + n.length) * rate)));
        events_.push_back({on, true, i});
        events_.push_back({off, false, i});
    }
    // Offs before ons at the same moment, so a repeated note retriggers.
    std::sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) {
        return a.at != b.at ? a.at < b.at : (!a.on && b.on);
    });
}

void FmSynth::allOff() {
    for (int ch = 0; ch < 18; ++ch) {
        writeReg(ch / 9, uint8_t(0xB0 + ch % 9), 0);
        voices_[ch] = Voice{};
    }
}

void FmSynth::noteOn(uint32_t index) {
    const Note& n = song_->notes[index];
    const Patch& p = song_->patches[size_t(n.patch)];

    // A free channel, the one released longest ago; else steal the oldest.
    int best = -1;
    for (int ch = 0; ch < 18; ++ch)
        if (!voices_[ch].keyed && (best < 0 || voices_[ch].since < voices_[best].since)) best = ch;
    if (best < 0)
        for (int ch = 0; ch < 18; ++ch)
            if (best < 0 || voices_[ch].since < voices_[best].since) best = ch;
    const int bank = best / 9, c = best % 9;
    writeReg(bank, uint8_t(0xB0 + c), 0);  // key off first (retrigger)

    // Velocity: up to 24 dB quieter, on the carrier (and the modulator when it's heard too).
    const float v = std::clamp(n.velocity, 0.01f, 1.0f);
    const int atten = int(std::lround(-20.0 * std::log10(v) / 0.75));
    auto opRegs = [&](const Operator& o, uint8_t off, bool heard) {
        const int tl = std::min(63, o.tl + (heard ? atten : 0));
        writeReg(bank, uint8_t(0x20 + off), uint8_t((o.am << 7) | (o.vib << 6) | (o.sustain << 5) | (o.ksr << 4) | (o.mult & 15)));
        writeReg(bank, uint8_t(0x40 + off), uint8_t(((o.ksl & 3) << 6) | tl));
        writeReg(bank, uint8_t(0x60 + off), uint8_t(((o.ar & 15) << 4) | (o.dr & 15)));
        writeReg(bank, uint8_t(0x80 + off), uint8_t(((o.sl & 15) << 4) | (o.rr & 15)));
        writeReg(bank, uint8_t(0xE0 + off), uint8_t(o.ws & 7));
    };
    opRegs(p.mod, kOpOffset[c], p.additive);
    opRegs(p.car, uint8_t(kOpOffset[c] + 3), true);
    writeReg(bank, uint8_t(0xC0 + c), uint8_t(0x30 | ((p.feedback & 7) << 1) | (p.additive ? 1 : 0)));

    const int key = p.fixedKey >= 0 ? p.fixedKey : n.key;
    const double freq = 440.0 * std::pow(2.0, (key - 69) / 12.0);
    int block = 0;
    double fnum = freq * double(1 << 20) / chipRate();
    while (fnum >= 1024 && block < 7) { ++block; fnum /= 2; }
    const int f = std::clamp(int(std::lround(fnum)), 0, 1023);
    writeReg(bank, uint8_t(0xA0 + c), uint8_t(f & 0xFF));
    writeReg(bank, uint8_t(0xB0 + c), uint8_t(0x20 | (block << 2) | (f >> 8)));
    voices_[best] = Voice{int32_t(index), pos_, true};
}

void FmSynth::noteOff(uint32_t index) {
    for (int ch = 0; ch < 18; ++ch) {
        if (voices_[ch].note != int32_t(index) || !voices_[ch].keyed) continue;
        const Note& n = song_->notes[index];
        const Patch& p = song_->patches[size_t(n.patch)];
        const int key = p.fixedKey >= 0 ? p.fixedKey : n.key;
        const double freq = 440.0 * std::pow(2.0, (key - 69) / 12.0);
        int block = 0;
        double fnum = freq * double(1 << 20) / chipRate();
        while (fnum >= 1024 && block < 7) { ++block; fnum /= 2; }
        const int f = std::clamp(int(std::lround(fnum)), 0, 1023);
        writeReg(ch / 9, uint8_t(0xB0 + ch % 9), uint8_t((block << 2) | (f >> 8)));
        voices_[ch].keyed = false;
        voices_[ch].since = pos_;
    }
}

void FmSynth::chipFrame(float& l, float& r) {
    if (song_) {
        while (nextEvent_ < events_.size() && events_[nextEvent_].at <= pos_) {
            const Event& e = events_[nextEvent_++];
            if (e.on) noteOn(e.note); else noteOff(e.note);
        }
        if (++pos_ >= length_) {   // loop
            for (int ch = 0; ch < 18; ++ch)
                if (voices_[ch].keyed) noteOff(uint32_t(voices_[ch].note));
            pos_ = 0;
            nextEvent_ = 0;
            for (auto& v : voices_) v.since = 0;
        }
        if (stopping_) {
            gain_ -= fadeStep_;
            if (gain_ <= 0) {
                auto nxt = std::move(next_);
                next_.reset();
                if (nxt) start(std::move(nxt)); else { allOff(); song_.reset(); stopping_ = false; gain_ = 1; }
            }
        }
    }
    ymfm::ymf262::output_data o;
    chip_->opl.generate(&o, 1);
    const float g = (song_ ? std::max(gain_, 0.0f) : 0.0f) * volume_;
    l = float(o.data[0]) * g;
    r = float(o.data[1]) * g;
}

void FmSynth::render(int16_t* out, int frames, int rate) {
    if (paused_) return;   // silence, and the song holds its place
    const double step = chipRate() / double(rate);
    for (int i = 0; i < frames; ++i) {
        // Linear interpolation between chip frames.
        while (frac_ >= 1.0) {
            prevL_ = curL_;
            prevR_ = curR_;
            chipFrame(curL_, curR_);
            frac_ -= 1.0;
        }
        const float t = float(frac_);
        const float l = prevL_ + (curL_ - prevL_) * t, r = prevR_ + (curR_ - prevR_) * t;
        frac_ += step;
        auto mix = [](int16_t& dst, float v) {
            const int s = int(dst) + int(std::lround(v));
            dst = int16_t(std::clamp(s, -32768, 32767));
        };
        mix(out[2 * i], l);
        mix(out[2 * i + 1], r);
    }
}

}  // namespace music
