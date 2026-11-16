-- Message boxes and simple dialogs in the Windows 3.11 / 95 style: navy
-- caption, gray body, raised border, push buttons with the focused one
-- marked. Left / Right (or Tab) move between buttons, Enter presses,
-- Esc picks the last button (Cancel / No / OK).
local Menu = require("ui.menu")

local Dialog = {}
Dialog.__index = Dialog

local CAPTION_H, LINE_H, BTN_W, BTN_H = 18, 14, 64, 22

-- opts: { title = "", lines = { "..." }, buttons = { "OK" },
--         onbutton = function(index, label) end }
-- Lines are centred; a dialog whose lines hold a tab ("Keys\tWhat") is laid
-- out as two left-aligned columns instead.
function Dialog.new(opts)
  local d = setmetatable({}, Dialog)
  d.title = opts.title or "Tetris for Windows"
  d.lines = opts.lines or {}
  d.buttons = opts.buttons or { "OK" }
  d.onbutton = opts.onbutton
  d.focus = 1
  return d
end

function Dialog:press(i)
  self.closed = true
  if self.onbutton then self.onbutton(i, self.buttons[i]) end
end

function Dialog:key(action)
  local n = #self.buttons
  if action == "left" then self.focus = (self.focus - 2) % n + 1
  elseif action == "right" or action == "next" then self.focus = self.focus % n + 1
  elseif action == "accept" then self:press(self.focus)
  elseif action == "back" or action == "menu" then self:press(n)
  end
  return true
end

local function columns(self)
  local left, right, tabbed = 0, 0, false
  for _, l in ipairs(self.lines) do
    local a, b = l:match("^(.-)\t(.*)$")
    if a then
      tabbed = true
      left = math.max(left, tp.text_width(a, 1, "system"))
      right = math.max(right, tp.text_width(b, 1, "system"))
    end
  end
  return tabbed, left, right
end

local function layout(self)
  local cw, ch = tp.canvas()
  local w = tp.text_width(self.title, 1, "system") + 40
  local tabbed, left, right = columns(self)
  if tabbed then w = math.max(w, left + 24 + right + 40) end
  for _, l in ipairs(self.lines) do
    if not l:find("\t") then w = math.max(w, tp.text_width(l, 1, "system") + 32) end
  end
  w = math.max(w, #self.buttons * (BTN_W + 10) + 30)
  local h = CAPTION_H + 16 + #self.lines * LINE_H + 12 + BTN_H + 14
  return math.floor((cw - w) / 2), math.floor((ch - h) / 2), w, h
end

local function button(x, y, label, focused)
  local f = function(...) tp.fill(...) end
  -- Default/focused buttons get the extra black outline, as in Windows.
  if focused then f(x - 1, y - 1, BTN_W + 2, BTN_H + 2, 0, 0, 0) end
  Menu.bevel(x, y, BTN_W, BTN_H)
  local tw = tp.text_width(label, 1, "system")
  local tx, ty = x + math.floor((BTN_W - tw) / 2), y + 6
  Menu.label(tx, ty, label, 0, 0, 0)
  if focused then  -- dotted focus rectangle
    for k = x + 4, x + BTN_W - 5, 2 do f(k, y + 4, 1, 1, 0, 0, 0); f(k, y + BTN_H - 5, 1, 1, 0, 0, 0) end
    for k = y + 4, y + BTN_H - 5, 2 do f(x + 4, k, 1, 1, 0, 0, 0); f(x + BTN_W - 5, k, 1, 1, 0, 0, 0) end
  end
end

function Dialog:draw()
  local x, y, w, h = layout(self)
  Menu.bevel(x, y, w, h)
  tp.fill(x + 3, y + 3, w - 6, CAPTION_H, 0, 0, 128)
  tp.text(x + math.floor((w - tp.text_width(self.title, 1, "system")) / 2), y + 7, self.title, 1, 255, 255, 255, "system")
  local ty = y + 3 + CAPTION_H + 12
  local tabbed, left = columns(self)
  for _, l in ipairs(self.lines) do
    local a, b = l:match("^(.-)\t(.*)$")
    if a then
      tp.text(x + 20, ty, a, 1, 0, 0, 0, "system")
      tp.text(x + 20 + left + 24, ty, b, 1, 0, 0, 0, "system")
    elseif tabbed then
      tp.text(x + 20, ty, l, 1, 0, 0, 0, "system")
    else
      tp.text(x + math.floor((w - tp.text_width(l, 1, "system")) / 2), ty, l, 1, 0, 0, 0, "system")
    end
    ty = ty + LINE_H
  end
  local n = #self.buttons
  local total = n * BTN_W + (n - 1) * 10
  local bx = x + math.floor((w - total) / 2)
  for i, b in ipairs(self.buttons) do
    button(bx + (i - 1) * (BTN_W + 10), y + h - BTN_H - 12, b, i == self.focus)
  end
end

return Dialog
