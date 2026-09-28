#pragma once

namespace stankface {

/** Every parameter the engine exposes.

    Values are in natural units -- Hz, seconds, linear gain -- not normalised.
    A host wrapper is the right place to deal with 0..1 automation ranges, so
    the descriptor table below carries the range and skew it needs to convert.
*/
/** How incoming notes are assigned to voices. */
enum class VoiceMode
{
    /** One voice, last-note priority. Playing over a held note steals the
        voice and retunes it without retriggering the envelope, and releasing
        that note hands the voice back to whatever is still down. Not the same
        thing as a pool of size one: the legato behaviour is the point, and it
        is most of what a bass part wants. */
    Mono = 0,

    /** A pool of voices with stealing. */
    Poly,

    NumModes
};

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
    VoiceMode,           ///< VoiceMode, as a float so setParam stays uniform.

    // The modulation envelope. Separate from the amplifier's, and per-voice, so
    // that each note's sweep starts when that note does. Appended rather than
    // slotted in beside the amp envelope because AU addresses parameters by
    // index, and reordering would repoint automation saved against the old one.
    EnvAttack,           ///< Seconds.
    EnvDecay,            ///< Seconds.
    EnvSustain,          ///< 0..1 level.
    EnvRelease,          ///< Seconds.
    EnvToPosition,       ///< -1..1 depth onto WavetablePosition.
    EnvToCutoff,         ///< -1..1 depth onto FilterCutoff, in octaves at full scale.

    NumParams
};

inline constexpr int kNumParams = static_cast<int>(ParamId::NumParams);

/** How far a cutoff modulation source at full depth moves the cutoff.

    Shared by the LFO and the envelope so that equal depths mean equal
    intervals, and a patch does not change character when a route is moved from
    one to the other. */
inline constexpr float kCutoffModOctaves = 4.0f;

/** How a control's travel maps onto its value range. */
enum class ParamCurve
{
    /** Even throughout. */
    Linear,

    /** A power curve. `skew` below 1 packs more resolution near minValue. */
    Skewed,

    /** Constant ratio per unit of travel, so equal movements are equal
        musical intervals wherever the knob is. What a frequency control
        wants, and unlike a power curve it is exactly that rather than an
        approximation of it. Requires minValue above zero. */
    Logarithmic,
};

/** How a value should be written out.

    Kept here rather than in the editor because the host shows values too, and
    the two disagreeing is the kind of thing nobody notices until a screenshot
    of the plugin says one number and the automation lane says another.
*/
enum class ParamDisplay
{
    Integer,      ///< No decimals.
    OneDecimal,
    Percent,      ///< 0..1 shown as 0..100.
    Milliseconds, ///< Seconds shown as milliseconds, no decimals.
};

struct ParamDescriptor
{
    const char* id;      ///< Stable identifier, for preset/automation IDs.
    const char* name;    ///< Human-readable.
    float minValue;
    float maxValue;
    float defaultValue;
    ParamCurve curve;
    float skew;          ///< Only read when curve is Skewed.
    ParamDisplay display;
    const char* unit;    ///< As shown, so a Percent parameter's unit is "%".

    /** Which section of the panel this belongs under.

        Layout lives here rather than in the editor so that adding a parameter
        to the engine still needs no edit to the editor, which stops being true
        the moment the editor has to know a hand-written ordering. Groups are
        laid out in the order they first appear below. */
    const char* group;
};

/** Sections in the order a panel should lay them out.

    Stated here rather than inferred from the order parameters appear in,
    because the two cannot be the same: parameters are appended as they are
    added, since AU addresses them by index and reordering would repoint saved
    automation, while sections should read in signal order. Adding a parameter
    to an existing section still needs no change here. */
inline constexpr const char* kParamGroupOrder[] = {
    "Oscillator",
    "Filter",
    "Amp Envelope",
    "Mod Envelope",
    "LFO",
    "Output",
};

inline constexpr int kNumParamGroups =
    static_cast<int>(sizeof(kParamGroupOrder) / sizeof(kParamGroupOrder[0]));

const ParamDescriptor& paramDescriptor(ParamId id);

/** Factor to multiply a natural value by before showing it. */
float paramDisplayScale(ParamId id);

/** Decimal places to show. */
int paramDisplayDecimals(ParamId id);

/** Convert a natural value to 0..1, honouring the descriptor's skew. */
float paramToNormalised(ParamId id, float naturalValue);

/** Convert 0..1 back to natural units. */
float paramFromNormalised(ParamId id, float normalisedValue);

} // namespace stankface
