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
  },
  genesis = { name = "Mega Drive" },
  opl3 = { name = "Sound Blaster" },
  -- General MIDI on a GS SoundFont, as Windows played .MID files through a
  -- Roland Sound Canvas or its GS wavetable synth: the song's own
  -- arrangement, with held strings, on real-instrument programs.
  midi = { name = "Windows MIDI" },
}
