// Impulse Tracker (.it) modules for the SNES build's music driver (SNESMOD,
// part of PVSnesLib): a song from the SNES music style, with its generated
// waveforms as samples, its envelopes as IT volume envelopes and its notes
// spread over SNESMOD's channels; and a module holding the sound effects.
// Written within SNESMOD's rules (pvsneslib_snesmod.txt): instrument mode,
// one sample per instrument, at most 8 channels (the 8th is left to sound
// effects), no NNAs, echo through S0x and the song message.
#pragma once

#include "music/song.h"

#include <cstdint>
#include <string>
#include <vector>

namespace music {

struct ItSettings {
    std::string title;
    double bpm = 120;
    int rowsPerBeat = 0;         // 0: the coarsest of 4, 6, 8, 12 that holds every note (speed 24 / rows, so the IT tempo is the BPM)
    int channels = 8;            // SNESMOD's eight (a sound effect takes the 8th while it plays)
    double oneShotRate = 16000;  // drums and other one-shot samples
    double oneShotSeconds = 0.6; // kept of each one-shot (faded out at the end)
    int maxEchoDelay = 3;        // SNESMOD EDL (16 ms steps, 2 KB of sound RAM each)
};

struct ItReport {
    int rows = 0, rowsPerBeat = 0, patterns = 0, notes = 0, dropped = 0;   // dropped: notes with no free channel
    size_t sampleBytes = 0;                                // 16-bit PCM in the module
};

std::vector<uint8_t> songToIt(const Song& song, const ItSettings& settings, ItReport* report = nullptr);

// One instrument and sample per sound, 16-bit mono at `rate`.
std::vector<uint8_t> soundsToIt(const std::vector<std::vector<int16_t>>& sounds,
                                const std::vector<std::string>& names, int rate,
                                const std::string& title);

}  // namespace music
