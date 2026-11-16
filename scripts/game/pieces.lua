-- The seven tetrominoes. Each rotation is a list of {x, y} cells inside a
-- 4x4 box, y down. Rotation here is plain box rotation with no wall kicks,
-- as early PC Tetris versions did; adjust once the original's rotation
-- rules are confirmed from TETRIS.EXE (re/README.md).
local pieces = {}

local shapes = {
  I = { "....", "####", "....", "...." },
  O = { ".##.", ".##.", "....", "...." },
  T = { ".#..", "###.", "....", "...." },
  S = { ".##.", "##..", "....", "...." },
  Z = { "##..", ".##.", "....", "...." },
  J = { "#...", "###.", "....", "...." },
  L = { "..#.", "###.", "....", "...." },
}

pieces.order = { "I", "O", "T", "S", "Z", "J", "L" }

local function cells(rows)
  local out = {}
  for y = 1, 4 do
    for x = 1, 4 do
      if rows[y]:sub(x, x) == "#" then out[#out + 1] = { x = x - 1, y = y - 1 } end
    end
  end
  return out
end

-- Rotate clockwise inside an n x n box (n = 4 for I, 2 for O, 3 otherwise).
local function rotate(cs, n)
  local out = {}
  for i, c in ipairs(cs) do out[i] = { x = n - 1 - c.y, y = c.x } end
  return out
end

pieces.rotations = {}
for name, rows in pairs(shapes) do
  local n = (name == "I") and 4 or 3
  local r = { cells(rows) }
  if name ~= "O" then
    for i = 2, 4 do r[i] = rotate(r[i - 1], n) end
  end
  pieces.rotations[name] = r
end

return pieces
