#include "TestFramework.h"
#include "TestSignal.h"

#include "stankface/WavetableData.h"
#include "stankface/WavetableOscillator.h"

using namespace stankface;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kFftSize = 8192;

std::vector<float> render(int table, float position, float frequency, int numSamples)
{
    WavetableOscillator osc;
    osc.setSampleRate(kSampleRate);
    osc.setTable(table);
    osc.setPosition(position);
    osc.setFrequency(frequency);
    osc.resetPhase();

    std::vector<float> out(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<std::size_t>(i)] = osc.nextSample();

    return out;
}

/** A frequency that completes exactly `harmonicBin` cycles in an FFT buffer,
    so every partial of the waveform lands on a bin centre. */
float binAlignedFrequency(int harmonicBin)
{
    return static_cast<float>(harmonicBin * kSampleRate / kFftSize);
}

/** Worst non-harmonic bin, in dB relative to the loudest harmonic.

    Aliased partials fold to frequencies that are not multiples of the
    fundamental, so anything loud sitting off the harmonic grid is aliasing.
*/
double worstNonHarmonicDb(const std::vector<float>& signal, int fundamentalBin)
{
    const std::vector<double> spectrum = test::magnitudeSpectrum(signal);

    double loudestHarmonic = 0.0;
    for (std::size_t bin = 0; bin < spectrum.size(); ++bin)
        if (bin != 0 && bin % static_cast<std::size_t>(fundamentalBin) == 0)
            loudestHarmonic = std::max(loudestHarmonic, spectrum[bin]);

    double worst = 0.0;
    for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
        if (bin % static_cast<std::size_t>(fundamentalBin) != 0)
            worst = std::max(worst, spectrum[bin]);

    return test::toDecibels(worst, loudestHarmonic);
}

// Everything off the harmonic grid has to sit below this.
//
// Set from what the engine actually measures, which is around -82 dB at its
// worst, with enough margin that ordinary numerical noise will not trip it.
// The number is deliberately tight: dropping the cubic interpolation back to
// linear costs about 18 dB, and getting the mip selection off by one level
// costs 40 dB or more, so either regression fails this immediately.
constexpr double kAliasFloorDb = -75.0;

} // namespace

TEST(oscillatorProducesACleanSineAtPositionZero)
{
    // Frame 0 of every table is very close to a pure sine, so this checks the
    // read path end to end: phase accumulation, interpolation, guard sample.
    const int fundamentalBin = 64;
    const std::vector<float> signal =
        render(0, 0.0f, binAlignedFrequency(fundamentalBin), kFftSize);

    const std::vector<double> spectrum = test::magnitudeSpectrum(signal);

    double worstOther = 0.0;
    for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
        if (bin != static_cast<std::size_t>(fundamentalBin))
            worstOther = std::max(worstOther, spectrum[bin]);

    const double db = test::toDecibels(worstOther,
                                       spectrum[static_cast<std::size_t>(fundamentalBin)]);
    CHECK_MESSAGE(db < -60.0,
                  "loudest non-fundamental partial at " + std::to_string(db) + " dB");
}

TEST(oscillatorDoesNotAliasOnHighNotes)
{
    // ~2.9 kHz: only eight harmonics fit under Nyquist, so an oscillator that
    // read the full-bandwidth table here would alias badly.
    const int fundamentalBin = 500;
    const float frequency = binAlignedFrequency(fundamentalBin);

    for (int table = 0; table < kNumWavetables; ++table)
    {
        for (float position : { 0.0f, 0.35f, 0.7f, 1.0f })
        {
            const std::vector<float> signal =
                render(table, position, frequency, kFftSize);

            const double db = worstNonHarmonicDb(signal, fundamentalBin);
            CHECK_MESSAGE(db < kAliasFloorDb,
                          std::string(kWavetableNames[table]) + " at position "
                              + std::to_string(position) + ": alias at "
                              + std::to_string(db) + " dB");
        }
    }
}

TEST(oscillatorDoesNotAliasOnBassNotes)
{
    // The register this synth actually lives in. Plenty of harmonics fit, so
    // this is really a check that the mip selection is not band-limiting more
    // than it needs to while still staying clean.
    const int fundamentalBin = 7; // ~41 Hz, near the bottom of a bass part
    const float frequency = binAlignedFrequency(fundamentalBin);

    for (int table = 0; table < kNumWavetables; ++table)
    {
        const std::vector<float> signal = render(table, 1.0f, frequency, kFftSize);
        const double db = worstNonHarmonicDb(signal, fundamentalBin);

        CHECK_MESSAGE(db < kAliasFloorDb,
                      std::string(kWavetableNames[table]) + ": alias at "
                          + std::to_string(db) + " dB");
    }
}

TEST(mipLevelRisesWithPitch)
{
    WavetableOscillator osc;
    osc.setSampleRate(kSampleRate);

    float previous = -1.0f;
    for (float frequency : { 30.0f, 60.0f, 120.0f, 480.0f, 2000.0f, 8000.0f })
    {
        osc.setFrequency(frequency);
        const float level = osc.currentMipLevel();

        CHECK(level >= previous);
        CHECK(level >= 0.0f);
        CHECK(level <= static_cast<float>(kNumMipLevels - 1));
        previous = level;
    }
}

TEST(oscillatorMorphIsContinuousAcrossFrames)
{
    // Stepping between frames instead of interpolating would show up as a jump
    // in output as position crosses a frame boundary.
    WavetableOscillator osc;
    osc.setSampleRate(kSampleRate);
    osc.setTable(0);
    osc.setFrequency(110.0f);

    float previous = 0.0f;
    bool first = true;

    for (int step = 0; step <= 400; ++step)
    {
        const float position = static_cast<float>(step) / 400.0f;
        osc.setPosition(position);
        osc.resetPhase();

        // Same phase every time, so any change is the morph alone.
        const float sample = osc.nextSample();

        if (!first)
            CHECK_MESSAGE(std::fabs(sample - previous) < 0.05f,
                          "morph jumped at position " + std::to_string(position));

        previous = sample;
        first = false;
    }
}

TEST(oscillatorOutputStaysInRange)
{
    for (int table = 0; table < kNumWavetables; ++table)
    {
        for (float position : { 0.0f, 0.5f, 1.0f })
        {
            const std::vector<float> signal = render(table, position, 220.0f, 4096);

            for (float sample : signal)
            {
                CHECK(std::isfinite(sample));
                CHECK(std::fabs(sample) <= 1.2f);
            }
        }
    }
}
