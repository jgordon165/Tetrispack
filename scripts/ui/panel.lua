-- The port's popup: a black window in the playfield's gray 3-D frame,
-- holding a scrollable list. Used for the settings menu and for every
-- message (Exit?, Game Over, help). Platform-agnostic: it only takes the
-- actions up / down / left / right / accept / back / menu (ui/input.lua maps
-- keyboard, controller and mouse onto them).
--
-- Rows:
--   { header = "GAME" }                         section title, not selectable
--   { text = "..." }                            a line of text ("a\tb": two columns)
--   { label = "New Game", action = fn }         accept runs it
--   { label = "Level", values = {...}, get = fn, set = fn }
--                                               left / right (or accept) change it
--   enabled = false | fn                        grayed out, can't be chosen
--   label may be a function (e.g. Pause / Resume).
local skin = require("game.skin")

local Panel = {}
Panel.__index = Panel

local ROW_H, PAD, TITLE_H, SCROLL_W = 16, 10, 22, 8

local function value(v, ...)
  if type(v) == "function" then return v(...) end
  return v
end

-- opts: { title, rows, width, visible (rows shown before scrolling),
--         focus (index of the first selected row), onclose = fn }
function Panel.new(opts)
  local p = setmetatable({}, Panel)
  p.title = opts.title
  p.rows = opts.rows
  p.width = opts.width or 280
  p.visible = math.min(opts.visible or 12, #p.rows)
  p.onclose = opts.onclose
  p.top = 1
  p.sel = nil
  if opts.focus then p.sel = opts.focus else
    for i, r in ipairs(p.rows) do
      if p:selectable(r) then p.sel = i; break end
    end
  end
  p:scrollTo(p.sel or 1)
  return p
end

function Panel:selectable(r)
  return (r.action or r.values) and value(r.enabled) ~= false
end

function Panel:scrollTo(i)
  if i < self.top then self.top = i end
  if i > self.top + self.visible - 1 then self.top = i - self.visible + 1 end
  -- Keep a header that leads a section in view with its first row.
  if self.top > 1 and self.rows[self.top - 1].header and i == self.top then self.top = self.top - 1 end
end

function Panel:move(d)
  if not self.sel then
    self.top = math.max(1, math.min(self.top + d, #self.rows - self.visible + 1))
    return
  end
  local i = self.sel
  for _ = 1, #self.rows do
    i = (i - 1 + d) % #self.rows + 1
    if self:selectable(self.rows[i]) then break end
  end
  if d > 0 and i < self.sel then self.top = 1 end   -- wrapped to the top
  self.sel = i
  self:scrollTo(i)
end

local function cycle(r, d)
  local cur, idx = r.get(), 1
  for k, v in ipairs(r.values) do if v == cur then idx = k end end
  r.set(r.values[(idx - 1 + d) % #r.values + 1])
end

-- Returns "close" when the panel should go away.
function Panel:key(action)
  local r = self.sel and self.rows[self.sel]
  if action == "up" then self:move(-1)
  elseif action == "down" then self:move(1)
  elseif (action == "left" or action == "right") and r and r.values then
    cycle(r, action == "left" and -1 or 1)
  elseif action == "accept" and r then
    if r.values then cycle(r, 1)
    elseif r.action then return r.action() end
  elseif action == "back" or action == "menu" then
    return "close"
  end
end

-- The selected row's label, for tests.
function Panel:current()
  local r = self.sel and self.rows[self.sel]
  return r and value(r.label) or nil
end

-- Drawing ------------------------------------------------------------------

local function geometry(self)
  local cw, ch = tp.canvas()
  local w = self.width
  local h = (self.title and TITLE_H or 4) + self.visible * ROW_H + 8
  return math.floor((cw - w) / 2), math.floor((ch - h) / 2), w, h
end

local function textWidth(s) return tp.text_width(s, 1, "system") end
local function text(x, y, s, c) tp.text(x, y, s, 1, c[1], c[2], c[3], "system") end

local WHITE, GRAY, DIM, YELLOW, HEAD = { 255, 255, 255 }, { 192, 192, 192 }, { 110, 110, 110 },
  { 255, 255, 0 }, { 0, 192, 192 }

-- A bar in the blue block's colours: the highlight.
local function bar(x, y, w, h)
  local base, light = skin.base[3], skin.light[3]
  tp.fill(x, y, w, h, base[1], base[2], base[3])
  tp.fill(x, y, w - 1, 1, light[1], light[2], light[3])
  tp.fill(x, y, 1, h - 1, light[1], light[2], light[3])
end

local function arrowLeft(x, y, c)   -- tip on the left
  for k = 0, 3 do tp.fill(x + k, y + 3 - k, 1, 1 + 2 * k, c[1], c[2], c[3]) end
end
local function arrowRight(x, y, c)  -- tip on the right
  for k = 0, 3 do tp.fill(x + 3 - k, y + 3 - k, 1, 1 + 2 * k, c[1], c[2], c[3]) end
end

function Panel:draw()
  local x, y, w, h = geometry(self)
  skin.frame(x, y, w, h, { 4, 4, 4, 4 })
  tp.fill(x, y, w, h, 0, 0, 0)
  local cy = y + 4
  if self.title then
    text(x + math.floor((w - textWidth(self.title)) / 2), cy + 2, self.title, WHITE)
    tp.fill(x + PAD, cy + 16, w - 2 * PAD, 1, 128, 128, 128)
    cy = cy + TITLE_H
  end

  local scroll = #self.rows > self.visible
  local right = x + w - PAD - (scroll and SCROLL_W + 4 or 0)

  -- Two-column text rows share one tab stop.
  local tab = 0
  for _, r in ipairs(self.rows) do
    local a = r.text and r.text:match("^(.-)\t")
    if a then tab = math.max(tab, textWidth(a)) end
  end

  for i = self.top, math.min(#self.rows, self.top + self.visible - 1) do
    local r = self.rows[i]
    local ry = cy + (i - self.top) * ROW_H
    local on = self:selectable(r)
    local hot = (i == self.sel)
    if r.header then
      text(x + PAD, ry + 4, r.header, HEAD)
    elseif r.text then
      local a, b = r.text:match("^(.-)\t(.*)$")
      if a then
        text(x + PAD, ry + 3, a, GRAY)
        text(x + PAD + tab + 16, ry + 3, b, GRAY)
      elseif r.center then
        text(x + math.floor((w - textWidth(r.text)) / 2), ry + 3, r.text, WHITE)
      else
        text(x + PAD, ry + 3, r.text, GRAY)
      end
    else
      if hot then bar(x + PAD - 4, ry, right - x - PAD + 8, ROW_H) end
      local label = value(r.label)
      local c = on and WHITE or DIM
      local lx = r.center and x + math.floor((w - textWidth(label)) / 2) or x + PAD + 6
      text(lx, ry + 3, label, c)
      if r.values then
        local v = tostring(r.get())
        local vw = textWidth(v)
        local vx = right - vw - (hot and 10 or 2)
        text(vx, ry + 3, v, on and (hot and WHITE or YELLOW) or DIM)
        if hot and on then
          arrowLeft(vx - 10, ry + 4, WHITE)
          arrowRight(right - 6, ry + 4, WHITE)
        end
      end
    end
  end

  if scroll then  -- scroll bar: gray track, raised thumb
    local tx, ty, th = x + w - PAD - SCROLL_W, cy, self.visible * ROW_H
    tp.fill(tx, ty, SCROLL_W, th, 64, 64, 64)
    local n = #self.rows
    local thumbH = math.max(10, math.floor(th * self.visible / n))
    local thumbY = ty + math.floor((th - thumbH) * (self.top - 1) / (n - self.visible))
    tp.fill(tx, thumbY, SCROLL_W, thumbH, 192, 192, 192)
    tp.fill(tx, thumbY, SCROLL_W - 1, 1, 255, 255, 255)
    tp.fill(tx, thumbY, 1, thumbH - 1, 255, 255, 255)
    tp.fill(tx + SCROLL_W - 1, thumbY, 1, thumbH, 128, 128, 128)
    tp.fill(tx, thumbY + thumbH - 1, SCROLL_W, 1, 128, 128, 128)
  end
end

return Panel
