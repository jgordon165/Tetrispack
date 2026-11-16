-- The instruments: two-operator FM patches for the OPL3, written for the
-- port in the spirit of a 1990 Sound Blaster General MIDI bank.
-- Fields per operator (OPL registers): mult (frequency multiple, 0 = 1/2),
-- tl (attenuation, 0 loudest - 63), ar / dr / rr (attack, decay, release
-- rates 0-15), sl (sustain level 0 loud - 15), ws (waveform 0-7: sine,
-- half-sine, abs-sine, pulse-sine, ... 6 square), sustain (hold at sl while
-- the key is down), vib / am (vibrato / tremolo).
-- feedback 0-7 brightens the modulator; additive = both operators heard.
-- key fixes a drum to one pitch.
return {
  -- A reedy square-wave lead, the Game Boy-ish voice of the melodies.
  lead = { feedback = 5,
    mod = { mult = 2, tl = 34, ar = 15, dr = 3, sl = 2, rr = 6, vib = 1 },
    car = { mult = 1, tl = 3, ar = 14, dr = 2, sl = 3, rr = 6, vib = 1 } },

  -- Electric piano: a bright attack that mellows.
  epiano = { feedback = 2,
    mod = { mult = 1, tl = 30, ar = 15, dr = 5, sl = 8, rr = 6, ksl = 1 },
    car = { mult = 1, tl = 6, ar = 15, dr = 3, sl = 7, rr = 6, sustain = 0 } },

  -- Bass: round with a little bite.
  bass = { feedback = 4,
    mod = { mult = 0, tl = 26, ar = 15, dr = 5, sl = 5, rr = 8 },
    car = { mult = 1, tl = 0, ar = 15, dr = 3, sl = 4, rr = 8 } },

  -- Slow strings pad.
  strings = { feedback = 3,
    mod = { mult = 1, tl = 38, ar = 6, dr = 2, sl = 2, rr = 5, vib = 1 },
    car = { mult = 1, tl = 10, ar = 5, dr = 1, sl = 2, rr = 5, vib = 1 } },

  -- Accordion: two operators heard together, with tremolo.
  accordion = { feedback = 3, additive = 1,
    mod = { mult = 2, tl = 14, ar = 13, dr = 1, sl = 1, rr = 7, am = 1 },
    car = { mult = 1, tl = 6, ar = 13, dr = 1, sl = 1, rr = 7, am = 1 } },

  -- Glockenspiel / music box.
  bells = { feedback = 0,
    mod = { mult = 7, tl = 32, ar = 15, dr = 6, sl = 10, rr = 6, sustain = 0 },
    car = { mult = 3, tl = 6, ar = 15, dr = 5, sl = 12, rr = 5, sustain = 0 } },

  -- Plucked strings (balalaika).
  pluck = { feedback = 3,
    mod = { mult = 3, tl = 34, ar = 15, dr = 7, sl = 12, rr = 6, sustain = 0 },
    car = { mult = 1, tl = 4, ar = 15, dr = 6, sl = 10, rr = 6, sustain = 0 } },

  -- Drums.
  kick = { feedback = 0, key = 28,
    mod = { mult = 0, tl = 10, ar = 15, dr = 9, sl = 15, rr = 15 },
    car = { mult = 1, tl = 0, ar = 15, dr = 7, sl = 15, rr = 15, sustain = 0 } },
  snare = { feedback = 7, key = 62,
    mod = { mult = 15, tl = 0, ar = 15, dr = 8, sl = 15, rr = 15 },
    car = { mult = 1, tl = 6, ar = 15, dr = 7, sl = 15, rr = 15, sustain = 0 } },
  hat = { feedback = 7, key = 96,
    mod = { mult = 15, tl = 0, ar = 15, dr = 10, sl = 15, rr = 15 },
    car = { mult = 15, tl = 14, ar = 15, dr = 10, sl = 15, rr = 15, sustain = 0 } },
}
