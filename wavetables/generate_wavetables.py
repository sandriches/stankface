#!/usr/bin/env python3
"""Generate band-limited, mipmapped wavetables as C++ source.

Each wavetable is a set of FRAMES single-cycle waveforms. Morphing between
adjacent frames is what the `wavetablePosition` parameter drives.

Every frame is stored at several mip levels. Level k contains harmonics
1..H_k. At render time the oscillator picks the brightest level whose harmonic
content still fits under Nyquist for the note being played, which is what keeps
hard position/pitch modulation from aliasing. Frames are synthesised additively,
so a level is band-limited by construction rather than by filtering after the
fact.

Levels are spaced half an octave apart rather than the obvious one octave.
The oscillator crossfades between levels so that changing pitch does not
switch band limit abruptly, and it can only ever crossfade *downwards*, into
a duller level -- fading in a brighter one would fade in the aliasing that
level was rejected for. That means the level in use is always a little darker
than strictly necessary, and halving the spacing halves how much is given up:
at worst half an octave of top end instead of a whole one. It costs about 1.7x
the memory of octave spacing, which at a few hundred KB is not a real cost.

Level k is stored at 4x its Nyquist length (len = 4 * H_k), with a floor, so
that interpolation between samples stays well under the noise floor. The
oscillator reads it with a four-point cubic, so each level is wrapped in guard
samples -- one copy of the last sample before it and two of the first two
after -- letting the read loop index i-1 through i+2 with no wrap check.

Output (both committed, so building needs no Python):
  engine/include/stankface/WavetableData.h
  engine/src/WavetableData.gen.cpp

Usage: python3 wavetables/generate_wavetables.py
"""

import math
import os

# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------

FRAMES = 8
MAX_HARMONICS = 512          # level 0; 512 harmonics of a 40 Hz note reaches 20 kHz
MIN_LENGTH = 256             # floor for the top levels, where 4 * H_k gets tiny
OVERSAMPLE = 4               # stored length = OVERSAMPLE * H_k
MIP_RATIO = math.sqrt(0.5)   # half an octave of harmonics per level

MIP_HARMONICS = []
_h = float(MAX_HARMONICS)
while True:
    n = max(1, int(round(_h)))
    if MIP_HARMONICS and n >= MIP_HARMONICS[-1]:
        n = MIP_HARMONICS[-1] - 1   # rounding collapsed two levels together
    if n < 1:
        break
    MIP_HARMONICS.append(n)
    if n == 1:
        break
    _h *= MIP_RATIO
NUM_MIPS = len(MIP_HARMONICS)

MIP_LENGTH = [max(OVERSAMPLE * hh, MIN_LENGTH) for hh in MIP_HARMONICS]
# +1 guard sample per level
# Each level is stored as [x[-1], x[0..len-1], x[0], x[1]]; the offset recorded
# is of x[0], so the oscillator's four-point read can index from -1 to +2.
GUARD_BEFORE = 1
GUARD_AFTER = 2

MIP_OFFSET = []
_off = 0
for L in MIP_LENGTH:
    MIP_OFFSET.append(_off + GUARD_BEFORE)
    _off += L + GUARD_BEFORE + GUARD_AFTER
FRAME_STRIDE = _off
TABLE_STRIDE = FRAME_STRIDE * FRAMES


# ---------------------------------------------------------------------------
# Spectra
#
# Each table is a function of the morph position t in [0, 1] returning
# (amplitude, phase) for harmonic n. Amplitudes are relative; each frame is
# peak-normalised afterwards.
# ---------------------------------------------------------------------------

def spectrum_sub_saw(t, n):
    """Sine to sawtooth.

    A 1/n^p spectrum with the exponent swept from very steep (only the
    fundamental survives) down to 1 (a textbook saw). This is the workhorse
    table: the low end of the morph is a clean sub, the top end has enough
    harmonic content to bite through a mix once it hits the drive stage.
    """
    p = 1.0 + (1.0 - t) ** 1.5 * 14.0
    return 1.0 / (n ** p), 0.0


def spectrum_reese(t, n):
    """Sawtooth through a sweeping comb, with Schroeder phases.

    Comb notches approximate the cancellation you get from detuned saws
    beating against each other, which is the core of a Reese bass. Schroeder
    (quadratic) phases flatten the crest factor, so the frame stays loud after
    peak normalisation instead of being dominated by one spike.
    """
    d = 0.03 + t * 0.45
    amp = (1.0 / n) * abs(math.sin(math.pi * n * d))
    phase = -math.pi * n * n / MAX_HARMONICS
    return amp, phase


def spectrum_growl(t, n):
    """Sawtooth with three resonant formant peaks that sweep with the morph.

    Sweeping formants rather than a plain lowpass is what reads as a vowel /
    growl when the LFO drives position, which is the usual wobble trick.
    """
    base = 1.0 / n
    centres = (4.0 + t * 5.0, 12.0 + t * 11.0, 26.0 + t * 15.0)
    widths = (2.0, 5.0, 9.0)
    gains = (9.0, 6.0, 4.0)
    boost = 1.0
    for c, w, g in zip(centres, widths, gains):
        boost += g * math.exp(-(((n - c) / w) ** 2))
    return base * boost, 0.0


TABLES = [
    ("SubSaw", spectrum_sub_saw),
    ("Reese", spectrum_reese),
    ("Growl", spectrum_growl),
]


# ---------------------------------------------------------------------------
# Synthesis
# ---------------------------------------------------------------------------

def synthesise(spectrum_fn, t, length, num_harmonics):
    """Additive synthesis of one cycle.

    Uses a sine lookup of exactly `length` entries indexed by (n * i) mod
    length. Because the table length is an integer number of cycles for every
    harmonic, that index is exact -- no phase drift, no accumulated error, and
    much faster than calling sin() per harmonic per sample.
    """
    sine = [math.sin(2.0 * math.pi * i / length) for i in range(length)]
    cosine = [math.cos(2.0 * math.pi * i / length) for i in range(length)]
    out = [0.0] * length

    for n in range(1, num_harmonics + 1):
        amp, phase = spectrum_fn(t, n)
        if amp < 1e-9:
            continue
        # amp * sin(w + phase) = (amp cos phase) sin w + (amp sin phase) cos w
        a_sin = amp * math.cos(phase)
        a_cos = amp * math.sin(phase)
        step = n % length
        idx = 0
        for i in range(length):
            out[i] += a_sin * sine[idx] + a_cos * cosine[idx]
            idx += step
            if idx >= length:
                idx -= length
    return out


def build_table(spectrum_fn):
    """All frames, all mip levels, peak-normalised per frame."""
    frames = []
    for f in range(FRAMES):
        t = f / (FRAMES - 1)
        levels = [synthesise(spectrum_fn, t, MIP_LENGTH[k], MIP_HARMONICS[k])
                  for k in range(NUM_MIPS)]
        # One scalar for the whole frame, taken from level 0. Normalising each
        # level independently would make the oscillator jump in level as it
        # crossed a mip boundary.
        peak = max(abs(v) for v in levels[0]) or 1.0
        gain = 1.0 / peak
        frames.append([[v * gain for v in lv] for lv in levels])
    return frames


# ---------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------

BANNER = """// Generated by wavetables/generate_wavetables.py -- do not edit by hand.
"""


def emit_header(path):
    lines = [BANNER, "#pragma once", "", "namespace stankface {", ""]
    lines += [
        "// Wavetable storage layout. See wavetables/generate_wavetables.py.",
        f"inline constexpr int kNumWavetables  = {len(TABLES)};",
        f"inline constexpr int kNumFrames      = {FRAMES};",
        f"inline constexpr int kNumMipLevels   = {NUM_MIPS};",
        f"inline constexpr int kFrameStride    = {FRAME_STRIDE};",
        f"inline constexpr int kTableStride    = {TABLE_STRIDE};",
        "",
        "// Harmonics 1..kMipHarmonics[k] are present at level k.",
        "inline constexpr int kMipHarmonics[kNumMipLevels] = { "
        + ", ".join(str(v) for v in MIP_HARMONICS) + " };",
        "",
        "// Samples per cycle at level k. Each level is stored wrapped in guard",
        "// samples -- one before, two after -- so the oscillator's four-point",
        "// read needs no wrap check.",
        "inline constexpr int kMipLength[kNumMipLevels] = { "
        + ", ".join(str(v) for v in MIP_LENGTH) + " };",
        "",
        "// Offset of level k within a frame.",
        "inline constexpr int kMipOffset[kNumMipLevels] = { "
        + ", ".join(str(v) for v in MIP_OFFSET) + " };",
        "",
        "extern const float kWavetableData[kNumWavetables * kTableStride];",
        "extern const char* const kWavetableNames[kNumWavetables];",
        "",
        "// Sample 0 of one frame's mip level. Indices -1 to length + 1 are",
        "// readable, courtesy of the guard samples.",
        "inline const float* wavetableFrame(int table, int frame, int mip)",
        "{",
        "    return kWavetableData + table * kTableStride",
        "                          + frame * kFrameStride",
        "                          + kMipOffset[mip];",
        "}",
        "",
        "} // namespace stankface",
        "",
    ]
    with open(path, "w") as fh:
        fh.write("\n".join(lines))


def format_float(v):
    """C++ float literal.

    %g drops the decimal point on whole numbers, and a bare `0f` or `1f` is not
    a valid literal, so make sure there is always a point or an exponent.
    """
    text = f"{v:.7g}"
    if "e" not in text and "." not in text:
        text += ".0"
    return text + "f"


def emit_source(path, tables):
    with open(path, "w") as fh:
        fh.write(BANNER)
        fh.write('\n#include "stankface/WavetableData.h"\n\n')
        fh.write("namespace stankface {\n\n")
        fh.write("const char* const kWavetableNames[kNumWavetables] = {\n")
        for name, _ in TABLES:
            fh.write(f'    "{name}",\n')
        fh.write("};\n\n")
        fh.write("const float kWavetableData[kNumWavetables * kTableStride] = {\n")
        col = 0
        for (name, _), frames in zip(TABLES, tables):
            fh.write(f"    // ---- {name} ----\n")
            for f, levels in enumerate(frames):
                for k, lv in enumerate(levels):
                    fh.write(f"    // {name} frame {f} mip {k} "
                             f"({MIP_HARMONICS[k]} harmonics, {MIP_LENGTH[k]} samples)\n")
                    vals = [lv[-1]] + lv + [lv[0], lv[1]]  # guard samples
                    col = 0
                    fh.write("    ")
                    for v in vals:
                        fh.write(format_float(v) + ", ")
                        col += 1
                        if col % 8 == 0:
                            fh.write("\n    ")
                    if col % 8 != 0:
                        fh.write("\n")
                    else:
                        fh.write("\n")
        fh.write("};\n\n} // namespace stankface\n")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    header = os.path.join(root, "engine", "include", "stankface", "WavetableData.h")
    source = os.path.join(root, "engine", "src", "WavetableData.gen.cpp")

    print(f"{len(TABLES)} tables x {FRAMES} frames x {NUM_MIPS} mips")
    print(f"harmonics per level: {MIP_HARMONICS}")
    print(f"samples per level:   {MIP_LENGTH}")
    print(f"{len(TABLES) * TABLE_STRIDE} floats total")

    tables = []
    for name, fn in TABLES:
        print(f"  synthesising {name}...")
        tables.append(build_table(fn))

    os.makedirs(os.path.dirname(header), exist_ok=True)
    os.makedirs(os.path.dirname(source), exist_ok=True)
    emit_header(header)
    emit_source(source, tables)
    print(f"wrote {header}")
    print(f"wrote {source}")


if __name__ == "__main__":
    main()
