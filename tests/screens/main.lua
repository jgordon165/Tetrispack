-- Screenshot scene (not a ctest): runs the real game, starts at the level in
-- TETRISPACK_LEVEL and lets the autoplayer play until the requested frame,
-- optionally freezing on a line-clear blink, then presses the keys in
-- TETRISPACK_KEYS (comma-separated key names) at frame TETRISPACK_KEYS_AT.
--   tetrispack --scripts tests/screens --data <dir> --frames N --screenshot out.bmp
package.path = (os.getenv("TETRISPACK_SCRIPTS") or "scripts") .. "/?.lua;" .. package.path
dofile((os.getenv("TETRISPACK_SCRIPTS") or "scripts") .. "/main.lua")
local autoplay = require("game.autoplay")
local lvl = tonumber(os.getenv("TETRISPACK_LEVEL") or "5")
game.keypressed(tostring(lvl % 10), false)
game.keypressed("f2", false)
local pieces = tonumber(os.getenv("TETRISPACK_PIECES") or "30")
local stopOnBlink = os.getenv("TETRISPACK_BLINK") == "1"
local s = game.inspect()
local placed = 0
local frame = 0
local keysAt = tonumber(os.getenv("TETRISPACK_KEYS_AT") or "300")
local keys = os.getenv("TETRISPACK_KEYS")
local update = game.update
function game.update(dt)
  frame = frame + 1
  if keys and frame == keysAt then
    for k in keys:gmatch("[^,]+") do game.keypressed(k, false) end
  end
  s = game.inspect()
  if s.state ~= "play" then return end
  if stopOnBlink and s.blink and s.blink.pass % 2 == 1 then return end
  if placed < pieces and not s.board:busy() then
    local t = autoplay.choose(s.board)
    while t and not autoplay.act(s.board, t) do end
    placed = placed + 1
  end
  update(dt)
end
