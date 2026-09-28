#pragma once

#include <cstdint>

#include "stankface/Lfo.h"
#include "stankface/Params.h"
#include "stankface/Voice.h"

namespace stankface {

/** The whole synthesiser, behind five methods.

    Deliberately the entire public surface of the DSP core: sample rate, note
    on/off, parameter set, render. Nothing here knows what a plugin is, so a
    wrapper for any host -- or a test harness with no host at all -- drives it
    the same way.

    Mono and poly are both real modes rather than one being a special case of
    the other, because mono is not a pool of size one: it retunes a held voice
    instead of starting a new one, and falls back to whatever is still down
    when the top note is released. See VoiceMode.
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

    /** Silences every voice and clears filter/envelope state. */
    void reset();

    /** True while any voice is sounding. */
    bool isActive() const;

    /** Where the modulation has actually landed, for a display to draw.

        The parameter values on their own cannot tell you this: the LFO moves
        position and cutoff per sample, so a UI drawing from the raw parameters
        would show a wave the synth is not playing.

        Written and read on the audio thread. Getting it to a UI thread safely
        is the wrapper's job, not the engine's -- thread plumbing for a
        particular host does not belong in the DSP. */
    struct DisplayState
    {
        float wavetablePosition = 0.0f; ///< After the LFO, 0..1.
        float filterCutoff = 1000.0f;   ///< After the LFO, in Hz.
    };

    DisplayState displayState() const { return { displayPosition_, displayCutoff_ }; }

private:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxHeldNotes = 16;

    double sampleRate_ = 44100.0;
    float params_[kNumParams] = {};

    // Fixed pool, never resized, so renderBlock stays allocation-free.
    Voice voices_[kMaxVoices];

    // When each voice was last started, used to pick which one to steal.
    // 64-bit so the counter cannot wrap back past a still-sounding voice.
    std::uint64_t voiceStartOrder_[kMaxVoices] = {};
    std::uint64_t nextStartOrder_ = 1;

    // One LFO for the whole instrument rather than one per voice, so that a
    // chord wobbles in lockstep instead of each note drifting against the
    // others. Per-voice modulation belongs to the modulation matrix later on,
    // where it can be a routing choice rather than a hardcoded one.
    Lfo lfo_;

    // Held notes, oldest first. A stack rather than a single note so that
    // releasing the top of a trill falls back to the note still held.
    int heldNotes_[kMaxHeldNotes] = {};
    int numHeldNotes_ = 0;

    // Mono bookkeeping. Unused in poly, where the pool tracks its own notes.
    int currentNote_ = -1;

    // The last values the LFO produced, kept only so that a display can show
    // what is actually happening. Nothing on the audio path reads them back.
    float displayPosition_ = 0.0f;
    float displayCutoff_ = 1000.0f;

    VoiceMode voiceMode() const;

    void applyParam(ParamId id, float value);
    void removeHeldNote(int midiNote);

    void monoNoteOn(int midiNote, float velocity);
    void monoNoteOff(int midiNote);
    void polyNoteOn(int midiNote, float velocity);
    void polyNoteOff(int midiNote);

    /** Picks the voice a new note should land on. Never fails: if everything
        is busy it returns one to steal. */
    int allocateVoice(int midiNote) const;

    void publishDisplayState();
};

} // namespace stankface
