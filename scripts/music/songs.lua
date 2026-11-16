-- The soundtrack: five pieces, one for each pair of levels (each pair of
-- background patterns). Korobeiniki is the 19th-century Russian folk song
-- (public domain) that became Tetris's theme, in an arrangement written for
-- this port; the other four are original.
--
-- The melodies are hand-written MML (music/mml.lua); the rest of each
-- arrangement is game-style accompaniment built from the chord per bar
-- (music/arrange.lua): a harmony line, chip arpeggios, a moving bass line,
-- drums with fills. Each track names an instrument (music/instruments.lua),
-- which the chosen sound engine plays its own way.
local A = require("music.arrange")

local function bars(s, n) return (s .. " "):rep(n) end
local function cat(...) local t = {} for _, l in ipairs({ ... }) do for _, v in ipairs(l) do t[#t + 1] = v end end return t end
local function up(text, n) return (text:gsub("o(%d)", function(o) return "o" .. (tonumber(o) + (n or 1)) end)) end
local function join(list, from, to) return table.concat({ table.unpack(list, from or 1, to or #list) }, " | ") .. " |" end

-- Adds the drum tracks of A.drums to a track list.
local function drums(tracks, nbars, pattern, opts, vols)
  vols = vols or {}
  local d = A.drums(nbars, pattern, opts)
  for _, part in ipairs({ "kick", "snare", "hat", "crash", "tom" }) do
    if #d[part] > 0 then tracks[#tracks + 1] = { inst = part, vol = vols[part] or 1, notes = d[part] } end
  end
end

-- Held chords, one per bar (or two halves).
local function padLine(chords, voicing, halves)
  local s = "q8 "
  for _, c in ipairs(chords) do
    s = s .. (halves and (voicing[c] .. "2 " .. voicing[c] .. "2") or (voicing[c] .. "1")) .. " | "
  end
  return s
end

-- 1. Korobeiniki (levels 1-2) ------------------------------------------------
local koro = { name = "Korobeiniki", tempo = 150, bars = 24 }
do
  local A1 = "o5 e4 <b8 >c8 d4 c8 <b8 | a4 a8 >c8 e4 d8 c8 | <b4. >c8 d4 e4 | c4 <a4 a2 |"
  local A2 = "o5 r8 d4 f8 a4 g8 f8 | e4. c8 e4 d8 c8 | <b4 b8 >c8 d4 e4 | c4 <a4 a4 r4 |"
  local B = "o5 e2 c2 | d2 <b2 | >c2 <a2 | g+2 b4 r4 | >e2 c2 | d2 <b2 | >c4 e4 a2 | g+1 |"
  local melody = "q7 " .. A1 .. A2 .. A1 .. A2 .. "q8 " .. B
  local chordsA = { "E", "Am", "E", "Am", "Dm", "Am", "E", "Am" }
  local chordsB = { "Am", "E", "Am", "E", "Am", "E", "Am", "E" }
  koro.chords = cat(chordsA, chordsA, chordsB)
  local voicing = { Am = "o4 (ace)", E = "o3 (b>eg+)", Dm = "o4 (dfa)" }
  koro.tracks = {
    { inst = "lead", vol = 0.9, mml = melody },
    { inst = "harmony", vol = 0.8, notes = A.harmony(melody, koro.chords) },
    { inst = "bells", vol = 0.5, mml = bars("r1", 8) .. "q6 " .. up(A1 .. A2) .. bars("r1", 8) },
    { inst = "arp", vol = 0.8, notes = A.arp(koro.chords, 64, 0.25, { from = 9, to = 24 }) },
    { inst = "bass", vol = 0.85, notes = A.bass(koro.chords, "octave", { low = 40 }) },
    { inst = "pad", vol = 0.4, mml = bars("r1", 16) .. padLine(chordsB, voicing) },
  }
  drums(koro.tracks, 24, "rock", { fillEvery = 8, fill = "snare" }, { hat = 0.6, crash = 0.7 })
end

-- 2. Program Manager (levels 3-4): a bright early-Windows MIDI demo tune ---
local progman = { name = "Program Manager", tempo = 120, bars = 16 }
do
  local mel = { "o5 f+4 a8 f+8 e4 d4", "o5 e8 f+8 g4 b4 a8 g8", "o5 f+2 r8 d8 e8 f+8", "o5 e2. r4",
    "o5 d4 f+8 d8 <b4 >d4", "o4 b8 >d8 g4 b4 a8 g8", "o5 f+4 e4 d4 e4", "o5 a2 r4 <a4",
    "o5 f+4 a8 f+8 e4 d4", "o5 e8 f+8 g4 b4 a8 g8", "o5 f+2 r8 d8 e8 f+8", "o5 e2. r4",
    "o5 d4 f+8 d8 <b4 >d4", "o4 b8 >d8 g4 b4 a8 g8", "o5 f+8 g8 a4 b4 >c+4", "o6 d2 r2" }
  local melody = "q7 " .. join(mel)
  progman.chords = { "D", "G", "D", "A", "Bm", "G", "Em", "A", "D", "G", "D", "A", "Bm", "G", "A", "D" }
  local voicing = { D = "o4 (df+a)", G = "o4 (dgb)", A = "o4 (c+ea)", Bm = "o4 (df+b)", Em = "o4 (egb)" }
  local stabs = ""
  for _, c in ipairs(progman.chords) do
    stabs = stabs .. voicing[c] .. "4. " .. voicing[c] .. "8 r4 " .. voicing[c] .. "4 | "
  end
  progman.tracks = {
    { inst = "lead", vol = 0.8, mml = melody },
    { inst = "harmony", vol = 0.8, notes = A.harmony(melody, progman.chords) },
    { inst = "bells", vol = 0.45, mml = bars("r1", 8) .. "q6 " .. up(join(mel, 9, 16)) },
    { inst = "arp", vol = 0.75, notes = A.arp(progman.chords, 62, 0.25) },
    { inst = "epiano", vol = 0.45, mml = "q6 " .. stabs },
    { inst = "pad", vol = 0.35, mml = padLine(progman.chords, voicing) },
    { inst = "bass", vol = 0.85, notes = A.bass(progman.chords, "synco", { low = 38 }) },
  }
  drums(progman.tracks, 16, "pop", { fillEvery = 8, fill = "toms" }, { hat = 0.55, crash = 0.6 })
end

-- 3. Samovar (levels 5-6): a Russian dance, accordion and balalaika ---------
local samovar = { name = "Samovar", tempo = 144, bars = 16 }
do
  local melody = "q7 " .. join({
    "o5 a4 a8 g8 f4 e8 d8", "o5 c+4 e4 a2", "o5 g8 f8 e8 d8 c+4 <a4", "o5 d2. r4",
    "o5 f8 e8 f8 g8 a4 a4", "o5 b-8 a8 g8 f8 g2", "o5 a8 g8 f8 e8 c+4 e4", "o5 d2 r4 <a4",
    "o5 d16 e16 f16 g16 a8 a8 a8 g8 f8 e8", "o5 e16 f16 g16 e16 c+8 e8 a4 a4",
    "o6 c+8 <b-8 a8 g8 f8 e8 d8 c+8", "o5 d4 f4 a4 >d4",
    "o6 d8 c8 <b-8 a8 b-8 a8 g8 f8", "o5 g8 a8 b-8 >c8 d4 <b-4",
    "o5 a8 b-8 a8 g8 f8 e8 c+8 e8", "o5 d4 <a4 >d2",
  })
  samovar.chords = { "Dm", "A7", "A7", "Dm", "Dm", "Gm", "A7", "Dm", "Dm", "A7", "A7", "Dm", "Dm", "Gm", "A7", "Dm" }
  samovar.tracks = {
    { inst = "accordion", vol = 0.75, mml = melody },
    { inst = "harmony", vol = 0.7, notes = A.harmony(melody, samovar.chords) },
    -- Balalaika tremolo: fast arpeggios in the second half.
    { inst = "pluck", vol = 0.6, notes = A.arp(samovar.chords, 57, 0.25, { from = 9, to = 16 }) },
    { inst = "arp", vol = 0.6, notes = A.arp(samovar.chords, 69, 0.125, { from = 13, to = 16 }) },
    { inst = "bass", vol = 0.85, notes = A.bass(samovar.chords, "oompah", { low = 38 }) },
  }
  drums(samovar.tracks, 16, "polka", { fillEvery = 8, fill = "snare" }, { hat = 0.6, crash = 0.6 })
end

-- 4. Midnight Desktop (levels 7-8): slow and moody ---------------------------
local midnight = { name = "Midnight Desktop", tempo = 100, bars = 16 }
do
  local mel = { "o5 b2 a4 g4", "o5 e2. r4", "o5 c4 e4 a4 g4", "o5 f+2. r4",
    "o5 g4 a4 b4 >d4", "o6 c2 <b4 g4", "o5 a2 f+4 d4", "o5 d+2 f+4 b4",
    "o5 b4. a8 g4 b4", "o5 a2. r4", "o5 g4 f+4 e4 g4", "o5 e2. g4",
    "o5 a4 >c4 e4 d4", "o6 d+2 <b2", "o5 e2. r4", "o5 f+2 d+4 <b4" }
  local melody = "q7 " .. join(mel)
  midnight.chords = { "Em", "C", "Am", "B7", "Em", "C", "D", "B7", "G", "D", "Em", "C", "Am", "B7", "Em", "B7" }
  local voicing = { Em = "o4 (egb)", C = "o4 (ceg)", Am = "o4 (ace)", B7 = "o3 (b>d+f+a)", D = "o4 (df+a)", G = "o4 (dgb)" }
  midnight.tracks = {
    { inst = "bells", vol = 0.75, mml = melody },
    { inst = "lead", vol = 0.5, mml = bars("r1", 8) .. "q7 " .. join(mel, 9, 16) },
    { inst = "harmony", vol = 0.6, notes = A.harmony(melody, midnight.chords, { minLen = 1 }) },
    { inst = "epiano", vol = 0.55, notes = A.arp(midnight.chords, 55, 0.5) },
    { inst = "arp", vol = 0.5, notes = A.arp(midnight.chords, 67, 0.25, { from = 9, to = 16 }) },
    { inst = "pad", vol = 0.4, mml = padLine(midnight.chords, voicing) },
    { inst = "bass", vol = 0.8, notes = A.bass(midnight.chords, "pump", { low = 36 }) },
  }
  drums(midnight.tracks, 16, "slow", { fillEvery = 8, fill = "toms" }, { hat = 0.5, crash = 0.5, kick = 0.8 })
end

-- 5. Steppe Express (levels 9-10): fast, driving, harmonic minor -------------
local steppe = { name = "Steppe Express", tempo = 172, bars = 16 }
do
  local mel = join({
    "o5 e8 f+8 g8 a8 b4 b4", "o5 b8 >c8 <b8 a8 g4 e4", "o5 a8 b8 >c8 <b8 a4 e4", "o5 d+8 e8 f+8 g8 f+4 <b4",
    "o5 e8 g8 b8 >e8 d+8 e8 <b4", "o6 c8 <b8 a8 g8 e4 g4", "o5 f+8 g8 a8 f+8 d+4 f+4", "o5 e2 r4 <b4",
  })
  local melody = "q6 " .. mel .. " " .. mel
  steppe.chords = { "Em", "Em", "Am", "B7", "Em", "C", "B7", "Em", "Em", "Em", "Am", "B7", "Em", "C", "B7", "Em" }
  local voicing = { Em = "o4 (egb)", Am = "o4 (ace)", B7 = "o3 (b>d+f+a)", C = "o4 (ceg)" }
  steppe.tracks = {
    { inst = "lead", vol = 0.85, mml = melody },
    { inst = "harmony", vol = 0.75, notes = A.harmony(melody, steppe.chords, { minLen = 0.5 }) },
    { inst = "bells", vol = 0.45, mml = bars("r1", 8) .. "q6 " .. up(mel) },
    { inst = "arp", vol = 0.7, notes = A.arp(steppe.chords, 64, 0.25) },
    { inst = "pad", vol = 0.35, mml = padLine(steppe.chords, voicing, true) },
    { inst = "bass", vol = 0.85, notes = A.bass(steppe.chords, "gallop", { low = 40 }) },
  }
  drums(steppe.tracks, 16, "drive", { fillEvery = 4, fill = "toms" }, { hat = 0.55, crash = 0.6 })
end

return { koro, progman, samovar, midnight, steppe }
