#include "music/player.h"

#include <algorithm>
#include <cmath>

namespace music {

double Player::position() const { return rate_ > 0 ? double(pos_) / rate_ : 0; }

void Player::play(std::shared_ptr<const Song> song, double fade) {
    if (!song_ || fade <= 0) { start(std::move(song)); return; }
    next_ = std::move(song);
    stopping_ = true;
    fadeStep_ = float(1.0 / (fade * rate_));
}

void Player::stop(double fade) {
    next_.reset();
    if (!song_) return;
    if (fade <= 0) {
        backend_->allOff();
        song_.reset();
        return;
    }
    stopping_ = true;
    fadeStep_ = float(1.0 / (fade * rate_));
}

void Player::start(std::shared_ptr<const Song> song) {
    if (backend_) backend_->allOff();
    song_ = std::move(song);
    events_.clear();
    nextEvent_ = 0;
    pos_ = 0;
    gain_ = 1;
    stopping_ = false;
    if (!song_) return;
    if (!backend_ || engine_ != song_->engine) {
        backend_ = Backend::create(song_->engine);
        engine_ = song_->engine;
        frac_ = 1;
        prevL_ = prevR_ = curL_ = curR_ = 0;
    }
    backend_->reset(*song_);
    rate_ = backend_->rate();
    length_ = std::max<uint64_t>(1, uint64_t(song_->length * rate_));
    for (uint32_t i = 0; i < song_->notes.size(); ++i) {
        const Note& n = song_->notes[i];
        const uint64_t on = uint64_t(n.start * rate_);
        if (on >= length_ || n.instrument < 0 || n.instrument >= int(song_->instruments.size())) continue;
        const uint64_t off = std::min(length_ - 1, std::max(on + 1, uint64_t((n.start + n.length) * rate_)));
        events_.push_back({on, true, i});
        events_.push_back({off, false, i});
    }
    // Offs before ons at the same moment, so a repeated note retriggers.
    std::sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) {
        return a.at != b.at ? a.at < b.at : (!a.on && b.on);
    });
}

void Player::engineFrame(float& l, float& r) {
    if (song_) {
        while (nextEvent_ < events_.size() && events_[nextEvent_].at <= pos_) {
            const Event& e = events_[nextEvent_++];
            const Note& n = song_->notes[e.note];
            if (e.on) backend_->noteOn(int(e.note), n, song_->instruments[size_t(n.instrument)]);
            else backend_->noteOff(int(e.note));
        }
        if (++pos_ >= length_) {   // loop
            for (uint32_t i = 0; i < song_->notes.size(); ++i) backend_->noteOff(int(i));
            pos_ = 0;
            nextEvent_ = 0;
        }
        if (stopping_) {
            gain_ -= fadeStep_;
            if (gain_ <= 0) {
                auto nxt = std::move(next_);
                next_.reset();
                if (nxt) start(std::move(nxt));
                else { backend_->allOff(); song_.reset(); stopping_ = false; gain_ = 1; }
            }
        }
    }
    if (!backend_) { l = r = 0; return; }
    backend_->frame(l, r);
    const float g = (song_ ? std::max(gain_, 0.0f) : 0.0f) * volume_;
    l *= g;
    r *= g;
}

void Player::render(int16_t* out, int frames, int rate) {
    if (paused_) return;   // silence, and the song holds its place
    for (int i = 0; i < frames; ++i) {
        const double step = rate_ / double(rate);
        while (frac_ >= 1.0) {
            prevL_ = curL_;
            prevR_ = curR_;
            engineFrame(curL_, curR_);
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
