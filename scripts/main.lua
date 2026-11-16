-- Tetrispack game entry: title (with attract mode), play, pause, game over.
-- The host calls game.update(dt), game.draw() and game.keypressed(key, repeat).
local Board = require("game.board")
local autoplay = require("game.autoplay")
local skin = require("game.skin")

local W, H = 320, 240
local CELL = 10

game = {}

local state, board, target, aiTimer = "title", nil, nil, 0
local fast = false

local function newBoard()
  board = Board.new()
  target, aiTimer = nil, 0
end

local function start()
  newBoard()
  state = "play"
end

do
  tp.canvas(W, H)
  local ok, err = tp.has_data()
  if ok then
    local name, winver = tp.module()
    tp.log(("data: module %s (Windows %s), %d resources"):format(name, winver, #tp.resources()))
    skin.load()
  else
    tp.log("no data: " .. err)
  end
  tp.title(ok and "Tetrispack" or "Tetrispack (no TETRIS.EXE: flat colours)")
  newBoard()
end

function game.keypressed(key, rep)
  if key == "escape" then
    if state == "title" then tp.quit(0) else state = "title"; newBoard() end
    return
  end
  if state == "title" or state == "over" then
    if (key == "return" or key == "f2" or key == "space") and not rep then start() end
    return
  end
  if key == "p" or key == "f3" then
    if not rep then state = (state == "pause") and "play" or "pause" end
    return
  end
  if state ~= "play" then return end
  if key == "left" then board:left()
  elseif key == "right" then board:right()
  elseif key == "up" then board:rotate(1)
  elseif key == "space" and not rep then board:drop()
  end
end

function game.update(dt)
  if state == "title" then
    if board.over then newBoard() end
    aiTimer = aiTimer + dt
    while aiTimer >= 0.05 do
      aiTimer = aiTimer - 0.05
      target = target or autoplay.choose(board)
      if not target or autoplay.act(board, target) then target = nil end
    end
  elseif state == "play" then
    fast = tp.key_down("down")
    board:update(fast and dt * 10 or dt)
    if board.over then state = "over" end
  end
end

local function drawBoard(ox, oy)
  tp.fill(ox - 2, oy - 2, board.w * CELL + 4, board.h * CELL + 4, 192, 192, 192)
  tp.fill(ox, oy, board.w * CELL, board.h * CELL, 0, 0, 0)
  for y = 0, board.h - 1 do
    for x = 0, board.w - 1 do
      local n = board.grid[y][x]
      if n then skin.block(n, ox + x * CELL, oy + y * CELL, CELL) end
    end
  end
  if not board.over then
    local p = board.piece
    for _, c in ipairs(board:cells()) do
      if p.y + c.y >= 0 then skin.block(p.name, ox + (p.x + c.x) * CELL, oy + (p.y + c.y) * CELL, CELL) end
    end
  end
end

local function centered(y, s, scale, r, g, b)
  tp.text(math.floor((W - tp.text_width(s, scale)) / 2), y, s, scale, r, g, b)
end

function game.draw()
  tp.clear(0, 0, 128)
  local ox, oy = 110, 18
  drawBoard(ox, oy)

  local sx = ox + board.w * CELL + 16
  tp.text(sx, oy, "NEXT", 2, 255, 255, 255)
  local pieces = require("game.pieces")
  for _, c in ipairs(pieces.rotations[board.next][1]) do
    skin.block(board.next, sx + c.x * CELL, oy + 16 + c.y * CELL, CELL)
  end
  tp.text(sx, oy + 70, "SCORE", 2, 255, 255, 255)
  tp.text(sx, oy + 84, tostring(board.score), 2, 255, 255, 0)
  tp.text(sx, oy + 106, "LINES", 2, 255, 255, 255)
  tp.text(sx, oy + 120, tostring(board.lines), 2, 255, 255, 0)
  tp.text(sx, oy + 142, "LEVEL", 2, 255, 255, 255)
  tp.text(sx, oy + 156, tostring(board.level), 2, 255, 255, 0)

  if state == "title" then
    centered(H - 14, "PRESS ENTER TO PLAY", 2, 255, 255, 255)
  elseif state == "pause" then
    centered(100, "PAUSED", 3, 255, 255, 255)
  elseif state == "over" then
    centered(100, "GAME OVER", 3, 255, 64, 64)
    centered(H - 14, "ENTER: NEW GAME  ESC: TITLE", 1, 255, 255, 255)
  end
end
