// A VGM 1.50 log of Mega Drive sound chip writes (YM2612 and SN76489), the
// format SGDK's rescomp turns into XGM2 music for the Genesis build. The
// Genesis engine (engine_genesis.cpp) logs into one while a song renders;
// see tools/render_music.cpp (style "genesis-vgm").
#pragma once

#include <cstdint>
#include <vector>

namespace music {

class VgmWriter {
public:
    static constexpr uint32_t kYm2612Clock = 7670453;   // NTSC
    static constexpr uint32_t kPsgClock = 3579545;

    // `at` is the time in 44100 Hz samples; calls must not go back in time.
    void ym2612(int port, uint8_t reg, uint8_t val, uint64_t at);
    void psg(uint8_t val, uint64_t at);
    // Marks where the song loops back to (the current position).
    void loopHere(uint64_t at);
    // Ends the log at `at`: the whole .vgm file.
    std::vector<uint8_t> finish(uint64_t at);

    size_t commands() const { return commands_; }

private:
    void waitUntil(uint64_t at);

    std::vector<uint8_t> data_;
    uint64_t now_ = 0;
    size_t loopOffset_ = 0;
    uint64_t loopAt_ = 0;
    bool looped_ = false;
    size_t commands_ = 0;
};

// Starts a log: Genesis engines created until endGenesisVgm() write their
// chip writes into the returned writer, which this module owns. A tool-only
// hook (tools/render_music.cpp, the tests); the game never calls it.
VgmWriter& beginGenesisVgm();
void endGenesisVgm();

}  // namespace music
