-- Drawing the original's look: its block colours and bevel, its gray 3-D
-- frames, and the level backgrounds (BITMAP 1-10 of TETRIS.EXE, tiled).
-- Values from TETRIS.EXE 2.00; see docs/original.md.
local skin = {}

-- Colour table (0-10 per channel) for block colours 1-7, from the
-- executable's palette setup; base = x19.2, light = x25.5, dark = x12.8.
local rgb10 = {
  { 10, 0, 0 }, { 0, 10, 0 }, { 0, 0, 10 }, { 0, 10, 10 },
  { 10, 0, 10 }, { 10, 10, 0 }, { 6, 6, 6 },
}
skin.base, skin.light, skin.dark = {}, {}, {}
local function scale(c, f) return { math.floor(c[1] * f), math.floor(c[2] * f), math.floor(c[3] * f) } end
for i, c in ipairs(rgb10) do
  skin.base[i] = scale(c, 19.2)
  skin.light[i] = scale(c, 25.5)
  skin.dark[i] = scale(c, 12.8)
end
-- On the reference screenshots the right and bottom bevel lines show in the
-- block's own colour, not the darker shade: draw them that way.
skin.dark_bevel = false

skin.gray = { 192, 192, 192 }
skin.shadow = { 128, 128, 128 }
skin.white = { 255, 255, 255 }
skin.black = { 0, 0, 0 }

local fill = function(x, y, w, h, c, inv)
  if w <= 0 or h <= 0 then return end
  if inv then tp.fill(x, y, w, h, 255 - c[1], 255 - c[2], 255 - c[3])
  else tp.fill(x, y, w, h, c[1], c[2], c[3]) end
end
skin.fill = fill

-- One block in cell (L, T) of cw x ch pixels, as the original's DrawBlock:
-- three light lines on the top and left, three dark ones on the right and
-- bottom, the colour inside, and the cell's last row and column left black
-- (the grid lines). inv draws it inverted (the line-clear blink).
function skin.block(color, L, T, cw, ch, inv)
  local base, light = skin.base[color], skin.light[color]
  if not base then return end
  local dark = skin.dark_bevel and skin.dark[color] or base
  local R1, B1 = L + cw - 2, T + ch - 2   -- R' and B': last coloured column and row
  fill(L, T, cw - 1, ch - 2, base, inv)                -- all but row B'
  fill(L + 1, B1, cw - 2, 1, base, inv)                -- row B' (its first pixel stays black)
  for k = 0, 2 do
    fill(L + k, T + k, (R1 - k) - (L + k), 1, light, inv)              -- top
    fill(L + k, T + k + 1, 1, (B1 - k) - (T + k + 1), light, inv)      -- left
  end
  if dark ~= base then
    fill(R1, T, 1, B1 - T, dark, inv)                   -- right, outer
    fill(L + 1, B1, R1 - L, 1, dark, inv)               -- bottom, outer
    fill(R1 - 1, T + 1, 1, B1 - 1 - T, dark, inv)
    fill(L + 1, B1 - 1, R1 - 2 - L, 1, dark, inv)
    fill(R1 - 2, T + 2, 1, B1 - T - 3, dark, inv)
    fill(L + 2, B1 - 2, R1 - 4 - L, 1, dark, inv)
  end
end

-- A Windows 3.x style gray frame around the content rect (x, y, w, h):
-- raised outer edge (4 px white top-left, 3 px dark bottom-right), 6 px of
-- gray, a sunken line (black top-left, white bottom-right), then `margin`
-- px of gray up to the content ({left, top, right, bottom}). Measured from
-- the original's screens.
function skin.frame(x, y, w, h, margin)
  local m = margin or { 0, 0, 0, 0 }
  local ax, ay, aw, ah = x - m[1], y - m[2], w + m[1] + m[3], h + m[2] + m[4]  -- A: content + margin
  local bx, by, bw, bh = ax - 1, ay - 1, aw + 2, ah + 2                          -- B: sunken line
  local cx, cy, cw, ch = bx - 6, by - 6, bw + 12, bh + 12                        -- C: gray band
  local dx, dy, dw, dh = cx - 4, cy - 4, cw + 7, ch + 7                          -- D: raised edge
  fill(dx, dy, dw, dh, skin.white)
  fill(dx + dw - 3, dy, 3, dh, skin.shadow)
  fill(dx, dy + dh - 3, dw, 3, skin.shadow)
  fill(cx, cy, cw, ch, skin.gray)
  fill(bx, by, bw, bh, skin.white)
  fill(bx, by, bw - 1, 1, skin.black)
  fill(bx, by, 1, bh - 1, skin.black)
  fill(ax, ay, aw, ah, skin.gray)
end

-- A sunken box (the next-piece box): black top-left, white bottom-right.
function skin.sunken(x, y, w, h)
  fill(x, y, w, h, skin.white)
  fill(x, y, w - 1, 1, skin.black)
  fill(x, y, 1, h - 1, skin.black)
  fill(x + 1, y + 1, w - 2, h - 2, skin.gray)
end

-- Level backgrounds: BITMAP n for level n, tiled over the window.
local bg = {}
function skin.background(level, w, h)
  if bg[level] == nil then
    bg[level] = false
    if tp.has_data() then
      local handle, bw, bh = tp.bitmap(level)
      if handle then bg[level] = { handle = handle, w = bw, h = bh } end
    end
  end
  local b = bg[level]
  if not b then
    tp.clear(0, 128, 128)   -- without TETRIS.EXE: a plain colour
    return
  end
  tp.clear(0, 0, 0)
  for y = 0, h - 1, b.h do
    for x = 0, w - 1, b.w do tp.draw(b.handle, 0, 0, b.w, b.h, x, y) end
  end
end

return skin
