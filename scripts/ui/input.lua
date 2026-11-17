-- Raw inputs to actions, per context: keys (SDL key names, lower case),
-- controller buttons ("pad <button>") and mouse buttons ("mouse right",
-- "mouse wheelup"). The game and its popups only ever see the actions, so the
-- same code runs with a keyboard, a controller (the Xbox 360 build: Start
-- opens the menu) or a mouse.
local input = {}

-- Anywhere, whatever is on screen.
input.global = {
  f12 = "menu", ["pad start"] = "menu", ["mouse right"] = "menu",
}

-- Popups (the settings menu and messages).
input.ui = {
  ["mouse wheelup"] = "up", ["mouse wheeldown"] = "down", ["mouse left"] = "accept",
  up = "up", ["keypad 8"] = "up", ["pad dpup"] = "up",
  down = "down", ["keypad 2"] = "down", ["pad dpdown"] = "down",
  left = "left", ["keypad 4"] = "left", ["pad dpleft"] = "left",
  right = "right", ["keypad 6"] = "right", ["pad dpright"] = "right",
  ["return"] = "accept", ["keypad enter"] = "accept", space = "accept", ["pad a"] = "accept",
  escape = "back", backspace = "back", ["pad b"] = "back",
}

-- Playing (the original's keys: numpad layout, Down drops, End / Page Down
-- slide to the wall).
input.game = {
  escape = "menu",
  f2 = "new", f3 = "pause", f5 = "preview", ["pad back"] = "pause",
  left = "left", ["keypad 4"] = "left", ["pad dpleft"] = "left",
  right = "right", ["keypad 6"] = "right", ["pad dpright"] = "right",
  up = "rotate", ["keypad 8"] = "rotate", ["keypad 5"] = "rotate", clear = "rotate",
  ["pad dpup"] = "rotate", ["pad a"] = "rotate",
  down = "drop", ["keypad 2"] = "drop", insert = "drop", ["keypad 0"] = "drop", space = "drop",
  ["pad dpdown"] = "drop", ["pad b"] = "drop",
  ["end"] = "slide_left", ["keypad 1"] = "slide_left", ["pad leftshoulder"] = "slide_left",
  pagedown = "slide_right", ["keypad 3"] = "slide_right", ["pad rightshoulder"] = "slide_right",
  ["return"] = "start",
}

function input.action(context, key)
  return input.global[key] or (input[context] or {})[key]
end

return input
