-- A small placement search that plays the attract mode on the title screen
-- (and lets the boot test exercise the rules for a while without input).
local pieces = require("game.pieces")

local autoplay = {}

local function landing(board, p)
  local q = { name = p.name, rot = p.rot, x = p.x, y = p.y }
  if not board:fits(q) then return nil end
  while true do
    q.y = q.y + 1
    if not board:fits(q) then q.y = q.y - 1; return q end
  end
end

local function evaluate(board, p)
  local filled = {}
  for _, c in ipairs(board:cells(p)) do filled[(p.y + c.y) * 100 + p.x + c.x] = true end
  local function at(x, y) return board.grid[y][x] or filled[y * 100 + x] end
  local lines, holes, height, bump, prev = 0, 0, 0, 0, nil
  for y = 0, board.h - 1 do
    local full = true
    for x = 0, board.w - 1 do if not at(x, y) then full = false; break end end
    if full then lines = lines + 1 end
  end
  for x = 0, board.w - 1 do
    local top = board.h
    for y = 0, board.h - 1 do
      if at(x, y) then
        if top == board.h then top = y end
      elseif top < board.h then
        holes = holes + 1
      end
    end
    local hcol = board.h - top
    height = height + hcol
    if prev then bump = bump + math.abs(hcol - prev) end
    prev = hcol
  end
  return lines * 3.4 - holes * 3.6 - height * 0.5 - bump * 0.18
end

-- Returns the best {rot, x} for the current piece.
function autoplay.choose(board)
  local p = board.piece
  local best, bestScore = nil, -math.huge
  for rot = 1, #pieces.rotations[p.name] do
    for x = -2, board.w do
      local q = landing(board, { name = p.name, rot = rot, x = x, y = p.y })
      if q then
        local s = evaluate(board, q)
        if s > bestScore then best, bestScore = { rot = rot, x = x }, s end
      end
    end
  end
  return best
end

-- One move toward the target per call: rotate first, then slide, then drop.
function autoplay.act(board, target)
  local p = board.piece
  if p.rot ~= target.rot then
    if not board:rotate(1) then board:drop(); return true end
  elseif p.x < target.x then
    if not board:right() then board:drop(); return true end
  elseif p.x > target.x then
    if not board:left() then board:drop(); return true end
  else
    board:drop()
    return true
  end
  return false
end

return autoplay
