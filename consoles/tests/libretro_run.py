#!/usr/bin/env python3
"""Runs a console ROM headless in a libretro core, for the SNES smoke test
(MAME's SNES needs Sony's SPC700 boot ROM, which we can't ship; the snes9x
core has its own).

  python3 -I consoles/tests/libretro_run.py <core.so> <rom> <out dir>
      --frames 4500 --snaps 300,1500,3000 [--board 7e:1234 --offsets 210,205,212]

Presses nothing, so the title screen's attract demo plays. Writes snapshots
(snap_<frame>.png), the sound (sound.wav) and, with --board, the game's
TcBoard read from work RAM (result.txt: lines, level, placed; little-endian).
Standard library only.
"""

import argparse
import ctypes as C
import os
import struct
import sys
import wave
import zlib

RETRO_ENVIRONMENT_GET_CAN_DUPE = 3
RETRO_ENVIRONMENT_SET_PIXEL_FORMAT = 10
RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY = 9
RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY = 31
RETRO_ENVIRONMENT_GET_LOG_INTERFACE = 27
RETRO_MEMORY_SYSTEM_RAM = 2
PIXEL_0RGB1555, PIXEL_XRGB8888, PIXEL_RGB565 = 0, 1, 2


class GameInfo(C.Structure):
    _fields_ = [("path", C.c_char_p), ("data", C.c_void_p), ("size", C.c_size_t), ("meta", C.c_char_p)]


class Geometry(C.Structure):
    _fields_ = [("base_width", C.c_uint), ("base_height", C.c_uint), ("max_width", C.c_uint),
                ("max_height", C.c_uint), ("aspect_ratio", C.c_float)]


class Timing(C.Structure):
    _fields_ = [("fps", C.c_double), ("sample_rate", C.c_double)]


class AvInfo(C.Structure):
    _fields_ = [("geometry", Geometry), ("timing", Timing)]


ENV = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VIDEO = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AUDIO = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
AUDIO_BATCH = C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)
POLL = C.CFUNCTYPE(None)
STATE = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)


def png(path, w, h, rgb):
    raw = b"".join(b"\0" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("core")
    ap.add_argument("rom")
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=4500)
    ap.add_argument("--snaps", default="300,1500,3000")
    ap.add_argument("--board", help="TcBoard address in work RAM, hex (7e:xxxx or 7f:xxxx)")
    ap.add_argument("--offsets", default="210,205,212")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    snaps = {int(n) for n in a.snaps.split(",") if n}

    core = C.CDLL(os.path.abspath(a.core))
    state = {"fmt": PIXEL_0RGB1555, "frame": None, "audio": bytearray()}
    sysdir = C.c_char_p(os.path.abspath(a.out).encode())

    @ENV
    def env(cmd, data):
        if cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            state["fmt"] = C.cast(data, C.POINTER(C.c_int)).contents.value
            return True
        if cmd == RETRO_ENVIRONMENT_GET_CAN_DUPE:
            C.cast(data, C.POINTER(C.c_bool)).contents.value = True
            return True
        if cmd in (RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY):
            C.cast(data, C.POINTER(C.c_char_p))[0] = sysdir.value
            return True
        return False

    @VIDEO
    def video(data, w, h, pitch):
        if data:
            state["frame"] = (C.string_at(data, pitch * h), w, h, pitch)

    @AUDIO
    def audio(l, r):
        state["audio"] += struct.pack("<hh", l, r)

    @AUDIO_BATCH
    def audio_batch(data, frames):
        state["audio"] += C.string_at(data, frames * 4)
        return frames

    @POLL
    def poll():
        pass

    @STATE
    def input_state(port, device, index, id_):
        return 0

    core.retro_set_environment(env)
    core.retro_set_video_refresh(video)
    core.retro_set_audio_sample(audio)
    core.retro_set_audio_sample_batch(audio_batch)
    core.retro_set_input_poll(poll)
    core.retro_set_input_state(input_state)
    core.retro_init()
    rom = open(a.rom, "rb").read()
    buf = C.create_string_buffer(rom, len(rom))
    info = GameInfo(os.path.abspath(a.rom).encode(), C.cast(buf, C.c_void_p), len(rom), None)
    core.retro_load_game.restype = C.c_bool
    if not core.retro_load_game(C.byref(info)):
        sys.exit("the core couldn't load the ROM")
    av = AvInfo()
    core.retro_get_system_av_info(C.byref(av))

    def snapshot(frame):
        data, w, h, pitch = state["frame"]
        rgb = bytearray(w * h * 3)
        for y in range(h):
            for x in range(w):
                o = y * pitch
                if state["fmt"] == PIXEL_XRGB8888:
                    b, g, r = data[o + x * 4], data[o + x * 4 + 1], data[o + x * 4 + 2]
                else:
                    v = data[o + x * 2] | data[o + x * 2 + 1] << 8
                    if state["fmt"] == PIXEL_RGB565:
                        r, g, b = (v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3
                    else:
                        r, g, b = ((v >> 10) & 31) << 3, ((v >> 5) & 31) << 3, (v & 31) << 3
                rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes((r, g, b))
        png(os.path.join(a.out, "snap_%d.png" % frame), w, h, bytes(rgb))

    for frame in range(1, a.frames + 1):
        core.retro_run()
        if frame in snaps and state["frame"]:
            snapshot(frame)

    with wave.open(os.path.join(a.out, "sound.wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(int(round(av.timing.sample_rate)))
        w.writeframes(bytes(state["audio"]))

    if a.board:
        core.retro_get_memory_data.restype = C.c_void_p
        core.retro_get_memory_size.restype = C.c_size_t
        ram = C.string_at(core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),
                          core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM))
        bank, addr = a.board.split(":") if ":" in a.board else ("7e", a.board)
        base = (int(bank, 16) - 0x7E) * 0x10000 + int(addr, 16)
        lines_o, level_o, placed_o = (int(n) for n in a.offsets.split(","))
        u16 = lambda o: ram[base + o] | ram[base + o + 1] << 8
        result = "lines=%d level=%d placed=%d\n" % (u16(lines_o), ram[base + level_o], u16(placed_o))
        open(os.path.join(a.out, "result.txt"), "w").write(result)
        print(result, end="")
    core.retro_unload_game()
    core.retro_deinit()


if __name__ == "__main__":
    main()
