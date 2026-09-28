#include "stankface/Params.h"

#include <cmath>

#include "stankface/Lfo.h"
#include "stankface/WavetableData.h"

namespace stankface {
namespace {

constexpr float kChoiceMax(int count)
{
    return static_cast<float>(count - 1);
}

// Indexed by ParamId. Columns: id, name, min, max, default, curve, skew,
// display, unit, group.
//
// Names stay qualified ("Amp Attack" rather than "Attack") because a host shows
// one flat list with no sections to disambiguate them.
const ParamDescriptor kDescriptors[kNumParams] = {
    { "wavetable",  "Wavetable", 0.0f, kChoiceMax(kNumWavetables), 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Integer, "", "Oscillator" },

    { "position",   "Position", 0.0f, 1.0f, 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "Oscillator" },

    // Logarithmic: an octave of cutoff should take the same amount of knob
    // wherever you are in the range, which a power curve only approximates.
    { "cutoff",     "Cutoff", 20.0f, 20000.0f, 1200.0f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Integer, "Hz", "Filter" },

    { "resonance",  "Resonance", 0.0f, 1.0f, 0.15f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "Filter" },

    { "drive",      "Drive", 0.0f, 1.0f, 0.25f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "Filter" },

    // Logarithmic, for the same reason as cutoff: a power curve crushed the
    // punchy end of the range into the bottom third of the control, where a few
    // degrees of travel was the difference between a click and a swell.
    //
    // Shown in milliseconds. These ranges start at 1 ms, and in seconds to one
    // decimal every fast setting would read "0.0".
    { "ampAttack",  "Amp Attack", 0.001f, 5.0f, 0.005f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Milliseconds, "ms", "Amp Envelope" },

    { "ampDecay",   "Amp Decay", 0.001f, 5.0f, 0.15f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Milliseconds, "ms", "Amp Envelope" },

    { "ampSustain", "Amp Sustain", 0.0f, 1.0f, 0.8f,
      ParamCurve::Linear, 1.0f, ParamDisplay::OneDecimal, "", "Amp Envelope" },

    { "ampRelease", "Amp Release", 0.001f, 10.0f, 0.25f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Milliseconds, "ms", "Amp Envelope" },

    // Bottoms out at 0.1 Hz: a ten-second cycle is already slower than this
    // instrument wants, and it stops the readout showing "0.0 Hz".
    { "lfoRate",    "LFO Rate", 0.1f, 20.0f, 2.0f,
      ParamCurve::Skewed, 0.4f, ParamDisplay::OneDecimal, "Hz", "LFO" },

    { "lfoShape",   "LFO Shape",
      0.0f, kChoiceMax(static_cast<int>(LfoShape::NumShapes)), 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Integer, "", "LFO" },

    { "lfoToPos",   "LFO > Position", -1.0f, 1.0f, 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "LFO" },

    { "lfoToCutoff", "LFO > Cutoff", -1.0f, 1.0f, 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "LFO" },

    { "outputGain", "Output", 0.0f, 1.0f, 0.8f,
      ParamCurve::Linear, 1.0f, ParamDisplay::OneDecimal, "", "Output" },

    // Defaults to mono: this is a bass instrument, and it keeps every patch
    // written before polyphony existed sounding the way it did.
    { "voiceMode",  "Voice Mode",
      0.0f, kChoiceMax(static_cast<int>(VoiceMode::NumModes)), 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Integer, "", "Output" },

    // The modulation envelope. Defaults to zero depth on both routes, so adding
    // it changes nothing until it is deliberately dialled in.
    { "envAttack",  "Env Attack", 0.001f, 5.0f, 0.005f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Milliseconds, "ms", "Mod Envelope" },

    { "envDecay",   "Env Decay", 0.001f, 5.0f, 0.2f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Milliseconds, "ms", "Mod Envelope" },

    { "envSustain", "Env Sustain", 0.0f, 1.0f, 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::OneDecimal, "", "Mod Envelope" },

    { "envRelease", "Env Release", 0.001f, 10.0f, 0.2f,
      ParamCurve::Logarithmic, 1.0f, ParamDisplay::Milliseconds, "ms", "Mod Envelope" },

    { "envToPos",   "Env > Position", -1.0f, 1.0f, 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "Mod Envelope" },

    { "envToCutoff", "Env > Cutoff", -1.0f, 1.0f, 0.0f,
      ParamCurve::Linear, 1.0f, ParamDisplay::Percent, "%", "Mod Envelope" },
};

float clamp01(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

} // namespace

const ParamDescriptor& paramDescriptor(ParamId id)
{
    return kDescriptors[static_cast<int>(id)];
}

float paramDisplayScale(ParamId id)
{
    switch (paramDescriptor(id).display)
    {
        case ParamDisplay::Percent:      return 100.0f;
        case ParamDisplay::Milliseconds: return 1000.0f;
        case ParamDisplay::Integer:
        case ParamDisplay::OneDecimal:
            break;
    }

    return 1.0f;
}

int paramDisplayDecimals(ParamId id)
{
    return paramDescriptor(id).display == ParamDisplay::OneDecimal ? 1 : 0;
}

float paramToNormalised(ParamId id, float naturalValue)
{
    const ParamDescriptor& d = paramDescriptor(id);
    const float range = d.maxValue - d.minValue;
    if (range <= 0.0f)
        return 0.0f;

    if (d.curve == ParamCurve::Logarithmic && d.minValue > 0.0f)
    {
        const float value = naturalValue < d.minValue ? d.minValue
                          : (naturalValue > d.maxValue ? d.maxValue : naturalValue);

        return clamp01(std::log(value / d.minValue)
                       / std::log(d.maxValue / d.minValue));
    }

    const float proportion = clamp01((naturalValue - d.minValue) / range);

    if (d.curve != ParamCurve::Skewed || d.skew == 1.0f || proportion <= 0.0f)
        return proportion;

    return std::pow(proportion, d.skew);
}

float paramFromNormalised(ParamId id, float normalisedValue)
{
    const ParamDescriptor& d = paramDescriptor(id);
    float proportion = clamp01(normalisedValue);

    if (d.curve == ParamCurve::Logarithmic && d.minValue > 0.0f)
        return d.minValue * std::pow(d.maxValue / d.minValue, proportion);

    if (d.curve == ParamCurve::Skewed && d.skew != 1.0f && proportion > 0.0f)
        proportion = std::pow(proportion, 1.0f / d.skew);

    return d.minValue + proportion * (d.maxValue - d.minValue);
}

} // namespace stankface
