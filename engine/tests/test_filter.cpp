#include "TestFramework.h"
#include "TestSignal.h"

#include "stankface/Filter.h"

#include <cmath>
#include <vector>

using namespace stankface;

namespace {

constexpr double kSampleRate = 48000.0;

/** Steady-state RMS of a sine at `frequency` after the filter, skipping the
    settling transient. */
double responseAt(float cutoff, float resonance, float drive, double frequency,
                  double amplitude = 0.1)
{
    Filter filter;
    filter.setSampleRate(kSampleRate);
    filter.setCutoff(cutoff);
    filter.setResonance(resonance);
    filter.setDrive(drive);

    const int settle = 8192;
    const int measure = 8192;

    double sumOfSquares = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        const double phase = 2.0 * test::kPi * frequency * i / kSampleRate;
        // Small by default so the saturating stages stay in their linear
        // region and this measures the filter response rather than the clipper.
        const float out = filter.process(static_cast<float>(amplitude * std::sin(phase)));

        if (i >= settle)
            sumOfSquares += static_cast<double>(out) * out;
    }

    return std::sqrt(sumOfSquares / measure);
}

} // namespace

TEST(filterPassesLowsAndStopsHighs)
{
    const double low = responseAt(1000.0f, 0.0f, 0.0f, 100.0);
    const double atCutoff = responseAt(1000.0f, 0.0f, 0.0f, 1000.0);
    const double high = responseAt(1000.0f, 0.0f, 0.0f, 10000.0);

    CHECK(low > high);
    CHECK(atCutoff < low);

    // Two poles: a decade above cutoff should be down by roughly 40 dB.
    const double rolloffDb = test::toDecibels(high, low);
    CHECK_MESSAGE(rolloffDb < -30.0,
                  "rolloff only " + std::to_string(rolloffDb) + " dB per decade");
}

TEST(filterCutoffTracksTheControl)
{
    // The same test tone should get quieter as the cutoff comes down past it.
    const double at4k = responseAt(4000.0f, 0.0f, 0.0f, 1000.0);
    const double at1k = responseAt(1000.0f, 0.0f, 0.0f, 1000.0);
    const double at250 = responseAt(250.0f, 0.0f, 0.0f, 1000.0);

    CHECK(at4k > at1k);
    CHECK(at1k > at250);
}

TEST(filterResonanceLiftsTheCutoffRegion)
{
    const double flat = responseAt(1000.0f, 0.0f, 0.0f, 1000.0);
    const double resonant = responseAt(1000.0f, 0.9f, 0.0f, 1000.0);

    CHECK_MESSAGE(resonant > flat * 2.0,
                  "resonance lifted the cutoff region by only "
                      + std::to_string(test::toDecibels(resonant, flat)) + " dB");
}

TEST(filterStaysStableUnderExtremeSettings)
{
    // Full resonance and full drive, with the cutoff swept across the whole
    // range every block -- the case that makes naive filter topologies blow up.
    Filter filter;
    filter.setSampleRate(kSampleRate);
    filter.setResonance(1.0f);
    filter.setDrive(1.0f);

    unsigned int noiseState = 22222u;

    for (int i = 0; i < 200000; ++i)
    {
        const float sweep = static_cast<float>(0.5 + 0.5 * std::sin(i * 0.0005));
        filter.setCutoff(20.0f + sweep * 19000.0f);

        noiseState = noiseState * 1664525u + 1013904223u;
        const float noise = static_cast<float>(noiseState >> 8) / 8388608.0f - 1.0f;

        const float out = filter.process(noise);

        CHECK_MESSAGE(std::isfinite(out), "filter output diverged at sample "
                                              + std::to_string(i));
        CHECK_MESSAGE(std::fabs(out) < 10.0f,
                      "filter output ran away at sample " + std::to_string(i));

        if (!std::isfinite(out))
            return; // No point flooding the log once it has gone.
    }
}

TEST(filterDriveHoldsLevelAtOscillatorLevels)
{
    // The compensation is trimmed for a signal near full scale, which is what
    // the oscillator actually delivers. At that level the drive control should
    // change the tone, not act as a volume control.
    const double reference = 0.7;

    const double clean = responseAt(8000.0f, 0.0f, 0.0f, 200.0, reference);
    const double half = responseAt(8000.0f, 0.0f, 0.5f, 200.0, reference);
    const double driven = responseAt(8000.0f, 0.0f, 1.0f, 200.0, reference);

    for (double measured : { half, driven })
    {
        const double changeDb = test::toDecibels(measured, clean);
        CHECK_MESSAGE(std::fabs(changeDb) < 4.0,
                      "drive changed level by " + std::to_string(changeDb) + " dB");
    }
}

TEST(filterDriveAddsHarmonics)
{
    // Distortion is the whole point of the stage, so check it actually
    // generates harmonics rather than just changing gain. The cutoff is set
    // high so the filter is not what is removing them.
    auto harmonicRatio = [](float drive) {
        Filter filter;
        filter.setSampleRate(kSampleRate);
        filter.setCutoff(20000.0f);
        filter.setResonance(0.0f);
        filter.setDrive(drive);

        const int fftSize = 8192;
        const int fundamentalBin = 64;
        const double frequency = fundamentalBin * kSampleRate / fftSize;

        std::vector<float> block(static_cast<std::size_t>(fftSize));
        for (int i = 0; i < fftSize; ++i)
        {
            const double phase = 2.0 * test::kPi * frequency * i / kSampleRate;
            block[static_cast<std::size_t>(i)] =
                filter.process(static_cast<float>(0.7 * std::sin(phase)));
        }

        const std::vector<double> spectrum = test::magnitudeSpectrum(block);

        double harmonics = 0.0;
        for (int h = 2; h * fundamentalBin < fftSize / 2; ++h)
        {
            const double m = spectrum[static_cast<std::size_t>(h * fundamentalBin)];
            harmonics += m * m;
        }

        const double f = spectrum[static_cast<std::size_t>(fundamentalBin)];
        return std::sqrt(harmonics) / f;
    };

    const double clean = harmonicRatio(0.0f);
    const double driven = harmonicRatio(1.0f);

    CHECK_MESSAGE(driven > clean * 5.0,
                  "distortion only went from " + std::to_string(clean) + " to "
                      + std::to_string(driven));
}

TEST(filterIsSilentWithSilentInput)
{
    Filter filter;
    filter.setSampleRate(kSampleRate);
    filter.setCutoff(500.0f);
    filter.setResonance(0.8f);

    for (int i = 0; i < 1000; ++i)
        CHECK_NEAR(filter.process(0.0f), 0.0, 1.0e-12);
}
