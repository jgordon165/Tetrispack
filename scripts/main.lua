-- Tetrispack game entry, laid out like Tetris for Windows 2.00: the level's
-- background tiled over the window, the score panel on the left, the
-- playfield in its gray frame, full rows blinking before they go.
-- The host calls game.update(dt), game.draw() and game.keypressed(key, repeat).
local Board = require("game.board")
local pieces = require("game.pieces")
local skin = require("game.skin")
local layout = require("game.layout")
local input = require("ui.input")
local Panel = require("ui.panel")
local jukebox = require("music.jukebox")

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
local popups = {}           -- open popups, the last one on top (ui/panel.lua)
local sfxVolume = 7         -- Options > Sound Effects, 0-10
local heardPlaced = 0       -- board.placed when we last played "place"
local resumeAfterMenu = false

local function newBoard()
  board = Board.new({ level = startLevel, rows = startRows })
  blink = nil
  heardPlaced = 0
end

-- A landing sound for every piece that has landed since we last looked
-- (dropped by the player or by gravity).
local function landed()
  if board.placed > heardPlaced then
    heardPlaced = board.placed
    tp.sfx("place")
  end
end

local function start()
  newBoard()
  state = "play"
end

local function pause(on)
  if on and state == "play" then state = "pause"
  elseif not on and state == "pause" then state = "play" end
end

local function push(p) popups[#popups + 1] = p end

-- Closes every popup; a game the menu paused carries on.
local function closeAll()
  popups = {}
  if resumeAfterMenu then pause(false) end
  resumeAfterMenu = false
end

-- A message: lines of text, then choices (default just OK). choice(i) runs
-- after the popup closes.
local function message(title, lines, choices, choice, focus)
  local rows = {}
  for _, l in ipairs(lines) do rows[#rows + 1] = { text = l, center = not l:find("\t") } end
  rows[#rows + 1] = { text = "" }
  local first = #rows + 1
  for i, c in ipairs(choices or { "OK" }) do
    rows[#rows + 1] = { label = c, center = true, action = function()
      popups[#popups] = nil
      if choice then choice(i) end
    end }
  end
  push(Panel.new({ title = title, rows = rows, visible = #rows, width = 300,
                   focus = first + (focus or 1) - 1 }))
end

-- Before a game, a new setting shows at once (level, background, rows).
local function settingsChanged()
  if state == "idle" or state == "over" then newBoard(); state = "idle" end
end

local function running() return state == "play" or state == "pause" end

local function settingsPanel()
  return Panel.new({ title = "Tetris", width = 280, visible = 13, rows = {
    { header = "GAME" },
    { label = "New Game", action = function() closeAll(); start() end },
    { label = function() return (state == "pause" and not resumeAfterMenu) and "Resume" or "Pause" end,
      enabled = running,
      action = function()
        -- The menu paused the game: Pause keeps it paused, Resume carries on.
        local resume = state == "pause" and not resumeAfterMenu
        popups = {}
        resumeAfterMenu = false
        pause(not resume)
      end },
    { label = "Exit", action = function()
        message("Tetris", { "End Tetris?" }, { "Yes", "No" },
                function(i) if i == 1 then tp.quit(0) end end, 2)
      end },
    { header = "SKILL" },
    { label = "Starting Level", values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      get = function() return startLevel end,
      set = function(v) startLevel = v; settingsChanged() end },
    { label = "Starting Rows", values = { 0, 1, 3, 5, 7, 9 },
      get = function() return startRows end,
      set = function(v) startRows = v; settingsChanged() end },
    { header = "OPTIONS" },
    { label = "Piece Preview", values = { "Off", "On" },
      get = function() return preview and "On" or "Off" end,
      set = function(v) preview = (v == "On") end },
    { label = "Players", values = { 1 }, get = function() return 1 end, set = function() end,
      enabled = false },
    { label = "Music", values = jukebox.choices,
      get = function() return jukebox.choice end,
      set = function(v) jukebox.choice = v end },
    { label = "Music Style", values = jukebox.styleNames,
      get = function() return jukebox.style end,
      set = function(v) jukebox.style = v end },
    { label = "Sound Effects", values = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      get = function() return sfxVolume end,
      set = function(v) sfxVolume = v; tp.sfx_volume(v / 10); tp.sfx("rotate") end },
    { label = "Music Volume", values = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      get = function() return jukebox.volume end,
      set = function(v) jukebox.volume = v end },
    { label = "High Scores", values = { "Off" }, get = function() return "Off" end, set = function() end,
      enabled = false },
    { header = "HELP" },
    { label = "How to Play", action = function()
        message("How to Play", {
          "Move and turn the falling pieces",
          "to fill whole rows. Full rows flash",
          "and go; everything above drops.",
          "",
          "1 row 100   2 rows 200",
          "3 rows 400   4 rows 800",
          "plus 6 x level for each piece.",
          "It speeds up as you clear lines.",
        })
      end },
    { label = "Controls", action = function()
        message("Controls", {
          "Left / Right\tMove",
          "Up\tRotate",
          "Down, Space\tDrop",
          "End / Pg Down\tSlide to wall",
          "F2 / F3\tNew / Pause",
          "Esc, F12\tMenu",
          "Right click\tMenu",
          "",
          "Controller\t",
          "D-pad\tMove, Up rotates",
          "A / B\tRotate / Drop",
          "Start\tMenu",
        })
      end },
    { label = "About", action = function()
        message("About", {
          "Tetrispack",
          "a port of Tetris for Windows 2.00",
          "(Microsoft Entertainment Pack),",
          "played from your own TETRIS.EXE.",
          "",
          "Original concept: Alexey Pajitnov",
          "Windows version: Dave Edson",
        })
      end },
  } })
end

local function openMenu()
  resumeAfterMenu = (state == "play")
  pause(true)
  popups = { settingsPanel() }
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
  tp.sfx_volume(sfxVolume / 10)
  newBoard()
end

function game.keypressed(key, rep)
  -- The top popup takes everything while one is open.
  if #popups > 0 then
    local top = popups[#popups]
    local a = input.action("ui", key)
    if a == "menu" then closeAll(); return end
    if a and top:key(a) == "close" then
      popups[#popups] = nil
      if #popups == 0 then closeAll() end
    end
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
  elseif a == "rotate" then
    if board:rotate(1) then tp.sfx("rotate") end
  elseif a == "drop" then board:drop()
  elseif a == "slide_left" then board:slide(-1)
  elseif a == "slide_right" then board:slide(1)
  end
  landed()
end

function game.update(dt)
  -- Music follows the level's background; it holds while the game is paused
  -- (not while the menu is open, so settings can be heard) and stops at
  -- game over.
  jukebox.update(board.level, state ~= "over")
  jukebox.pause(state == "pause" and #popups == 0)
  if state ~= "play" or #popups > 0 then return end
  if board.clearing and not blink then
    blink = { pass = BLINK_PASSES, t = 0 }
    -- A chime as the rows start to blink; a bigger one for three or more.
    tp.sfx(#board.clearing >= 3 and "clear_big" or "clear")
  end
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
  landed()
  if board.over then
    state = "over"
    message("Tetris", { "Game Over!" })
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
  if state == "pause" and #popups == 0 then
    local wx, wy, ww, wh = layout.well()
    local s = "Paused"
    tp.text(wx + math.floor((ww - tp.text_width(s, 1, "system")) / 2), wy + math.floor(wh / 2) - 5, s, 1, 255, 255, 255, "system")
  end
  for _, p in ipairs(popups) do p:draw() end
end

-- For tests and screenshots: the current state.
function game.inspect() return { state = state, board = board, blink = blink, popups = popups } end
