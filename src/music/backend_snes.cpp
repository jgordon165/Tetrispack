// Backend::create for a host that synthesizes only the Super NES engine (the
// Xbox 360 build: its jukebox offers only the SNES style, tp.music_engines).
// Any other engine asked for gets the SNES one.
#include "music/backend.h"

namespace music {

std::unique_ptr<Backend> makeSnes();

std::unique_ptr<Backend> Backend::create(Engine) { return makeSnes(); }

}  // namespace music
