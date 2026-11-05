#!/usr/bin/env python3
"""Ogg Vorbis to WAV for the Amiga drawer, so that the 68k never decodes Vorbis.

    ogg2wav.py pcm   in.ogg out.wav   16-bit PCM, mono, 22050 Hz (sound effects)
    ogg2wav.py adpcm in.ogg out.wav   IMA ADPCM, mono, 22050 Hz (music)

The host's GStreamer decodes, mixes down to mono and converts the rate
(gst-launch-1.0 with vorbisdec, audioconvert, audioresample, wavenc).
ADPCM is Microsoft's IMA ADPCM WAV (format 0x11): 1024-byte blocks of
2041 samples, each with its own predictor and step index, so the game's
mixer (amiga/sdl_mixer_lite.c) decodes it as it plays and seeks to a block.

The AmiCradle Team. GPL 3 or later, like Wizznic.
"""
import os
import struct
import subprocess
import sys
import tempfile
import wave

RATE = 22050
BLOCK_ALIGN = 1024
SPB = (BLOCK_ALIGN - 4) * 2 + 1  # 2041 samples a block

STEPS = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def decode(src, dst):
    subprocess.run(
        ["gst-launch-1.0", "-q", "filesrc", "location=" + src, "!", "oggdemux", "!",
         "vorbisdec", "!", "audioconvert", "!", "audioresample", "!",
         "audio/x-raw,format=S16LE,channels=1,rate=%d" % RATE, "!", "wavenc", "!",
         "filesink", "location=" + dst],
        check=True)
    with wave.open(dst, "rb") as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == RATE
        data = w.readframes(w.getnframes())
    return struct.unpack("<%dh" % (len(data) // 2), data)


def write_pcm(samples, out):
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(struct.pack("<%dh" % len(samples), *samples))


def write_adpcm(samples, out):
    n = len(samples)
    blocks = (n + SPB - 1) // SPB
    body = bytearray()
    index = 0
    for b in range(blocks):
        base = b * SPB
        pred = samples[base] if base < n else 0
        body += struct.pack("<hBB", pred, index, 0)
        nib = bytearray(BLOCK_ALIGN - 4)
        for k in range(1, SPB):
            s = samples[base + k] if base + k < n else 0
            step = STEPS[index]
            diff = s - pred
            code = 0
            if diff < 0:
                code = 8
                diff = -diff
            vp = step >> 3
            if diff >= step:
                code |= 4
                diff -= step
                vp += step
            step >>= 1
            if diff >= step:
                code |= 2
                diff -= step
                vp += step
            step >>= 1
            if diff >= step:
                code |= 1
                vp += step
            pred = pred - vp if code & 8 else pred + vp
            pred = 32767 if pred > 32767 else -32768 if pred < -32768 else pred
            index += INDEX[code]
            index = 0 if index < 0 else 88 if index > 88 else index
            j = k - 1
            if j & 1:
                nib[j >> 1] |= code << 4
            else:
                nib[j >> 1] = code
        body += nib
    fmt = struct.pack("<HHIIHHHH", 0x11, 1, RATE, RATE * BLOCK_ALIGN // SPB, BLOCK_ALIGN, 4, 2, SPB)
    fact = struct.pack("<I", n)
    riff = (b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt +
            b"fact" + struct.pack("<I", 4) + fact + b"data" + struct.pack("<I", len(body)) + bytes(body))
    with open(out, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(riff)) + riff)


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("pcm", "adpcm"):
        sys.exit("usage: ogg2wav.py pcm|adpcm in.ogg out.wav")
    mode, src, out = sys.argv[1:]
    fd, tmp = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        samples = decode(src, tmp)
    finally:
        os.unlink(tmp)
    (write_adpcm if mode == "adpcm" else write_pcm)(samples, out)
    print("%s: %.1f s, %s" % (os.path.basename(out), len(samples) / RATE, mode))


if __name__ == "__main__":
    main()
