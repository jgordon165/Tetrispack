-- The high score table: the top ten, each with a name and a "cool quote",
-- as Tetris for Windows 2.00 keeps them ("Tetris Experts", its Hall of
-- Fame). Saved between runs with tp.storage_write("highscores"); the first
-- time, it starts from the original's own table if TETRIS.HST is beside the
-- player's TETRIS.EXE (read, never written).
local scores = { max = 10, nameMax = 31 }

-- One line per entry: score, level, lines, name, quote, tab-separated.
local function clean(s)
  return (tostring(s or ""):gsub("[%c\t]", " "):sub(1, scores.nameMax))
end

function scores.serialize(list)
  local out = {}
  for _, e in ipairs(list) do
    out[#out + 1] = table.concat({ math.floor(e.score), e.level or 0, e.lines or 0, clean(e.name), clean(e.quote) }, "\t")
  end
  return table.concat(out, "\n") .. "\n"
end

function scores.parse(text)
  local list = {}
  for line in (text or ""):gmatch("[^\n]+") do
    local s, lv, ln, name, quote = line:match("^(%d+)\t(%d+)\t(%d+)\t([^\t]*)\t?([^\t]*)$")
    if s then
      list[#list + 1] = { score = tonumber(s), level = tonumber(lv), lines = tonumber(ln), name = name, quote = quote }
    end
  end
  table.sort(list, function(a, b) return a.score > b.score end)
  while #list > scores.max do list[#list] = nil end
  return list
end

-- TETRIS.HST (docs/formats.md): ten 16-bit scores, then ten records of a
-- 32-byte name and a 32-byte quote, NUL-terminated, highest first.
function scores.parseHst(bytes)
  if type(bytes) ~= "string" or #bytes < 20 + 64 * 10 then return nil end
  local function field(at)
    local s = bytes:sub(at + 1, at + 32)
    return (s:match("^([^%z]*)") or ""):gsub("%s+$", "")
  end
  local list = {}
  for i = 0, 9 do
    local score = string.unpack("<I2", bytes, 1 + 2 * i)
    local base = 20 + 64 * i
    if score > 0 then
      list[#list + 1] = { score = score, level = 0, lines = 0, name = field(base), quote = field(base + 32) }
    end
  end
  table.sort(list, function(a, b) return a.score > b.score end)
  return list
end

-- Would `score` make the table?
function scores.qualifies(list, score)
  if not score or score <= 0 then return false end
  return #list < scores.max or score > list[#list].score
end

-- Puts the entry in its place; returns its rank (1-10), or nil if it
-- didn't make the table.
function scores.insert(list, entry)
  if not scores.qualifies(list, entry.score) then return nil end
  local rank = #list + 1
  for i, e in ipairs(list) do
    if entry.score > e.score then rank = i; break end
  end
  table.insert(list, rank, { score = entry.score, level = entry.level or 0, lines = entry.lines or 0,
                             name = clean(entry.name), quote = clean(entry.quote) })
  while #list > scores.max do list[#list] = nil end
  return rank
end

-- The saved table, else the original's, else an empty one.
function scores.load()
  if tp and tp.storage_read then
    local text = tp.storage_read("highscores")
    if text then return scores.parse(text) end
  end
  if tp and tp.data_file then
    local list = scores.parseHst(tp.data_file("TETRIS.HST"))
    if list then return list end
  end
  return {}
end

function scores.save(list)
  if tp and tp.storage_write then tp.storage_write("highscores", scores.serialize(list)) end
end

return scores
