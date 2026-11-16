// Keyboard names shared with Lua: SDL's key names in lower case
// ("left", "space", "return", "escape", "f2", "p", ...).
#pragma once

#include <SDL.h>
#include <string>

namespace input {

std::string keyName(SDL_Keycode key);
bool isDown(const std::string& name);

}  // namespace input
