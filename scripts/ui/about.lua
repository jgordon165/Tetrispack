-- The About box Tetris for Windows 2.00 shows (Help > About Tetris), used as
-- the intro when the game starts. Its art and credits come from the player's
-- own files: the Microsoft Entertainment Pack logo (bitmap 666) and the
-- credit lines (dialog 100) from ABOUTTET.DLL, and the version and copyright
-- lines TETRIS.EXE hands it. Positions are the original's, measured from a
-- Windows 3.1 screenshot in native pixels. Without the DLL the same box
-- shows the port's own credits and no logo.
--
-- A popup like ui/panel.lua: key(action) returns "close" when it's done, and
-- pointer(kind, x, y) takes clicks so the OK button works like a Windows
-- button (it sinks while held and fires when let go over it).
local About = {}
About.__index = About

About.W, About.H = 356, 375            -- the whole box, frame included
local LOGO_X, LOGO_Y = 53, 15
local OK_X, OK_Y, OK_W, OK_H = 154, 323, 60, 30
local CENTER = 178                     -- the static text is centred on the box
local VERSION_Y, COPYRIGHT_Y = 82, 106
-- Tops of the dialog's eight credit lines, in the template's order.
local CREDIT_Y = { 140, 160, 194, 210, 240, 256, 280, 296 }

local BLACK, WHITE = { 0, 0, 0 }, { 255, 255, 255 }
local GRAY, SHADOW = { 192, 192, 192 }, { 128, 128, 128 }
local FRAME = { 0, 0, 192 }

local FALLBACK = {
  version = "Tetrispack",
  copyright = "a port of Tetris for Windows 2.00",
  credits = {
    "(Microsoft Entertainment Pack),",
    "played from your own TETRIS.EXE.",
    "",
    "Original concept: Alexey Pajitnov",
    "Windows version: Dave Edson",
    "",
    "Put ABOUTTET.DLL beside TETRIS.EXE",
    "for the original About box.",
  },
}

-- The art and text, read once from the data files.
local assets
local function load()
  if assets then return assets end
  assets = { version = FALLBACK.version, copyright = FALLBACK.copyright, credits = FALLBACK.credits }
  local dlg = tp.dialog and tp.dialog(100, "about")
  if dlg then
    -- Static text with something in it, top to bottom (the version and
    -- copyright controls are empty: the game fills them in).
    local lines = {}
    for _, c in ipairs(dlg.controls) do
      if c.class == "static" and c.text ~= "" and c.text:sub(1, 1) ~= "#" then lines[#lines + 1] = c end
    end
    table.sort(lines, function(a, b) return a.y < b.y end)
    if #lines == #CREDIT_Y then
      assets.credits = {}
      for i, c in ipairs(lines) do assets.credits[i] = c.text end
      for d = 1, 9 do
        local v = tp.exe_string("Tetris " .. d .. ".")
        if v then assets.version = v; break end
      end
      assets.copyright = tp.exe_string("(c) 19") or ""
      local h, w, hh = tp.bitmap(666, "about")
      if h then assets.logo = { h = h, w = w, h2 = hh } end
    end
  end
  return assets
end

-- opts.x, opts.y: the box's top-left on the canvas.
function About.new(opts)
  local self = setmetatable({}, About)
  self.x, self.y = opts.x, opts.y
  self.pressed = false   -- the left button went down on OK and is held
  self.over = false      -- ... and the pointer is still over it
  self.assets = load()
  return self
end

function About:okRect() return self.x + OK_X, self.y + OK_Y, OK_W, OK_H end

function About:onOk(x, y)
  if not x then return false end
  local bx, by, bw, bh = self:okRect()
  return x >= bx and x < bx + bw and y >= by and y < by + bh
end

-- Enter (the default button), Space (the focused one), Esc and the
-- controller's A / B all mean OK, as in the original.
function About:key(action)
  if action == "accept" or action == "back" then return "close" end
end

-- "mouse left" / "mouse left up" with the canvas position. Returns "close"
-- when OK was clicked, true when the event was taken.
function About:pointer(kind, x, y)
  if kind == "mouse left" then
    self.pressed = self:onOk(x, y)
    self.over = self.pressed
    return true
  elseif kind == "mouse left up" then
    local fire = self.pressed and self:onOk(x, y)
    self.pressed, self.over = false, false
    return fire and "close" or true
  end
  return false
end

local function fill(x, y, w, h, c) tp.fill(x, y, w, h, c[1], c[2], c[3]) end
local function text(x, y, s) tp.text(x, y, s, 1, 0, 0, 0, "system") end
local function centred(x, y, s) text(x - math.floor(tp.text_width(s, 1, "system") / 2), y, s) end

-- A Windows 3.x default push button: a 2 px black border with the corners
-- cut, a 2 px white highlight and gray shadow (one dark line when held), the
-- caption with its mnemonic underlined, and the dotted focus rectangle.
local function button(x, y, w, h, caption, down)
  fill(x + 1, y, w - 2, 2, BLACK); fill(x + 1, y + h - 2, w - 2, 2, BLACK)
  fill(x, y + 1, 2, h - 2, BLACK); fill(x + w - 2, y + 1, 2, h - 2, BLACK)
  local ix, iy, iw, ih = x + 2, y + 2, w - 4, h - 4
  fill(ix, iy, iw, ih, GRAY)
  local shift = 0
  if down then
    fill(ix, iy, iw, 1, SHADOW); fill(ix, iy, 1, ih, SHADOW)
    shift = 1
  else
    fill(ix, iy, iw - 1, 2, WHITE); fill(ix, iy, 2, ih - 1, WHITE)
    fill(ix + 1, iy + ih - 2, iw - 1, 2, SHADOW); fill(ix + iw - 2, iy + 1, 2, ih - 1, SHADOW)
  end
  local tw = tp.text_width(caption, 1, "system")
  local tx = x + math.floor((w - tw) / 2) + shift
  local ty = y + math.floor((h - 10) / 2) + shift
  text(tx, ty, caption)
  local uw = tp.text_width(caption:sub(1, 1), 1, "system")
  fill(tx, ty + 12, uw, 1, BLACK)
  -- Dotted focus rectangle.
  local fx, fy, fw, fh = tx - 3, ty - 3, tw + 6, 18
  for i = 0, fw - 1, 2 do fill(fx + i, fy, 1, 1, SHADOW); fill(fx + i, fy + fh - 1, 1, 1, SHADOW) end
  for i = 0, fh - 1, 2 do fill(fx, fy + i, 1, 1, SHADOW); fill(fx + fw - 1, fy + i, 1, 1, SHADOW) end
end

function About:draw()
  local x, y, a = self.x, self.y, self.assets
  -- The dialog frame: a black line, then the thick colour band.
  fill(x, y, About.W, About.H, BLACK)
  fill(x + 1, y + 1, About.W - 2, About.H - 2, FRAME)
  fill(x + 5, y + 5, About.W - 10, About.H - 10, WHITE)
  if a.logo then
    tp.draw(a.logo.h, 0, 0, a.logo.w, a.logo.h2, x + LOGO_X, y + LOGO_Y)
  end
  local cx = x + CENTER
  centred(cx, y + VERSION_Y, a.version)
  centred(cx, y + COPYRIGHT_Y, a.copyright)
  for i, s in ipairs(a.credits) do
    if CREDIT_Y[i] and s ~= "" then centred(cx, y + CREDIT_Y[i], s) end
  end
  button(x + OK_X, y + OK_Y, OK_W, OK_H, "OK", self.pressed and self.over)
end

-- For tests: forget the loaded art and text.
function About._reset() assets = nil end

return About
