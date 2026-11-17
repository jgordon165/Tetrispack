-- How each sound engine restyles the arrangement (the songs' melodies and
-- chords stay the same everywhere).
--
-- Super NES: an early-90s racing-game feel - a faster tempo, sixteenth-note
-- synth bass jumping octaves, power-chord stabs on the off-beats, a rock
-- beat with open hats, quick fills - played on bright saw and pulse
-- samples with the echo. The Mega Drive and the Sound Blaster keep each
-- song's own bass line and beat, as does Windows MIDI.
local function clamp(x, lo, hi) return math.max(lo, math.min(hi, x)) end

return {
  snes = {
    name = "Super NES",
    tempo = function(t) return math.floor(clamp(t * 1.06, 124, 176) + 0.5) end,
    bass = "drive",
    drums = "racer", fill = "racer", fillEvery = 4,
    stabs = { rhythm = "x..x..x...x.x...", low = 50, vol = 0.8 },
    drop = { bells = true },     -- eight voices: the melody doublings go
    echo = { feedback = 0.36, volume = 0.3 },
    -- Songs with a style of their own on the Super NES. `voices` swaps
    -- instruments (track instrument -> another), the rest overrides the above.
    songs = {
      -- An early-90s workstation house feel: the bright chorused piano on the
      -- melody, house-organ chord stabs, round bass, four on the floor.
      Korobeiniki = {
        tempo = function() return 124 end,
        bass = "house",
        drums = "house", fill = "house", fillEvery = 8,
        stabs = { rhythm = "..x..x....x..x..", low = 55, vol = 0.8, tones = "triad", inst = "organ" },
        voices = { lead = "m1piano", harmony = "m1piano_soft", bass = "m1bass" },
        drop = { bells = true, arp = true },
      },
      -- A bouncy platformer overworld: shuffle, a whistly flute, marimba,
      -- staccato bass, bongos and congas.
      Samovar = {
        tempo = function() return 152 end,
        swing = 0.64,
        bass = "oompah",
        drums = "smw", fill = "smw", fillEvery = 8,
        stabs = false,
        voices = { accordion = "flute", harmony = "marimba", pluck = "marimba", arp = "marimba_hi", bass = "bouncy" },
        drop = {},
        echo = { feedback = 0.3, volume = 0.26 },
      },
    },
  },
  genesis = { name = "Mega Drive" },
  opl3 = { name = "Sound Blaster" },
  -- General MIDI on a GS SoundFont, as Windows played .MID files through a
  -- Roland Sound Canvas or its GS wavetable synth: the song's own
  -- arrangement, with held strings, on real-instrument programs.
  midi = { name = "Windows MIDI" },
}
