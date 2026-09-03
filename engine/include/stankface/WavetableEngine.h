#pragma once

#include "stankface/Envelope.h"
#include "stankface/Filter.h"
#include "stankface/Lfo.h"
#include "stankface/Params.h"
#include "stankface/WavetableOscillator.h"

namespace stankface {

/** The whole synthesiser, behind five methods.

    Deliberately the entire public surface of the DSP core: sample rate, note
    on/off, parameter set, render. Nothing here knows what a plugin is, so a
    wrapper for any host -- or a test harness with no host at all -- drives it
    the same way.

    Monophonic for now, with last-note priority: playing a new note while one
    is held steals the voice, and releasing it hands the voice back to whatever
    is still down. Polyphony is a voice-pool change behind this same interface.
*/
class WavetableEngine
{
public:
    WavetableEngine();

    void setSampleRate(double sampleRate);

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);

    /** Sets a parameter in natural units. See ParamId. */
    void setParam(ParamId id, float value);
    float getParam(ParamId id) const;

    /** Renders mono into `output`, overwriting it. Allocation-free; safe to
        call from an audio thread. */
    void renderBlock(float* output, int numSamples);

    /** Silences the voice and clears filter/envelope state. */
    void reset();

    /** True while the voice is sounding. */
    bool isActive() const { return ampEnv_.isActive(); }

private:
    static constexpr int kMaxHeldNotes = 16;

    double sampleRate_ = 44100.0;
    float params_[kNumParams] = {};

    WavetableOscillator osc_;
    Filter filter_;
    Envelope ampEnv_;
    Lfo lfo_;

    // Held notes, oldest first. A stack rather than a single note so that
    // releasing the top of a trill falls back to the note still held.
    int heldNotes_[kMaxHeldNotes] = {};
    int numHeldNotes_ = 0;

    int currentNote_ = -1;
    float velocity_ = 1.0f;
    float noteFrequency_ = 440.0f;

    void applyParam(ParamId id, float value);
    void startNote(int midiNote, float velocity);
    void removeHeldNote(int midiNote);
};

} // namespace stankface
