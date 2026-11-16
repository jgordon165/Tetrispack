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
  tom = fm(0, 2, {
    { mul = 0, tl = 30, ar = 31, d1r = 14, d1l = 15, rr = 15 },
    { mul = 0, tl = 30, ar = 31, d1r = 14, d1l = 15, rr = 15 },
    { mul = 1, tl = 24, ar = 31, d1r = 14, d1l = 15, rr = 15 },
    { mul = 1, tl = 0, ar = 31, d1r = 10, d1l = 15, rr = 15 },
  }, { gain = 0.6 }),
}

-- Super NES: eight sampled voices with echo --------------------------------
local function s(wave, env, extra)
  local t = { kind = "sample", wave = wave, env = env }
  for k, v in pairs(extra or {}) do t[k] = v end
  return t
end
local drum = { attack = 0.001, decay = 10, sustain = 1, release = 0.4 }
I.snes = {
  lead = s("pulse25", { attack = 0.004, decay = 0.5, sustain = 0.7, release = 0.15 }, { vibrato = 0.25, echo = 0.5, gain = 0.8 }),
  harmony = s("brass", { attack = 0.01, decay = 0.5, sustain = 0.65, release = 0.15 }, { gain = 0.42, pan = -0.45, echo = 0.45 }),
  arp = s("pulse12", { attack = 0.002, decay = 0.12, sustain = 0.25, release = 0.05 }, { gain = 0.38, pan = 0.5, echo = 0.4 }),
  epiano = s("epiano", { attack = 0.003, decay = 0.9, sustain = 0.3, release = 0.3 }, { gain = 0.55, echo = 0.5, pan = 0.25 }),
  bass = s("saw", { attack = 0.002, decay = 0.3, sustain = 0.55, release = 0.06 }, { gain = 0.75 }),
  pad = false,
  accordion = s("organ", { attack = 0.02, decay = 0.3, sustain = 0.8, release = 0.12 }, { vibrato = 0.15, echo = 0.4, gain = 0.6 }),
  bells = s("bell", { attack = 0.001, decay = 0.7, sustain = 0, release = 0.4 }, { gain = 0.4, pan = 0.4, echo = 0.7 }),
  pluck = s("pluck", { attack = 0.001, decay = 10, sustain = 1, release = 0.2 }, { gain = 0.6, pan = 0.35, echo = 0.35 }),
  kick = s("kick", drum, { key = 60, gain = 0.95 }),
  snare = s("snare", drum, { key = 60, gain = 0.7, echo = 0.25 }),
  hat = s("hat", drum, { key = 60, gain = 0.45, pan = 0.3 }),
  crash = s("crash", drum, { key = 60, gain = 0.5, pan = -0.3, echo = 0.3 }),
  tom = s("tom", drum, { gain = 0.75, echo = 0.2 }),
}

return I
