// Music: the PC game's Genesis style as VGM chip logs (tools/render_music.cpp
// genesis-vgm), one XGM2 track per song. Sound effects: the PC game's, as
// 13.3 kHz samples for the XGM2 driver's PCM channels. build.sh renders all
// of them into this folder first.
XGM2 music "song1.vgm" "song2.vgm" "song3.vgm" "song4.vgm" "song5.vgm"
WAV sfx_rotate "rotate.wav" XGM2
WAV sfx_place "place.wav" XGM2
WAV sfx_clear "clear.wav" XGM2
WAV sfx_clear_big "clear_big.wav" XGM2
