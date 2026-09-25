#pragma once

#include "stankface/Envelope.h"
#include "stankface/Filter.h"
#include "stankface/WavetableOscillator.h"

namespace stankface {

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

    /** One sample, at the given wavetable position. Cutoff is set separately
        because it only needs updating per sample when something modulates it. */
    float nextSample(float position);

private:
    WavetableOscillator osc_;
    Filter filter_;
    Envelope ampEnv_;

    int note_ = -1;
    float velocity_ = 1.0f;
};

} // namespace stankface
