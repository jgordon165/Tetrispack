#include "input.h"

#include <cctype>

namespace input {

std::string keyName(SDL_Keycode key) {
    std::string s = SDL_GetKeyName(key);
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool isDown(const std::string& name) {
    const SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    return state && state[sc];
}

}  // namespace input
