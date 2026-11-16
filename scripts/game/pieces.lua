-- The seven pieces as TETRIS.EXE numbers them (1-7, also their colour
-- numbers): cells relative to a pivot, x right and y down, in the
-- orientation the original spawns them (its spawn routine writes these cells
-- into rows 0-1, columns 3-6; see docs/original.md). Rotation turns the cells
-- about the pivot; the original's exact rotation rule is still to confirm.
local pieces = {}

pieces.order = { "I", "Z", "S", "O", "J", "L", "T" }

-- type number (= colour), spawn pivot column, spawn cells
local defs = {
  I = { type = 1, x = 4, cells = { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } } },
  Z = { type = 2, x = 4, cells = { { -1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 } } },
  S = { type = 3, x = 5, cells = { { 0, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 } } },
  O = { type = 4, x = 4, cells = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } }, fixed = true },
  J = { type = 5, x = 5, cells = { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 1, 1 } } },
  L = { type = 6, x = 4, cells = { { -1, 0 }, { 0, 0 }, { 1, 0 }, { -1, 1 } } },
  T = { type = 7, x = 4, cells = { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 0, 1 } } },
}

pieces.type, pieces.spawn_x, pieces.rotations = {}, {}, {}
for name, d in pairs(defs) do
  pieces.type[name] = d.type
  pieces.spawn_x[name] = d.x
  local rots = {}
  local cur = {}
  for i, c in ipairs(d.cells) do cur[i] = { x = c[1], y = c[2] } end
  rots[1] = cur
  if not d.fixed then
    for r = 2, 4 do
      local nxt = {}
      for i, c in ipairs(rots[r - 1]) do nxt[i] = { x = -c.y, y = c.x } end  -- clockwise, y down
      rots[r] = nxt
    end
  end
  pieces.rotations[name] = rots
end

return pieces
