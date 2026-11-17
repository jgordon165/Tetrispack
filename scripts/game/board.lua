-- The playfield and its rules, with no drawing or input (so the tests can
-- drive it directly). Numbers come from TETRIS.EXE 2.00 (docs/original.md).
local pieces = require("game.pieces")

local Board = {}
Board.__index = Board

-- Points for rows cleared at once (no level factor in the original).
Board.line_points = { 100, 200, 400, 800 }
-- Points for every piece that lands: 6 x level.
Board.piece_points = 6
-- Gravity timer per level, milliseconds per row (the original's SetTimer values).
Board.gravity_ms = { 600, 600, 520, 450, 381, 306, 230, 155, 84, 50 }
-- Total lines that take you to level 2, 3, ... 10 (never down).
Board.level_lines = { 10, 16, 24, 34, 36, 50, 64, 78, 92 }

function Board.new(opts)
  opts = opts or {}
  local b = setmetatable({}, Board)
  b.w = opts.w or 10
  b.h = opts.h or 20
  b.rng = opts.rng or math.random
  b.level = opts.level or 1
  b.score, b.lines = 0, 0
  b.over = false
  b.fall = 0
  b.clearing = nil   -- rows waiting to be removed (the blink plays meanwhile)
  b.grid = {}
  for y = 0, b.h - 1 do b.grid[y] = {} end
  b:fill_rows(opts.rows or 0)
  b.next = b:random_piece()
  b:spawn()
  return b
end

-- Skill > Starting Rows: the bottom n rows start partly filled. (The
-- original's fill pattern isn't confirmed yet: here each cell is filled at
-- random, at least one gap and one block per row.)
function Board:fill_rows(n)
  for y = self.h - n, self.h - 1 do
    local row, count = {}, 0
    repeat
      row, count = {}, 0
      for x = 0, self.w - 1 do
        if self.rng(2) == 1 then
          row[x] = pieces.order[self.rng(#pieces.order)]
          count = count + 1
        end
      end
    until count > 0 and count < self.w
    self.grid[y] = row
  end
end

function Board:random_piece()
  return pieces.order[self.rng(#pieces.order)]
end

function Board:cells(p)
  p = p or self.piece
  return pieces.rotations[p.name][p.rot]
end

function Board:fits(p)
  for _, c in ipairs(self:cells(p)) do
    local x, y = p.x + c.x, p.y + c.y
    if x < 0 or x >= self.w or y >= self.h then return false end
    if y >= 0 and self.grid[y][x] then return false end
  end
  return true
end

function Board:spawn()
  local name = self.next
  self.next = self:random_piece()
  self.piece = { name = name, rot = 1, x = pieces.spawn_x[name], y = 0 }
  self.fall = 0
  if not self:fits(self.piece) then self.over = true end
end

local function moved(p, dx, dy, drot)
  local n = #pieces.rotations[p.name]
  return { name = p.name, rot = (p.rot - 1 + (drot or 0)) % n + 1, x = p.x + dx, y = p.y + dy }
end

function Board:busy() return self.over or self.clearing ~= nil end

function Board:try(dx, dy, drot)
  if self:busy() then return false end
  local p = moved(self.piece, dx, dy, drot)
  if self:fits(p) then self.piece = p; return true end
  return false
end

function Board:left() return self:try(-1, 0) end
function Board:right() return self:try(1, 0) end
function Board:rotate(dir) return self:try(0, 0, dir or 1) end
-- Home / End in the original: slide all the way.
function Board:slide(dx) while self:try(dx, 0) do end end

-- Moves down a row; lands the piece when it can't. Returns rows filled.
function Board:step()
  if self:busy() then return 0 end
  if self:try(0, 1) then return 0 end
  return self:lock()
end

function Board:drop()
  if self:busy() then return 0, 0 end
  local rows = 0
  while self:try(0, 1) do rows = rows + 1 end
  return self:lock(), rows
end

function Board:full_rows()
  local rows = {}
  for y = 0, self.h - 1 do
    local full = true
    for x = 0, self.w - 1 do
      if not self.grid[y][x] then full = false; break end
    end
    if full then rows[#rows + 1] = y end
  end
  return rows
end

-- Lands the piece. Full rows are left in place, listed in self.clearing,
-- until collapse() (so the blink can show them); returns how many.
function Board:lock()
  for _, c in ipairs(self:cells()) do
    local y = self.piece.y + c.y
    if y < 0 then self.over = true; return 0 end
    self.grid[y][self.piece.x + c.x] = self.piece.name
  end
  self.score = self.score + Board.piece_points * self.level
  local rows = self:full_rows()
  if #rows > 0 then
    self.clearing = rows
    return #rows
  end
  self:spawn()
  return 0
end

-- Removes the full rows, scores them and brings in the next piece.
function Board:collapse()
  local rows = self.clearing
  if not rows then return 0 end
  self.clearing = nil
  local full = {}
  for _, y in ipairs(rows) do full[y] = true end
  local dst = self.h - 1
  for y = self.h - 1, 0, -1 do
    if not full[y] then
      self.grid[dst] = self.grid[y]
      dst = dst - 1
    end
  end
  for y = dst, 0, -1 do self.grid[y] = {} end

  local n = #rows
  self.score = self.score + (Board.line_points[n] or Board.line_points[#Board.line_points])
  self.lines = self.lines + n
  for i, need in ipairs(Board.level_lines) do
    if self.lines >= need and self.level < i + 1 then self.level = i + 1 end
  end
  self:spawn()
  return n
end

function Board:gravity_delay()
  return Board.gravity_ms[math.min(self.level, #Board.gravity_ms)] / 1000
end

-- Advances gravity by dt seconds.
function Board:update(dt)
  if self:busy() then return end
  self.fall = self.fall + dt
  local d = self:gravity_delay()
  while self.fall >= d and not self:busy() do
    self.fall = self.fall - d
    self:step()
  end
end

return Board
