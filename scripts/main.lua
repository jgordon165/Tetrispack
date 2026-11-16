-- Tetrispack game entry, laid out like Tetris for Windows 2.00: the level's
-- background tiled over the window, the score panel on the left, the
-- playfield in its gray frame, full rows blinking before they go.
-- The host calls game.update(dt), game.draw() and game.keypressed(key, repeat).
local Board = require("game.board")
local pieces = require("game.pieces")
local skin = require("game.skin")
local layout = require("game.layout")

local W, H = layout.w, layout.h
-- The original blinks full rows by inverting them on 10 passes (5 flashes).
local BLINK_PASSES, BLINK_STEP = 10, 0.06

game = {}

local state = "idle"        -- idle (before F2), play, pause, over
local board
local startLevel = 1
local preview = false       -- Options > Piece Preview (off in the original until chosen; F5)
local blink = nil           -- { pass = n, t = seconds } while rows clear

local function newBoard()
  board = Board.new({ level = startLevel })
  blink = nil
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
  else
    tp.log("no data: " .. err)
  end
  tp.title("Tetris for Windows")
  newBoard()
end

local keys = {
  left = "left", ["keypad 4"] = "left",
  right = "right", ["keypad 6"] = "right",
  up = "rotate", ["keypad 8"] = "rotate", ["keypad 5"] = "rotate", clear = "rotate",
  down = "drop", ["keypad 2"] = "drop", insert = "drop", ["keypad 0"] = "drop", space = "drop",
  ["end"] = "slide_left", ["keypad 1"] = "slide_left",
  ["pagedown"] = "slide_right", ["keypad 3"] = "slide_right",
}

function game.keypressed(key, rep)
  if key == "escape" then
    if state == "play" or state == "pause" then state = "over" else tp.quit(0) end
    return
  end
  if key == "f2" and not rep then start(); return end
  if key == "f3" and not rep then
    if state == "play" then state = "pause" elseif state == "pause" then state = "play" end
    return
  end
  if key == "f5" and not rep then preview = not preview; return end
  if state == "idle" or state == "over" then
    -- Skill > Starting Level: 1-9, 0 for 10.
    local n = tonumber(key)
    if n and #key == 1 then
      startLevel = (n == 0) and 10 or n
      newBoard()
      state = "idle"
    elseif key == "return" and not rep then
      if state == "over" then state = "idle"; newBoard() else start() end
    end
    return
  end
  if state ~= "play" or board:busy() then return end
  local a = keys[key]
  if a == "left" then board:left()
  elseif a == "right" then board:right()
  elseif a == "rotate" then board:rotate(1)
  elseif a == "drop" and not rep then board:drop()
  elseif a == "slide_left" then board:slide(-1)
  elseif a == "slide_right" then board:slide(1)
  end
end

function game.update(dt)
  if state ~= "play" then return end
  if board.clearing and not blink then blink = { pass = BLINK_PASSES, t = 0 } end
  if blink then
    blink.t = blink.t + dt
    while blink and blink.t >= BLINK_STEP do
      blink.t = blink.t - BLINK_STEP
      blink.pass = blink.pass - 1
      if blink.pass <= 0 then
        blink = nil
        board:collapse()
      end
    end
  else
    board:update(dt)
  end
  if board.over then state = "over" end
end

local function color(name) return pieces.type[name] end

local function drawWell()
  local g = layout.grid
  local wx, wy, ww, wh = layout.well()
  skin.frame(wx, wy, ww, wh, g.margin)
  tp.fill(wx, wy, ww, wh, 0, 0, 0)
  if state == "idle" then return end

  local inverted = {}
  if blink and blink.pass % 2 == 1 then
    for _, y in ipairs(board.clearing) do inverted[y] = true end
  end
  for y = 0, board.h - 1 do
    local inv = inverted[y]
    if inv then tp.fill(wx, wy + y * g.ch, board.w * g.cw, g.ch, 255, 255, 255) end
    for x = 0, board.w - 1 do
      local n = board.grid[y][x]
      if n then skin.block(color(n), wx + x * g.cw, wy + y * g.ch, g.cw, g.ch, inv) end
    end
  end
  if not board:busy() then
    local p = board.piece
    for _, c in ipairs(board:cells()) do
      local y = p.y + c.y
      if y >= 0 then skin.block(color(p.name), wx + (p.x + c.x) * g.cw, wy + y * g.ch, g.cw, g.ch) end
    end
  end
end

local function drawPanel()
  local p = layout.panel
  skin.frame(p.x, p.y, p.w, p.h)
  for _, l in ipairs(layout.labels) do
    tp.text(l.x, l.y, l.text, 1, 0, 0, 0, "system")
    tp.text(l.vx, l.vy, tostring(board[l.field]), 1, 0, 0, 255, "system")
  end
  local n = layout.next
  skin.sunken(n.x, n.y, n.w, n.h)
  if preview and state ~= "idle" and not board.over then
    local cells = pieces.rotations[board.next][1]
    local minx, maxx, miny, maxy = 9, -9, 9, -9
    for _, c in ipairs(cells) do
      minx, maxx = math.min(minx, c.x), math.max(maxx, c.x)
      miny, maxy = math.min(miny, c.y), math.max(maxy, c.y)
    end
    local ox = n.x + math.floor((n.w - (maxx - minx + 1) * n.cw) / 2)
    local oy = n.y + math.floor((n.h - (maxy - miny + 1) * n.ch) / 2)
    tp.fill(ox, oy, (maxx - minx + 1) * n.cw, (maxy - miny + 1) * n.ch, 0, 0, 0)
    for _, c in ipairs(cells) do
      skin.block(color(board.next), ox + (c.x - minx) * n.cw, oy + (c.y - miny) * n.ch, n.cw, n.ch)
    end
  end
end

-- A Windows 3.x message box, like the original's "Game Over!".
local function messageBox(text)
  local w, h = 200, 84
  local x, y = math.floor((W - w) / 2), math.floor((H - h) / 2)
  tp.fill(x, y, w, h, 0, 0, 0)
  tp.fill(x + 1, y + 1, w - 2, 18, 0, 0, 128)
  local title = "Tetris for Windows"
  tp.text(x + math.floor((w - tp.text_width(title, 1, "system")) / 2), y + 5, title, 1, 255, 255, 255, "system")
  tp.fill(x + 1, y + 20, w - 2, h - 21, 255, 255, 255)
  tp.text(x + math.floor((w - tp.text_width(text, 1, "system")) / 2), y + 30, text, 1, 0, 0, 0, "system")
  local bw, bh = 56, 20
  local bx, by = x + math.floor((w - bw) / 2), y + h - bh - 8
  tp.fill(bx, by, bw, bh, 0, 0, 0)
  tp.fill(bx + 1, by + 1, bw - 2, bh - 2, 255, 255, 255)
  tp.fill(bx + 2, by + 2, bw - 4, bh - 4, 128, 128, 128)
  tp.fill(bx + 2, by + 2, bw - 5, bh - 5, 192, 192, 192)
  tp.text(bx + math.floor((bw - tp.text_width("OK", 1, "system")) / 2), by + 5, "OK", 1, 0, 0, 0, "system")
end

function game.draw()
  skin.background(board.level, W, H)
  drawPanel()
  drawWell()
  if state == "pause" then
    local wx, wy, ww, wh = layout.well()
    local s = "Paused"
    tp.text(wx + math.floor((ww - tp.text_width(s, 1, "system")) / 2), wy + math.floor(wh / 2) - 5, s, 1, 255, 255, 255, "system")
  elseif state == "over" then
    messageBox("Game Over!")
  end
end

-- For tests and screenshots: the current state.
function game.inspect() return { state = state, board = board, blink = blink } end
