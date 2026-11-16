-- Lua tests, run by the engine (ctest "lua"): the game modules come from the
-- real scripts/ folder through TETRISPACK_SCRIPTS.
local Board = require("game.board")
local pieces = require("game.pieces")
local autoplay = require("game.autoplay")

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

-- Every piece has 4 cells in every rotation, and 4 rotations bring it back.
for _, name in ipairs(pieces.order) do
  for r, cells in ipairs(pieces.rotations[name]) do
    check(#cells == 4, name .. " rotation " .. r .. " has 4 cells")
  end
end
check(#pieces.rotations.O == 1, "O has one rotation")
check(#pieces.rotations.T == 4, "T has four rotations")

-- Walls stop movement.
do
  local b = Board.new({ rng = seq(2) })  -- always O
  for _ = 1, 20 do b:left() end
  local minx = math.huge
  for _, c in ipairs(b:cells()) do minx = math.min(minx, b.piece.x + c.x) end
  check(minx == 0, "piece stops at the left wall")
end

-- Clearing: fill the bottom row except where an I drops in flat.
do
  local b = Board.new({ rng = seq(1) })  -- always I
  for x = 4, 9 do b.grid[19][x] = "Z" end
  b.piece = { name = "I", rot = 1, x = 0, y = 0 }  -- flat, cells x 0..3
  local cleared = b:drop()
  check(cleared == 1, "one line cleared")
  check(b.lines == 1, "lines counted")
  check(b.score == Board.line_points[1], "single line scored")
  for x = 0, 9 do check(b.grid[19][x] == nil, "row emptied at x=" .. x) end
end

-- A tetris (four lines) scores line_points[4] x level.
do
  local b = Board.new({ rng = seq(1), level = 2 })
  for y = 16, 19 do for x = 1, 9 do b.grid[y][x] = "S" end end
  b.piece = { name = "I", rot = 2, x = -2, y = 0 }  -- vertical, column x = 0
  check(b:fits(b.piece), "vertical I fits at the wall")
  check(b:drop() == 4, "four lines cleared")
  check(b.score == Board.line_points[4] * 2, "tetris scored times level")
end

-- Game over when the spawn is blocked.
do
  local b = Board.new({ rng = seq(2) })
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
  end
  check(b.lines > 0, "autoplay clears lines (" .. b.lines .. ")")
end

-- With data: the executable's resources are listed.
if tp.has_data() then
  check(#tp.resources() > 0, "[data] resources listed")
end

print(("lua: %d checks, %d failed"):format(count, failures))
tp.quit(failures == 0 and 0 or 1)

game = {}
