-- Lua tests, run by the engine (ctest "lua"): the game modules come from the
-- real scripts/ folder through TETRISPACK_SCRIPTS.
local Board = require("game.board")
local pieces = require("game.pieces")
local autoplay = require("game.autoplay")
local layout = require("game.layout")

local failures, count = 0, 0
local function check(cond, what)
  count = count + 1
  if not cond then
    failures = failures + 1
    print("FAIL: " .. what)
  end
end

local function seq(...)
  local list, i = { ... }, 0
  return function() i = i % #list + 1; return list[i] end
end
local index = {}
for i, n in ipairs(pieces.order) do index[n] = i end

-- Pieces: 4 cells in every rotation; 4 rotations come back to the start.
for _, name in ipairs(pieces.order) do
  local r = pieces.rotations[name]
  for k, cells in ipairs(r) do check(#cells == 4, name .. " rotation " .. k .. " has 4 cells") end
  if #r == 4 then
    local back = {}
    for i, c in ipairs(r[4]) do back[i] = { x = -c.y, y = c.x } end
    for i, c in ipairs(back) do check(c.x == r[1][i].x and c.y == r[1][i].y, name .. " rotates back") end
  end
end
check(#pieces.rotations.O == 1, "O has one rotation")

-- Spawn cells are the original's (rows 0-1, columns 3-6).
local function spawnCells(name)
  local b = Board.new({ rng = seq(index[name]) })
  local set = {}
  for _, c in ipairs(b:cells()) do set[#set + 1] = (b.piece.y + c.y) * 10 + b.piece.x + c.x end
  table.sort(set)
  return table.concat(set, ",")
end
check(spawnCells("I") == "3,4,5,6", "I spawn")
check(spawnCells("Z") == "3,4,14,15", "Z spawn")
check(spawnCells("S") == "5,6,14,15", "S spawn")
check(spawnCells("O") == "4,5,14,15", "O spawn")
check(spawnCells("J") == "4,5,6,16", "J spawn")
check(spawnCells("L") == "3,4,5,13", "L spawn")
check(spawnCells("T") == "3,4,5,14", "T spawn")
check(pieces.type.I == 1 and pieces.type.S == 3 and pieces.type.T == 7, "colour numbers")

-- Walls stop movement; slide goes all the way.
do
  local b = Board.new({ rng = seq(index.O) })
  b:slide(-1)
  local minx = math.huge
  for _, c in ipairs(b:cells()) do minx = math.min(minx, b.piece.x + c.x) end
  check(minx == 0, "slide stops at the left wall")
end

-- A single line: blink first (rows kept), then collapse and score.
do
  local b = Board.new({ rng = seq(index.I), level = 3 })
  for x = 4, 9 do b.grid[19][x] = "Z" end
  b.piece = { name = "I", rot = 1, x = 1, y = 0 }   -- cells x 0..3
  local filled = b:drop()
  check(filled == 1, "one row filled")
  check(b.clearing and b.clearing[1] == 19, "row kept for the blink")
  check(b.grid[19][0] ~= nil, "row still there while blinking")
  check(b:busy(), "board busy while blinking")
  local before = b.score
  check(before == Board.piece_points * 3, "landing scores 6 x level")
  check(b:collapse() == 1, "collapse removes it")
  check(b.lines == 1, "lines counted")
  check(b.score == before + 100, "single line: 100")
  for x = 0, 9 do check(b.grid[19][x] == nil, "row emptied at x=" .. x) end
end

-- Four lines: 800, rows above move down.
do
  local b = Board.new({ rng = seq(index.I) })
  for y = 16, 19 do for x = 1, 9 do b.grid[y][x] = "S" end end
  b.grid[15][5] = "T"
  b.piece = { name = "I", rot = 2, x = 0, y = 1 }   -- vertical: cells y -1..2 at x 0
  check(b:fits(b.piece), "vertical I fits at the wall")
  check(b:drop() == 4, "four rows filled")
  local before = b.score
  b:collapse()
  check(b.score == before + 800, "tetris: 800")
  check(b.grid[19][5] == "T", "block above dropped four rows")
end

-- Levels go up with lines as the original's table says.
do
  local b = Board.new({ rng = seq(index.I) })
  b.lines = 9
  for x = 4, 9 do b.grid[19][x] = "Z" end
  b.piece = { name = "I", rot = 1, x = 1, y = 0 }
  b:drop(); b:collapse()
  check(b.level == 2, "10 lines: level 2")
  check(b:gravity_delay() == 0.6, "level 2 timer 600 ms")
  b.level = 10
  check(b:gravity_delay() == 0.05, "level 10 timer 50 ms")
end

-- Game over when the spawn is blocked.
do
  local b = Board.new({ rng = seq(index.O) })
  for y = 0, 1 do for x = 0, 9 do b.grid[y][x] = "T" end end
  b:spawn()
  check(b.over, "blocked spawn ends the game")
end

-- The autoplayer survives a while and clears lines.
do
  math.randomseed(1)
  local b = Board.new()
  for _ = 1, 400 do
    if b.over then break end
    local t = autoplay.choose(b)
    while t and not autoplay.act(b, t) do end
    if b.clearing then b:collapse() end
  end
  check(b.lines > 0, "autoplay clears lines (" .. b.lines .. ")")
end


-- Popup menu: navigation, submenus, mnemonics, disabled items.
do
  local Menu = require("ui.menu")
  local input = require("ui.input")
  local log = {}
  local m = Menu.new({
    { label = "&Game", sub = {
      { label = "&New", accel = "F2", action = function() log[#log + 1] = "new" end },
      { separator = true },
      { label = "&High Scores...", enabled = false, action = function() log[#log + 1] = "hs" end },
    } },
    { label = "&Skill", sub = {
      { label = "Starting &Level", sub = {
        { label = "&1", radio = true, action = function() log[#log + 1] = "l1" end },
        { label = "&2", radio = true, action = function() log[#log + 1] = "l2" end },
      } },
    } },
  })
  local closed = 0
  m.onclose = function() closed = closed + 1 end
  check(input.action("game", "escape") == "menu" and input.action("game", "f12") == "menu"
        and input.action("game", "pad start") == "menu", "Esc, F12 and Start open the menu")
  m:open(10, 10)
  check(m:isOpen() and m:path()[1] == "Game", "opens on Game")
  m:key("down"); check(m:path()[1] == "Skill", "down moves")
  m:key("down"); check(m:path()[1] == "Game", "wraps")
  m:key("right"); check(#m:path() == 2 and m:path()[2] == "New", "right opens the submenu")
  m:key("down"); check(m:path()[2] == "High Scores...", "down skips the separator")
  m:key("accept"); check(m:isOpen() and #log == 0, "disabled item does nothing")
  m:key("up"); m:key("accept")
  check(not m:isOpen() and log[1] == "new" and closed == 1, "accept runs the action and closes")
  m:open(10, 10)
  m:key(nil, "s"); m:key(nil, "l"); m:key("down"); m:key("accept")
  check(log[2] == "l2", "mnemonics and cascades: Skill > Starting Level > 2")
  m:open(10, 10)
  m:key("right"); m:key("left"); check(#m:path() == 1 and m:isOpen(), "left closes a submenu")
  m:key("back"); check(not m:isOpen(), "Esc at the top closes the menu")
  m:open(10, 10); m:key("right"); m:key("menu"); check(not m:isOpen(), "F12 / Start closes it all")
end

-- Dialogs: buttons, focus, Esc picks the last.
do
  local Dialog = require("ui.dialog")
  local got
  local d = Dialog.new({ lines = { "End Tetris?" }, buttons = { "&Yes", "&No" }, onbutton = function(i) got = i end })
  d:key("right"); d:key("accept"); check(got == 2 and d.closed, "focus moves, Enter presses")
  d = Dialog.new({ buttons = { "&Yes", "&No" }, onbutton = function(i) got = i end })
  d:key("back"); check(got == 2, "Esc picks the last button")
end

-- Starting rows: partly filled, never full.
do
  math.randomseed(3)
  local b = Board.new({ rows = 5 })
  local ok = true
  for y = 15, 19 do
    local n = 0
    for x = 0, 9 do if b.grid[y][x] then n = n + 1 end end
    if n == 0 or n == 10 then ok = false end
  end
  for y = 0, 14 do if next(b.grid[y]) then ok = false end end
  check(ok, "starting rows fill the bottom 5 rows, with gaps")
end

-- Layout: the playfield is where the original's WM_SIZE puts it.
check(layout.grid.cw == 22 and layout.grid.ch == 19, "cell size 22x19 at 640x440")

-- With data: the executable's resources are listed, backgrounds load.
if tp.has_data() then
  check(#tp.resources() > 0, "[data] resources listed")
  for i = 1, 10 do
    local h, w = tp.bitmap(i)
    check(h ~= nil and w == 32, "[data] background bitmap " .. i)
  end
end

print(("lua: %d checks, %d failed"):format(count, failures))
tp.quit(failures == 0 and 0 or 1)

game = {}
