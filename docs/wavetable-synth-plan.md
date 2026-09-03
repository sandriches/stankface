# Portable Wavetable Synth Engine — Project Plan

A shared-core wavetable synthesizer, morphing-style (Serum-adjacent), tuned for UK bass / dubstep / grime / jungle sound design. The MVP targets a standalone native plugin: polyphonic VST3/AU built on JUCE, wrapping a platform-agnostic DSP engine. A browser version (WASM + Web Audio) is a future nice-to-have, not part of the MVP.

## Architecture

/architecture-then:~~

```
/engine          — platform-agnostic C++ DSP core (no JUCE, no Web Audio deps)
/wrappers
  /juce-plugin   — VST3/AU wrapper, links engine via CMake (MVP target)
/ui              — JUCE control surface for the native plugin
/wavetables      — source wavetable data + generation scripts
/docs            — architecture notes, DSP write-ups, demo audio
```

The engine exposes only sample-rate, note on/off, parameter set, and `renderBlock()`. Everything host-specific (plugin format boilerplate, audio graph wiring, GUI) lives in the wrapper. This keeps the door open for a WASM/browser wrapper later, but that is explicitly not MVP scope.

## MVP (Step 1)

Goal: a real, playable, demo-able instrument — native plugin and browser version both working — before adding anything beyond this.

**Engine core**
- [ ] `WavetableEngine` class: `setSampleRate`, `noteOn`, `noteOff`, `setParam`, `renderBlock`
- [ ] 2–3 hand-picked or generated wavetables (bass-focused waveshapes)
- [ ] Mipmapped/band-limited frames per wavetable to avoid aliasing at high modulation
- [ ] Morphing between wavetable frames via a single `wavetablePosition` parameter
- [ ] One resonant filter (lowpass, with drive/saturation stage) — this is where a lot of the "UK bass" character comes from
- [ ] One LFO, routable to `wavetablePosition` and filter cutoff
- [ ] Basic ADSR amplitude envelope
- [ ] Monophonic voice (defer polyphony to a later step)
- [ ] Unit tests for the engine in isolation (no host needed — feed it MIDI-like calls, assert on output buffers)

**Native wrapper**
- [ ] JUCE `AudioProcessor` subclass forwarding to the engine
- [ ] Minimal parameter UI (sliders/knobs, no custom graphics yet)
- [ ] Builds and loads as VST3 in Ableton (or AU if you're on Mac)

**Web wrapper**
- [ ] Emscripten build of the engine to WASM
- [ ] `AudioWorkletProcessor` loading the WASM module, real-time-safe
- [ ] Minimal HTML/TS control UI, playable via computer keyboard or on-screen keys
- [ ] Deployed somewhere link-shareable (e.g. static hosting) for portfolio use

**Docs / portfolio framing**
- [ ] README explaining the shared-core architecture decision and why
- [ ] Short write-up on the wavetable/mipmapping approach and the aliasing problem it solves
- [ ] Demo audio/video: same patch played in the plugin and in the browser, side by side
- [ ] Note on genre-specific tuning choices (sub layering, drive staging) even if not all implemented yet

## Future updates (post-MVP)

Roughly ordered by impact vs. effort — pick and choose rather than treating as a sequence:

- **Polyphony** — voice pool/stealing logic in the engine; biggest usability jump
- **Sub-oscillator layer** — dedicated sine/triangle sub with independent level, key to dubstep/grime weight
- **Unison/detune** — multiple slightly-detuned voices per note for width
- **Second wavetable oscillator** — two-oscillator morphing/mixing, more Serum-like sound design space
- **Modulation matrix** — generalize beyond hardcoded LFO→position/cutoff routing to arbitrary source→destination mapping
- **More wavetable sets + a simple table-import pipeline** (e.g. from single-cycle waveform sources)
- **Second filter type / filter FM** — many UK bass sounds use filter modulation as a primary color, not just cutoff sweeps
- **Envelope-per-parameter** (not just amplitude) — modulation envelopes for filter, wavetable position
- **Real GUI** — custom-drawn XY morph pad, wavetable visualizer (nice tie-in to your existing XY pad instrument work)
- **Preset system** — save/load patches, shareable preset format across native and web
- **MPE / pitch bend / glide** — portamento is common in bass sound design
- **Performance pass** — SIMD in the render loop, profiling under load, documented in a short write-up (good interview material on its own)

## Notes on scope discipline

The MVP list above is intentionally the ceiling for "must ship to call this done." Everything in Future Updates is legitimate scope for a `v2` branch or an ongoing "still actively developed" story in your README — which is itself a positive signal (shows sustained engagement, not a one-off weekend project) as long as the MVP stands on its own first.
