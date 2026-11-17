-- The instruments, per sound engine. Each song's tracks name an instrument
-- (lead, harmony, bass, arp, ...); each engine says how to play it, or
-- false to leave that part out (the Mega Drive and the SNES have fewer
-- voices than the Sound Blaster, so their arrangements thin out the same
-- way the consoles' composers had to). All patches and samples are this
-- port's own.
local I = {}

local function with(base, changes)
  local t = {}
  for k, v in pairs(base) do t[k] = v end
  for k, v in pairs(changes) do t[k] = v end
  return t
end

-- Sound Blaster 16 (OPL3): two-operator FM ---------------------------------
local function opl(def, extra)
  local t = { kind = "opl", opl = def }
  for k, v in pairs(extra or {}) do t[k] = v end
  return t
end
local oplLead = opl({ feedback = 5,
  mod = { mult = 2, tl = 34, ar = 15, dr = 3, sl = 2, rr = 6, vib = 1 },
  car = { mult = 1, tl = 3, ar = 14, dr = 2, sl = 3, rr = 6, vib = 1 } })
I.opl3 = {
  lead = oplLead,
  harmony = with(oplLead, { gain = 0.55, pan = -1 }),
  arp = opl({ feedback = 6,
    mod = { mult = 1, tl = 30, ar = 15, dr = 6, sl = 6, rr = 8 },
    car = { mult = 1, tl = 4, ar = 15, dr = 5, sl = 8, rr = 8, ws = 6 } }, { gain = 0.45, pan = 1 }),
  epiano = opl({ feedback = 2,
    mod = { mult = 1, tl = 30, ar = 15, dr = 5, sl = 8, rr = 6, ksl = 1 },
    car = { mult = 1, tl = 6, ar = 15, dr = 3, sl = 7, rr = 6, sustain = 0 } }),
  bass = opl({ feedback = 4,
    mod = { mult = 0, tl = 26, ar = 15, dr = 5, sl = 5, rr = 8 },
    car = { mult = 1, tl = 0, ar = 15, dr = 3, sl = 4, rr = 8 } }),
  pad = opl({ feedback = 3,
    mod = { mult = 1, tl = 38, ar = 6, dr = 2, sl = 2, rr = 5, vib = 1 },
    car = { mult = 1, tl = 10, ar = 5, dr = 1, sl = 2, rr = 5, vib = 1 } }),
  accordion = opl({ feedback = 3, additive = 1,
    mod = { mult = 2, tl = 14, ar = 13, dr = 1, sl = 1, rr = 7, am = 1 },
    car = { mult = 1, tl = 6, ar = 13, dr = 1, sl = 1, rr = 7, am = 1 } }),
  bells = opl({ feedback = 0,
    mod = { mult = 7, tl = 32, ar = 15, dr = 6, sl = 10, rr = 6, sustain = 0 },
    car = { mult = 3, tl = 6, ar = 15, dr = 5, sl = 12, rr = 5, sustain = 0 } }),
  pluck = opl({ feedback = 3,
    mod = { mult = 3, tl = 34, ar = 15, dr = 7, sl = 12, rr = 6, sustain = 0 },
    car = { mult = 1, tl = 4, ar = 15, dr = 6, sl = 10, rr = 6, sustain = 0 } }),
  kick = opl({ mod = { mult = 0, tl = 10, ar = 15, dr = 9, sl = 15, rr = 15 },
    car = { mult = 1, tl = 0, ar = 15, dr = 7, sl = 15, rr = 15, sustain = 0 } }, { key = 28 }),
  snare = opl({ feedback = 7, mod = { mult = 15, tl = 0, ar = 15, dr = 8, sl = 15, rr = 15 },
    car = { mult = 1, tl = 6, ar = 15, dr = 7, sl = 15, rr = 15, sustain = 0 } }, { key = 62 }),
  hat = opl({ feedback = 7, mod = { mult = 15, tl = 0, ar = 15, dr = 10, sl = 15, rr = 15 },
    car = { mult = 15, tl = 14, ar = 15, dr = 10, sl = 15, rr = 15, sustain = 0 } }, { key = 96 }),
  open = opl({ feedback = 7, mod = { mult = 15, tl = 0, ar = 15, dr = 7, sl = 15, rr = 7 },
    car = { mult = 15, tl = 12, ar = 15, dr = 7, sl = 15, rr = 7, sustain = 0 } }, { key = 96, gain = 0.6 }),
  stabs = with(oplLead, { gain = 0.4 }),
  crash = opl({ feedback = 7, mod = { mult = 15, tl = 0, ar = 15, dr = 4, sl = 15, rr = 5 },
    car = { mult = 13, tl = 8, ar = 15, dr = 4, sl = 15, rr = 5, sustain = 0 } }, { key = 90, gain = 0.6 }),
  tom = opl({ mod = { mult = 0, tl = 20, ar = 15, dr = 8, sl = 15, rr = 15 },
    car = { mult = 1, tl = 0, ar = 15, dr = 6, sl = 15, rr = 15, sustain = 0 } }),
}

-- Mega Drive: YM2612 four-operator FM + SN76489 PSG -----------------------
-- Operators listed 1-4; alg 4 = two pairs (1->2, 3->4), alg 7 = all heard.
local function fm(alg, fb, ops, extra)
  local t = { kind = "fm4", fm4 = { alg = alg, fb = fb, ops = ops, ams = (extra or {}).ams, fms = (extra or {}).fms } }
  for k, v in pairs(extra or {}) do if k ~= "ams" and k ~= "fms" then t[k] = v end end
  return t
end
local mdLead = fm(4, 5, {   -- a bright detuned synth-brass lead, LFO vibrato
  { mul = 1, tl = 28, ar = 31, d1r = 6, d2r = 0, d1l = 2, rr = 7, dt = 3 },
  { mul = 1, tl = 4, ar = 31, d1r = 4, d2r = 1, d1l = 2, rr = 7, dt = 3 },
  { mul = 2, tl = 34, ar = 31, d1r = 8, d2r = 0, d1l = 3, rr = 7, dt = 7 },
  { mul = 1, tl = 8, ar = 31, d1r = 4, d2r = 1, d1l = 2, rr = 7, dt = 7 },
}, { fms = 3 })
local mdPsgEnv = { attack = 0.002, decay = 0.12, sustain = 0.35, release = 0.04 }
I.genesis = {
  lead = mdLead,
  harmony = with(mdLead, { gain = 0.5, pan = -1 }),
  arp = { kind = "psg", env = mdPsgEnv, gain = 0.5, pan = 1 },
  epiano = fm(4, 3, {
    { mul = 14, tl = 44, ar = 31, d1r = 14, d2r = 4, d1l = 8, rr = 6 },
    { mul = 1, tl = 6, ar = 31, d1r = 7, d2r = 3, d1l = 5, rr = 6 },
    { mul = 1, tl = 32, ar = 31, d1r = 9, d2r = 3, d1l = 6, rr = 6 },
    { mul = 1, tl = 8, ar = 31, d1r = 6, d2r = 3, d1l = 4, rr = 6 },
  }, { gain = 0.55 }),
  bass = fm(0, 6, {   -- the classic punchy FM slap bass
    { mul = 0, tl = 28, ar = 31, d1r = 12, d2r = 2, d1l = 5, rr = 8 },
    { mul = 1, tl = 30, ar = 31, d1r = 10, d2r = 2, d1l = 5, rr = 8 },
    { mul = 0, tl = 22, ar = 31, d1r = 12, d2r = 2, d1l = 6, rr = 8 },
    { mul = 1, tl = 2, ar = 31, d1r = 7, d2r = 3, d1l = 2, rr = 9 },
  }),
  pad = false,    -- six FM voices: no room for held chords
  accordion = fm(7, 2, {
    { mul = 1, tl = 18, ar = 26, d1r = 2, d1l = 1, rr = 7, am = 1, dt = 2 },
    { mul = 2, tl = 22, ar = 26, d1r = 2, d1l = 1, rr = 7, am = 1, dt = 6 },
    { mul = 1, tl = 16, ar = 26, d1r = 2, d1l = 1, rr = 7, dt = 6 },
    { mul = 4, tl = 32, ar = 26, d1r = 2, d1l = 1, rr = 7 },
  }, { ams = 2, fms = 2 }),
  bells = { kind = "psg", env = { attack = 0.001, decay = 0.35, sustain = 0, release = 0.1 }, gain = 0.45, vibrato = 0.1 },
  pluck = fm(4, 4, {
    { mul = 3, tl = 36, ar = 31, d1r = 14, d2r = 6, d1l = 10, rr = 7 },
    { mul = 1, tl = 4, ar = 31, d1r = 10, d2r = 6, d1l = 8, rr = 7 },
    { mul = 1, tl = 30, ar = 31, d1r = 12, d2r = 6, d1l = 10, rr = 7 },
    { mul = 2, tl = 10, ar = 31, d1r = 11, d2r = 6, d1l = 9, rr = 7 },
  }, { gain = 0.5 }),
  kick = fm(0, 3, {
    { mul = 0, tl = 20, ar = 31, d1r = 20, d1l = 15, rr = 15 },
    { mul = 0, tl = 26, ar = 31, d1r = 22, d1l = 15, rr = 15 },
    { mul = 0, tl = 24, ar = 31, d1r = 24, d1l = 15, rr = 15 },
    { mul = 1, tl = 0, ar = 31, d1r = 14, d1l = 15, rr = 15 },
  }, { key = 33, gain = 0.85 }),
  snare = fm(7, 7, {
    { mul = 15, tl = 6, ar = 31, d1r = 16, d1l = 15, rr = 15 },
    { mul = 1, tl = 127, ar = 31, d1r = 16, d1l = 15, rr = 15 },
    { mul = 3, tl = 20, ar = 31, d1r = 18, d1l = 15, rr = 15 },
    { mul = 1, tl = 14, ar = 31, d1r = 16, d1l = 15, rr = 15 },
  }, { key = 57, gain = 0.8 }),
  hat = { kind = "noise", noise_hz = 22000, env = { attack = 0.001, decay = 0.04, sustain = 0, release = 0.02 }, gain = 0.2 },
  crash = { kind = "noise", noise_hz = 16000, env = { attack = 0.001, decay = 0.9, sustain = 0, release = 0.3 }, gain = 0.3 },
  open = { kind = "noise", noise_hz = 22000, env = { attack = 0.001, decay = 0.18, sustain = 0, release = 0.05 }, gain = 0.2 },
  stabs = with(mdLead, { gain = 0.4 }),
  tom = fm(0, 2, {
    { mul = 0, tl = 30, ar = 31, d1r = 14, d1l = 15, rr = 15 },
    { mul = 0, tl = 30, ar = 31, d1r = 14, d1l = 15, rr = 15 },
    { mul = 1, tl = 24, ar = 31, d1r = 14, d1l = 15, rr = 15 },
    { mul = 1, tl = 0, ar = 31, d1r = 10, d1l = 15, rr = 15 },
  }, { gain = 0.6 }),
}

-- Super NES: eight sampled voices with echo --------------------------------
-- Voiced for an early-90s racing-game sound: a bright saw lead that glides
-- between notes, pulse harmony, a punchy saw bass, brass stabs, rock kit.
-- priority: who keeps a voice when all eight are busy (higher stays).
local function s(wave, env, extra)
  local t = { kind = "sample", wave = wave, env = env }
  for k, v in pairs(extra or {}) do t[k] = v end
  return t
end
local drum = { attack = 0.001, decay = 10, sustain = 1, release = 0.4 }
I.snes = {
  lead = s("saw", { attack = 0.003, decay = 0.6, sustain = 0.75, release = 0.12 },
           { vibrato = 0.2, glide = 0.035, echo = 0.45, gain = 0.62, priority = 3 }),
  harmony = s("pulse25", { attack = 0.004, decay = 0.5, sustain = 0.6, release = 0.12 },
              { gain = 0.4, pan = -0.45, echo = 0.4, priority = 2 }),
  arp = s("pulse12", { attack = 0.002, decay = 0.12, sustain = 0.25, release = 0.05 }, { gain = 0.36, pan = 0.5, echo = 0.4 }),
  epiano = s("epiano", { attack = 0.003, decay = 0.9, sustain = 0.3, release = 0.3 }, { gain = 0.5, echo = 0.5, pan = 0.25 }),
  bass = s("saw", { attack = 0.002, decay = 0.14, sustain = 0.45, release = 0.04 }, { gain = 0.78, priority = 3 }),
  stabs = s("brass", { attack = 0.003, decay = 0.18, sustain = 0.35, release = 0.08 }, { gain = 0.3, pan = 0.25, echo = 0.35 }),
  pad = false,
  accordion = s("organ", { attack = 0.02, decay = 0.3, sustain = 0.8, release = 0.12 },
                { vibrato = 0.15, glide = 0.02, echo = 0.4, gain = 0.55, priority = 3 }),
  bells = s("bell", { attack = 0.001, decay = 0.7, sustain = 0, release = 0.4 }, { gain = 0.4, pan = 0.4, echo = 0.7 }),
  pluck = s("pluck", { attack = 0.001, decay = 10, sustain = 1, release = 0.2 }, { gain = 0.55, pan = 0.35, echo = 0.35 }),
  kick = s("kick", drum, { key = 60, gain = 0.95, priority = 2 }),
  snare = s("snare", drum, { key = 60, gain = 0.75, echo = 0.3, priority = 2 }),
  hat = s("hat", drum, { key = 60, gain = 0.42, pan = 0.3 }),
  open = s("openhat", drum, { key = 60, gain = 0.32, pan = 0.3 }),
  crash = s("crash", drum, { key = 60, gain = 0.5, pan = -0.3, echo = 0.3, priority = 2 }),
  tom = s("tom", drum, { gain = 0.75, echo = 0.2, priority = 2 }),

  -- Korobeiniki's workstation-house set.
  m1piano = s("m1piano", { attack = 0.001, decay = 10, sustain = 1, release = 0.25 }, { gain = 0.62, echo = 0.35, priority = 3 }),
  m1piano_soft = s("m1piano", { attack = 0.001, decay = 10, sustain = 1, release = 0.25 }, { gain = 0.3, pan = -0.4, echo = 0.35, priority = 2 }),
  organ = s("m1organ", { attack = 0.002, decay = 0.25, sustain = 0.5, release = 0.08 }, { gain = 0.24, pan = 0.3, echo = 0.3 }),
  m1bass = s("m1bass", { attack = 0.002, decay = 0.25, sustain = 0.6, release = 0.05 }, { gain = 0.85, priority = 3 }),
  clap = s("clap", drum, { key = 60, gain = 0.6, echo = 0.35, priority = 2 }),

  -- Samovar's platformer set.
  flute = s("flute", { attack = 0.025, decay = 0.4, sustain = 0.8, release = 0.1 },
            { vibrato = 0.25, echo = 0.4, gain = 0.6, priority = 3 }),
  marimba = s("marimba", { attack = 0.001, decay = 10, sustain = 1, release = 0.2 }, { gain = 0.42, pan = -0.35, echo = 0.3, priority = 2 }),
  marimba_hi = s("marimba", { attack = 0.001, decay = 10, sustain = 1, release = 0.15 }, { gain = 0.3, pan = 0.4, echo = 0.3 }),
  bouncy = s("m1bass", { attack = 0.002, decay = 0.12, sustain = 0.3, release = 0.04 }, { gain = 0.85, priority = 3 }),
  conga = s("conga", drum, { gain = 0.5, pan = -0.4 }),
  bongo = s("bongo", drum, { gain = 0.42, pan = 0.45 }),
}

-- Windows MIDI: General MIDI programs on a GS SoundFont ----------------------
-- (program numbers 0-127, as in a .MID file; drums on channel 10, kit 0).
local function gm(program, extra)
  local t = { kind = "gm", program = program }
  for k, v in pairs(extra or {}) do t[k] = v end
  return t
end
local function kit(key, extra)
  local t = { kind = "gm", program = 0, drums = true, key = key }
  for k, v in pairs(extra or {}) do t[k] = v end
  return t
end
I.midi = {
  lead = gm(80, { gain = 0.85 }),                       -- Square Lead
  harmony = gm(71, { gain = 0.7, pan = -0.35 }),        -- Clarinet
  arp = gm(11, { gain = 0.55, pan = 0.4 }),             -- Vibraphone
  epiano = gm(4, { gain = 0.7, pan = 0.2 }),            -- Electric Piano 1
  bass = gm(33, { gain = 0.95 }),                       -- Fingered Bass
  pad = gm(48, { gain = 0.6, pan = -0.15 }),            -- String Ensemble
  accordion = gm(21, { gain = 0.85 }),                  -- Accordion
  bells = gm(9, { gain = 0.6, pan = 0.35 }),            -- Glockenspiel
  pluck = gm(105, { gain = 0.7, pan = 0.3 }),           -- Banjo (for the balalaika)
  stabs = gm(61, { gain = 0.6 }),                       -- Brass Section
  kick = kit(36, { gain = 0.95 }),
  snare = kit(38, { gain = 0.8 }),
  hat = kit(42, { gain = 0.55 }),
  open = kit(46, { gain = 0.5 }),
  crash = kit(49, { gain = 0.6 }),
  tom = kit(-1, { gain = 0.8 }),                        -- the fills' notes are GM tom keys
}

-- Windows FM MIDI: General MIDI instruments as two-operator FM patches
-- for the OPL3, in the character of a 1990s Sound Blaster 16 FM driver
-- playing Windows' .MID files - dry, a little buzzy, panned hard left,
-- centre or right. The port's own patches (the drivers' banks are theirs).
-- Named after the General MIDI program each one stands in for.
local function fmgm(fb, mod, car, extra)
  local t = { kind = "opl", opl = { feedback = fb, mod = mod, car = car } }
  for k, v in pairs(extra or {}) do t[k] = v end
  return t
end
local W = {
  -- 0 Acoustic Grand Piano: a bright hammer that fades.
  piano = fmgm(3, { mult = 1, tl = 27, ar = 15, dr = 3, sl = 5, rr = 4, ksl = 1, sustain = 0 },
                  { mult = 1, tl = 0, ar = 15, dr = 2, sl = 9, rr = 5, ksr = 1, sustain = 0 }),
  -- 4 Electric Piano 1: the bell-like tine.
  epiano = fmgm(1, { mult = 1, tl = 34, ar = 15, dr = 4, sl = 15, rr = 6, sustain = 0 },
                   { mult = 1, tl = 2, ar = 15, dr = 3, sl = 8, rr = 5, sustain = 0 }),
  -- 11 Vibraphone, 9 Glockenspiel.
  vibes = fmgm(0, { mult = 4, tl = 36, ar = 15, dr = 5, sl = 15, rr = 5, am = 1 },
                  { mult = 1, tl = 2, ar = 15, dr = 4, sl = 9, rr = 4, am = 1, sustain = 0 }),
  glock = fmgm(0, { mult = 7, tl = 30, ar = 15, dr = 6, sl = 12, rr = 6, sustain = 0 },
                  { mult = 4, tl = 4, ar = 15, dr = 5, sl = 12, rr = 5, sustain = 0 }),
  -- 21 Accordion: two operators heard, with tremolo.
  accordion = fmgm(3, { mult = 2, tl = 16, ar = 13, dr = 1, sl = 1, rr = 7, am = 1 },
                      { mult = 1, tl = 6, ar = 13, dr = 1, sl = 1, rr = 7, am = 1 }, { additive = 1 }),
  -- 24 Nylon Guitar, 29 Overdriven Guitar, 105 Banjo.
  guitar = fmgm(4, { mult = 3, tl = 30, ar = 15, dr = 5, sl = 10, rr = 6, sustain = 0 },
                   { mult = 1, tl = 3, ar = 15, dr = 4, sl = 10, rr = 6, sustain = 0 }),
  dist = fmgm(7, { mult = 1, tl = 16, ar = 15, dr = 2, sl = 3, rr = 6 },
                 { mult = 1, tl = 0, ar = 15, dr = 2, sl = 3, rr = 6, ws = 1 }),
  banjo = fmgm(5, { mult = 4, tl = 32, ar = 15, dr = 7, sl = 13, rr = 6, sustain = 0 },
                  { mult = 1, tl = 3, ar = 15, dr = 6, sl = 12, rr = 6, sustain = 0 }),
  -- 33 Electric Bass (finger), 38 Synth Bass 1.
  bass = fmgm(4, { mult = 0, tl = 28, ar = 15, dr = 5, sl = 6, rr = 8 },
                 { mult = 1, tl = 0, ar = 15, dr = 3, sl = 5, rr = 8 }),
  synbass = fmgm(6, { mult = 1, tl = 22, ar = 15, dr = 7, sl = 8, rr = 8 },
                    { mult = 1, tl = 0, ar = 15, dr = 4, sl = 5, rr = 8 }),
  -- 48 String Ensemble: the slow, buzzy FM strings.
  strings = fmgm(2, { mult = 1, tl = 30, ar = 6, dr = 1, sl = 2, rr = 5, vib = 1 },
                    { mult = 1, tl = 6, ar = 6, dr = 1, sl = 2, rr = 5, vib = 1, ws = 1 }),
  -- 61 Brass Section, 62 Synth Brass.
  brass = fmgm(5, { mult = 1, tl = 22, ar = 11, dr = 2, sl = 2, rr = 6, vib = 1 },
                  { mult = 1, tl = 3, ar = 12, dr = 2, sl = 2, rr = 6, vib = 1 }),
  synbrass = fmgm(6, { mult = 1, tl = 20, ar = 13, dr = 3, sl = 3, rr = 6 },
                     { mult = 1, tl = 2, ar = 14, dr = 2, sl = 3, rr = 6 }),
  -- 71 Clarinet (hollow: odd harmonics), 73 Flute (breathy sine).
  clarinet = fmgm(4, { mult = 2, tl = 26, ar = 13, dr = 2, sl = 2, rr = 6, vib = 1 },
                     { mult = 1, tl = 2, ar = 13, dr = 1, sl = 1, rr = 6, vib = 1 }),
  flute = fmgm(2, { mult = 1, tl = 40, ar = 10, dr = 2, sl = 2, rr = 6, vib = 1 },
                  { mult = 1, tl = 4, ar = 11, dr = 1, sl = 1, rr = 6, vib = 1 }),
  -- 80 Square Lead.
  square = fmgm(6, { mult = 1, tl = 26, ar = 15, dr = 2, sl = 2, rr = 6 },
                   { mult = 1, tl = 4, ar = 15, dr = 2, sl = 2, rr = 6, ws = 6 }),
  -- Drums, as the FM drivers made them: fixed-pitch percussive patches.
  kick = fmgm(0, { mult = 0, tl = 10, ar = 15, dr = 9, sl = 15, rr = 15 },
                 { mult = 1, tl = 0, ar = 15, dr = 7, sl = 15, rr = 15, sustain = 0 }, { key = 28 }),
  snare = fmgm(7, { mult = 15, tl = 0, ar = 15, dr = 8, sl = 15, rr = 15 },
                  { mult = 1, tl = 4, ar = 15, dr = 7, sl = 15, rr = 15, sustain = 0 }, { key = 62 }),
  hat = fmgm(7, { mult = 15, tl = 0, ar = 15, dr = 10, sl = 15, rr = 15 },
                { mult = 15, tl = 12, ar = 15, dr = 10, sl = 15, rr = 15, sustain = 0 }, { key = 96, pan = 1 }),
  open = fmgm(7, { mult = 15, tl = 0, ar = 15, dr = 7, sl = 15, rr = 7 },
                 { mult = 15, tl = 12, ar = 15, dr = 7, sl = 15, rr = 7, sustain = 0 }, { key = 96, pan = 1 }),
  crash = fmgm(7, { mult = 15, tl = 0, ar = 15, dr = 4, sl = 15, rr = 5 },
                  { mult = 13, tl = 8, ar = 15, dr = 4, sl = 15, rr = 5, sustain = 0 }, { key = 90, pan = -1 }),
  tom = fmgm(0, { mult = 0, tl = 20, ar = 15, dr = 8, sl = 15, rr = 15 },
                { mult = 1, tl = 0, ar = 15, dr = 6, sl = 15, rr = 15, sustain = 0 }),
}
-- The roles the songs use, mapped onto that bank (songs can recast them
-- in music/styles.lua), with the hard panning of an OPL3 MIDI mix.
local function cast(name, extra) local t = with(W[name], extra or {}) return t end
I.winfm = {
  lead = cast("clarinet", { gain = 0.95 }),
  harmony = cast("strings", { gain = 0.55, pan = -1 }),
  comp = cast("piano", { gain = 0.5, pan = 1 }),
  arp = cast("piano", { gain = 0.45, pan = 1 }),
  epiano = cast("epiano", { gain = 0.55, pan = 1 }),
  bass = cast("bass", { gain = 0.8 }),
  pad = cast("strings", { gain = 0.5, pan = -1 }),
  accordion = cast("accordion", { gain = 0.9 }),
  bells = cast("glock", { gain = 0.5, pan = 1 }),
  pluck = cast("banjo", { gain = 0.6, pan = -1 }),
  kick = W.kick, snare = with(W.snare, { gain = 0.85 }), hat = with(W.hat, { gain = 0.7 }),
  open = with(W.open, { gain = 0.6 }), crash = with(W.crash, { gain = 0.6 }), tom = W.tom,
  -- Extra casts for particular songs.
  brass = cast("synbrass", { gain = 0.9 }),
  brass_h = cast("brass", { gain = 0.55, pan = -1 }),
  flute = cast("flute", { gain = 0.9 }),
  vibes = cast("vibes", { gain = 1.0 }),
  guitar = cast("guitar", { gain = 0.6, pan = 1 }),
  dist = cast("dist", { gain = 1.0 }),
  square = cast("square", { gain = 0.8 }),
  synbass = cast("synbass", { gain = 0.85 }),
  piano_lead = cast("piano", { gain = 0.85 }),
  clarinet_lead = cast("clarinet", { gain = 0.95 }),
}

return I
