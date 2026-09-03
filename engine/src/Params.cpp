#include "stankface/Params.h"

#include <cmath>

#include "stankface/Lfo.h"
#include "stankface/WavetableData.h"

namespace stankface {
namespace {

// Indexed by ParamId. Skews below 1 give frequency and time controls more
// resolution at the bottom of their range, where the ear needs it.
const ParamDescriptor kDescriptors[kNumParams] = {
    { "wavetable",  "Wavetable",      0.0f, static_cast<float>(kNumWavetables - 1), 0.0f,    1.0f,  "" },
    { "position",   "Position",       0.0f,     1.0f,     0.0f,    1.0f,  "" },
    { "cutoff",     "Cutoff",        20.0f, 20000.0f,  1200.0f,    0.3f,  "Hz" },
    { "resonance",  "Resonance",      0.0f,     1.0f,     0.15f,   1.0f,  "" },
    { "drive",      "Drive",          0.0f,     1.0f,     0.25f,   1.0f,  "" },
    { "ampAttack",  "Attack",         0.001f,   5.0f,     0.005f,  0.3f,  "s" },
    { "ampDecay",   "Decay",          0.001f,   5.0f,     0.15f,   0.3f,  "s" },
    { "ampSustain", "Sustain",        0.0f,     1.0f,     0.8f,    1.0f,  "" },
    { "ampRelease", "Release",        0.001f,  10.0f,     0.25f,   0.3f,  "s" },
    { "lfoRate",    "LFO Rate",       0.01f,   20.0f,     2.0f,    0.4f,  "Hz" },
    { "lfoShape",   "LFO Shape",      0.0f, static_cast<float>(static_cast<int>(LfoShape::NumShapes) - 1), 0.0f, 1.0f, "" },
    { "lfoToPos",   "LFO > Position", -1.0f,    1.0f,     0.0f,    1.0f,  "" },
    { "lfoToCutoff","LFO > Cutoff",   -1.0f,    1.0f,     0.0f,    1.0f,  "" },
    { "outputGain", "Output",         0.0f,     1.0f,     0.8f,    1.0f,  "" },
};

} // namespace

const ParamDescriptor& paramDescriptor(ParamId id)
{
    return kDescriptors[static_cast<int>(id)];
}

float paramToNormalised(ParamId id, float naturalValue)
{
    const ParamDescriptor& d = paramDescriptor(id);
    const float range = d.maxValue - d.minValue;
    if (range <= 0.0f)
        return 0.0f;

    float proportion = (naturalValue - d.minValue) / range;
    proportion = proportion < 0.0f ? 0.0f : (proportion > 1.0f ? 1.0f : proportion);

    if (d.skew == 1.0f || proportion <= 0.0f)
        return proportion;

    return std::pow(proportion, d.skew);
}

float paramFromNormalised(ParamId id, float normalisedValue)
{
    const ParamDescriptor& d = paramDescriptor(id);

    float proportion = normalisedValue < 0.0f ? 0.0f
                     : (normalisedValue > 1.0f ? 1.0f : normalisedValue);

    if (d.skew != 1.0f && proportion > 0.0f)
        proportion = std::pow(proportion, 1.0f / d.skew);

    return d.minValue + proportion * (d.maxValue - d.minValue);
}

} // namespace stankface
