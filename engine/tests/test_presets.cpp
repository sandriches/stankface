#include "TestFramework.h"

#include "stankface/Params.h"
#include "stankface/Presets.h"
#include "stankface/WavetableEngine.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace stankface;

namespace {

constexpr double kSampleRate = 48000.0;

WavetableEngine engineFor(int preset)
{
    WavetableEngine engine;
    engine.setSampleRate(kSampleRate);

    for (int i = 0; i < kNumParams; ++i)
        engine.setParam(static_cast<ParamId>(i),
                        presetValue(preset, static_cast<ParamId>(i)));

    return engine;
}

bool isPoly(int preset)
{
    return presetValue(preset, ParamId::VoiceMode) > 0.5f;
}

/** What a player would do with the preset: a low note on a mono patch, a
    four-note chord on a poly one. */
std::vector<int> notesFor(int preset)
{
    return isPoly(preset) ? std::vector<int>{ 48, 55, 60, 63 } : std::vector<int>{ 36 };
}

double peakOf(const std::vector<float>& block)
{
    double worst = 0.0;
    for (float sample : block)
        worst = std::max(worst, static_cast<double>(std::fabs(sample)));
    return worst;
}

} // namespace

TEST(presetsHaveDistinctNames)
{
    for (int a = 0; a < kNumPresets; ++a)
    {
        CHECK(presetName(a) != nullptr && std::strlen(presetName(a)) > 0);

        for (int b = a + 1; b < kNumPresets; ++b)
            CHECK_MESSAGE(std::string(presetName(a)) != presetName(b),
                          std::string("duplicate preset name ") + presetName(a));
    }
}

TEST(initPresetIsAllDefaults)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        CHECK_NEAR(presetValue(0, id), paramDescriptor(id).defaultValue, 0.0);
    }
}

TEST(presetValuesAreInRange)
{
    for (int p = 0; p < kNumPresets; ++p)
    {
        for (int i = 0; i < kNumParams; ++i)
        {
            const ParamId id = static_cast<ParamId>(i);
            const ParamDescriptor& d = paramDescriptor(id);
            const float value = presetValue(p, id);

            CHECK_MESSAGE(value >= d.minValue && value <= d.maxValue,
                          std::string(presetName(p)) + ": " + d.name + " out of range");

            // Choices are stored as floats; a fractional one would be rounded
            // differently by the host and by the engine.
            if (d.display == ParamDisplay::Integer && d.unit[0] == '\0')
                CHECK_MESSAGE(std::floor(value) == value,
                              std::string(presetName(p)) + ": " + d.name + " not a whole choice");
        }
    }
}

TEST(presetsSoundAndStayUnderFullScale)
{
    for (int p = 0; p < kNumPresets; ++p)
    {
        WavetableEngine engine = engineFor(p);
        const std::vector<int> notes = notesFor(p);

        for (int note : notes)
            engine.noteOn(note, 1.0f);

        std::vector<float> held(static_cast<std::size_t>(kSampleRate * 2.0));
        engine.renderBlock(held.data(), static_cast<int>(held.size()));

        bool finite = true;
        for (float sample : held)
            finite = finite && std::isfinite(sample);

        const double peak = peakOf(held);

        CHECK_MESSAGE(finite, std::string(presetName(p)) + " produced a non-finite sample");
        CHECK_MESSAGE(peak > 0.1, std::string(presetName(p)) + " is near silent");

        // The engine does not clip its output, so a preset that goes past full
        // scale distorts in the host instead. Leaves room for velocity and for
        // players who stack more notes than the test does.
        CHECK_MESSAGE(peak < 1.0, std::string(presetName(p)) + " peaks past full scale");
    }
}

TEST(presetsFallSilentAfterRelease)
{
    for (int p = 0; p < kNumPresets; ++p)
    {
        WavetableEngine engine = engineFor(p);
        const std::vector<int> notes = notesFor(p);

        for (int note : notes)
            engine.noteOn(note, 1.0f);

        std::vector<float> block(static_cast<std::size_t>(kSampleRate * 0.5));
        engine.renderBlock(block.data(), static_cast<int>(block.size()));

        for (int note : notes)
            engine.noteOff(note);

        // Longest release in the factory set is two seconds; give it a margin.
        std::vector<float> tail(static_cast<std::size_t>(kSampleRate * 3.0));
        engine.renderBlock(tail.data(), static_cast<int>(tail.size()));

        const std::vector<float> end(tail.end() - static_cast<long>(kSampleRate * 0.1),
                                     tail.end());

        CHECK_MESSAGE(peakOf(end) < 1.0e-4,
                      std::string(presetName(p)) + " still sounding after release");
    }
}
