// The sound device: plays the music (music/player.h) and the sound
// effects (music/sfx.h) from SDL's audio
// thread. Every call from the game side locks the device for the moment it
// changes the synth.
#pragma once

#include "music/player.h"
#include "music/sfx.h"

#include <SDL.h>
#include <memory>
#include <string>

class Audio {
public:
    ~Audio();
    bool open();                    // false: no sound device (the game still runs)
    bool isOpen() const { return dev_ != 0; }

    void play(std::shared_ptr<const music::Song> song, double fade);
    void stop(double fade);
    void setPaused(bool p);
    void setVolume(float v);
    double position();

    void playSfx(const std::string& name, float gain);
    void setSfxVolume(float v);

private:
    static void callback(void* user, Uint8* stream, int len);
    SDL_AudioDeviceID dev_ = 0;
    int rate_ = 48000;
    music::Player synth_;
    music::Sfx sfx_;
};
