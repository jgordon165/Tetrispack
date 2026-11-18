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
#include <map>
#include <string>
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

class Platform;

// What a platform with no sound system of its own does with the game's
// music and effect calls (Platform's playMusic ... setSfxVolume): songs and
// effects read as WAVE files through the platform, a song started once the
// last has faded out, the mixing done here. The console mixes on a thread
// of its own and passes a Lock; elsewhere the default does nothing.
class Audio {
public:
    struct Lock {
        virtual ~Lock() {}
        virtual void lock() {}
        virtual void unlock() {}
    };

    Audio(Platform& files, int outRate, Lock* lock = 0);

    void playMusic(const std::string& path, double fade);
    void stopMusic(double fade);
    void pauseMusic(bool on);
    void setMusicVolume(float v);
    double musicPosition();
    // `path` is the effect's WAVE file; read on first use and kept.
    void playSfx(const std::string& path, float gain);
    void setSfxVolume(float v);

    // Once a frame, on the game's thread: starts a waiting song once the
    // last has faded (its file is read here, never while mixing).
    void tick();
    // The next `frames` stereo frames (any thread).
    void mix(int16_t* out, int frames);

    const std::string& playing() const { return playing_; }

private:
    void startPending();

    Platform& files_;
    Lock none_;
    Lock* lock_;
    Mixer mixer_;
    std::map<std::string, Sound> sfx_;
    std::string playing_, pending_;
    double pendingFade_;
};

}  // namespace x360
