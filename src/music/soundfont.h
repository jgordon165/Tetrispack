// SoundFont banks (.sf2) for the Windows MIDI style, loaded once on the
// game thread and shared: each song plays on a cheap copy that shares the
// samples.
#pragma once

#include <string>

namespace music {

// Loads `path` (once; later calls return at once). False if it can't be read.
bool loadSoundFont(const std::string& path);
// The loaded bank for `path`, or nullptr. A tsf*.
void* soundFont(const std::string& path);

}  // namespace music
