#pragma once

namespace stankface {

/** Every parameter the engine exposes.

    Values are in natural units -- Hz, seconds, linear gain -- not normalised.
    A host wrapper is the right place to deal with 0..1 automation ranges, so
    the descriptor table below carries the range and skew it needs to convert.
*/
enum class ParamId
{
    WavetableSelect = 0, ///< Which wavetable set, as an index.
    WavetablePosition,   ///< 0..1 morph across the frames of that set.
    FilterCutoff,        ///< Hz.
    FilterResonance,     ///< 0..1, where 1 is just short of self-oscillation.
    Drive,               ///< 0..1 saturation into the filter.
    AmpAttack,           ///< Seconds.
    AmpDecay,            ///< Seconds.
    AmpSustain,          ///< 0..1 level.
    AmpRelease,          ///< Seconds.
    LfoRate,             ///< Hz.
    LfoShape,            ///< LfoShape, as a float so setParam stays uniform.
    LfoToPosition,       ///< -1..1 depth onto WavetablePosition.
    LfoToCutoff,         ///< -1..1 depth onto FilterCutoff, in octaves at full scale.
    OutputGain,          ///< Linear.
    NumParams
};

inline constexpr int kNumParams = static_cast<int>(ParamId::NumParams);

/** How far LfoToCutoff at full depth moves the cutoff. */
inline constexpr float kLfoCutoffOctaves = 4.0f;

struct ParamDescriptor
{
    const char* id;      ///< Stable identifier, for preset/automation IDs.
    const char* name;    ///< Human-readable.
    float minValue;
    float maxValue;
    float defaultValue;
    /** Shapes the normalised mapping. 1 is linear; below 1 packs more
        resolution near minValue, which is what frequency and time controls
        want so the useful range is not crushed into the top of the knob. */
    float skew;
    const char* unit;
};

const ParamDescriptor& paramDescriptor(ParamId id);

/** Convert a natural value to 0..1, honouring the descriptor's skew. */
float paramToNormalised(ParamId id, float naturalValue);

/** Convert 0..1 back to natural units. */
float paramFromNormalised(ParamId id, float normalisedValue);

} // namespace stankface
