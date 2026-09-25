#include "TestFramework.h"
#include "TestSignal.h"

#include "stankface/Params.h"
#include "stankface/WavetableData.h"
#include "stankface/WavetableEngine.h"

#include <cmath>
#include <vector>

using namespace stankface;

namespace {

constexpr double kSampleRate = 48000.0;

double rms(const std::vector<float>& block)
{
    double sum = 0.0;
    for (float sample : block)
        sum += static_cast<double>(sample) * sample;
    return std::sqrt(sum / static_cast<double>(block.size()));
}

double peak(const std::vector<float>& block)
{
    double worst = 0.0;
    for (float sample : block)
        worst = std::max(worst, static_cast<double>(std::fabs(sample)));
    return worst;
}

std::vector<float> render(WavetableEngine& engine, int numSamples)
{
    std::vector<float> block(static_cast<std::size_t>(numSamples));
    engine.renderBlock(block.data(), numSamples);
    return block;
}

WavetableEngine makeEngine(VoiceMode mode = VoiceMode::Mono)
{
    WavetableEngine engine;
    engine.setSampleRate(kSampleRate);
    engine.setParam(ParamId::VoiceMode, static_cast<float>(static_cast<int>(mode)));
    return engine;
}

/** An engine set up to hold a steady tone, so that a spectrum taken from it
    reflects the notes sounding rather than an envelope in motion. */
WavetableEngine makeSustainingEngine(VoiceMode mode)
{
    WavetableEngine engine = makeEngine(mode);
    engine.setParam(ParamId::AmpAttack, 0.0f);
    engine.setParam(ParamId::AmpDecay, 0.0f);
    engine.setParam(ParamId::AmpSustain, 1.0f);
    engine.setParam(ParamId::FilterCutoff, 20000.0f);
    engine.setParam(ParamId::FilterResonance, 0.0f);
    engine.setParam(ParamId::Drive, 0.0f);
    engine.setParam(ParamId::WavetablePosition, 0.0f);
    return engine;
}

double midiNoteToHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

/** Energy in a few bins either side of a note's fundamental.

    A band rather than a single bin because a note's frequency does not land on
    a bin centre, and without a window the partial spreads into its neighbours.
*/
double energyAtNote(const std::vector<float>& block, int midiNote)
{
    const std::vector<double> spectrum = test::magnitudeSpectrum(block);
    const double bin = midiNoteToHz(midiNote)
                     * static_cast<double>(block.size()) / kSampleRate;

    const int lowest = std::max(1, static_cast<int>(bin) - 2);
    const int highest = std::min(static_cast<int>(spectrum.size()) - 1,
                                 static_cast<int>(bin) + 3);

    double energy = 0.0;
    for (int b = lowest; b <= highest; ++b)
        energy += spectrum[static_cast<std::size_t>(b)]
                * spectrum[static_cast<std::size_t>(b)];

    return energy;
}

} // namespace

TEST(engineIsSilentBeforeAnyNote)
{
    WavetableEngine engine = makeEngine();
    const std::vector<float> block = render(engine, 4096);

    CHECK(!engine.isActive());
    for (float sample : block)
        CHECK_NEAR(sample, 0.0, 0.0);
}

TEST(engineSoundsOnNoteOn)
{
    WavetableEngine engine = makeEngine();
    engine.noteOn(36, 1.0f);

    const std::vector<float> block = render(engine, 4096);

    CHECK(engine.isActive());
    CHECK_MESSAGE(rms(block) > 0.01, "note produced only " + std::to_string(rms(block)) + " RMS");
}

TEST(engineFallsSilentAfterNoteOff)
{
    WavetableEngine engine = makeEngine();
    engine.setParam(ParamId::AmpRelease, 0.05f);

    engine.noteOn(36, 1.0f);
    render(engine, 4096);

    engine.noteOff(36);

    // Release plus a margin for the filter tail.
    render(engine, static_cast<int>(0.2 * kSampleRate));

    const std::vector<float> tail = render(engine, 4096);
    CHECK(!engine.isActive());
    CHECK_MESSAGE(peak(tail) < 1.0e-6,
                  "still ringing at " + std::to_string(peak(tail)));
}

TEST(engineHonoursVelocity)
{
    WavetableEngine engine = makeEngine();
    engine.setParam(ParamId::AmpAttack, 0.001f);

    engine.noteOn(36, 1.0f);
    const double loud = rms(render(engine, 4096));

    engine.reset();
    engine.noteOn(36, 0.25f);
    const double quiet = rms(render(engine, 4096));

    CHECK(loud > quiet * 2.0);
}

TEST(engineUsesLastNotePriority)
{
    // Play a note over a held one, then let go of the top: the held note has
    // to come back rather than the voice cutting out.
    WavetableEngine engine = makeEngine();
    engine.setParam(ParamId::AmpSustain, 1.0f);

    engine.noteOn(36, 1.0f);
    render(engine, 2048);

    engine.noteOn(48, 1.0f);
    render(engine, 2048);

    engine.noteOff(48);
    const std::vector<float> afterRelease = render(engine, 4096);

    CHECK_MESSAGE(engine.isActive(), "voice stopped while a note was still held");
    CHECK(rms(afterRelease) > 0.01);

    engine.noteOff(36);
    render(engine, static_cast<int>(0.5 * kSampleRate));
    CHECK(!engine.isActive());
}

TEST(engineTracksPitch)
{
    // Two octaves apart should measure two octaves apart.
    const int fftSize = 8192;

    auto fundamentalBinOf = [&](int midiNote) {
        WavetableEngine engine = makeEngine();
        engine.setParam(ParamId::AmpAttack, 0.0f);
        engine.setParam(ParamId::AmpSustain, 1.0f);
        engine.setParam(ParamId::FilterCutoff, 20000.0f);
        engine.setParam(ParamId::Drive, 0.0f);
        engine.setParam(ParamId::WavetablePosition, 0.0f);
        engine.noteOn(midiNote, 1.0f);

        const std::vector<float> block = render(engine, fftSize);
        const std::vector<double> spectrum = test::magnitudeSpectrum(block);

        std::size_t loudest = 1;
        for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
            if (spectrum[bin] > spectrum[loudest])
                loudest = bin;

        return static_cast<double>(loudest);
    };

    const double low = fundamentalBinOf(36);   // 65.4 Hz
    const double high = fundamentalBinOf(60);  // 261.6 Hz

    CHECK_NEAR(high / low, 4.0, 0.1);
}

TEST(engineParametersClampToTheirRange)
{
    WavetableEngine engine = makeEngine();

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        const ParamDescriptor& d = paramDescriptor(id);

        engine.setParam(id, d.minValue - 1000.0f);
        CHECK_NEAR(engine.getParam(id), d.minValue, 1.0e-6);

        engine.setParam(id, d.maxValue + 1000.0f);
        CHECK_NEAR(engine.getParam(id), d.maxValue, 1.0e-6);
    }
}

TEST(normalisedParameterMappingRoundTrips)
{
    // Wrappers convert through this in both directions on every automation
    // move, so a mismatch would show up as controls that drift.
    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        const ParamDescriptor& d = paramDescriptor(id);
        const float range = d.maxValue - d.minValue;

        for (float normalised : { 0.0f, 0.1f, 0.5f, 0.9f, 1.0f })
        {
            const float natural = paramFromNormalised(id, normalised);
            CHECK(natural >= d.minValue - 1.0e-4f);
            CHECK(natural <= d.maxValue + 1.0e-4f);

            const float back = paramToNormalised(id, natural);
            CHECK_NEAR(back, normalised, 1.0e-4);
        }

        CHECK_NEAR(paramFromNormalised(id, 0.0f), d.minValue, range * 1.0e-5f + 1.0e-6f);
        CHECK_NEAR(paramFromNormalised(id, 1.0f), d.maxValue, range * 1.0e-5f + 1.0e-6f);
    }
}

TEST(engineStaysBoundedWithEverythingModulating)
{
    // Every table, full drive and resonance, both LFO routings at full depth.
    // Nothing here should be able to produce a non-finite or runaway sample.
    for (int table = 0; table < kNumWavetables; ++table)
    {
        WavetableEngine engine = makeEngine();
        engine.setParam(ParamId::WavetableSelect, static_cast<float>(table));
        engine.setParam(ParamId::WavetablePosition, 0.5f);
        engine.setParam(ParamId::FilterCutoff, 400.0f);
        engine.setParam(ParamId::FilterResonance, 1.0f);
        engine.setParam(ParamId::Drive, 1.0f);
        engine.setParam(ParamId::AmpSustain, 1.0f);
        engine.setParam(ParamId::LfoRate, 18.0f);
        engine.setParam(ParamId::LfoToPosition, 1.0f);
        engine.setParam(ParamId::LfoToCutoff, 1.0f);
        engine.setParam(ParamId::OutputGain, 1.0f);

        for (int note : { 24, 36, 48, 72, 96 })
        {
            engine.noteOn(note, 1.0f);
            const std::vector<float> block = render(engine, 16384);

            for (float sample : block)
            {
                CHECK(std::isfinite(sample));
                if (!std::isfinite(sample))
                    return;
            }

            CHECK_MESSAGE(peak(block) < 4.0,
                          std::string(kWavetableNames[table]) + " note "
                              + std::to_string(note) + " peaked at "
                              + std::to_string(peak(block)));
            engine.noteOff(note);
        }
    }
}

TEST(engineRespondsToWavetablePosition)
{
    // Position 0 is nearly a sine, position 1 is rich; the difference has to
    // show up as brightness or the morph is not wired up.
    auto brightness = [](float position) {
        WavetableEngine engine = makeEngine();
        engine.setParam(ParamId::AmpAttack, 0.0f);
        engine.setParam(ParamId::AmpSustain, 1.0f);
        engine.setParam(ParamId::FilterCutoff, 20000.0f);
        engine.setParam(ParamId::FilterResonance, 0.0f);
        engine.setParam(ParamId::Drive, 0.0f);
        engine.setParam(ParamId::WavetablePosition, position);
        engine.noteOn(36, 1.0f);

        const std::vector<float> block = render(engine, 8192);
        const std::vector<double> spectrum = test::magnitudeSpectrum(block);

        double lowEnergy = 0.0;
        double highEnergy = 0.0;
        for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
            (bin < 64 ? lowEnergy : highEnergy) += spectrum[bin] * spectrum[bin];

        return highEnergy / (lowEnergy + 1.0e-12);
    };

    CHECK(brightness(1.0f) > brightness(0.0f) * 10.0);
}

TEST(engineBlockSizeDoesNotChangeOutput)
{
    // Rendering the same note in one big block and in small ones has to give
    // the same samples, or the engine is carrying per-block state it should not.
    const int total = 4096;

    WavetableEngine a = makeEngine();
    a.setParam(ParamId::LfoToPosition, 0.8f);
    a.setParam(ParamId::LfoToCutoff, 0.5f);
    a.noteOn(40, 0.9f);
    const std::vector<float> oneBlock = render(a, total);

    WavetableEngine b = makeEngine();
    b.setParam(ParamId::LfoToPosition, 0.8f);
    b.setParam(ParamId::LfoToCutoff, 0.5f);
    b.noteOn(40, 0.9f);

    std::vector<float> manyBlocks;
    manyBlocks.reserve(static_cast<std::size_t>(total));
    for (int offset = 0; offset < total; offset += 37)
    {
        const int count = std::min(37, total - offset);
        const std::vector<float> chunk = render(b, count);
        manyBlocks.insert(manyBlocks.end(), chunk.begin(), chunk.end());
    }

    CHECK(manyBlocks.size() == oneBlock.size());
    for (std::size_t i = 0; i < oneBlock.size(); ++i)
        CHECK_NEAR(manyBlocks[i], oneBlock[i], 1.0e-6);
}


// ---------------------------------------------------------------------------
// Polyphony
// ---------------------------------------------------------------------------

TEST(engineDefaultsToMono)
{
    // This is a bass instrument, and every patch written before the pool
    // existed assumed one voice. Defaulting to poly would change how they all
    // sound on load.
    WavetableEngine engine;
    CHECK(static_cast<int>(engine.getParam(ParamId::VoiceMode))
          == static_cast<int>(VoiceMode::Mono));
}

TEST(monoPlaysOnlyTheLatestNote)
{
    WavetableEngine engine = makeSustainingEngine(VoiceMode::Mono);

    engine.noteOn(36, 1.0f);
    engine.noteOn(46, 1.0f);
    render(engine, 2048); // let the retune settle

    const std::vector<float> block = render(engine, 8192);

    const double firstNote = energyAtNote(block, 36);
    const double secondNote = energyAtNote(block, 46);

    CHECK_MESSAGE(secondNote > firstNote * 100.0,
                  "mono should be sounding only the newer note");
}

TEST(polyPlaysBothNotesAtOnce)
{
    WavetableEngine engine = makeSustainingEngine(VoiceMode::Poly);

    engine.noteOn(36, 1.0f);
    engine.noteOn(46, 1.0f);
    render(engine, 2048);

    const std::vector<float> block = render(engine, 8192);

    const double firstNote = energyAtNote(block, 36);
    const double secondNote = energyAtNote(block, 46);

    // Neither should be more than a few dB down on the other: both notes are
    // held, at the same velocity, on the same patch.
    CHECK_MESSAGE(firstNote > secondNote * 0.25,
                  "first note is missing or much quieter than the second");
    CHECK_MESSAGE(secondNote > firstNote * 0.25,
                  "second note is missing or much quieter than the first");
}

TEST(polyNoteOffReleasesOnlyThatNote)
{
    WavetableEngine engine = makeSustainingEngine(VoiceMode::Poly);
    engine.setParam(ParamId::AmpRelease, 0.01f);

    engine.noteOn(36, 1.0f);
    engine.noteOn(46, 1.0f);
    render(engine, 2048);

    engine.noteOff(36);
    render(engine, static_cast<int>(0.1 * kSampleRate)); // let it finish releasing

    const std::vector<float> block = render(engine, 8192);

    CHECK(engine.isActive());
    CHECK_MESSAGE(energyAtNote(block, 46) > energyAtNote(block, 36) * 100.0,
                  "releasing one note disturbed the one still held");

    engine.noteOff(46);
    render(engine, static_cast<int>(0.2 * kSampleRate));
    CHECK(!engine.isActive());
}

TEST(polyRepeatedNoteReusesItsVoice)
{
    // Two note-ons for the same pitch with no note-off between them should
    // retrigger one voice, not leave two copies running and beating.
    WavetableEngine engine = makeSustainingEngine(VoiceMode::Poly);

    engine.noteOn(36, 1.0f);
    render(engine, 2048);
    const double single = rms(render(engine, 4096));

    engine.noteOn(36, 1.0f);
    render(engine, 2048);
    const double repeated = rms(render(engine, 4096));

    CHECK_MESSAGE(repeated < single * 1.5,
                  "repeating a note stacked a second voice on the same pitch");
}

TEST(polyStealsWhenThePoolIsFull)
{
    // More notes held at once than there are voices. The newest note still has
    // to sound, and nothing may blow up on the way there.
    WavetableEngine engine = makeSustainingEngine(VoiceMode::Poly);

    for (int note = 30; note < 60; ++note)
    {
        engine.noteOn(note, 1.0f);
        const std::vector<float> block = render(engine, 256);

        for (float sample : block)
        {
            CHECK(std::isfinite(sample));
            if (!std::isfinite(sample))
                return;
        }
    }

    render(engine, 2048);
    const std::vector<float> block = render(engine, 8192);

    CHECK(engine.isActive());
    CHECK_MESSAGE(energyAtNote(block, 59) > 0.0,
                  "the most recent note did not get a voice");
    CHECK(std::isfinite(peak(block)));
}

TEST(polyChordStaysBounded)
{
    // A chord through full drive and resonance. Voices are summed without
    // scaling, so this is a headroom check rather than a claim of unity gain.
    WavetableEngine engine = makeEngine(VoiceMode::Poly);
    engine.setParam(ParamId::FilterResonance, 1.0f);
    engine.setParam(ParamId::Drive, 1.0f);
    engine.setParam(ParamId::AmpSustain, 1.0f);
    engine.setParam(ParamId::LfoRate, 12.0f);
    engine.setParam(ParamId::LfoToPosition, 1.0f);
    engine.setParam(ParamId::LfoToCutoff, 1.0f);
    engine.setParam(ParamId::OutputGain, 1.0f);

    for (int note : { 36, 43, 48, 51, 55, 60 })
        engine.noteOn(note, 1.0f);

    const std::vector<float> block = render(engine, 16384);

    for (float sample : block)
    {
        CHECK(std::isfinite(sample));
        if (!std::isfinite(sample))
            return;
    }

    CHECK_MESSAGE(peak(block) < static_cast<double>(6 * 4),
                  "chord peaked at " + std::to_string(peak(block)));
}

TEST(switchingVoiceModeReleasesHeldNotes)
{
    // Stale voices must not survive the switch, or notes held in one mode
    // would hang once the other took over their note handling.
    WavetableEngine engine = makeEngine(VoiceMode::Poly);
    engine.setParam(ParamId::AmpSustain, 1.0f);
    engine.setParam(ParamId::AmpRelease, 0.01f);

    engine.noteOn(36, 1.0f);
    engine.noteOn(48, 1.0f);
    render(engine, 2048);
    CHECK(engine.isActive());

    engine.setParam(ParamId::VoiceMode,
                    static_cast<float>(static_cast<int>(VoiceMode::Mono)));
    render(engine, static_cast<int>(0.2 * kSampleRate));

    CHECK_MESSAGE(!engine.isActive(), "a note survived the voice mode switch");
}

TEST(repeatedParameterPushesDoNotDisturbHeldNotes)
{
    // Wrappers push every parameter every block. A mode switch acts on change
    // only, so pushing the same value again must be inert.
    WavetableEngine engine = makeSustainingEngine(VoiceMode::Poly);
    engine.noteOn(36, 1.0f);
    render(engine, 2048);

    const double before = rms(render(engine, 4096));

    for (int i = 0; i < 16; ++i)
    {
        for (int p = 0; p < kNumParams; ++p)
        {
            const ParamId id = static_cast<ParamId>(p);
            engine.setParam(id, engine.getParam(id));
        }
        render(engine, 256);
    }

    const double after = rms(render(engine, 4096));

    CHECK(engine.isActive());
    CHECK_NEAR(after, before, before * 0.05);
}
