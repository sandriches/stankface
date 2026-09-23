# Stankface

A hard hitting morphing wavetable synthesiser.

The DSP core is plain C++17 with no framework dependency. A JUCE wrapper builds
it as a VST3/AU plugin. The tests and the offline renderer drive the same engine
directly, with no host involved.

## Building

The engine, its tests and the renderer need only CMake and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

```bash
./build/engine/tests/engine_tests
./build/tools/render_demo demo.wav
```

The plugin is off by default, because configuring it downloads JUCE.

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DSTANKFACE_BUILD_PLUGIN=ON && cmake --build build-plugin
```

Add `-DSTANKFACE_INSTALL_PLUGIN=ON` to copy the built plugin into the system
plug-in folders.

## What it does

- Three wavetables of eight frames each, built additively from harmonic
  spectra: `SubSaw`, `Reese` (sweeping comb) and `Growl` (sweeping formants).
  Frame 0 of each is close to a pure sine, so the bottom of the morph is a
  usable sub.
- Continuous morph across frames from a single parameter.
- Band-limited mipmapping, so hard pitch and position modulation doesn't alias.
  Worst-case non-harmonic content measures about −82 dB across every table,
  position and register the tests cover. Details in
  [docs/wavetables-and-aliasing.md](docs/wavetables-and-aliasing.md).
- A TPT state variable lowpass with a drive stage in front of it and a
  saturating resonant integrator. Most of what reads as "UK bass" comes from
  those two rather than from the wavetable.
- One LFO routable to position and cutoff, an analogue-style ADSR on amplitude,
  and a monophonic voice with last-note priority.

## Engine API

The whole public surface of the core:

```cpp
void setSampleRate(double sampleRate);
void noteOn(int midiNote, float velocity);
void noteOff(int midiNote);
void setParam(ParamId id, float value);
void renderBlock(float* output, int numSamples);
```

Plugin boilerplate, parameter objects, MIDI decoding and GUI all live in the
wrapper. Nothing under `engine/` includes a JUCE header, which is why the tests
can assert on rendered buffers instead of relying on a listen through a DAW.

## Layout

```
engine/       DSP core: include/, src/, tests/
wrappers/     JUCE VST3/AU/Standalone wrapper
ui/           plugin control surface
wavetables/   generator script
tools/        offline renderer
docs/         write-ups and demo audio
```

Wavetable data is generated but committed, so a build needs no Python.
Regenerate it with `python3 wavetables/generate_wavetables.py`.

## Demo

[`docs/stankface-demo.wav`](docs/stankface-demo.wav): eight seconds, one
section per wavetable, LFO on position and cutoff.

## State

The engine is complete against the MVP and tested. The JUCE wrapper builds but
has not been opened in a DAW yet.

Next up: polyphony, a dedicated sub-oscillator layer, unison/detune, a second
wavetable oscillator, and a modulation matrix to replace the hardcoded LFO
routing.
