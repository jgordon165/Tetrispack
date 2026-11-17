// Windows MIDI: the songs as General MIDI played on a GS SoundFont
// (GeneralUser GS by default), like Windows 3.x / 95's MIDI Mapper sending
// a .MID to a Roland Sound Canvas or the GS wavetable synth: General MIDI
// programs, the drum kit on channel 10, GS-style room reverb.
// TinySoundFont (MIT) does the synthesis.
#include "music/backend.h"
#include "music/music_fx.h"
#include "music/soundfont.h"

#include "tsf.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

namespace music {

namespace {

std::mutex& bankMutex() { static std::mutex m; return m; }
std::map<std::string, tsf*>& banks() { static std::map<std::string, tsf*> b; return b; }

constexpr int kRate = 44100;
constexpr int kBlock = 64;
constexpr int kDrums = 9;          // MIDI channel 10
constexpr float kGain = 11000.0f;

class Midi : public Backend {
public:
    ~Midi() override { if (f_) tsf_close(f_); }
    double rate() const override { return kRate; }

    void reset(const Song& song) override {
        if (f_) { tsf_close(f_); f_ = nullptr; }
        held_.clear();
        if (void* bank = soundFont(song.soundfont)) f_ = tsf_copy(static_cast<tsf*>(bank));
        fill_ = pos_ = 0;
        fx_.init(kRate, 1.5f, 1.0f, 0.55f, 0.35f, 0.22f);   // a little warmth, GS "Room" reverb
        if (!f_) return;
        tsf_set_output(f_, TSF_STEREO_INTERLEAVED, kRate, 0.0f);
        tsf_set_max_voices(f_, 96);   // allocate now, not on the audio thread later
        // One MIDI channel per instrument (10 for the drums), as a .MID would.
        channel_.clear();
        int next = 0;
        for (const Instrument& in : song.instruments) {
            if (in.kind != Kind::Gm) { channel_.push_back(-1); continue; }
            int ch;
            if (in.drums) ch = kDrums;
            else {
                if (next == kDrums) ++next;
                ch = std::min(next++, 15);
            }
            channel_.push_back(ch);
            if (in.drums) tsf_channel_set_presetnumber(f_, ch, in.program, 1);
            else if (!tsf_channel_set_bank_preset(f_, ch, in.bank, in.program))
                tsf_channel_set_presetnumber(f_, ch, in.program, 0);
            tsf_channel_set_pan(f_, ch, 0.5f + 0.5f * std::clamp(in.pan, -1.0f, 1.0f));
        }
        song_ = &song;
    }

    void noteOn(int id, const Note& n, const Instrument& inst) override {
        if (!f_ || inst.kind != Kind::Gm || !song_) return;
        const int idx = int(&inst - song_->instruments.data());
        if (idx < 0 || idx >= int(channel_.size()) || channel_[size_t(idx)] < 0) return;
        const int ch = channel_[size_t(idx)];
        const int key = inst.fixedKey >= 0 ? inst.fixedKey : n.key;
        tsf_channel_note_on(f_, ch, key, std::clamp(n.velocity * inst.gain, 0.0f, 1.0f));
        held_[id] = {ch, key};
    }

    void noteOff(int id) override {
        auto it = held_.find(id);
        if (it == held_.end()) return;
        if (f_) tsf_channel_note_off(f_, it->second.first, it->second.second);
        held_.erase(it);
    }

    void allOff() override {
        if (f_) tsf_note_off_all(f_);
        held_.clear();
    }

    void frame(float& l, float& r) override {
        if (pos_ >= fill_) {
            std::fill(std::begin(buf_), std::end(buf_), 0.0f);
            if (f_) tsf_render_float(f_, buf_, kBlock, 0);
            fx_.run(buf_, kBlock);
            fill_ = kBlock;
            pos_ = 0;
        }
        l = buf_[2 * pos_] * kGain;
        r = buf_[2 * pos_ + 1] * kGain;
        ++pos_;
    }

private:
    tsf* f_ = nullptr;
    const Song* song_ = nullptr;
    std::vector<int> channel_;
    std::map<int, std::pair<int, int>> held_;
    float buf_[2 * kBlock] = {};
    int fill_ = 0, pos_ = 0;
    musicfx::Chain fx_;
};

}  // namespace

bool loadSoundFont(const std::string& path) {
    std::lock_guard<std::mutex> lock(bankMutex());
    auto& b = banks();
    if (b.count(path)) return b[path] != nullptr;
    tsf* f = tsf_load_filename(path.c_str());
    b[path] = f;
    return f != nullptr;
}

void* soundFont(const std::string& path) {
    std::lock_guard<std::mutex> lock(bankMutex());
    auto it = banks().find(path);
    return it == banks().end() ? nullptr : it->second;
}

std::unique_ptr<Backend> makeMidi() { return std::make_unique<Midi>(); }

}  // namespace music
