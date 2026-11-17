// The SNES-style engine's generated waveforms, for the SNES build's music
// exporter (itwriter.h): what engine_snes.cpp plays, as raw samples.
#pragma once

#include "music/song.h"

#include <vector>

namespace music {

struct SnesWave {
    std::vector<float> data;   // -1..1
    bool loop = true;          // a single looped cycle of `cycle` samples
    int baseKey = 69;          // one-shots: the MIDI key recorded at `rate`
    double rate = 32000;       // one-shots: their sample rate
    int cycle = 256;           // loops: samples per cycle
};

SnesWave snesWave(Wave w);

}  // namespace music
