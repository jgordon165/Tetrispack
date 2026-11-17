-- MAME autoboot script for the console ROMs' smoke test: presses Start on
-- the title screen, then plays (a fixed pattern of moves and drops) and
-- saves snapshots at the frames in TETRISPACK_SNAPS (comma-separated).
-- TETRISPACK_PAD names the pad port (":ctrl1:mdpad:PAD" on the Genesis).
-- With TETRISPACK_DEMO=1 it touches nothing, so the title screen's attract
-- demo plays by itself (it clears lines, so the blink shows up).
--   mame genesis -cart rom.bin -autoboot_script consoles/tests/mame_play.lua ...
local padTag = os.getenv("TETRISPACK_PAD") or ":ctrl1:mdpad:PAD"
local prefix = os.getenv("TETRISPACK_PREFIX") or "P1 "
local snaps = {}
for n in (os.getenv("TETRISPACK_SNAPS") or "600"):gmatch("%d+") do snaps[tonumber(n)] = true end

local frame = 0
local held = {}
local function field(name)
  local port = manager.machine.ioport.ports[padTag]
  return port and port.fields[prefix .. name]
end
local function press(name, frames) held[name] = frames end

-- A move every 12 frames: a few sideways steps, a turn, then a drop.
local plan = { "Left", "Left", "Up", "Down", "Right", "Right", "Right", "Down",
               "Up", "Up", "Down", "Left", "Down", "Right", "Up", "Down", "Down" }

local demo = os.getenv("TETRISPACK_DEMO") == "1"

-- The smoke test's report: the game's board (TcBoard) read from RAM at
-- TETRISPACK_BOARD (hex), with the field offsets TETRISPACK_OFFSETS
-- ("lines,level,placed") and byte order TETRISPACK_ENDIAN ("big" / "little"),
-- written to TETRISPACK_RESULT every 10 seconds (the last one stands).
local boardAddr = tonumber(os.getenv("TETRISPACK_BOARD") or "", 16)
local result = os.getenv("TETRISPACK_RESULT")
local function report()
  if not (boardAddr and result) then return end
  local offs = {}
  for n in (os.getenv("TETRISPACK_OFFSETS") or "210,205,212"):gmatch("%d+") do offs[#offs + 1] = tonumber(n) end
  local big = (os.getenv("TETRISPACK_ENDIAN") or "big") == "big"
  local space = manager.machine.devices[":maincpu"].spaces["program"]
  local function u8(a) return space:read_u8(a) end
  local function u16(a)
    local x, y = u8(a), u8(a + 1)
    return big and (x * 256 + y) or (y * 256 + x)
  end
  local f = io.open(result, "w")
  f:write(("lines=%d level=%d placed=%d\n"):format(u16(boardAddr + offs[1]), u8(boardAddr + offs[2]), u16(boardAddr + offs[3])))
  f:close()
end

emu.register_frame_done(function()
  frame = frame + 1
  if frame % 600 == 0 then report() end
  if demo then
    if snaps[frame] then manager.machine.video:snapshot() end
    return
  end
  if frame == 120 then press("Start", 4) end
  if frame > 200 and frame % 12 == 0 then
    press(plan[(frame // 12) % #plan + 1], 3)
  end
  for name, left in pairs(held) do
    local f = field(name)
    if f then f:set_value(left > 0 and 1 or 0) end
    held[name] = left > 0 and left - 1 or nil
  end
  if snaps[frame] then manager.machine.video:snapshot() end
end)
