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
local About = require("ui.about")
local Entry = require("ui.entry")
local scores = require("game.scores")
local settings = require("game.settings")
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
local sfxVolume = 1         -- Options > Sound Effects, 0-10
local highscores = {}       -- the top ten (game/scores.lua)
local playerName, playerQuote = "", ""   -- the last ones entered in the Hall of Fame
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

-- Closes popup `p` (wherever it is in the stack); a game the menu paused
-- carries on once none are left.
local closeAll
local function close(p)
  for i = #popups, 1, -1 do
    if popups[i] == p then table.remove(popups, i); break end
  end
  if #popups == 0 then closeAll() end
end

-- The options, as saved between runs (game/settings.lua).
local function saveSettings()
  settings.save({
    start_level = startLevel, start_rows = startRows, preview = preview, sfx_volume = sfxVolume,
    music = jukebox.choice, music_style = jukebox.style, music_volume = jukebox.volume,
    player_name = playerName, player_quote = playerQuote,
  })
end

local function oneOf(v, list)
  for _, x in ipairs(list) do if x == v then return v end end
end

local function loadSettings()
  local t = settings.load()
  local tenth = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }
  startLevel = oneOf(t.start_level, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }) or startLevel
  startRows = oneOf(t.start_rows, { 0, 1, 3, 5, 7, 9 }) or startRows
  if type(t.preview) == "boolean" then preview = t.preview end
  sfxVolume = oneOf(t.sfx_volume, tenth) or sfxVolume
  jukebox.choice = oneOf(t.music, jukebox.choices) or jukebox.choice
  jukebox.style = oneOf(t.music_style, jukebox.styleNames) or jukebox.style
  jukebox.volume = oneOf(t.music_volume, tenth) or jukebox.volume
  if type(t.player_name) == "string" then playerName = t.player_name end
  if type(t.player_quote) == "string" then playerQuote = t.player_quote end
end

-- Closes every popup; a game the menu paused carries on.
function closeAll()
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

-- The original's About box, centred on the window.
local function openAbout()
  push(About.new({ x = math.floor((W - About.W) / 2), y = math.floor((H - About.H) / 2) }))
end

-- "Tetris Experts": the top ten as the original lays them out - the score,
-- the name, the cool quote. `rank` stands out (a new entry).
local function showScores(rank)
  local rows = {}
  for i = 1, scores.max do
    local e = highscores[i]
    rows[#rows + 1] = e and { cols = { e.score, e.name ~= "" and e.name or "-", e.quote }, hot = (i == rank) }
                        or { cols = { "", "-", "" } }
  end
  rows[#rows + 1] = { text = "" }
  local panel
  rows[#rows + 1] = { label = "OK", center = true, action = function() close(panel) end }
  rows[#rows + 1] = { label = "Clear High Scores", center = true, action = function()
      message("High Scores", { "Clear the high score table?" }, { "Yes", "No" }, function(i)
        if i == 1 then
          highscores = {}
          scores.save(highscores)
          close(panel)
          showScores()
        end
      end, 2)
    end }
  panel = Panel.new({ title = "Tetris Experts", rows = rows, visible = #rows, width = 600,
                      focus = scores.max + 2,
                      columns = { { x = 0, w = 70, right = true }, { x = 90, w = 220 }, { x = 324, w = 250 } } })
  push(panel)
end

-- A game is over: the sad sound, then the Hall of Fame if the score made
-- the table, else just "Game Over!".
local function gameOver()
  tp.sfx("game_over")
  if not scores.qualifies(highscores, board.score) then
    message("Tetris", { "Game Over!" })
    return
  end
  local score, level, lines = board.score, board.level, board.lines
  push(Entry.new({ name = playerName, quote = playerQuote, score = score, done = function(name, quote)
    playerName, playerQuote = name, quote
    saveSettings()
    local rank = scores.insert(highscores, { score = score, level = level, lines = lines, name = name, quote = quote })
    scores.save(highscores)
    showScores(rank)
  end }))
end

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
    { label = "High Scores", action = function() showScores() end },
    { label = "Exit", action = function()
        message("Tetris", { "End Tetris?" }, { "Yes", "No" },
                function(i) if i == 1 then tp.quit(0) end end, 2)
      end },
    { header = "SKILL" },
    { label = "Starting Level", values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      get = function() return startLevel end,
      set = function(v) startLevel = v; settingsChanged(); saveSettings() end },
    { label = "Starting Rows", values = { 0, 1, 3, 5, 7, 9 },
      get = function() return startRows end,
      set = function(v) startRows = v; settingsChanged(); saveSettings() end },
    { header = "OPTIONS" },
    { label = "Piece Preview", values = { "Off", "On" },
      get = function() return preview and "On" or "Off" end,
      set = function(v) preview = (v == "On"); saveSettings() end },
    { label = "Players", values = { 1 }, get = function() return 1 end, set = function() end,
      enabled = false },
    { label = "Music", values = jukebox.choices,
      get = function() return jukebox.choice end,
      set = function(v) jukebox.choice = v; saveSettings() end },
    { label = "Music Style", values = jukebox.styleNames,
      get = function() return jukebox.style end,
      set = function(v) jukebox.style = v; saveSettings() end },
    { label = "Sound Effects", values = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      get = function() return sfxVolume end,
      set = function(v) sfxVolume = v; tp.sfx_volume(v / 10); tp.sfx("rotate"); saveSettings() end },
    { label = "Music Volume", values = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      get = function() return jukebox.volume end,
      set = function(v) jukebox.volume = v; saveSettings() end },
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
    { label = "About Tetris", action = openAbout },
    { label = "About Tetrispack", action = function()
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
  loadSettings()
  highscores = scores.load()
  tp.sfx_volume(sfxVolume / 10)
  newBoard()
  -- The intro: the About box over the waiting game, until OK.
  openAbout()
end

-- key: a key, controller or mouse name (ui/input.lua). Mouse clicks also
-- pass the canvas position under the pointer (nil outside the canvas).
function game.keypressed(key, rep, x, y)
  -- The top popup takes everything while one is open.
  if #popups > 0 then
    local top = popups[#popups]
    -- Popups that want keys as they are (text entry) or clicks where they land.
    for _, hook in ipairs({ "raw", "pointer" }) do
      if top[hook] then
        local r = top[hook](top, key, x, y)
        if r == "close" then close(top); return
        elseif r then return end
      end
    end
    local a = input.action("ui", key)
    if a == "menu" then closeAll(); return end
    if a and top:key(a) == "close" then close(top) end
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
    gameOver()
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

-- Typed text (UTF-8) for a popup that takes it (the Hall of Fame).
function game.textinput(s)
  local top = popups[#popups]
  if top and top.text then top:text(s) end
end

-- For tests and screenshots: the current state.
function game.inspect() return { state = state, board = board, blink = blink, popups = popups, highscores = highscores } end
