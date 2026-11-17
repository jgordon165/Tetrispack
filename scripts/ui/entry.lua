-- The Hall of Fame: asks for a name and a "cool quote" when a game makes the
-- high score table, as Tetris for Windows 2.00 does (its words are read from
-- the player's TETRIS.EXE, dialog 3, when it's there). Drawn like the port's
-- other popups (ui/panel.lua).
--
-- Keyboard: type (game.textinput), Backspace deletes, Up / Down or Tab move
-- between the name, the quote and OK, Enter goes on (on OK: done), Esc is
-- done. Controller: Up / Down on the D-pad change the last letter, Right
-- adds a letter, Left removes one, A goes on, B removes a letter.
-- Mouse: click a box to type in it, click OK when done.
-- A popup: key(action) / raw(key, x, y) / text(s) return "close" when done;
-- opts.done(name, quote) then runs.
local skin = require("game.skin")

local Entry = {}
Entry.__index = Entry

local W, PAD, MAX = 420, 12, 31
local FIELDS = { "name", "quote", "ok" }
-- Letters a controller cycles through.
local LETTERS = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?'-&"

local words
local function load()
  if words then return words end
  words = { head = { "Congrats! You have made the TETRIS Hall of Fame!",
                     "Enter your cool name and your cool quote." },
            name = "Your Cool Name...", quote = "Your Cool Quote..." }
  local d = tp.dialog and tp.dialog(3)
  if d then
    local text = {}
    for _, c in ipairs(d.controls) do
      -- Text only: SS_LEFT / CENTER / RIGHT (not its icons or separator lines).
      if c.class == "static" and c.text ~= "" and (c.style & 0xF) <= 2 then text[#text + 1] = c end
    end
    table.sort(text, function(a, b) return a.y < b.y end)
    -- By position: the two long lines on top, then the two labels.
    local lines, labels = {}, {}
    for _, c in ipairs(text) do
      if c.w > 120 then lines[#lines + 1] = c.text:gsub("%s+$", "") else labels[#labels + 1] = c.text end
    end
    if #lines == 2 and #labels == 2 then
      -- The original's two lines break mid-sentence for its dialog; join
      -- and rewrap them for this wider box.
      local all = lines[1] .. " " .. lines[2]
      local first, rest = all:match("^(.-Fame!)%s*(.*)$")
      if first then words.head = { first, rest } end
      words.name, words.quote = labels[1], labels[2]
    end
  end
  return words
end

-- opts: { name = "...", quote = "...", score = n, done = function(name, quote) }
function Entry.new(opts)
  local e = setmetatable({}, Entry)
  e.values = { name = opts.name or "", quote = opts.quote or "" }
  e.focus = 1
  e.score = opts.score
  e.done = opts.done
  e.words = load()
  e.blink = 0
  return e
end

function Entry:field() return FIELDS[self.focus] end

function Entry:finish()
  if self.done then self.done(self.values.name, self.values.quote) end
  return "close"
end

function Entry:moveFocus(d) self.focus = (self.focus - 1 + d) % #FIELDS + 1 end

function Entry:next()
  if self:field() == "ok" then return self:finish() end
  self:moveFocus(1)
end

function Entry:erase()
  local f = self:field()
  if f ~= "ok" then self.values[f] = self.values[f]:sub(1, -2) end
end

-- Typed text (UTF-8): printable ASCII only, as the original's edit boxes.
function Entry:text(s)
  local f = self:field()
  if f == "ok" then return end
  for c in s:gmatch(".") do
    local b = c:byte()
    if b >= 32 and b < 127 and #self.values[f] < MAX then self.values[f] = self.values[f] .. c end
  end
  return true
end

-- The controller's letter picker on the last letter of the field.
function Entry:cycle(d)
  local f = self:field()
  if f == "ok" then return end
  local v = self.values[f]
  if v == "" then v = " " end
  local last = v:sub(-1)
  local i = LETTERS:find(last, 1, true) or 1
  i = (i - 1 + d) % #LETTERS + 1
  self.values[f] = v:sub(1, -2) .. LETTERS:sub(i, i)
end

local function inside(r, x, y) return r and x and x >= r[1] and y >= r[2] and x < r[1] + r[3] and y < r[2] + r[4] end

-- Keys before they become actions: the keyboard's and the controller's own.
function Entry:raw(key, x, y)
  if key == "mouse left" then
    self.rects = self.rects or {}
    for i, f in ipairs(FIELDS) do
      if inside(self.rects[f], x, y) then
        self.focus = i
        if f == "ok" then return self:finish() end
      end
    end
    return true
  end
  if key == "mouse left up" or key:match("^mouse wheel") then return true end
  if key == "backspace" or key == "pad b" or key == "pad dpleft" then self:erase(); return true end
  if key == "return" or key == "keypad enter" or key == "pad a" or key == "pad start" then
    return self:next() or true
  end
  if key == "escape" then return self:finish() end
  if key == "tab" or key == "down" then self:moveFocus(1); return true end
  if key == "up" then self:moveFocus(-1); return true end
  if key == "pad dpup" then self:cycle(1); return true end
  if key == "pad dpdown" then self:cycle(-1); return true end
  if key == "pad dpright" then
    local f = self:field()
    if f ~= "ok" and #self.values[f] < MAX then self.values[f] = self.values[f] .. "A" end
    return true
  end
  -- Letters, digits and space come in as text; swallow their key presses.
  return #key == 1 or key == "space"
end

function Entry:key(action)
  if action == "menu" then return self:finish() end
end

-- Drawing ------------------------------------------------------------------

local WHITE, GRAY, YELLOW = { 255, 255, 255 }, { 192, 192, 192 }, { 255, 255, 0 }
local function text(x, y, s, c) tp.text(x, y, s, 1, c[1], c[2], c[3], "system") end
local function width(s) return tp.text_width(s, 1, "system") end

function Entry:draw()
  local cw, ch = tp.canvas()
  local h = 228
  local x, y = math.floor((cw - W) / 2), math.floor((ch - h) / 2)
  skin.frame(x, y, W, h, { 4, 4, 4, 4 })
  tp.fill(x, y, W, h, 0, 0, 0)
  local title = "Tetris Hall of Fame"
  text(x + math.floor((W - width(title)) / 2), y + 6, title, WHITE)
  tp.fill(x + PAD, y + 22, W - 2 * PAD, 1, 128, 128, 128)
  local ty = y + 32
  for _, line in ipairs(self.words.head) do
    text(x + math.floor((W - width(line)) / 2), ty, line, GRAY)
    ty = ty + 16
  end
  if self.score then
    local s = "Score: " .. self.score
    text(x + math.floor((W - width(s)) / 2), ty + 2, s, YELLOW)
  end
  self.blink = (self.blink + 1) % 60
  local fy = y + 100
  self.rects = {}
  for i, f in ipairs({ "name", "quote" }) do
    local hot = self.focus == i
    text(x + PAD, fy, self.words[f], hot and WHITE or GRAY)
    -- A sunken edit box: black top-left, white bottom-right, white inside.
    local bx, by, bw, bh = x + PAD, fy + 15, W - 2 * PAD, 20
    self.rects[f] = { bx, by, bw, bh }
    tp.fill(bx, by, bw, bh, 255, 255, 255)
    tp.fill(bx, by, bw - 1, 1, 128, 128, 128)
    tp.fill(bx, by, 1, bh - 1, 128, 128, 128)
    tp.fill(bx + 1, by + 1, bw - 2, bh - 2, 255, 255, 255)
    local v = self.values[f]
    tp.text(bx + 4, by + 4, v, 1, 0, 0, 0, "system")
    if hot and self.blink < 30 then
      local cx = bx + 4 + (v == "" and 0 or width(v) + 2)
      tp.fill(cx, by + 3, 2, 14, 0, 0, 0)
    end
    fy = fy + 46
  end
  -- OK: highlighted like the menu's selection when it has the focus.
  local ok = "OK"
  local bw, bh = 80, 20
  local bx, by = x + math.floor((W - bw) / 2), y + h - bh - 12
  self.rects.ok = { bx, by, bw, bh }
  if self.focus == 3 then
    local base, light = skin.base[3], skin.light[3]
    tp.fill(bx, by, bw, bh, base[1], base[2], base[3])
    tp.fill(bx, by, bw - 1, 1, light[1], light[2], light[3])
    tp.fill(bx, by, 1, bh - 1, light[1], light[2], light[3])
  else
    tp.fill(bx, by, bw, bh, 64, 64, 64)
  end
  text(bx + math.floor((bw - width(ok)) / 2), by + 4, ok, WHITE)
end

return Entry
