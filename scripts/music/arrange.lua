-- Building blocks for game-style arrangements, written once and used by
-- every song: a harmony line under the melody, chip arpeggios, moving bass
-- lines and drum patterns with fills. All return notes in beats
-- ({ start, length, key, velocity }), from a song's chord per bar.
local mml = require("music.mml")

local A = {}

-- Chords: pitch classes, root first.
A.chords = {
  C = { 0, 4, 7 }, D = { 2, 6, 9 }, E = { 4, 8, 11 }, G = { 7, 11, 2 }, A = { 9, 1, 4 },
  Am = { 9, 0, 4 }, Bm = { 11, 2, 6 }, Dm = { 2, 5, 9 }, Em = { 4, 7, 11 }, Gm = { 7, 10, 2 },
  A7 = { 9, 1, 4, 7 }, B7 = { 11, 3, 6, 9 },
}

local function chordAt(chords, beat) return A.chords[chords[math.floor(beat / 4) + 1] or chords[#chords]] end

local function isTone(chord, key)
  for _, pc in ipairs(chord) do if key % 12 == pc then return true end end
  return false
end

-- Harmony: under each melody note, the nearest chord tone at least a minor
-- third below (and no more than an octave). Notes shorter than `minLen`
-- beats are left to the melody alone, which keeps fast runs clean.
function A.harmony(melodyText, chords, opts)
  opts = opts or {}
  local minLen = opts.minLen or 0.5
  local melody = mml.parse(melodyText)
  local out = {}
  for _, n in ipairs(melody) do
    local start, len, key, vel = n[1], n[2], n[3], n[4]
    if len >= minLen * 0.8 then
      local chord = chordAt(chords, start)
      for k = key - 3, key - 12, -1 do
        if isTone(chord, k) then
          out[#out + 1] = { start, len, k, vel * 0.8 }
          break
        end
      end
    end
  end
  return out
end

-- Arpeggios: chord tones from `low` upward, up and down, `step` beats each
-- (0.25 = sixteenths), accent on the beat.
function A.arp(chords, low, step, opts)
  opts = opts or {}
  local out = {}
  local from, to = opts.from or 1, opts.to or #chords
  for bar = from, to do
    local chord = A.chords[chords[bar]]
    local tones = {}
    for _, pc in ipairs(chord) do
      local k = low + ((pc - low) % 12)
      tones[#tones + 1] = k
    end
    table.sort(tones)
    tones[#tones + 1] = tones[1] + 12
    local pattern = {}
    for i = 1, #tones do pattern[#pattern + 1] = tones[i] end
    for i = #tones - 1, 2, -1 do pattern[#pattern + 1] = tones[i] end
    local t, i = (bar - 1) * 4, 0
    while t < bar * 4 - 1e-9 do
      local onBeat = math.abs(t - math.floor(t + 1e-9)) < 1e-6
      out[#out + 1] = { t, step * 0.7, pattern[i % #pattern + 1], onBeat and 0.8 or 0.55 }
      t = t + step
      i = i + 1
    end
  end
  return out
end

local function rootKey(chord, low) return low + ((chord[1] - low) % 12) end
local function fifthKey(chord, low) return rootKey(chord, low) + 7 end

-- Bass lines, one bar at a time, from the root at or above `low` (36 = C2).
A.bassStyles = {
  -- Octave bounce on eighths, walking up to the next bar on the last beat.
  octave = function(chord, low, nextChord)
    local r, f = rootKey(chord, low), fifthKey(chord, low)
    local nr = nextChord and rootKey(nextChord, low) or r
    local approach = (nr > r) and nr - 1 or nr + 1
    return { { 0, r }, { 0.5, r + 12 }, { 1, r }, { 1.5, r + 12 }, { 2, r }, { 2.5, f }, { 3, r + 12 }, { 3.5, approach } }, 0.45
  end,
  -- One eighth and two sixteenths per beat: a gallop.
  gallop = function(chord, low)
    local r, out = rootKey(chord, low), {}
    for b = 0, 3 do
      local k = (b == 2) and fifthKey(chord, low) or r
      out[#out + 1] = { b, k }; out[#out + 1] = { b + 0.5, k }; out[#out + 1] = { b + 0.75, k }
    end
    return out, 0.22
  end,
  -- Root and fifth on the beats (a polka's oom), with a pickup.
  oompah = function(chord, low)
    local r, f = rootKey(chord, low), fifthKey(chord, low) - 12
    return { { 0, r }, { 1, r + 12 }, { 2, f }, { 3, r + 12 }, { 3.5, r + 7 } }, 0.4
  end,
  -- Syncopated funk: root, root, fifth, octave.
  synco = function(chord, low)
    local r, f = rootKey(chord, low), fifthKey(chord, low)
    return { { 0, r }, { 0.75, r }, { 1.5, f }, { 2, r + 12 }, { 2.75, r }, { 3, f }, { 3.5, r + 12 } }, 0.35
  end,
  -- Racing synth bass: sixteenths with octave jumps and a syncopated push
  -- (the early-90s Euro-rock driving style).
  drive = function(chord, low, nextChord)
    local r, f = rootKey(chord, low), fifthKey(chord, low)
    local nr = nextChord and rootKey(nextChord, low) or r
    local seq = { r, r, r + 12, r, r, r + 12, r, r + 12, r, r, r + 12, r, f, f + 12, r + 12, (nr > r) and nr - 2 or nr + 2 }
    local out = {}
    for i, k in ipairs(seq) do out[#out + 1] = { (i - 1) * 0.25, k } end
    return out, 0.18
  end,
  -- Steady pumping eighths with an octave lift every other beat.
  pump = function(chord, low)
    local r, out = rootKey(chord, low), {}
    for e = 0, 7 do out[#out + 1] = { e * 0.5, (e % 4 == 3) and r + 12 or r } end
    return out, 0.4
  end,
}

function A.bass(chords, style, opts)
  opts = opts or {}
  local low = opts.low or 36
  local out = {}
  for bar = opts.from or 1, opts.to or #chords do
    local chord, nxt = A.chords[chords[bar]], A.chords[chords[bar + 1] or chords[1]]
    local steps, len = A.bassStyles[style](chord, low, nxt)
    for _, st in ipairs(steps) do
      out[#out + 1] = { (bar - 1) * 4 + st[1], len, st[2], (st[1] % 1 == 0) and 0.9 or 0.75 }
    end
  end
  return out
end

-- Drums from 16-step patterns per bar ("x" hit, "o" soft hit, "." rest).
-- A fill replaces the last bar of every `fillEvery` bars; a crash marks
-- the start of each phrase.
A.patterns = {
  rock  = { kick = "x.....x.x.......", snare = "....x.......x...", hat = "x.o.x.o.x.o.x.o." },
  drive = { kick = "x...x...x...x...", snare = "....x.......x...", hat = "xoxoxoxoxoxoxoxo" },
  polka = { kick = "x.......x.......", snare = "....o.......o...", hat = "..x...x...x...x." },
  pop   = { kick = "x.....x...x.....", snare = "....x.......x...", hat = "x.o.x.o.x.o.x.oo" },
  slow  = { kick = "x.........x.....", snare = "........x.......", hat = "x...o...x...o..." },
  -- Racing rock: kick on every beat plus pushes, open hats on the off-beats.
  racer = { kick = "x...x..xx...x..x", snare = "....x.......x...", hat = "xoxoxoxoxoxoxoxo", open = "..x...x...x...x." },
}
A.fills = {
  snare = { snare = "....x.x.xoxoxxxx", kick = "x.......x.......", tom = "................" },
  toms  = { snare = "....x.......x.x.", kick = "x.......x.......", tom = "........x.x.x.xx" },
  racer = { snare = "x.x.x.xxx.x.....", kick = "x...x...x.......", tom = "........x.x.x.xx" },
}

function A.drums(bars, patternName, opts)
  opts = opts or {}
  local p = A.patterns[patternName]
  local fill = A.fills[opts.fill or "snare"]
  local every = opts.fillEvery or 8
  local out = { kick = {}, snare = {}, hat = {}, crash = {}, tom = {}, open = {} }
  local tomKeys = { 50, 47, 45, 43 }
  for bar = 1, bars do
    local isFill = (bar % every == 0)
    for part, list in pairs(out) do
      local steps = (isFill and fill[part]) or p[part]
      if (part == "hat" or part == "open") and isFill then steps = (part == "hat") and "x.o.x.o........." or nil end
      if steps then
        local n = 0
        for i = 1, 16 do
          local c = steps:sub(i, i)
          if c == "x" or c == "o" then
            n = n + 1
            local vel = (c == "x") and 0.9 or 0.55
            if isFill and part == "snare" and i > 8 then vel = 0.5 + 0.03 * i end
            local key = 60
            if part == "tom" then key = tomKeys[math.min(#tomKeys, math.floor((n + 1) / 2))] end
            list[#list + 1] = { (bar - 1) * 4 + (i - 1) * 0.25, 0.2, key, math.min(1, vel) }
          end
        end
      end
    end
    if (bar - 1) % every == 0 and opts.crash ~= false then
      out.crash[#out.crash + 1] = { (bar - 1) * 4, 0.2, 60, 0.8 }
    end
  end
  return out
end

-- Power-chord stabs (root, fifth, octave) on a 16-step rhythm per bar,
-- from the root at or above `low`.
function A.stabs(chords, rhythm, low, opts)
  opts = opts or {}
  local out = {}
  for bar = opts.from or 1, opts.to or #chords do
    local r = rootKey(A.chords[chords[bar]], low)
    for i = 1, 16 do
      local c = rhythm:sub(i, i)
      if c == "x" or c == "o" then
        local t = (bar - 1) * 4 + (i - 1) * 0.25
        local vel = (c == "x") and 0.85 or 0.6
        for _, k in ipairs({ r, r + 7, r + 12 }) do out[#out + 1] = { t, 0.2, k, vel } end
      end
    end
  end
  return out
end

return A
