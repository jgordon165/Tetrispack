-- A popup menu in the Windows 3.11 / 95 style: gray, raised 3-D border,
-- navy highlight, check marks, radio bullets, cascading submenus, & mnemonic
-- letters underlined. Keyboard and controller driven (ui.input actions).
--
-- Items: { label = "&New", accel = "F2", action = fn, sub = { items },
--          checked = bool|fn, radio = bool, enabled = bool|fn } or
--        { separator = true }.
local Menu = {}
Menu.__index = Menu

local ITEM_H, SEP_H, PAD_L, PAD_R, ACCEL_GAP = 18, 8, 20, 18, 18

local function value(v, ...)
  if type(v) == "function" then return v(...) end
  return v
end

-- "&New" -> "New", 1 (the mnemonic's position), "n"
local function parse(label)
  local i = label:find("&", 1, true)
  if not i then return label, nil, nil end
  local text = label:sub(1, i - 1) .. label:sub(i + 1)
  return text, i, text:sub(i, i):lower()
end

function Menu.new(items)
  return setmetatable({ items = items, levels = {}, open_ = false }, Menu)
end

function Menu:isOpen() return self.open_ end

local function enabled(item) return not item.separator and value(item.enabled) ~= false end

local function measure(items)
  local w, h, aw = 0, 6, 0
  for _, it in ipairs(items) do
    if it.separator then
      h = h + SEP_H
    else
      local text = parse(it.label)
      w = math.max(w, tp.text_width(text, 1, "system"))
      if it.accel then aw = math.max(aw, tp.text_width(it.accel, 1, "system")) end
      h = h + ITEM_H
    end
  end
  return PAD_L + w + (aw > 0 and ACCEL_GAP + aw or 0) + PAD_R, h
end

local function firstSelectable(items)
  for i, it in ipairs(items) do if not it.separator then return i end end
  return 1
end

function Menu:push(items, x, y)
  local w, h = measure(items)
  local cw, ch = tp.canvas()
  x = math.max(0, math.min(x, cw - w))
  y = math.max(0, math.min(y, ch - h))
  self.levels[#self.levels + 1] = { items = items, sel = firstSelectable(items), x = x, y = y, w = w, h = h }
end

-- Opens at (x, y) (top-left of the first popup).
function Menu:open(x, y)
  self.levels = {}
  self.open_ = true
  self:push(self.items, x, y)
end

function Menu:close()
  self.levels = {}
  self.open_ = false
  if self.onclose then self.onclose() end
end

local function itemY(level, index)
  local y = level.y + 3
  for i = 1, index - 1 do
    y = y + (level.items[i].separator and SEP_H or ITEM_H)
  end
  return y
end

function Menu:openSub()
  local lv = self.levels[#self.levels]
  local it = lv.items[lv.sel]
  if it and it.sub and enabled(it) then
    self:push(it.sub, lv.x + lv.w - 3, itemY(lv, lv.sel) - 3)
    return true
  end
  return false
end

function Menu:activate()
  local lv = self.levels[#self.levels]
  local it = lv.items[lv.sel]
  if not it or it.separator then return end
  if it.sub then self:openSub(); return end
  if not enabled(it) then return end
  self:close()
  if it.action then it.action() end
end

function Menu:move(d)
  local lv = self.levels[#self.levels]
  local n = #lv.items
  local i = lv.sel
  for _ = 1, n do
    i = (i - 1 + d) % n + 1
    if not lv.items[i].separator then break end
  end
  lv.sel = i
end

-- Handles an action ("up", "accept", ...) or a raw key (for mnemonics).
-- Returns true if the menu used it.
function Menu:key(action, raw)
  if not self.open_ then return false end
  if action == "menu" then self:close()
  elseif action == "up" then self:move(-1)
  elseif action == "down" then self:move(1)
  elseif action == "right" then self:openSub()
  elseif action == "left" then
    if #self.levels > 1 then self.levels[#self.levels] = nil end
  elseif action == "back" then
    if #self.levels > 1 then self.levels[#self.levels] = nil else self:close() end
  elseif action == "accept" then self:activate()
  elseif raw and #raw == 1 then
    local lv = self.levels[#self.levels]
    for i, it in ipairs(lv.items) do
      if not it.separator then
        local _, _, m = parse(it.label)
        if m == raw then lv.sel = i; self:activate(); break end
      end
    end
  end
  return true
end

-- The open levels, for tests: { {items=, sel=}, ... }.
function Menu:path()
  local out = {}
  for _, lv in ipairs(self.levels) do
    local it = lv.items[lv.sel]
    out[#out + 1] = it and parse(it.label or "") or ""
  end
  return out
end

-- Drawing ------------------------------------------------------------------

local function fill(x, y, w, h, r, g, b) tp.fill(x, y, w, h, r, g, b) end

-- Windows 95 raised border: light gray / white outside-in on the top-left,
-- black / dark gray on the bottom-right.
function Menu.bevel(x, y, w, h)
  fill(x, y, w, h, 0, 0, 0)
  fill(x, y, w - 1, h - 1, 192, 192, 192)
  fill(x + 1, y + 1, w - 2, h - 2, 128, 128, 128)
  fill(x + 1, y + 1, w - 3, h - 3, 255, 255, 255)
  fill(x + 2, y + 2, w - 4, h - 4, 192, 192, 192)
end

local function check(x, y, c)
  -- a 7x7 tick
  local rows = { "......#", ".....##", "#...###", "##.###.", "#####..", ".###...", "..#...." }
  for r, line in ipairs(rows) do
    for k = 1, #line do
      if line:sub(k, k) == "#" then fill(x + k - 1, y + r - 1, 1, 1, c, c, c) end
    end
  end
end

local function bullet(x, y, c)
  fill(x + 1, y, 3, 5, c, c, c)
  fill(x, y + 1, 5, 3, c, c, c)
end

local function arrow(x, y, c)
  for k = 0, 3 do fill(x + k, y + k, 1, 7 - 2 * k, c, c, c) end
end

-- Text with its mnemonic underlined; disabled text is embossed (Win95).
function Menu.label(x, y, label, r, g, b, disabled)
  local text, mi = parse(label)
  local function draw(dx, dy, cr, cg, cb)
    tp.text(x + dx, y + dy, text, 1, cr, cg, cb, "system")
    if mi then
      local pre = text:sub(1, mi - 1)
      local ux = x + dx + (pre ~= "" and tp.text_width(pre, 1, "system") + 1 or 0)
      fill(ux, y + dy + 11, tp.text_width(text:sub(mi, mi), 1, "system"), 1, cr, cg, cb)
    end
  end
  if disabled then
    draw(1, 1, 255, 255, 255)
    draw(0, 0, 128, 128, 128)
  else
    draw(0, 0, r, g, b)
  end
end

function Menu:draw()
  for depth, lv in ipairs(self.levels) do
    Menu.bevel(lv.x, lv.y, lv.w, lv.h)
    local y = lv.y + 3
    for i, it in ipairs(lv.items) do
      if it.separator then
        fill(lv.x + 3, y + 3, lv.w - 6, 1, 128, 128, 128)
        fill(lv.x + 3, y + 4, lv.w - 6, 1, 255, 255, 255)
        y = y + SEP_H
      else
        local on = enabled(it)
        local hot = (i == lv.sel) and (depth == #self.levels or it.sub)
        local fg = 0
        if hot then
          fill(lv.x + 3, y, lv.w - 6, ITEM_H, 0, 0, 128)
          fg = 255
        end
        local ty = y + 4
        local disabled = not on and not hot
        local tr, tg, tb = fg, fg, fg
        if not on and hot then tr, tg, tb = 128, 128, 128 end
        if value(it.checked) then
          local c = disabled and 128 or (hot and 255 or 0)
          if it.radio then bullet(lv.x + 8, ty + 3, c) else check(lv.x + 6, ty + 2, c) end
        end
        Menu.label(lv.x + PAD_L, ty, it.label, tr, tg, tb, disabled)
        if it.accel then
          local aw = tp.text_width(it.accel, 1, "system")
          local ax = lv.x + lv.w - PAD_R - aw
          if disabled then
            tp.text(ax + 1, ty + 1, it.accel, 1, 255, 255, 255, "system")
            tp.text(ax, ty, it.accel, 1, 128, 128, 128, "system")
          else
            tp.text(ax, ty, it.accel, 1, tr, tg, tb, "system")
          end
        end
        if it.sub then arrow(lv.x + lv.w - 12, ty + 1, disabled and 128 or fg) end
        y = y + ITEM_H
      end
    end
  end
end

return Menu
