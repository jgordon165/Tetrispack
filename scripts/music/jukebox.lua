-- Which song plays, on which sound engine: by default the song for the
-- current level's background (levels 1-2 song 1, 3-4 song 2, ...), or one
-- chosen in the menu, or none; played as a Super NES, a Mega Drive or a
-- Sound Blaster would. Builds each (song, engine) once.
local mml = require("music.mml")
local instruments = require("music.instruments")
local songs = require("music.songs")

local jukebox = { songs = songs }

-- The sound engines, in menu order.
jukebox.styles = {
  { id = "snes", name = "Super NES" },
  { id = "genesis", name = "Mega Drive" },
  { id = "opl3", name = "Sound Blaster" },
}

-- song, engine id -> the table tp.music_play takes (see src/music/song_lua.h).
function jukebox.build(song, style)
  style = style or "snes"
  local set = instruments[style] or error("no such music style " .. tostring(style))
  local spb = 60 / song.tempo
  local beats = song.bars * 4
  local out = { engine = style, instruments = {}, notes = {}, length = beats * spb }
  -- The SNES echo: a dotted-eighth-ish delay, up to the chip's 240 ms.
  out.echo = { delay = math.min(0.24, 0.75 * spb), feedback = 0.42, volume = 0.32 }
  local index = {}
  for _, t in ipairs(song.tracks) do
    local inst = set[t.inst]
    if inst == nil then error(("%s has no instrument %s"):format(style, tostring(t.inst))) end
    if inst then
      if not index[t.inst] then
        out.instruments[#out.instruments + 1] = inst
        index[t.inst] = #out.instruments
      end
      local notes = t.notes
      if t.mml then notes = mml.parse(t.mml) end
      for _, n in ipairs(notes) do
        if n[1] < beats then
          out.notes[#out.notes + 1] = { n[1] * spb, n[2] * spb, n[3], n[4] * (t.vol or 1), index[t.inst] }
        end
      end
    end
  end
  return out
end

local built = {}
local function get(i, style)
  local key = i .. "/" .. style
  built[key] = built[key] or jukebox.build(songs[i], style)
  return built[key]
end

-- Menu choices.
jukebox.choices = { "By Level" }
for _, s in ipairs(songs) do jukebox.choices[#jukebox.choices + 1] = s.name end
jukebox.choices[#jukebox.choices + 1] = "Off"
jukebox.styleNames = {}
for _, s in ipairs(jukebox.styles) do jukebox.styleNames[#jukebox.styleNames + 1] = s.name end

jukebox.choice = "By Level"
jukebox.style = "Super NES"
jukebox.volume = 7          -- 0-10
local current = nil         -- "song/style" playing

function jukebox.styleId()
  for _, s in ipairs(jukebox.styles) do if s.name == jukebox.style then return s.id end end
  return "snes"
end

function jukebox.songFor(level)
  if jukebox.choice == "Off" then return nil end
  if jukebox.choice == "By Level" then
    return math.min(#songs, math.floor((level - 1) / 2) + 1)
  end
  for i, s in ipairs(songs) do if s.name == jukebox.choice then return i end end
end

-- Call every frame: the level (for "By Level"), and whether music should
-- be heard at all right now (not after a game over).
function jukebox.update(level, on)
  tp.music_volume(jukebox.volume / 10)
  local want = (on ~= false) and jukebox.songFor(level) or nil
  local key = want and (want .. "/" .. jukebox.styleId()) or nil
  if key == current then return end
  current = key
  if want then tp.music_play(get(want, jukebox.styleId()), 0.8) else tp.music_stop(0.8) end
end

function jukebox.pause(on) tp.music_pause(on) end

return jukebox
