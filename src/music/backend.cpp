#include "music/backend.h"

namespace music {

std::unique_ptr<Backend> makeOpl3();
std::unique_ptr<Backend> makeGenesis();
std::unique_ptr<Backend> makeSnes();
std::unique_ptr<Backend> makeMidi();

std::unique_ptr<Backend> Backend::create(Engine e) {
    switch (e) {
        case Engine::Genesis: return makeGenesis();
        case Engine::Snes: return makeSnes();
        case Engine::Midi: return makeMidi();
        default: return makeOpl3();
    }
}

}  // namespace music
