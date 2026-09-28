#pragma once

#include "stankface/Envelope.h"
#include "stankface/Filter.h"
#include "stankface/Params.h"
#include "stankface/WavetableOscillator.h"

namespace stankface {

/** Everything outside a voice that shapes its position and cutoff.

    Handed over per sample rather than pre-applied, because the modulation
    envelope is per-voice: only the voice knows where its own envelope has got
    to, so only the voice can combine it with the shared LFO. That is also why
    the engine cannot compute one position and cutoff and hand the same pair to
    everything, which is what it used to do.
*/
struct Modulation
{
    float basePosition = 0.0f;
    float baseCutoff = 1000.0f;

    float lfo = 0.0f;            ///< -1..1, shared by every voice.
    float lfoToPosition = 0.0f;
    float lfoToCutoff = 0.0f;

    float envToPosition = 0.0f;
    float envToCutoff = 0.0f;
};

/** One note's worth of signal path: oscillator into filter into amplifier.

    Each voice carries its own filter rather than sharing one across the mix.
    That costs almost nothing measurably, and it matters here because the
    filter is where the drive stage lives: voices summed ahead of a shared
    saturator would hit it harder the more notes were held, so the drive
    control's meaning would depend on how many keys were down.

    The voice knows nothing about allocation or stealing. It is told which note
    to play and when to release, and the engine decides which voice that is.
*/
class Voice
{
public:
    void setSampleRate(double sampleRate);
    void reset();

    void setTable(int tableIndex);
    void setCutoff(float hz);
    void setResonance(float resonance);
    void setDrive(float drive);

    void setAmpAttack(float seconds);
    void setAmpDecay(float seconds);
    void setAmpSustain(float level);
    void setAmpRelease(float seconds);

    void setEnvAttack(float seconds);
    void setEnvDecay(float seconds);
    void setEnvSustain(float level);
    void setEnvRelease(float seconds);

    /** Starts this voice on a note.

        `restartPhase` resets the oscillator and clears the filter. Wanted when
        the voice is starting from silence, and not wanted when it is being
        taken over from a note still sounding, where restarting would click and
        would throw away the phase a held wobble has built up. */
    void noteOn(int midiNote, float velocity, bool restartPhase);

    /** Changes pitch without disturbing the envelope, for mono legato. */
    void retune(int midiNote);

    void noteOff();

    bool isActive() const { return ampEnv_.isActive(); }
    bool isReleasing() const { return ampEnv_.stage() == Envelope::Stage::Release; }

    /** The note this voice was last started on, or -1 if it has never run. */
    int note() const { return note_; }

    /** One sample, with the modulation applied. */
    float nextSample(const Modulation& mod);

    /** Where this voice's modulation actually landed on the last sample, for a
        display to draw. Meaningless before the first sample. */
    float lastPosition() const { return lastPosition_; }
    float lastCutoff() const { return lastCutoff_; }

private:
    WavetableOscillator osc_;
    Filter filter_;

    Envelope ampEnv_;

    /** Drives position and cutoff, never amplitude. Governed by the same note
        events as the amplifier's envelope, but it does not decide when the
        voice is finished -- that stays with ampEnv_, since a voice whose
        amplifier has closed is silent whatever this is still doing. */
    Envelope modEnv_;

    int note_ = -1;
    float velocity_ = 1.0f;

    float lastPosition_ = 0.0f;
    float lastCutoff_ = 1000.0f;
};

} // namespace stankface
