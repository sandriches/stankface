#include "stankface/Presets.h"

#include "stankface/Lfo.h"

namespace stankface {
namespace {

struct Setting
{
    ParamId id;
    float value;
};

struct Preset
{
    const char* name;
    const Setting* settings;
    int numSettings;
};

// Choice parameters are stored as floats, so these read better than bare
// indices in the tables below.
constexpr float kSubSaw = 0.0f;
constexpr float kReese  = 1.0f;
constexpr float kGrowl  = 2.0f;

constexpr float shape(LfoShape s) { return static_cast<float>(static_cast<int>(s)); }
constexpr float mode(VoiceMode m) { return static_cast<float>(static_cast<int>(m)); }

// A clean sub with a knock on the front. Nearly a sine at the bottom of the
// SubSaw table, with just enough drive to give it harmonics a small speaker can
// find, and a quick envelope blip on the cutoff for the attack.
const Setting kSubPressure[] = {
    { ParamId::WavetableSelect,   kSubSaw },
    { ParamId::WavetablePosition, 0.08f },
    { ParamId::FilterCutoff,      260.0f },
    { ParamId::FilterResonance,   0.05f },
    { ParamId::Drive,             0.40f },
    { ParamId::AmpAttack,         0.002f },
    { ParamId::AmpSustain,        1.0f },
    { ParamId::AmpRelease,        0.09f },
    { ParamId::EnvDecay,          0.08f },
    { ParamId::EnvToCutoff,       0.25f },
    { ParamId::OutputGain,        0.80f },
};

// The classic: a detuned-sounding Reese with the LFO opening and closing the
// filter, and a little of it on position so each wobble is not identical.
const Setting kReeseWobble[] = {
    { ParamId::WavetableSelect,   kReese },
    { ParamId::WavetablePosition, 0.45f },
    { ParamId::FilterCutoff,      420.0f },
    { ParamId::FilterResonance,   0.35f },
    { ParamId::Drive,             0.55f },
    { ParamId::AmpAttack,         0.004f },
    { ParamId::AmpSustain,        1.0f },
    { ParamId::AmpRelease,        0.18f },
    { ParamId::LfoRate,           2.0f },
    { ParamId::LfoShape,          shape(LfoShape::Sine) },
    { ParamId::LfoToPosition,     0.15f },
    { ParamId::LfoToCutoff,       0.60f },
    { ParamId::OutputGain,        0.80f },
};

// Sweeping the Growl table's formants with a triangle LFO is what makes it
// talk; the filter sits well open so the vowels come through.
const Setting kGrowlTalker[] = {
    { ParamId::WavetableSelect,   kGrowl },
    { ParamId::WavetablePosition, 0.35f },
    { ParamId::FilterCutoff,      1600.0f },
    { ParamId::FilterResonance,   0.50f },
    { ParamId::Drive,             0.70f },
    { ParamId::AmpAttack,         0.004f },
    { ParamId::AmpSustain,        1.0f },
    { ParamId::AmpRelease,        0.15f },
    { ParamId::LfoRate,           3.0f },
    { ParamId::LfoShape,          shape(LfoShape::Triangle) },
    { ParamId::LfoToPosition,     0.55f },
    { ParamId::LfoToCutoff,       0.15f },
    { ParamId::OutputGain,        0.80f },
};

// Short, plucked chords. Poly, no sustain, and the mod envelope snapping the
// filter shut so every hit has the same bite.
const Setting kGarageStab[] = {
    { ParamId::WavetableSelect,   kReese },
    { ParamId::WavetablePosition, 0.25f },
    { ParamId::FilterCutoff,      350.0f },
    { ParamId::FilterResonance,   0.45f },
    { ParamId::Drive,             0.30f },
    { ParamId::VoiceMode,         mode(VoiceMode::Poly) },
    { ParamId::AmpAttack,         0.001f },
    { ParamId::AmpDecay,          0.35f },
    { ParamId::AmpSustain,        0.0f },
    { ParamId::AmpRelease,        0.25f },
    { ParamId::EnvAttack,         0.001f },
    { ParamId::EnvDecay,          0.22f },
    { ParamId::EnvSustain,        0.0f },
    { ParamId::EnvToCutoff,       0.75f },
    { ParamId::EnvToPosition,     0.20f },
    // Lower than the mono presets because a chord sums its voices: four
    // notes at the mono level peak past full scale.
    { ParamId::OutputGain,        0.30f },
};

// Everything up: full drive, resonance high, and a fast saw LFO pulling the
// cutoff down in ramps for the rhythmic snarl. The envelope throws the
// position forward on each note so the attack is the harshest part.
const Setting kNeuroSnarl[] = {
    { ParamId::WavetableSelect,   kGrowl },
    { ParamId::WavetablePosition, 0.70f },
    { ParamId::FilterCutoff,      2400.0f },
    { ParamId::FilterResonance,   0.62f },
    { ParamId::Drive,             1.0f },
    { ParamId::AmpAttack,         0.002f },
    { ParamId::AmpSustain,        1.0f },
    { ParamId::AmpRelease,        0.12f },
    { ParamId::LfoRate,           6.0f },
    { ParamId::LfoShape,          shape(LfoShape::Saw) },
    { ParamId::LfoToCutoff,       -0.45f },
    { ParamId::EnvDecay,          0.5f },
    { ParamId::EnvToPosition,     0.30f },
    { ParamId::OutputGain,        0.80f },
};

// The one that is not a bass: slow attack and release, poly, and a very slow
// LFO drifting through the Reese table so held chords keep moving.
const Setting kDriftPad[] = {
    { ParamId::WavetableSelect,   kReese },
    { ParamId::WavetablePosition, 0.20f },
    { ParamId::FilterCutoff,      1400.0f },
    { ParamId::FilterResonance,   0.20f },
    { ParamId::Drive,             0.15f },
    { ParamId::VoiceMode,         mode(VoiceMode::Poly) },
    { ParamId::AmpAttack,         0.8f },
    { ParamId::AmpDecay,          1.0f },
    { ParamId::AmpSustain,        0.8f },
    { ParamId::AmpRelease,        2.0f },
    { ParamId::LfoRate,           0.2f },
    { ParamId::LfoShape,          shape(LfoShape::Sine) },
    { ParamId::LfoToPosition,     0.45f },
    { ParamId::LfoToCutoff,       0.20f },
    // Lower than the mono presets because a chord sums its voices: four
    // notes at the mono level peak past full scale.
    { ParamId::OutputGain,        0.30f },
};

template <int N>
constexpr Preset preset(const char* name, const Setting (&settings)[N])
{
    return { name, settings, N };
}

const Preset kPresets[kNumPresets] = {
    { "Init", nullptr, 0 },
    preset("Sub Pressure", kSubPressure),
    preset("Reese Wobble", kReeseWobble),
    preset("Growl Talker", kGrowlTalker),
    preset("Garage Stab",  kGarageStab),
    preset("Neuro Snarl",  kNeuroSnarl),
    preset("Drift Pad",    kDriftPad),
};

const Preset& presetAt(int index)
{
    return kPresets[index >= 0 && index < kNumPresets ? index : 0];
}

} // namespace

const char* presetName(int index)
{
    return presetAt(index).name;
}

float presetValue(int index, ParamId id)
{
    const Preset& p = presetAt(index);

    for (int i = 0; i < p.numSettings; ++i)
        if (p.settings[i].id == id)
            return p.settings[i].value;

    return paramDescriptor(id).defaultValue;
}

} // namespace stankface
