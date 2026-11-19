-- Lua tests, run by the engine (ctest "lua"): the game modules come from the
-- real scripts/ folder through TETRISPACK_SCRIPTS.
local Board = require("game.board")
local pieces = require("game.pieces")
local autoplay = require("game.autoplay")
local layout = require("game.layout")

local failures, count = 0, 0
local function check(cond, what)
  count = count + 1
  if not cond then
    failures = failures + 1
    print("FAIL: " .. what)
  end
end

local function seq(...)
  local list, i = { ... }, 0
  return function() i = i % #list + 1; return list[i] end
end
local index = {}
for i, n in ipairs(pieces.order) do index[n] = i end

-- Pieces: 4 cells in every rotation; 4 rotations come back to the start.
for _, name in ipairs(pieces.order) do
  local r = pieces.rotations[name]
  for k, cells in ipairs(r) do check(#cells == 4, name .. " rotation " .. k .. " has 4 cells") end
  if #r == 4 then
    local back = {}
    for i, c in ipairs(r[4]) do back[i] = { x = -c.y, y = c.x } end
    for i, c in ipairs(back) do check(c.x == r[1][i].x and c.y == r[1][i].y, name .. " rotates back") end
  end
end
check(#pieces.rotations.O == 1, "O has one rotation")

-- Spawn cells are the original's (rows 0-1, columns 3-6).
local function spawnCells(name)
  local b = Board.new({ rng = seq(index[name]) })
  local set = {}
  for _, c in ipairs(b:cells()) do set[#set + 1] = (b.piece.y + c.y) * 10 + b.piece.x + c.x end
  table.sort(set)
  return table.concat(set, ",")
end
check(spawnCells("I") == "3,4,5,6", "I spawn")
check(spawnCells("Z") == "3,4,14,15", "Z spawn")
check(spawnCells("S") == "5,6,14,15", "S spawn")
check(spawnCells("O") == "4,5,14,15", "O spawn")
check(spawnCells("J") == "4,5,6,16", "J spawn")
check(spawnCells("L") == "3,4,5,13", "L spawn")
check(spawnCells("T") == "3,4,5,14", "T spawn")
check(pieces.type.I == 1 and pieces.type.S == 3 and pieces.type.T == 7, "colour numbers")

-- Walls stop movement; slide goes all the way.
do
  local b = Board.new({ rng = seq(index.O) })
  b:slide(-1)
  local minx = math.huge
  for _, c in ipairs(b:cells()) do minx = math.min(minx, b.piece.x + c.x) end
  check(minx == 0, "slide stops at the left wall")
end

-- A single line: blink first (rows kept), then collapse and score.
do
  local b = Board.new({ rng = seq(index.I), level = 3 })
  for x = 4, 9 do b.grid[19][x] = "Z" end
  b.piece = { name = "I", rot = 1, x = 1, y = 0 }   -- cells x 0..3
  local filled = b:drop()
  check(filled == 1, "one row filled")
  check(b.clearing and b.clearing[1] == 19, "row kept for the blink")
  check(b.grid[19][0] ~= nil, "row still there while blinking")
  check(b:busy(), "board busy while blinking")
  local before = b.score
  check(before == Board.piece_points * 3, "landing scores 6 x level")
  check(b:collapse() == 1, "collapse removes it")
  check(b.lines == 1, "lines counted")
  check(b.placed == 1, "a landing counted (for its sound)")
  check(b.score == before + 100, "single line: 100")
  for x = 0, 9 do check(b.grid[19][x] == nil, "row emptied at x=" .. x) end
end

-- Four lines: 800, rows above move down.
do
  local b = Board.new({ rng = seq(index.I) })
  for y = 16, 19 do for x = 1, 9 do b.grid[y][x] = "S" end end
  b.grid[15][5] = "T"
  b.piece = { name = "I", rot = 2, x = 0, y = 1 }   -- vertical: cells y -1..2 at x 0
  check(b:fits(b.piece), "vertical I fits at the wall")
  check(b:drop() == 4, "four rows filled")
  local before = b.score
  b:collapse()
  check(b.score == before + 800, "tetris: 800")
  check(b.grid[19][5] == "T", "block above dropped four rows")
end

-- Levels go up with lines as the original's table says.
do
  local b = Board.new({ rng = seq(index.I) })
  b.lines = 9
  for x = 4, 9 do b.grid[19][x] = "Z" end
  b.piece = { name = "I", rot = 1, x = 1, y = 0 }
  b:drop(); b:collapse()
  check(b.level == 2, "10 lines: level 2")
  check(b:gravity_delay() == 0.6, "level 2 timer 600 ms")
  b.level = 10
  check(b:gravity_delay() == 0.05, "level 10 timer 50 ms")
end

-- Game over when the spawn is blocked.
do
  local b = Board.new({ rng = seq(index.O) })
  for y = 0, 1 do for x = 0, 9 do b.grid[y][x] = "T" end end
  b:spawn()
  check(b.over, "blocked spawn ends the game")
end

-- The autoplayer survives a while and clears lines.
do
  math.randomseed(1)
  local b = Board.new()
  for _ = 1, 400 do
    if b.over then break end
    local t = autoplay.choose(b)
    while t and not autoplay.act(b, t) do end
    if b.clearing then b:collapse() end
  end
  check(b.lines > 0, "autoplay clears lines (" .. b.lines .. ")")
end


-- The popup panel: scrolling list, values, actions, back.
do
  local Panel = require("ui.panel")
  local input = require("ui.input")
  check(input.action("game", "escape") == "menu" and input.action("game", "f12") == "menu"
        and input.action("game", "pad start") == "menu" and input.action("game", "mouse right") == "menu",
        "Esc, F12, Start and right click open the menu")
  local log, level = {}, 1
  local rows = { { header = "GAME" }, { label = "New", action = function() log[#log + 1] = "new" end },
                 { label = "Off", action = function() end, enabled = false },
                 { header = "SKILL" },
                 { label = "Level", values = { 1, 2, 3 }, get = function() return level end,
                   set = function(v) level = v end } }
  for k = 1, 10 do rows[#rows + 1] = { label = "Item " .. k, action = function() log[#log + 1] = k end } end
  local p = Panel.new({ rows = rows, visible = 6 })
  check(p:current() == "New", "starts on the first choice, past the header")
  p:key("down"); check(p:current() == "Level", "skips headers and disabled rows")
  p:key("right"); check(level == 2, "right raises a value")
  p:key("left"); p:key("left"); check(level == 3, "left wraps round")
  p:key("accept"); check(level == 1, "accept steps a value")
  for _ = 1, 10 do p:key("down") end
  check(p:current() == "Item 10" and p.top == #rows - 5, "scrolls to keep the choice in view")
  p:key("down"); check(p:current() == "New" and p.top == 1, "wraps to the top")
  p:key("up"); check(p:current() == "Item 10", "wraps to the bottom")
  p:key("accept"); check(log[1] == 10, "accept runs the action")
  check(p:key("back") == "close" and p:key("menu") == "close", "back and menu close it")
end

-- Starting rows: partly filled, never full.
do
  math.randomseed(3)
  local b = Board.new({ rows = 5 })
  local ok = true
  for y = 15, 19 do
    local n = 0
    for x = 0, 9 do if b.grid[y][x] then n = n + 1 end end
    if n == 0 or n == 10 then ok = false end
  end
  for y = 0, 14 do if next(b.grid[y]) then ok = false end end
  check(ok, "starting rows fill the bottom 5 rows, with gaps")
end


-- Music: the MML parser, the songs, the level -> song choice.
do
  local mml = require("music.mml")
  local notes, beats = mml.parse("o4 l8 c d4 e4. | r8 (ceg)2 ^4 [c16]4 > c < c | b- c+")
  check(beats == 0.5 + 1 + 1.5 + 0.5 + 3 + 4 * 0.25 + 0.5 + 0.5 + 0.5 + 0.5, "mml lengths (" .. beats .. ")")
  check(notes[1][3] == 60 and notes[2][3] == 62 and notes[3][3] == 64, "mml pitches, o4 c = 60")
  check(math.abs(notes[2][2] - 1 * 7 / 8) < 1e-9, "mml gate q7")
  check(notes[4][1] == 3.5 and notes[5][1] == 3.5 and notes[6][1] == 3.5, "mml chord starts together")
  check(math.abs(notes[4][2] - 3 * 7 / 8) < 1e-9, "mml tie lengthens the chord")
  local repeats = 0
  for _, n in ipairs(notes) do if n[1] >= 6.5 and n[1] < 7.5 then repeats = repeats + 1 end end
  check(repeats == 4, "mml repeat")
  check(notes[11][3] == 72 and notes[12][3] == 60, "mml octave up and down")
  check(notes[13][3] == 70 and notes[14][3] == 61, "mml flat and sharp")
  check(not pcall(mml.parse, "c4 x"), "mml rejects junk")

  local jukebox = require("music.jukebox")
  for _, song in ipairs(jukebox.songs) do
    check(#song.chords == song.bars, song.name .. ": a chord per bar")
    for _, t in ipairs(song.tracks) do
      if t.mml then
        local _, len = mml.parse(t.mml)
        check(len == song.bars * 4, song.name .. ": " .. t.inst .. " track is " .. song.bars .. " bars (" .. len / 4 .. ")")
      else
        local last = 0
        for _, n in ipairs(t.notes) do last = math.max(last, n[1] + n[2]) end
        check(last <= song.bars * 4 + 1e-6, song.name .. ": " .. t.inst .. " notes inside the song")
      end
    end
    for _, style in ipairs(jukebox.styles) do
      local b = jukebox.build(song, style.id)
      check(#b.notes > 300 and b.length > 80 and b.length < 240,
            song.name .. " on " .. style.id .. " (" .. #b.notes .. " notes, " .. b.length .. " s)")
    end
  end
  -- The Super NES restyles: faster, racing bass, stabs, open hats; the others don't.
  local koro = jukebox.songs[1]
  local pm = jukebox.songs[2]   -- in the general racing style
  local function has(tracks, inst) for _, t in ipairs(tracks) do if t.inst == inst then return true end end return false end
  local snes, md = jukebox.tracks(pm, "snes"), jukebox.tracks(pm, "genesis")
  check(has(snes, "stabs") and has(snes, "open") and not has(snes, "bells"), "snes: stabs and open hats, no doublings")
  check(not has(md, "stabs") and has(md, "bells"), "mega drive: the song's own arrangement")
  check(jukebox.tempo(pm, "snes") > pm.tempo and jukebox.tempo(pm, "genesis") == pm.tempo, "snes plays faster")
  local bassSnes
  for _, t in ipairs(snes) do if t.inst == "bass" then bassSnes = t end end
  check(#bassSnes.notes == pm.bars * 16, "snes bass in sixteenths")
  -- Per-song Super NES styles: Korobeiniki house, Samovar platformer.
  local function insts(tracks) local t = {} for _, x in ipairs(tracks) do t[x.inst] = x end return t end
  local k = insts(jukebox.tracks(koro, "snes"))
  check(k.m1piano and k.organ and k.clap and k.m1bass and not k.arp, "snes Korobeiniki: piano, organ stabs, claps, no arps")
  check(jukebox.tempo(koro, "snes") == 124, "snes Korobeiniki at a house tempo")
  local sam = jukebox.songs[3]
  local s3 = insts(jukebox.tracks(sam, "snes"))
  check(s3.flute and s3.marimba and s3.conga and s3.bongo and not s3.stabs, "snes Samovar: flute, marimba, hand drums")
  local swung = false
  for _, n in ipairs(s3.flute.notes) do
    local f = n[1] - math.floor(n[1])
    if math.abs(f - 0.64) < 1e-6 then swung = true end
  end
  check(swung, "snes Samovar swings its off-beats")
  check(insts(jukebox.tracks(sam, "genesis")).accordion ~= nil, "other engines keep the accordion")
  -- Windows FM MIDI: the default; a General MIDI band on the OPL3, no chip arps.
  check(jukebox.style == "Windows FM MIDI", "Windows FM MIDI is the default style")
  local wf = jukebox.build(koro, "winfm")
  check(wf.engine == "opl3", "Windows FM MIDI plays on the OPL3")
  local wt = insts(jukebox.tracks(koro, "winfm"))
  check(wt.comp and not wt.arp and wt.clarinet_lead, "Windows FM MIDI: piano chords, no arpeggios, clarinet lead")
  -- The full piece: intro, A, A with melody and harmony swapped, breakdown,
  -- A a whole step up.
  local A = require("music.arrange")
  local piece, pbars = jukebox.piece(koro, "winfm")
  check(pbars == 4 + 24 + 24 + 8 + 24, "Korobeiniki's form is " .. pbars .. " bars")
  local melodyIn = function(t0, t1, inst)
    for _, t in ipairs(piece) do
      if t.inst == inst then
        for _, n in ipairs(t.notes) do if n[1] >= t0 and n[1] < t1 then return n end end
      end
    end
  end
  check(melodyIn(0, 16, "clarinet_lead") == nil, "no melody in the intro")
  local first = melodyIn(16, 17, "clarinet_lead")
  check(first and first[3] == 76, "the melody starts after the intro, on E5")
  check(melodyIn(16 + 96, 16 + 97, "harmony") ~= nil, "second time the strings take the melody")
  local last = melodyIn((4 + 24 + 24 + 8) * 4, (4 + 24 + 24 + 8) * 4 + 1, "clarinet_lead")
  check(last and last[3] == 78, "last time a whole step up (F#5)")
  check(jukebox.soundfont ~= nil, "the GS SoundFont loads (Windows MIDI)")
  local midi = jukebox.build(koro, "midi")
  check(midi.soundfont == jukebox.soundfont and #midi.instruments > 5, "midi build names the SoundFont")
  check(jukebox.songFor(1) == 1 and jukebox.songFor(2) == 1 and jukebox.songFor(5) == 3 and jukebox.songFor(10) == 5,
        "a song per pair of levels")
  jukebox.choice = "Samovar"; check(jukebox.songFor(1) == 3, "a chosen song")
  jukebox.choice = "Off"; check(jukebox.songFor(1) == nil, "music off")
  jukebox.choice = "By Level"
  for _, name in ipairs(jukebox.styleNames) do
    jukebox.style = name
    jukebox.update(1)   -- plays through the dummy audio driver without error
  end
  jukebox.style = "Super NES"
end

-- Layout: the playfield is where the original's WM_SIZE puts it.
check(layout.grid.cw == 22 and layout.grid.ch == 19, "cell size 22x19 at 640x440")

-- The intro (the original's About box): Enter / Space / Esc close it, and a
-- click on OK closes it when the button is let go over it.
do
  local About = require("ui.about")
  local a = About.new({ x = 142, y = 32 })
  check(a:key("accept") == "close", "About: Enter closes")
  check(a:key("back") == "close", "About: Esc closes")
  check(a:key("up") == nil, "About: arrows do nothing")
  local bx, by, bw, bh = a:okRect()
  check(bx == 142 + 154 and by == 32 + 323 and bw == 60 and bh == 30, "About: OK where the original has it")
  local cx, cy = bx + 30, by + 15
  check(a:pointer("mouse left", cx, cy) == true and a.pressed, "About: OK sinks when clicked")
  check(a:pointer("mouse left up", cx, cy) == "close", "About: OK fires when let go over it")
  a:pointer("mouse left", cx, cy)
  check(a:pointer("mouse left up", bx - 20, cy) == true and not a.pressed, "About: dragging off OK cancels")
  a:pointer("mouse left", 150, 100)
  check(not a.pressed and a:pointer("mouse left up", cx, cy) == true, "About: a click elsewhere doesn't fire OK")
  check(a:pointer("mouse left", nil, nil) == true and not a.pressed, "About: clicks outside the canvas are ignored")
  check(a:pointer("mouse right", cx, cy) == false, "About: right click is left to the menu")
  if tp.module("about") then
    check(a.assets.version == "Tetris 2.00", "[data] About: version from TETRIS.EXE")
    check(a.assets.copyright == "(c) 1990 Microsoft Corporation.", "[data] About: copyright from TETRIS.EXE")
    check(a.assets.credits[1] == "All Rights Reserved." and #a.assets.credits == 8, "[data] About: credits from ABOUTTET.DLL")
    check(a.assets.logo and a.assets.logo.w == 259, "[data] About: the logo")
  end
end

-- High scores: the original's TETRIS.HST, the top ten's rules, saving.
do
  local scores = require("game.scores")
  -- A TETRIS.HST built here: ten 16-bit scores, then name / quote records.
  local pad = function(s) return s .. string.rep("\0", 32 - #s) end
  local hst = string.pack("<I2I2I2I2I2I2I2I2I2I2", 21597, 20301, 19996, 0, 0, 0, 0, 0, 0, 0)
  hst = hst .. pad("hanzi") .. pad("hanzi") .. pad("") .. pad("Boa Peterson") .. pad("sas") .. pad("Petri")
  for _ = 4, 10 do hst = hst .. pad("") .. pad("") end
  local list = scores.parseHst(hst)
  check(list and #list == 3, "TETRIS.HST: the scores that are set")
  check(list[1].score == 21597 and list[1].name == "hanzi" and list[2].quote == "Boa Peterson", "TETRIS.HST: names and quotes")
  check(scores.parseHst("short") == nil, "TETRIS.HST: too short is no table")

  local t = {}
  for i = 1, 10 do t[i] = { score = 1000 - i * 10, name = "p" .. i, quote = "" } end
  check(scores.qualifies(t, 991) and not scores.qualifies(t, 900) and not scores.qualifies({}, 0), "a score has to beat the tenth")
  check(scores.insert(t, { score = 975, name = "new\tname", quote = "hi" }) == 3, "a new score goes in its place")
  check(#t == 10 and t[10].score == 910 and t[3].name == "new name", "the table stays ten; tabs can't break the file")
  check(scores.insert(t, { score = 5 }) == nil, "too low: no rank")
  local back = scores.parse(scores.serialize(t))
  check(#back == 10 and back[3].score == 975 and back[3].quote == "hi" and back[1].name == "p1", "the table survives saving")

  local settings = require("game.settings")
  local st = settings.parse(settings.serialize({ start_level = 5, preview = true, music = "By Level", name = "a=b" }))
  check(st.start_level == 5 and st.preview == true and st.music == "By Level" and st.name == "a=b", "settings survive saving")
end

-- The game: a lost game plays its sound and, with a score that makes the
-- table, asks for a name, then shows the table; options are saved.
do
  local heard = {}
  local sfx = tp.sfx
  tp.sfx = function(name, ...) heard[name] = (heard[name] or 0) + 1; return sfx(name, ...) end
  dofile((os.getenv("TETRISPACK_SCRIPTS") or "scripts") .. "/main.lua")
  local function top() local s = game.inspect(); return s.popups[#s.popups] end
  game.keypressed("return", false)                                   -- past the About box
  check(#game.inspect().popups == 0, "game: the intro closes")
  game.keypressed("f2", false)
  local s = game.inspect()
  check(s.state == "play", "game: started")
  -- Bury the spawn rows (each with a gap, so nothing clears) and land a piece.
  for y = 0, 3 do s.board.grid[y] = { [0] = "I", "I", "I", "I", "I", "I", "I", "I", "I" } end
  s.board.score = 4321
  s.board:drop()
  for _ = 1, 5 do game.update(1 / 60) end
  s = game.inspect()
  check(s.state == "over", "game: over")
  check((heard.game_over or 0) == 1, "game: the game-over sound plays once")
  local entry = top()
  check(entry and entry.text and entry.score and entry.score >= 4321, "game: a high score asks for a name")
  game.textinput("Ann")
  game.keypressed("tab", false)
  game.textinput("Hi!")
  game.keypressed("return", false)                                   -- quote -> OK
  game.keypressed("return", false)                                   -- OK
  s = game.inspect()
  check(s.highscores[1] and s.highscores[1].name == "Ann" and s.highscores[1].quote == "Hi!", "game: the new entry is in the table")
  local board = top()
  check(board and board.title == "Tetris Experts" and board.rows[1].hot, "game: the table shows, the new entry highlighted")
  check(tp.storage_read("highscores"):find("Ann", 1, true), "game: the table is saved")
  check(tp.storage_read("settings"):find("player_name=Ann", 1, true), "game: the name is remembered")
  check(tp.storage_read("settings"):find("sfx_volume=7\n", 1, true), "game: sound effects default to 7")
  check(tp.storage_read("settings"):find("music_volume=1\n", 1, true), "game: music defaults to 1")
  check(tp.storage_read("settings"):find("bg_gradient=false\n", 1, true), "game: the background gradient starts off")
  tp.sfx = sfx
end

-- Options > Background Gradient: black, clear at the top, darkest at the
-- bottom, over the background only (drawn with it, before the rest).
do
  local skin = require("game.skin")
  local fills, fill = {}, tp.fill
  tp.fill = function(x, y, w, h, r, g, b, a) fills[#fills + 1] = { x = x, y = y, w = w, h = h, a = a or 255, rgb = r + g + b } end
  skin.shade(640, 440)
  tp.fill = fill
  local first, last, rising, black = fills[1], fills[#fills], true, true
  for i, f in ipairs(fills) do
    if i > 1 and f.a < fills[i - 1].a then rising = false end
    if f.rgb ~= 0 or f.w ~= 640 or f.h ~= 1 then black = false end
  end
  check(first.y > 0 and first.a <= 2, "gradient: transparent at the top")
  check(last.y == 439 and last.a == skin.gradient_max, "gradient: darkest on the bottom row")
  check(rising and black, "gradient: black rows, darker going down")
  local mid
  for _, f in ipairs(fills) do if f.y == 220 then mid = f.a end end
  check(mid and mid < skin.gradient_max / 2, "gradient: the top half stays mostly clear")

  local drew, bg = false, skin.shade
  skin.shade = function(...) drew = true; return bg(...) end
  skin.gradient = true
  game.draw()
  skin.gradient = false
  local off = drew
  drew = false
  game.draw()
  skin.shade = bg
  check(off and not drew, "gradient: drawn only when the option is on")
end

-- With data: the executable's resources are listed, backgrounds load.
if tp.has_data() then
  check(#tp.resources() > 0, "[data] resources listed")
  for i = 1, 10 do
    local h, w = tp.bitmap(i)
    check(h ~= nil and w == 32, "[data] background bitmap " .. i)
  end
end

print(("lua: %d checks, %d failed"):format(count, failures))
tp.quit(failures == 0 and 0 or 1)

game = {}
