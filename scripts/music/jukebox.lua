-- Which song plays: by default the one for the current level's background
-- (levels 1-2 song 1, 3-4 song 2, ...), or one chosen in the menu, or none.
-- Builds songs into the note lists tp.music_play takes, once each.
local mml = require("music.mml")
local patches = require("music.patches")
local songs = require("music.songs")

local jukebox = { songs = songs }

local PATCH_KEYS = { "mult", "tl", "ar", "dr", "sl", "rr", "ws", "ksl", "vib", "am", "sustain", "ksr" }

-- song -> { length = seconds, patches = {...}, notes = { {start, len, key, vel, patch}, ... } }
function jukebox.build(song)
  local spb = 60 / song.tempo
  local out = { patches = {}, notes = {}, length = 0 }
  local index = {}
  for _, t in ipairs(song.tracks) do
    local p = patches[t.patch] or error("no patch " .. tostring(t.patch))
    if not index[t.patch] then
      local copy = { feedback = p.feedback, additive = p.additive, key = p.key, mod = {}, car = {} }
      for _, k in ipairs(PATCH_KEYS) do copy.mod[k] = p.mod[k]; copy.car[k] = p.car[k] end
      out.patches[#out.patches + 1] = copy
      index[t.patch] = #out.patches
    end
    local notes, beats = mml.parse(t.mml)
    out.length = math.max(out.length, beats * spb)
    for _, n in ipairs(notes) do
      out.notes[#out.notes + 1] = { n[1] * spb, n[2] * spb, n[3], n[4] * (t.vol or 1), index[t.patch] }
    end
  end
  return out
end

local built = {}
local function get(i)
  built[i] = built[i] or jukebox.build(songs[i])
  return built[i]
end

-- Menu choices: "By Level", the song names, "Off".
jukebox.choices = { "By Level" }
for _, s in ipairs(songs) do jukebox.choices[#jukebox.choices + 1] = s.name end
jukebox.choices[#jukebox.choices + 1] = "Off"

jukebox.choice = "By Level"
jukebox.volume = 7          -- 0-10
local current = nil         -- index of the song playing

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
  if want == current then return end
  current = want
  if want then tp.music_play(get(want), 0.8) else tp.music_stop(0.8) end
end

function jukebox.pause(on) tp.music_pause(on) end

return jukebox
