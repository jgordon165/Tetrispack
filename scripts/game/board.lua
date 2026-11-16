-- The playfield and its rules, with no drawing or input (so the tests can
-- drive it directly). Values marked "verify" are placeholders until they're
-- confirmed from the original executable.
local pieces = require("game.pieces")

local Board = {}
Board.__index = Board

-- Points per lines cleared at once, times the level. Hard Drop's wiki lists
-- 200/400/800 for two/three/four rows in this version; the single-row value
-- and the level factor are "verify".
Board.line_points = { 100, 200, 400, 800 }
Board.lines_per_level = 10        -- verify
-- Seconds per row of gravity by level (verify against the original's timer).
Board.gravity = { 0.80, 0.72, 0.63, 0.55, 0.47, 0.38, 0.30, 0.22, 0.13, 0.10, 0.08 }

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
  b.grid = {}
  for y = 0, b.h - 1 do
    b.grid[y] = {}
  end
  b.next = b:random_piece()
  b:spawn()
  return b
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
  self.piece = { name = name, rot = 1, x = math.floor(self.w / 2) - 2, y = 0 }
  if not self:fits(self.piece) then self.over = true end
end

local function moved(p, dx, dy, drot)
  local n = #pieces.rotations[p.name]
  return { name = p.name, rot = (p.rot - 1 + (drot or 0)) % n + 1, x = p.x + dx, y = p.y + dy }
end

function Board:try(dx, dy, drot)
  if self.over then return false end
  local p = moved(self.piece, dx, dy, drot)
  if self:fits(p) then self.piece = p; return true end
  return false
end

function Board:left() return self:try(-1, 0) end
function Board:right() return self:try(1, 0) end
function Board:rotate(dir) return self:try(0, 0, dir or 1) end

-- Moves down a row; locks the piece when it can't. Returns rows cleared.
function Board:step()
  if self.over then return 0 end
  if self:try(0, 1) then return 0 end
  return self:lock()
end

function Board:drop()
  if self.over then return 0 end
  local rows = 0
  while self:try(0, 1) do rows = rows + 1 end
  return self:lock(), rows
end

function Board:lock()
  for _, c in ipairs(self:cells()) do
    local y = self.piece.y + c.y
    if y < 0 then self.over = true; return 0 end
    self.grid[y][self.piece.x + c.x] = self.piece.name
  end
  local cleared = self:clear_lines()
  if cleared > 0 then
    self.score = self.score + (Board.line_points[cleared] or 0) * self.level
    self.lines = self.lines + cleared
    self.level = math.max(self.level, math.floor(self.lines / Board.lines_per_level) + 1)
  end
  self:spawn()
  return cleared
end

function Board:full(y)
  for x = 0, self.w - 1 do
    if not self.grid[y][x] then return false end
  end
  return true
end

function Board:clear_lines()
  local cleared, y = 0, self.h - 1
  while y >= 0 do
    if self:full(y) then
      for yy = y, 1, -1 do self.grid[yy] = self.grid[yy - 1] end
      self.grid[0] = {}
      cleared = cleared + 1
    else
      y = y - 1
    end
  end
  return cleared
end

function Board:gravity_delay()
  return Board.gravity[math.min(self.level, #Board.gravity)]
end

-- Advances gravity by dt seconds.
function Board:update(dt)
  self.fall = self.fall + dt
  local d = self:gravity_delay()
  while self.fall >= d and not self.over do
    self.fall = self.fall - d
    self:step()
  end
end

return Board
