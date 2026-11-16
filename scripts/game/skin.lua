-- How blocks are drawn. With the player's TETRIS.EXE, blocks come from its
-- BITMAP resources; without it (or until the mapping below is filled in from
-- the resource listing, tools/identify_exe.py --list), flat colours are used.
local skin = { bitmap = nil, size = 10 }

-- Flat fallback colours, one per piece (the port's own, not the original's).
skin.colors = {
  I = { 0, 170, 170 }, O = { 170, 170, 0 }, T = { 170, 0, 170 }, S = { 0, 170, 0 },
  Z = { 170, 0, 0 }, J = { 0, 0, 170 }, L = { 170, 85, 0 },
}

-- Fill in once the block bitmap is identified: the resource name (e.g. "#101")
-- and, per piece, the source rectangle inside it.
skin.mapping = nil  -- { resource = "#101", cells = { I = { x, y }, ... }, size = 10 }

function skin.load()
  if not skin.mapping or not tp.has_data() then return end
  local h, err = tp.bitmap(skin.mapping.resource)
  if h then
    skin.bitmap = h
    skin.size = skin.mapping.size or skin.size
  else
    tp.log("skin: " .. tostring(err))
  end
end

function skin.block(name, x, y, size)
  if skin.bitmap and skin.mapping.cells[name] then
    local c = skin.mapping.cells[name]
    tp.draw(skin.bitmap, c[1], c[2], skin.size, skin.size, x, y, size, size)
    return
  end
  local c = skin.colors[name] or { 128, 128, 128 }
  tp.fill(x, y, size, size, c[1], c[2], c[3])
  tp.fill(x, y, size, 1, 255, 255, 255, 110)        -- light top edge
  tp.fill(x, y + size - 1, size, 1, 0, 0, 0, 110)   -- dark bottom edge
end

return skin
