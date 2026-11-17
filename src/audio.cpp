#include "audio.h"

#include <cstdio>
#include <cstring>

Audio::~Audio() {
    if (dev_) SDL_CloseAudioDevice(dev_);
}

bool Audio::open() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "audio unavailable: %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want{}, have{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &Audio::callback;
    want.userdata = this;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!dev_) {
        std::fprintf(stderr, "audio device: %s\n", SDL_GetError());
        return false;
    }
    rate_ = have.freq;
    sfx_.prepare(rate_);
    SDL_PauseAudioDevice(dev_, 0);
    return true;
}

void Audio::callback(void* user, Uint8* stream, int len) {
    auto* self = static_cast<Audio*>(user);
    std::memset(stream, 0, size_t(len));
    self->synth_.render(reinterpret_cast<int16_t*>(stream), len / 4, self->rate_);
    self->sfx_.render(reinterpret_cast<int16_t*>(stream), len / 4);
}

void Audio::play(std::shared_ptr<const music::Song> song, double fade) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    synth_.play(std::move(song), fade);
    SDL_UnlockAudioDevice(dev_);
}

void Audio::stop(double fade) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    synth_.stop(fade);
    SDL_UnlockAudioDevice(dev_);
}

void Audio::setPaused(bool p) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    synth_.setPaused(p);
    SDL_UnlockAudioDevice(dev_);
}

void Audio::setVolume(float v) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    synth_.setVolume(v);
    SDL_UnlockAudioDevice(dev_);
}

double Audio::position() {
    if (!dev_) return 0;
    SDL_LockAudioDevice(dev_);
    const double p = synth_.position();
    SDL_UnlockAudioDevice(dev_);
    return p;
}

void Audio::playSfx(const std::string& name, float gain) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    sfx_.play(name, gain);
    SDL_UnlockAudioDevice(dev_);
}

void Audio::setSfxVolume(float v) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    sfx_.setVolume(v);
    SDL_UnlockAudioDevice(dev_);
}
