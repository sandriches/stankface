#include "stankface/WavetableEngine.h"

#include <cmath>

#include "stankface/WavetableData.h"

namespace stankface {
namespace {

float midiNoteToHz(int midiNote)
{
    return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f);
}

} // namespace

WavetableEngine::WavetableEngine()
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        params_[i] = paramDescriptor(id).defaultValue;
        applyParam(id, params_[i]);
    }

    setSampleRate(sampleRate_);
}

void WavetableEngine::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    osc_.setSampleRate(sampleRate_);
    filter_.setSampleRate(sampleRate_);
    ampEnv_.setSampleRate(sampleRate_);
    lfo_.setSampleRate(sampleRate_);

    reset();
}

void WavetableEngine::reset()
{
    osc_.reset();
    filter_.reset();
    ampEnv_.reset();
    lfo_.reset();

    numHeldNotes_ = 0;
    currentNote_ = -1;
}

void WavetableEngine::startNote(int midiNote, float velocity)
{
    currentNote_ = midiNote;
    velocity_ = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
    noteFrequency_ = midiNoteToHz(midiNote);
    osc_.setFrequency(noteFrequency_);
}

void WavetableEngine::noteOn(int midiNote, float velocity)
{
    // A repeated note-on for a note already down should not stack up.
    removeHeldNote(midiNote);

    if (numHeldNotes_ < kMaxHeldNotes)
        heldNotes_[numHeldNotes_++] = midiNote;

    const bool wasSilent = !ampEnv_.isActive();

    startNote(midiNote, velocity);

    // Restarting the oscillator and LFO mid-note would click and would throw
    // away the phase relationship a held wobble has built up, so only do it
    // when the voice is actually starting from silence.
    if (wasSilent)
    {
        osc_.resetPhase();
        lfo_.retrigger();
    }

    ampEnv_.noteOn();
}

void WavetableEngine::removeHeldNote(int midiNote)
{
    int write = 0;
    for (int read = 0; read < numHeldNotes_; ++read)
    {
        if (heldNotes_[read] != midiNote)
            heldNotes_[write++] = heldNotes_[read];
    }
    numHeldNotes_ = write;
}

void WavetableEngine::noteOff(int midiNote)
{
    removeHeldNote(midiNote);

    if (numHeldNotes_ > 0)
    {
        // Something is still held: fall back to it rather than releasing.
        if (midiNote == currentNote_)
            startNote(heldNotes_[numHeldNotes_ - 1], velocity_);
        return;
    }

    if (midiNote == currentNote_ || currentNote_ < 0)
    {
        ampEnv_.noteOff();
        currentNote_ = -1;
    }
}

void WavetableEngine::applyParam(ParamId id, float value)
{
    switch (id)
    {
        case ParamId::WavetableSelect:
            osc_.setTable(static_cast<int>(value + 0.5f));
            break;

        case ParamId::FilterResonance:
            filter_.setResonance(value);
            break;

        case ParamId::Drive:
            filter_.setDrive(value);
            break;

        case ParamId::AmpAttack:
            ampEnv_.setAttack(value);
            break;

        case ParamId::AmpDecay:
            ampEnv_.setDecay(value);
            break;

        case ParamId::AmpSustain:
            ampEnv_.setSustain(value);
            break;

        case ParamId::AmpRelease:
            ampEnv_.setRelease(value);
            break;

        case ParamId::LfoRate:
            lfo_.setRate(value);
            break;

        case ParamId::LfoShape:
            lfo_.setShape(static_cast<LfoShape>(static_cast<int>(value + 0.5f)));
            break;

        // Read directly by renderBlock, because the LFO modulates them per
        // sample and the stored value is only the starting point.
        case ParamId::WavetablePosition:
        case ParamId::FilterCutoff:
        case ParamId::LfoToPosition:
        case ParamId::LfoToCutoff:
        case ParamId::OutputGain:
        case ParamId::NumParams:
            break;
    }
}

void WavetableEngine::setParam(ParamId id, float value)
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= kNumParams)
        return;

    const ParamDescriptor& d = paramDescriptor(id);
    const float clamped = value < d.minValue ? d.minValue
                        : (value > d.maxValue ? d.maxValue : value);

    params_[index] = clamped;
    applyParam(id, clamped);
}

float WavetableEngine::getParam(ParamId id) const
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= kNumParams)
        return 0.0f;

    return params_[index];
}

void WavetableEngine::renderBlock(float* output, int numSamples)
{
    const float basePosition = params_[static_cast<int>(ParamId::WavetablePosition)];
    const float baseCutoff   = params_[static_cast<int>(ParamId::FilterCutoff)];
    const float lfoToPos     = params_[static_cast<int>(ParamId::LfoToPosition)];
    const float lfoToCutoff  = params_[static_cast<int>(ParamId::LfoToCutoff)];
    const float gain         = params_[static_cast<int>(ParamId::OutputGain)];

    const bool modulatesCutoff = lfoToCutoff != 0.0f;
    if (!modulatesCutoff)
        filter_.setCutoff(baseCutoff);

    // Oscillator into filter into amplifier, in that order. Putting the
    // envelope after the filter rather than before it matters here because the
    // filter saturates: driving it with an already-enveloped signal would mean
    // quiet notes hit the drive stage softly and get pushed back up by its
    // makeup gain, which flattens out velocity and makes note tails swell.
    for (int i = 0; i < numSamples; ++i)
    {
        if (!ampEnv_.isActive())
        {
            output[i] = 0.0f;
            // Clear the ringing left behind, so the next note starts from
            // silence rather than from whatever the last one left in the
            // integrators. Safe to do here: the amplifier is already closed,
            // so nothing about this is audible.
            filter_.reset();
            continue;
        }

        const float lfo = lfo_.nextSample();

        float position = basePosition + lfo * lfoToPos;
        position = position < 0.0f ? 0.0f : (position > 1.0f ? 1.0f : position);
        osc_.setPosition(position);

        if (modulatesCutoff)
        {
            // Exponential, so a given LFO depth moves the cutoff by the same
            // musical interval wherever the knob is set.
            const float cutoff = baseCutoff
                * std::exp2(lfo * lfoToCutoff * kLfoCutoffOctaves);
            filter_.setCutoff(cutoff);
        }

        const float voice = filter_.process(osc_.nextSample());

        output[i] = voice * ampEnv_.nextSample() * velocity_ * gain;
    }
}

} // namespace stankface
