# Stankface

A morphing wavetable synthesiser built around a portable DSP core, tuned for UK
bass, dubstep, grime and jungle sound design.

The engine is plain C++ with no framework dependency. A JUCE wrapper turns it
into a VST3/AU plugin; the same engine is driven directly by the test suite and
by an offline renderer, with no host involved in either case.

## Why the core is separate

The entire public surface of the DSP engine is five methods:

```cpp
void setSampleRate(double sampleRate);
void noteOn(int midiNote, float velocity);
void noteOff(int midiNote);
void setParam(ParamId id, float value);
void renderBlock(float* output, int numSamples);
```

Everything host-specific — plugin format boilerplate, parameter objects, MIDI
decoding, GUI — lives in the wrapper. Nothing in `engine/` includes a JUCE
header or knows what a plugin is.

That split buys three things:

- **The DSP is testable without a host.** The suite in `engine/tests` feeds the
  engine note and parameter calls and asserts on the buffers that come back.
  Checking that the oscillator does not alias is an FFT over a rendered block,
  not a listening test in a DAW.
- **A second wrapper is additive, not a rewrite.** A WASM/Web Audio build is a
  plausible next target and would link the same engine unchanged. It is
  explicitly not in the current scope.
- **Host quirks stay out of the DSP.** Plugin formats disagree about buffer
  layouts, parameter ranges and threading. That belongs in one place, not
  smeared through the audio path.

The cost is real and worth naming: no host niceties inside the engine. No
`AudioBuffer`, no parameter smoothing helpers, no MIDI classes. Those get
written by hand or done without.

## Layout

```
engine/          platform-agnostic C++ DSP core
  include/       public headers
  src/           implementation, plus generated wavetable data
  tests/         engine tests -- no host, no plugin, no audio device
wrappers/
  juce-plugin/   VST3/AU/Standalone wrapper
ui/              JUCE control surface for the plugin
wavetables/      wavetable generation script
tools/           offline renderer, for listening without a DAW
docs/            write-ups and demo audio
```

## Building

The engine, its tests and the offline renderer need only CMake and a C++17
compiler:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

Run the tests:

```bash
./build/engine/tests/engine_tests
```

Render the demo patch to a WAV without opening a DAW:

```bash
./build/tools/render_demo demo.wav
```

The plugin is off by default, because configuring it downloads JUCE:

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DSTANKFACE_BUILD_PLUGIN=ON && cmake --build build-plugin
```

Add `-DSTANKFACE_INSTALL_PLUGIN=ON` to have the build copy the results into the
system plug-in folders. It is off by default so that a build never writes
outside the source tree unasked.

The wavetable data in `engine/src/WavetableData.gen.cpp` is generated but
committed, so building needs no Python. Regenerate it after editing the
spectra with:

```bash
python3 wavetables/generate_wavetables.py
```

## What the engine does

- **Three wavetables**, each eight frames, synthesised additively from harmonic
  spectra: `SubSaw` (sine through to sawtooth), `Reese` (sawtooth through a
  sweeping comb), `Growl` (sawtooth with three sweeping formant peaks).
- **Morphing** across frames from a single `WavetablePosition` parameter.
- **Band-limited mipmapping** so that hard pitch and position modulation does
  not alias. Worst-case non-harmonic content measures around −82 dB across
  every table, position and register the tests cover. See
  [docs/wavetables-and-aliasing.md](docs/wavetables-and-aliasing.md).
- **A saturating resonant lowpass** — a TPT state variable filter with a drive
  stage into it and a saturator on the resonant integrator.
- **One LFO**, routable to wavetable position and filter cutoff.
- **An analogue-style ADSR** on amplitude.
- **A monophonic voice** with last-note priority.

## Tuning for the genre

The choices that are about the target sound rather than about correctness:

- **The filter is doing the work, not the oscillator.** The drive stage sits in
  front of the filter and the resonant integrator saturates, so pushing
  resonance compresses rather than rings on cleanly. Most of the character
  people hear as "UK bass" is that pair, not the wavetable.
- **Signal chain is oscillator → filter → amplifier**, in that order. Putting
  the envelope before the filter meant quiet notes hit the drive stage softly
  and got pushed back up by its makeup gain, which flattened velocity out and
  made note tails swell.
- **Drive is compensated at a realistic level**, trimmed so that a signal near
  full scale keeps its level across the whole range of the control. Quieter
  material still gets pushed, which is what a drive stage should do.
- **Frame 0 of every table is close to a pure sine**, so the bottom of the
  morph is a usable sub without a separate oscillator. A dedicated sub layer is
  on the list; until then the morph covers it.
- **The mip levels are generous at the bottom.** Level 0 carries 512 harmonics,
  which reaches 20 kHz on a 40 Hz note, so bass notes are not quietly dulled by
  the anti-aliasing.
- **`Growl` exists because wobbles need formants.** Sweeping resonant peaks
  read as a vowel in a way a plain lowpass sweep does not.

## Demo

[`docs/stankface-demo.wav`](docs/stankface-demo.wav) — eight seconds, rendered
offline by `tools/render_demo`, one section per wavetable, each a held bass note
with the LFO on position and cutoff.

## State

The engine is complete against the MVP and tested. The JUCE wrapper builds; it
has not yet been through a DAW, so treat "loads in Ableton" as unverified.

Planned next, roughly in order of impact: polyphony (a voice pool behind the
same interface), a dedicated sub-oscillator layer, unison/detune, a second
wavetable oscillator, and a general modulation matrix in place of the current
hardcoded LFO routing. A WASM/Web Audio wrapper is a plausible later target and
is the reason the core has no framework dependency, but it is not scoped.
