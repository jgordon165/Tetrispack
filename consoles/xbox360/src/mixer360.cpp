#include "mixer360.h"

#include <cstring>

namespace x360 {

namespace {

uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

// Sample i of channel c, linearly between frames, the song's end wrapping
// to its start when it loops.
float sampleAt(const Sound& s, double pos, int c, bool loop) {
    const long n = s.frames();
    long i = long(pos);
    const float t = float(pos - double(i));
    long j = i + 1;
    if (j >= n) j = loop ? 0 : n - 1;
    if (i >= n) i = n - 1;
    const float a = s.samples[size_t(i * s.channels + c)], b = s.samples[size_t(j * s.channels + c)];
    return a + (b - a) * t;
}

int16_t clip(float v) {
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return int16_t(v);
}

}  // namespace

bool decodeWave(const std::vector<uint8_t>& f, Sound& out) {
    out = Sound();
    if (f.size() < 12 || std::memcmp(&f[0], "RIFF", 4) || std::memcmp(&f[8], "WAVE", 4)) return false;
    bool pcm16 = false;
    for (size_t p = 12; p + 8 <= f.size();) {
        const uint32_t len = le32(&f[p + 4]);
        const size_t body = p + 8;
        if (len > f.size() - body) break;
        if (!std::memcmp(&f[p], "fmt ", 4) && len >= 16) {
            out.channels = le16(&f[body + 2]);
            out.rate = int(le32(&f[body + 4]));
            pcm16 = le16(&f[body]) == 1 && le16(&f[body + 14]) == 16;
        } else if (!std::memcmp(&f[p], "data", 4)) {
            out.samples.resize(len / 2);
            for (size_t i = 0; i < out.samples.size(); ++i) out.samples[i] = int16_t(le16(&f[body + i * 2]));
        }
        p = body + len + (len & 1);
    }
    if (!pcm16 || out.rate <= 0 || (out.channels != 1 && out.channels != 2)) {
        out = Sound();
        return false;
    }
    out.samples.resize(out.samples.size() - out.samples.size() % size_t(out.channels));
    return !out.samples.empty();
}

Mixer::Mixer(int outRate)
    : outRate_(outRate), musicPos_(0), musicVol_(0.7f), fade_(1), fadeStep_(0), paused_(false), dropAtZero_(false),
      sfxVol_(1) {}

void Mixer::setMusic(Sound& s, double fade) {
    music_ = Sound();
    music_.samples.swap(s.samples);
    music_.rate = s.rate;
    music_.channels = music_.samples.empty() ? 0 : s.channels;
    s = Sound();
    musicPos_ = 0;
    paused_ = false;
    dropAtZero_ = false;
    if (fade > 0.01) {
        fade_ = 0;
        fadeStep_ = float(1.0 / (fade * outRate_));
    } else {
        fade_ = 1;
        fadeStep_ = 0;
    }
}

void Mixer::fadeOutMusic(double fade) {
    if (!hasMusic()) return;
    dropAtZero_ = true;
    if (fade > 0.01) fadeStep_ = -float(1.0 / (fade * outRate_));
    else music_ = Sound();
}

double Mixer::musicPosition() const { return hasMusic() ? musicPos_ / music_.rate : 0; }

void Mixer::playSfx(const Sound* s, float gain) {
    if (!s || !s->channels) return;
    for (size_t i = 0; i < voices_.size(); ++i)
        if (voices_[i].sound == s) {
            voices_[i].pos = 0;
            voices_[i].gain = gain;
            return;
        }
    Voice v = { s, 0.0, gain };
    voices_.push_back(v);
}

void Mixer::mix(int16_t* out, int frames) {
    for (int f = 0; f < frames; ++f) {
        float l = 0, r = 0;
        if (hasMusic() && !paused_) {
            const float g = musicVol_ * fade_;
            l = sampleAt(music_, musicPos_, 0, true) * g;
            r = sampleAt(music_, musicPos_, music_.channels - 1, true) * g;
            musicPos_ += double(music_.rate) / outRate_;
            if (musicPos_ >= music_.frames()) musicPos_ -= music_.frames();
            if (fadeStep_ != 0) {
                fade_ += fadeStep_;
                if (fade_ >= 0.9999f) {   // (a float sum of the steps can fall just short)
                    fade_ = 1;
                    fadeStep_ = 0;
                } else if (fade_ <= 0.0001f) {
                    fade_ = 0;
                    fadeStep_ = 0;
                    if (dropAtZero_) music_ = Sound();
                }
            }
        }
        for (size_t i = 0; i < voices_.size();) {
            Voice& v = voices_[i];
            const Sound& s = *v.sound;
            const float g = sfxVol_ * v.gain;
            l += sampleAt(s, v.pos, 0, false) * g;
            r += sampleAt(s, v.pos, s.channels - 1, false) * g;
            v.pos += double(s.rate) / outRate_;
            if (v.pos >= s.frames()) voices_.erase(voices_.begin() + long(i));
            else ++i;
        }
        out[2 * f] = clip(l);
        out[2 * f + 1] = clip(r);
    }
}

}  // namespace x360
