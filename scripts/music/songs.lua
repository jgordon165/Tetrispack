-- The soundtrack: five pieces, one for each pair of levels (each pair of
-- background patterns). Korobeiniki is the 19th-century Russian folk song
-- (public domain) that became Tetris's theme, in an arrangement written for
-- this port; the other four are original, in the same Russian-folk-meets-
-- 1990-MIDI spirit. Written in the MML of music/mml.lua, one string per
-- track; every track of a song is the same length so the loop is seamless.

-- Helpers ------------------------------------------------------------------

local function rep(s, n) return (s .. " "):rep(n) end

-- Bass in octaves on eighths: "o2 e8 >e8 <e8 >e8 ..."
local function octaves(root) return "o2 " .. rep(root .. "8 >" .. root .. "8 <", 4) end
-- Galloping bass: one eighth and two sixteenths per beat.
local function gallop(root) return "o2 " .. rep(root .. "8 " .. root .. "16 " .. root .. "16", 4) end
-- Off-beat chord stabs.
local function offbeats(chord) return rep("r8 " .. chord .. "8", 4) end
local function bars(s, n) return rep(s, n) end

-- Drums: one bar each.
local KICK_14, SNARE_24 = "c4 r4 c4 r4", "r4 c4 r4 c4"
local HATS = "v11 c8 v6 c8 v11 c8 v6 c8 v11 c8 v6 c8 v11 c8 v6 c8"

-- 1. Korobeiniki (levels 1-2) ------------------------------------------------
local koro = { name = "Korobeiniki", tempo = 144, tracks = {} }
do
  local A1 = "o5 e4 <b8 >c8 d4 c8 <b8 | a4 a8 >c8 e4 d8 c8 | <b4. >c8 d4 e4 | c4 <a4 a2 |"
  local A2 = "o5 r8 d4 f8 a4 g8 f8 | e4. c8 e4 d8 c8 | <b4 b8 >c8 d4 e4 | c4 <a4 a4 r4 |"
  local B = "o5 e2 c2 | d2 <b2 | >c2 <a2 | g+2 b4 r4 | >e2 c2 | d2 <b2 | >c4 e4 a2 | g+1 |"
  local chordsA = { "e", "a", "e", "a", "d", "a", "e", "a" }
  local chordsB = { "a", "e", "a", "e", "a", "e", "a", "e" }
  local triad = { a = "o4 (ace)", e = "o3 (b>eg+)", d = "o4 (dfa)" }
  local function line(roots, f) local s = "" for _, r in ipairs(roots) do s = s .. f(r) .. " | " end return s end

  koro.tracks = {
    { patch = "lead", vol = 0.9, mml = "q7 " .. A1 .. A2 .. A1 .. A2 .. "q8 " .. B },
    { patch = "bells", vol = 0.45, mml = bars("r1", 8) .. "q6 o6 e4 <b8 >c8 d4 c8 <b8 | a4 a8 >c8 e4 d8 c8 | <b4. >c8 d4 e4 | c4 <a4 a2 |"
        .. "o6 r8 d4 f8 a4 g8 f8 | e4. c8 e4 d8 c8 | <b4 b8 >c8 d4 e4 | c4 <a4 a4 r4 |" .. bars("r1", 8) },
    { patch = "bass", vol = 0.8, mml = "q6 " .. line(chordsA, octaves) .. line(chordsA, octaves)
        .. "q8 " .. line(chordsB, function(r) return "o2 " .. r .. "2 >" .. r .. "2" end) },
    { patch = "epiano", vol = 0.45, mml = "q5 " .. line(chordsA, function(r) return offbeats(triad[r]) end)
        .. line(chordsA, function(r) return offbeats(triad[r]) end)
        .. "q8 " .. line(chordsB, function(r) return triad[r] .. "1" end) },
    { patch = "kick", vol = 0.9, mml = bars(KICK_14, 16) .. bars("c2 c2", 8) },
    { patch = "snare", vol = 0.55, mml = bars(SNARE_24, 16) .. bars("r1", 7) .. "r2 v8 c8 c8 v12 c8 c8" },
    { patch = "hat", vol = 0.35, mml = bars(HATS, 16) .. bars("v10 c4 c4 c4 c4", 8) },
  }
end

-- 2. Program Manager (levels 3-4): a bright early-Windows MIDI demo tune ---
local progman = { name = "Program Manager", tempo = 116, tracks = {} }
do
  local first = { "o5 f+4 a8 f+8 e4 d4", "o5 e8 f+8 g4 b4 a8 g8", "o5 f+2 r8 d8 e8 f+8", "o5 e2. r4",
    "o5 d4 f+8 d8 <b4 >d4", "o4 b8 >d8 g4 b4 a8 g8", "o5 f+4 e4 d4 e4", "o5 a2 r4 <a4" }
  local second = { "o5 f+4 a8 f+8 e4 d4", "o5 e8 f+8 g4 b4 a8 g8", "o5 f+2 r8 d8 e8 f+8", "o5 e2. r4",
    "o5 d4 f+8 d8 <b4 >d4", "o4 b8 >d8 g4 b4 a8 g8", "o5 f+8 g8 a4 b4 >c+4", "o6 d2 r2" }
  local mel = table.concat(first, " | ") .. " | " .. table.concat(second, " | ")
  local up = (table.concat(second, " | ")):gsub("o(%d)", function(o) return "o" .. (tonumber(o) + 1) end)
  local prog = { "D", "G", "D", "A", "Bm", "G", "Em", "A", "D", "G", "D", "A", "Bm", "G", "A", "D" }
  local voicing = { D = "o4 (df+a)", G = "o4 (dgb)", A = "o4 (c+ea)", Bm = "o4 (df+b)", Em = "o4 (egb)" }
  local bassBar = { D = "o2 d4. d8 a4 >d4", G = "o2 g4. g8 >d4 g4", A = "o2 a4. a8 >e4 a4",
                    Bm = "o2 b4. b8 >f+4 b4", Em = "o2 e4. e8 b4 >e4" }
  local chords, pad, bass = "", "", ""
  for _, c in ipairs(prog) do
    chords = chords .. voicing[c] .. "4. " .. voicing[c] .. "8 r4 " .. voicing[c] .. "4 | "
    pad = pad .. voicing[c] .. "1 | "
    bass = bass .. bassBar[c] .. " | "
  end
  progman.tracks = {
    { patch = "lead", vol = 0.75, mml = "q7 " .. mel },
    { patch = "bells", vol = 0.35, mml = bars("r1", 8) .. "q6 " .. up },
    { patch = "epiano", vol = 0.5, mml = "q6 " .. chords },
    { patch = "strings", vol = 0.35, mml = "q8 " .. pad },
    { patch = "bass", vol = 0.8, mml = "q6 " .. bass },
    { patch = "kick", vol = 0.85, mml = bars("c4 r8 c8 c4 r4", 16) },
    { patch = "snare", vol = 0.5, mml = bars(SNARE_24, 15) .. "r4 c4 v9 c8 c8 v12 c8 c8" },
    { patch = "hat", vol = 0.3, mml = bars(HATS, 16) },
  }
end

-- 3. Samovar (levels 5-6): a Russian dance, accordion and oom-pah -----------
local samovar = { name = "Samovar", tempo = 138, tracks = {} }
do
  local mel = table.concat({
    "o5 a4 a8 g8 f4 e8 d8", "o5 c+4 e4 a2", "o5 g8 f8 e8 d8 c+4 <a4", "o5 d2. r4",
    "o5 f8 e8 f8 g8 a4 a4", "o5 b-8 a8 g8 f8 g2", "o5 a8 g8 f8 e8 c+4 e4", "o5 d2 r4 <a4",
    "o5 d16 e16 f16 g16 a8 a8 a8 g8 f8 e8", "o5 e16 f16 g16 e16 c+8 e8 a4 a4",
    "o6 c+8 <b-8 a8 g8 f8 e8 d8 c+8", "o5 d4 f4 a4 >d4",
    "o6 d8 c8 <b-8 a8 b-8 a8 g8 f8", "o5 g8 a8 b-8 >c8 d4 <b-4",
    "o5 a8 b-8 a8 g8 f8 e8 c+8 e8", "o5 d4 <a4 >d2",
  }, " | ")
  local prog = { "Dm", "A7", "A7", "Dm", "Dm", "Gm", "A7", "Dm", "Dm", "A7", "A7", "Dm", "Dm", "Gm", "A7", "Dm" }
  local chord = { Dm = "o4 (dfa)", A7 = "o4 (c+eg)", Gm = "o4 (dgb-)" }
  local root = { Dm = { "d", "a" }, A7 = { "a", "e" }, Gm = { "g", "d" } }
  local bass, pah = "", ""
  for _, c in ipairs(prog) do
    local r = root[c]
    bass = bass .. "o2 " .. r[1] .. "4 r4 " .. r[2] .. "4 r4 | "
    pah = pah .. "r4 " .. chord[c] .. "4 r4 " .. chord[c] .. "4 | "
  end
  samovar.tracks = {
    { patch = "accordion", vol = 0.7, mml = "q7 " .. mel },
    { patch = "pluck", vol = 0.55, mml = "q4 " .. pah },
    { patch = "bass", vol = 0.85, mml = "q4 " .. bass },
    { patch = "kick", vol = 0.8, mml = bars(KICK_14, 16) },
    { patch = "hat", vol = 0.4, mml = bars("r8 v12 c8 r8 v8 c8 r8 v12 c8 r8 v8 c8", 15) .. "v12 c16 c16 c16 c16 c8 c8 c4 r4" },
  }
end

-- 4. Midnight Desktop (levels 7-8): slow and moody ---------------------------
local midnight = { name = "Midnight Desktop", tempo = 96, tracks = {} }
do
  local mel = table.concat({
    "o5 b2 a4 g4", "o5 e2. r4", "o5 c4 e4 a4 g4", "o5 f+2. r4",
    "o5 g4 a4 b4 >d4", "o6 c2 <b4 g4", "o5 a2 f+4 d4", "o5 d+2 f+4 b4",
    "o5 b4. a8 g4 b4", "o5 a2. r4", "o5 g4 f+4 e4 g4", "o5 e2. g4",
    "o5 a4 >c4 e4 d4", "o6 d+2 <b2", "o5 e2. r4", "o5 f+2 d+4 <b4",
  }, " | ")
  local prog = { "Em", "C", "Am", "B7", "Em", "C", "D", "B7", "G", "D", "Em", "C", "Am", "B7", "Em", "B7" }
  local arp = {
    Em = "o4 e8 g8 b8 >e8 <b8 g8 e8 g8", C = "o4 c8 e8 g8 >c8 <g8 e8 c8 e8",
    Am = "o3 a8 >c8 e8 a8 e8 c8 <a8 >c8", B7 = "o3 b8 >d+8 f+8 a8 f+8 d+8 <b8 >d+8",
    D = "o4 d8 f+8 a8 >d8 <a8 f+8 d8 f+8", G = "o4 d8 g8 b8 >d8 <b8 g8 d8 g8",
  }
  local pad = { Em = "o4 (egb)", C = "o4 (ceg)", Am = "o4 (ace)", B7 = "o3 (b>d+f+a)", D = "o4 (df+a)", G = "o4 (dgb)" }
  local root = { Em = "e", C = "c", Am = "a", B7 = "b", D = "d", G = "g" }
  local a, p, b = "", "", ""
  for _, c in ipairs(prog) do
    a = a .. arp[c] .. " | "
    p = p .. pad[c] .. "1 | "
    b = b .. "o2 " .. root[c] .. "2. " .. root[c] .. "4 | "
  end
  midnight.tracks = {
    { patch = "bells", vol = 0.7, mml = "q7 " .. mel },
    { patch = "epiano", vol = 0.45, mml = "q7 " .. a },
    { patch = "strings", vol = 0.45, mml = "q8 " .. p },
    { patch = "bass", vol = 0.75, mml = "q7 " .. b },
    { patch = "kick", vol = 0.6, mml = bars("c4 r4 r8 c8 r4", 16) },
    { patch = "hat", vol = 0.25, mml = bars("v9 c4 c4 c4 c4", 16) },
  }
end

-- 5. Steppe Express (levels 9-10): fast, driving, harmonic minor -------------
local steppe = { name = "Steppe Express", tempo = 168, tracks = {} }
do
  local mel = table.concat({
    "o5 e8 f+8 g8 a8 b4 b4", "o5 b8 >c8 <b8 a8 g4 e4", "o5 a8 b8 >c8 <b8 a4 e4", "o5 d+8 e8 f+8 g8 f+4 <b4",
    "o5 e8 g8 b8 >e8 d+8 e8 <b4", "o6 c8 <b8 a8 g8 e4 g4", "o5 f+8 g8 a8 f+8 d+4 f+4", "o5 e2 r4 <b4",
  }, " | ")
  local prog = { "Em", "Em", "Am", "B7", "Em", "C", "B7", "Em" }
  local root = { Em = "e", Am = "a", B7 = "b", C = "c" }
  local pad = { Em = "o4 (egb)", Am = "o4 (ace)", B7 = "o3 (b>d+f+a)", C = "o4 (ceg)" }
  local b, p = "", ""
  for _, c in ipairs(prog) do
    b = b .. gallop(root[c]) .. " | "
    p = p .. pad[c] .. "2 " .. pad[c] .. "2 | "
  end
  local up = mel:gsub("o(%d)", function(o) return "o" .. (tonumber(o) + 1) end)
  steppe.tracks = {
    { patch = "lead", vol = 0.85, mml = "q6 " .. mel .. " " .. mel },
    { patch = "bells", vol = 0.4, mml = bars("r1", 8) .. "q6 " .. up },
    { patch = "strings", vol = 0.4, mml = "q8 " .. p .. p },
    { patch = "bass", vol = 0.8, mml = "q5 " .. b .. b },
    { patch = "kick", vol = 0.9, mml = bars("c4 c4 c4 c4", 16) },
    { patch = "snare", vol = 0.55, mml = bars(SNARE_24, 15) .. "c8 c8 c8 c8 c16 c16 c16 c16 c8 c8" },
    { patch = "hat", vol = 0.35, mml = bars(HATS, 16) },
  }
end

return { koro, progman, samovar, midnight, steppe }
