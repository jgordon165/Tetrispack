-- Tetrispack game entry, laid out like Tetris for Windows 2.00: the level's
-- background tiled over the window, the score panel on the left, the
-- playfield in its gray frame, full rows blinking before they go.
-- The host calls game.update(dt), game.draw() and game.keypressed(key, repeat).
local Board = require("game.board")
local pieces = require("game.pieces")
local skin = require("game.skin")
local layout = require("game.layout")
local input = require("ui.input")
local Menu = require("ui.menu")
local Dialog = require("ui.dialog")

local W, H = layout.w, layout.h
-- The original blinks full rows by inverting them on 10 passes (5 flashes).
local BLINK_PASSES, BLINK_STEP = 10, 0.06

game = {}

local state = "idle"        -- idle (before F2), play, pause, over
local board
local startLevel = 1        -- Skill > Starting Level
local startRows = 0         -- Skill > Starting Rows
local preview = false       -- Options > Piece Preview (off in the original until chosen)
local blink = nil           -- { pass = n, t = seconds } while rows clear
local menu, dialog          -- the popup menu, and a dialog box if one is up
local resumeAfterMenu = false

local function newBoard()
  board = Board.new({ level = startLevel, rows = startRows })
  blink = nil
end

local function start()
  newBoard()
  state = "play"
end

local function pause(on)
  if on and state == "play" then state = "pause"
  elseif not on and state == "pause" then state = "play" end
end

local function showDialog(opts) dialog = Dialog.new(opts) end

-- Before a game, a new setting shows at once (level, background, rows).
local function settingsChanged()
  if state == "idle" or state == "over" then newBoard(); state = "idle" end
end

local function levelItems()
  local items = {}
  for n = 1, 10 do
    items[n] = {
      label = (n == 10) and "1&0" or ("&" .. n), radio = true,
      checked = function() return startLevel == n end,
      action = function() startLevel = n; settingsChanged() end,
    }
  end
  return items
end

local function rowItems()
  local items = {}
  for _, n in ipairs({ 0, 1, 3, 5, 7, 9 }) do
    items[#items + 1] = {
      label = "&" .. n, radio = true,
      checked = function() return startRows == n end,
      action = function() startRows = n; settingsChanged() end,
    }
  end
  return items
end

local notYet = function() return false end   -- items the port doesn't do yet

-- The original's menus (TETRIS.EXE MENU 1), as a popup.
menu = Menu.new({
  { label = "&Game", sub = {
    { label = "&New", accel = "F2", action = start },
    { label = "&Pause", accel = "F3", checked = function() return state == "pause" and not resumeAfterMenu end,
      enabled = function() return state == "play" or state == "pause" end,
      action = function()
        -- The menu paused the game; choosing Pause keeps it paused (or resumes).
        if state == "pause" and not resumeAfterMenu then pause(false) else pause(true) end
        resumeAfterMenu = false
      end },
    { label = "&High Scores...", enabled = notYet },
    { separator = true },
    { label = "E&xit", action = function()
        showDialog({ lines = { "End Tetris?" }, buttons = { "&Yes", "&No" },
                     onbutton = function(i) if i == 1 then tp.quit(0) end end })
      end },
  } },
  { label = "&Skill", sub = {
    { label = "Starting &Level", sub = levelItems() },
    { label = "Starting &Rows", sub = rowItems() },
  } },
  { label = "&Options", sub = {
    { label = "&Sound", enabled = notYet },
    { separator = true },
    { label = "&1 Player", radio = true, checked = true },
    { label = "&2 Player", radio = true, enabled = notYet },
    { label = "&Penalize Other Player", enabled = notYet },
    { separator = true },
    { label = "Piece Pre&view", accel = "F5", checked = function() return preview end,
      action = function() preview = not preview end },
    { label = "&Track High Scores", enabled = notYet },
  } },
  { label = "&Help", sub = {
    { label = "&How to Play", action = function()
        showDialog({ title = "How to Play", lines = {
          "Move and turn the falling pieces to fill",
          "whole rows across the playfield. Full rows",
          "flash and disappear; the rest drops down.",
          "",
          "1 row 100, 2 rows 200, 3 rows 400, 4 rows 800,",
          "plus 6 x level for every piece that lands.",
          "The game speeds up as you clear more lines.",
        } })
      end },
    { label = "&Keyboard", action = function()
        showDialog({ title = "Keyboard", lines = {
          "Left / Right\tMove",
          "Up, keypad 5\tRotate",
          "Down, Space, Insert\tDrop",
          "End / Page Down\tSlide to the wall",
          "F2\tNew game",
          "F3\tPause",
          "F5\tPiece preview",
          "Esc or F12\tMenu",
          "",
          "Controller\tD-pad move, Up or A rotate,",
          "\tDown or B drop, Start menu",
        } })
      end },
    { separator = true },
    { label = "&About Tetris...", action = function()
        showDialog({ title = "About Tetris", lines = {
          "Tetrispack",
          "A port of Tetris for Windows 2.00",
          "(Microsoft Entertainment Pack, 1990),",
          "playing from your own TETRIS.EXE.",
          "",
          "Original concept by Alexey Pajitnov.",
          "Windows version by Dave Edson.",
          "Tetris is a trademark of The Tetris Company.",
        } })
      end },
  } },
})
menu.onclose = function()
  if resumeAfterMenu then pause(false) end
  resumeAfterMenu = false
end

local function openMenu()
  resumeAfterMenu = (state == "play")
  pause(true)
  local g = layout.grid
  menu:open(g.x + 24, g.y + 24)
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

function game.keypressed(key, rep)
  -- A dialog takes everything, then the menu, then the game.
  if dialog then
    local a = input.action("ui", key)
    if a then dialog:key(a) end
    if dialog and dialog.closed then dialog = nil end
    return
  end
  if menu:isOpen() then
    menu:key(input.action("ui", key), key)
    return
  end

  local a = input.action("game", key)
  if a == "menu" then openMenu(); return end
  if rep and (a == "new" or a == "pause" or a == "preview" or a == "drop" or a == "start") then return end
  if a == "new" then start(); return end
  if a == "pause" then pause(state == "play"); return end
  if a == "preview" then preview = not preview; return end

  if state == "idle" or state == "over" then
    -- Skill > Starting Level from the keyboard: 1-9, 0 for 10.
    local n = tonumber(key)
    if n and #key == 1 then
      startLevel = (n == 0) and 10 or n
      settingsChanged()
    elseif a == "start" then
      start()
    end
    return
  end
  if state ~= "play" or board:busy() then return end
  if a == "left" then board:left()
  elseif a == "right" then board:right()
  elseif a == "rotate" then board:rotate(1)
  elseif a == "drop" then board:drop()
  elseif a == "slide_left" then board:slide(-1)
  elseif a == "slide_right" then board:slide(1)
  end
end

function game.update(dt)
  if state ~= "play" or dialog or menu:isOpen() then return end
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
  if board.over then
    state = "over"
    showDialog({ lines = { "Game Over!" } })
  end
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

function game.draw()
  skin.background(board.level, W, H)
  drawPanel()
  drawWell()
  if state == "pause" and not menu:isOpen() then
    local wx, wy, ww, wh = layout.well()
    local s = "Paused"
    tp.text(wx + math.floor((ww - tp.text_width(s, 1, "system")) / 2), wy + math.floor(wh / 2) - 5, s, 1, 255, 255, 255, "system")
  end
  menu:draw()
  if dialog then dialog:draw() end
end

-- For tests and screenshots: the current state.
function game.inspect() return { state = state, board = board, blink = blink, menu = menu, dialog = dialog } end
