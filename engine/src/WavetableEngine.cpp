#include "stankface/WavetableEngine.h"

#include <cmath>

namespace stankface {

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

    for (Voice& voice : voices_)
        voice.setSampleRate(sampleRate_);

    lfo_.setSampleRate(sampleRate_);

    reset();
}

void WavetableEngine::reset()
{
    for (Voice& voice : voices_)
        voice.reset();

    lfo_.reset();

    numHeldNotes_ = 0;
    currentNote_ = -1;
    nextStartOrder_ = 1;
}

bool WavetableEngine::isActive() const
{
    for (const Voice& voice : voices_)
        if (voice.isActive())
            return true;

    return false;
}

VoiceMode WavetableEngine::voiceMode() const
{
    const float value = params_[static_cast<int>(ParamId::VoiceMode)];
    return static_cast<VoiceMode>(static_cast<int>(value + 0.5f));
}

void WavetableEngine::noteOn(int midiNote, float velocity)
{
    if (voiceMode() == VoiceMode::Mono)
        monoNoteOn(midiNote, velocity);
    else
        polyNoteOn(midiNote, velocity);
}

void WavetableEngine::noteOff(int midiNote)
{
    if (voiceMode() == VoiceMode::Mono)
        monoNoteOff(midiNote);
    else
        polyNoteOff(midiNote);
}

void WavetableEngine::monoNoteOn(int midiNote, float velocity)
{
    // A repeated note-on for a note already down should not stack up.
    removeHeldNote(midiNote);

    if (numHeldNotes_ < kMaxHeldNotes)
        heldNotes_[numHeldNotes_++] = midiNote;

    // Restarting the oscillator and LFO mid-note would click and would throw
    // away the phase relationship a held wobble has built up, so only do it
    // when the voice is actually starting from silence.
    Voice& voice = voices_[0];
    const bool wasSilent = !voice.isActive();

    currentNote_ = midiNote;
    voice.noteOn(midiNote, velocity, wasSilent);
    voiceStartOrder_[0] = nextStartOrder_++;

    if (wasSilent)
        lfo_.retrigger();
}

void WavetableEngine::monoNoteOff(int midiNote)
{
    removeHeldNote(midiNote);

    if (numHeldNotes_ > 0)
    {
        // Something is still held: fall back to it rather than releasing.
        if (midiNote == currentNote_)
        {
            currentNote_ = heldNotes_[numHeldNotes_ - 1];
            voices_[0].retune(currentNote_);
        }
        return;
    }

    if (midiNote == currentNote_ || currentNote_ < 0)
    {
        voices_[0].noteOff();
        currentNote_ = -1;
    }
}

void WavetableEngine::polyNoteOn(int midiNote, float velocity)
{
    // Retrigger the LFO only when the whole instrument was silent, so that
    // adding a note to a chord does not jolt a wobble already in progress.
    const bool poolWasSilent = !isActive();

    const int index = allocateVoice(midiNote);
    Voice& voice = voices_[index];

    // A voice taken over while it is still sounding keeps its oscillator phase
    // and filter state, which is most of what stops a steal from clicking.
    const bool startsFromSilence = !voice.isActive();

    voice.noteOn(midiNote, velocity, startsFromSilence);
    voiceStartOrder_[index] = nextStartOrder_++;

    if (poolWasSilent)
        lfo_.retrigger();
}

void WavetableEngine::polyNoteOff(int midiNote)
{
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Voice& voice = voices_[i];
        if (voice.isActive() && !voice.isReleasing() && voice.note() == midiNote)
            voice.noteOff();
    }
}

int WavetableEngine::allocateVoice(int midiNote) const
{
    // Re-use the voice already on this note, so that a repeated note-on
    // retriggers it rather than leaving two copies of the same pitch running
    // and beating against each other.
    for (int i = 0; i < kMaxVoices; ++i)
        if (voices_[i].isActive() && voices_[i].note() == midiNote)
            return i;

    for (int i = 0; i < kMaxVoices; ++i)
        if (!voices_[i].isActive())
            return i;

    // Everything is busy, so something has to go. Prefer a voice already in
    // its release: it is on its way out and quieter than the rest, so cutting
    // it short is the least audible choice available.
    int oldestReleasing = -1;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (!voices_[i].isReleasing())
            continue;

        if (oldestReleasing < 0
            || voiceStartOrder_[i] < voiceStartOrder_[oldestReleasing])
        {
            oldestReleasing = i;
        }
    }

    if (oldestReleasing >= 0)
        return oldestReleasing;

    // Everything is still held. Take the one that has been sounding longest.
    int oldest = 0;
    for (int i = 1; i < kMaxVoices; ++i)
        if (voiceStartOrder_[i] < voiceStartOrder_[oldest])
            oldest = i;

    return oldest;
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

void WavetableEngine::applyParam(ParamId id, float value)
{
    switch (id)
    {
        case ParamId::WavetableSelect:
            for (Voice& voice : voices_)
                voice.setTable(static_cast<int>(value + 0.5f));
            break;

        case ParamId::FilterResonance:
            for (Voice& voice : voices_)
                voice.setResonance(value);
            break;

        case ParamId::Drive:
            for (Voice& voice : voices_)
                voice.setDrive(value);
            break;

        case ParamId::AmpAttack:
            for (Voice& voice : voices_)
                voice.setAmpAttack(value);
            break;

        case ParamId::AmpDecay:
            for (Voice& voice : voices_)
                voice.setAmpDecay(value);
            break;

        case ParamId::AmpSustain:
            for (Voice& voice : voices_)
                voice.setAmpSustain(value);
            break;

        case ParamId::AmpRelease:
            for (Voice& voice : voices_)
                voice.setAmpRelease(value);
            break;

        case ParamId::EnvAttack:
            for (Voice& voice : voices_)
                voice.setEnvAttack(value);
            break;

        case ParamId::EnvDecay:
            for (Voice& voice : voices_)
                voice.setEnvDecay(value);
            break;

        case ParamId::EnvSustain:
            for (Voice& voice : voices_)
                voice.setEnvSustain(value);
            break;

        case ParamId::EnvRelease:
            for (Voice& voice : voices_)
                voice.setEnvRelease(value);
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
        case ParamId::EnvToPosition:
        case ParamId::EnvToCutoff:
        case ParamId::OutputGain:
        // Handled by setParam, which can see whether it actually changed.
        case ParamId::VoiceMode:
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

    const float previous = params_[index];
    params_[index] = clamped;
    applyParam(id, clamped);

    // Wrappers push every parameter every block, so a mode switch has to be
    // driven by an actual change. Acting on each call would release held notes
    // continuously and the instrument would never sound.
    if (id == ParamId::VoiceMode && clamped != previous)
    {
        // Let whatever is sounding release rather than cutting it, and drop
        // the mono note stack so it cannot resurrect a note in the new mode.
        for (Voice& voice : voices_)
            voice.noteOff();

        numHeldNotes_ = 0;
        currentNote_ = -1;
    }
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
    Modulation mod;
    mod.basePosition   = params_[static_cast<int>(ParamId::WavetablePosition)];
    mod.baseCutoff     = params_[static_cast<int>(ParamId::FilterCutoff)];
    mod.lfoToPosition  = params_[static_cast<int>(ParamId::LfoToPosition)];
    mod.lfoToCutoff    = params_[static_cast<int>(ParamId::LfoToCutoff)];
    mod.envToPosition  = params_[static_cast<int>(ParamId::EnvToPosition)];
    mod.envToCutoff    = params_[static_cast<int>(ParamId::EnvToCutoff)];

    const float gain = params_[static_cast<int>(ParamId::OutputGain)];

    // What a display should show while nothing is sounding: no note means no
    // envelope and a held LFO, so the unmodulated settings are the honest
    // answer. Overwritten below if any voice is actually running.
    displayPosition_ = mod.basePosition;
    displayCutoff_ = mod.baseCutoff;

    for (int i = 0; i < numSamples; ++i)
    {
        // The LFO is held still while nothing is sounding, so that it always
        // starts a phrase from the top of its cycle rather than from wherever
        // it happened to drift to during the silence.
        if (!isActive())
        {
            output[i] = 0.0f;
            continue;
        }

        mod.lfo = lfo_.nextSample();

        // Voices are summed straight, with no division by how many are
        // sounding. Scaling by the active count would duck the whole instrument
        // every time a note was added or released, and that pumping is more
        // obvious than the headroom it would buy back.
        float mix = 0.0f;
        for (Voice& voice : voices_)
        {
            if (voice.isActive())
                mix += voice.nextSample(mod);
        }

        output[i] = mix * gain;
    }

    publishDisplayState();
}

void WavetableEngine::publishDisplayState()
{
    // Drawn from the most recently started voice. With a per-voice modulation
    // envelope there is no single answer for the instrument as a whole, and the
    // note just played is the one an eye follows.
    int newest = -1;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices_[i].isActive()
            && (newest < 0 || voiceStartOrder_[i] > voiceStartOrder_[newest]))
        {
            newest = i;
        }
    }

    if (newest < 0)
        return;

    displayPosition_ = voices_[newest].lastPosition();
    displayCutoff_ = voices_[newest].lastCutoff();
}

} // namespace stankface
