// The Xbox 360 host's sound mixer, for platforms with no mixer of their own
// (platform_xsdk.cpp: the kernel's render driver only takes finished
// frames). Plays one looping song and one sound per effect at a time,
// resampled to the output rate, into 16-bit stereo. Portable C++03: it also
// builds on a PC, where tests/test_mixer360.cpp checks it.
//
// Not thread-safe: a platform that mixes on another thread holds a lock
// around every call.
#pragma once

#include <stdint.h>
#include <vector>

namespace x360 {

// 16-bit PCM in native byte order, channels interleaved.
struct Sound {
    int rate, channels;
    std::vector<int16_t> samples;
    Sound() : rate(0), channels(0) {}
    long frames() const { return channels ? long(samples.size() / channels) : 0; }
};

// A RIFF WAVE file of 16-bit PCM, mono or stereo, into `out`.
bool decodeWave(const std::vector<uint8_t>& file, Sound& out);

class Mixer {
public:
    explicit Mixer(int outRate);

    // The song: takes s's samples (s is left empty) and loops them, fading
    // in over `fade` seconds (0: at once).
    void setMusic(Sound& s, double fade);
    // Fades the song out over `fade` seconds, then drops it.
    void fadeOutMusic(double fade);
    bool hasMusic() const { return music_.channels != 0; }
    void pauseMusic(bool on) { paused_ = on; }
    void setMusicVolume(float v) { musicVol_ = v; }
    double musicPosition() const;   // seconds into the song (looping back to 0)

    // Starts `s` (owned by the caller, kept alive while it plays), cutting
    // off the same sound if it is still playing.
    void playSfx(const Sound* s, float gain);
    void setSfxVolume(float v) { sfxVol_ = v; }

    // The next `frames` stereo frames.
    void mix(int16_t* out, int frames);

private:
    struct Voice {
        const Sound* sound;
        double pos;
        float gain;
    };

    int outRate_;
    Sound music_;
    double musicPos_;               // in the song's frames
    float musicVol_, fade_, fadeStep_;
    bool paused_, dropAtZero_;
    std::vector<Voice> voices_;
    float sfxVol_;
};

}  // namespace x360
