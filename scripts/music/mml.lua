-- A small Music Macro Language for writing the songs as text.
--
--   c d e f g a b   notes; + or # sharp, - flat; then a length: 1 2 4 8 16 32,
--                   with dots (4. = dotted quarter). No length: the l value.
--   r               rest (same lengths)
--   o4              octave (o4 c = middle C, MIDI 60); < down, > up
--   l8              default length
--   v12             velocity 1-15
--   q7              gate: notes sound for 7/8 of their length (q8 = legato)
--   (ceg)4          a chord: the notes together, one length
--   ^4              tie: lengthen the last note (or rest) by a quarter
--   [ ... ]3        repeat three times
--   | and spaces    ignored (bar lines for reading)
--
-- parse(text) -> notes { { start, length, key, velocity } ... } in beats
-- (quarter note = 1), and the total length in beats.
local mml = {}

local SEMI = { c = 0, d = 2, e = 4, f = 5, g = 7, a = 9, b = 11 }

local function expand(text)
  -- Repeats, innermost first.
  local changed = true
  while changed do
    changed = false
    text = text:gsub("%[([^%[%]]*)%](%d*)", function(body, n)
      changed = true
      return body:rep(tonumber(n) or 2, " ")
    end)
  end
  return text
end

function mml.parse(text)
  text = expand(text:lower())
  local i, n = 1, #text
  local octave, deflen, vel, gate = 4, 1, 12, 7
  local t = 0
  local notes, last = {}, nil

  local function peek() return text:sub(i, i) end
  local function number()
    local s = text:match("^%d+", i)
    if not s then return nil end
    i = i + #s
    return tonumber(s)
  end
  -- A length: 4 -> 1 beat, 8 -> 0.5, with dots.
  local function length()
    local d = number()
    local beats = d and (4 / d) or deflen
    local add = beats
    while peek() == "." do
      i = i + 1
      add = add / 2
      beats = beats + add
    end
    return beats
  end
  local function pitch(letter)
    local k = SEMI[letter] + 12 * (octave + 1)
    while true do
      local c = peek()
      if c == "+" or c == "#" then k = k + 1; i = i + 1
      elseif c == "-" then k = k - 1; i = i + 1
      else break end
    end
    return k
  end

  while i <= n do
    local c = peek()
    i = i + 1
    if SEMI[c] then
      local k = pitch(c)
      local len = length()
      last = { t, len * gate / 8, k, vel / 15, full = len }
      notes[#notes + 1] = last
      t = t + len
    elseif c == "r" then
      local len = length()
      last = { rest = true, full = len }
      t = t + len
    elseif c == "(" then
      local chord = {}
      while i <= n and peek() ~= ")" do
        local d = peek()
        i = i + 1
        if SEMI[d] then chord[#chord + 1] = pitch(d)
        elseif d == "<" then octave = octave - 1
        elseif d == ">" then octave = octave + 1
        elseif d == "o" then octave = number() or octave end
      end
      i = i + 1  -- ")"
      local len = length()
      local group = {}
      for _, k in ipairs(chord) do
        local note = { t, len * gate / 8, k, vel / 15, full = len }
        notes[#notes + 1] = note
        group[#group + 1] = note
      end
      last = { group = group, full = len }
      t = t + len
    elseif c == "^" then
      local len = length()
      if last then
        for _, note in ipairs(last.group or (last.rest and {} or { last })) do
          note.full = note.full + len
          note[2] = note.full * gate / 8
        end
        last.full = last.full + len
      end
      t = t + len
    elseif c == "o" then octave = number() or octave
    elseif c == "<" then octave = octave - 1
    elseif c == ">" then octave = octave + 1
    elseif c == "l" then deflen = length()
    elseif c == "v" then vel = number() or vel
    elseif c == "q" then gate = number() or gate
    elseif c:match("[%s|]") then -- ignore
    else
      error(("mml: unexpected '%s' at %d in: %s"):format(c, i - 1, text:sub(math.max(1, i - 20), i + 20)))
    end
  end
  for _, note in ipairs(notes) do note.full = nil end
  return notes, t
end

return mml
