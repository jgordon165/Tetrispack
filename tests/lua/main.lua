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
