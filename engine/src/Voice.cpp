#include "stankface/Voice.h"

#include <cmath>

namespace stankface {
namespace {

float midiNoteToHz(int midiNote)
{
    return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f);
}

} // namespace

void Voice::setSampleRate(double sampleRate)
{
    osc_.setSampleRate(sampleRate);
    filter_.setSampleRate(sampleRate);
    ampEnv_.setSampleRate(sampleRate);
    modEnv_.setSampleRate(sampleRate);
}

void Voice::reset()
{
    osc_.reset();
    filter_.reset();
    ampEnv_.reset();
    modEnv_.reset();
    note_ = -1;
}

void Voice::setTable(int tableIndex)     { osc_.setTable(tableIndex); }
void Voice::setCutoff(float hz)          { filter_.setCutoff(hz); }
void Voice::setResonance(float resonance){ filter_.setResonance(resonance); }
void Voice::setDrive(float drive)        { filter_.setDrive(drive); }

void Voice::setAmpAttack(float seconds)  { ampEnv_.setAttack(seconds); }
void Voice::setAmpDecay(float seconds)   { ampEnv_.setDecay(seconds); }
void Voice::setAmpSustain(float level)   { ampEnv_.setSustain(level); }
void Voice::setAmpRelease(float seconds) { ampEnv_.setRelease(seconds); }

void Voice::setEnvAttack(float seconds)  { modEnv_.setAttack(seconds); }
void Voice::setEnvDecay(float seconds)   { modEnv_.setDecay(seconds); }
void Voice::setEnvSustain(float level)   { modEnv_.setSustain(level); }
void Voice::setEnvRelease(float seconds) { modEnv_.setRelease(seconds); }

void Voice::retune(int midiNote)
{
    note_ = midiNote;
    osc_.setFrequency(midiNoteToHz(midiNote));
}

void Voice::noteOn(int midiNote, float velocity, bool restartPhase)
{
    retune(midiNote);
    velocity_ = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);

    if (restartPhase)
    {
        osc_.resetPhase();

        // Clear whatever the previous note left ringing in the integrators, so
        // this one starts from silence. Only when starting fresh: a voice
        // being taken over mid-note wants that state kept, or the handover
        // clicks.
        filter_.reset();
    }

    ampEnv_.noteOn();
    modEnv_.noteOn();
}

void Voice::noteOff()
{
    ampEnv_.noteOff();
    modEnv_.noteOff();
}

float Voice::nextSample(const Modulation& mod)
{
    if (!ampEnv_.isActive())
        return 0.0f;

    const float env = modEnv_.nextSample();

    float position = mod.basePosition
                   + mod.lfo * mod.lfoToPosition
                   + env * mod.envToPosition;
    position = position < 0.0f ? 0.0f : (position > 1.0f ? 1.0f : position);

    // Both sources add in octaves before being applied, so a given depth moves
    // the cutoff by the same musical interval wherever the knob is set, and the
    // two routes combine the way a player would expect rather than one scaling
    // the other. The filter clamps the result to something it can run at.
    const float octaves = (mod.lfo * mod.lfoToCutoff + env * mod.envToCutoff)
                        * kCutoffModOctaves;
    const float cutoff = mod.baseCutoff * std::exp2(octaves);

    lastPosition_ = position;
    lastCutoff_ = cutoff;

    osc_.setPosition(position);
    filter_.setCutoff(cutoff);

    // Oscillator into filter into amplifier, in that order. Putting the
    // envelope after the filter rather than before it matters here because the
    // filter saturates: driving it with an already-enveloped signal would mean
    // quiet notes hit the drive stage softly and get pushed back up by its
    // makeup gain, which flattens out velocity and makes note tails swell.
    const float filtered = filter_.process(osc_.nextSample());

    return filtered * ampEnv_.nextSample() * velocity_;
}

} // namespace stankface
