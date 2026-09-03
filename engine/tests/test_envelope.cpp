#include "TestFramework.h"

#include "stankface/Envelope.h"

using namespace stankface;

namespace {

constexpr double kSampleRate = 48000.0;

int samplesFor(double seconds)
{
    return static_cast<int>(seconds * kSampleRate);
}

} // namespace

TEST(envelopeRunsThroughAllStages)
{
    Envelope env;
    env.setSampleRate(kSampleRate);
    env.setAttack(0.01f);
    env.setDecay(0.05f);
    env.setSustain(0.5f);
    env.setRelease(0.02f);

    CHECK(!env.isActive());
    CHECK_NEAR(env.currentLevel(), 0.0, 1.0e-6);

    env.noteOn();
    CHECK(env.isActive());

    // Attack should reach full scale within the time asked for. An envelope
    // aiming straight at its target would still be climbing here.
    for (int i = 0; i < samplesFor(0.01); ++i)
        env.nextSample();
    CHECK_NEAR(env.currentLevel(), 1.0, 0.02);

    for (int i = 0; i < samplesFor(0.05); ++i)
        env.nextSample();
    CHECK_NEAR(env.currentLevel(), 0.5, 0.02);

    // Sustain holds indefinitely.
    for (int i = 0; i < samplesFor(0.2); ++i)
        env.nextSample();
    CHECK_NEAR(env.currentLevel(), 0.5, 1.0e-5);
    CHECK(env.stage() == Envelope::Stage::Sustain);

    env.noteOff();
    for (int i = 0; i < samplesFor(0.02); ++i)
        env.nextSample();

    CHECK_NEAR(env.currentLevel(), 0.0, 1.0e-5);
    CHECK(!env.isActive());
}

TEST(envelopeAttackIsMonotonic)
{
    Envelope env;
    env.setSampleRate(kSampleRate);
    env.setAttack(0.05f);
    env.setDecay(0.05f);
    env.setSustain(1.0f);
    env.setRelease(0.05f);
    env.noteOn();

    float previous = -1.0f;
    for (int i = 0; i < samplesFor(0.05); ++i)
    {
        const float level = env.nextSample();
        CHECK(level >= previous);
        previous = level;
    }
}

TEST(envelopeHandlesZeroLengthStages)
{
    // A zero attack has to jump straight to full scale rather than stall,
    // which is exactly what a percussive bass patch asks for.
    Envelope env;
    env.setSampleRate(kSampleRate);
    env.setAttack(0.0f);
    env.setDecay(0.0f);
    env.setSustain(0.7f);
    env.setRelease(0.0f);

    env.noteOn();
    env.nextSample();
    CHECK_NEAR(env.currentLevel(), 1.0, 1.0e-6);

    env.nextSample();
    CHECK_NEAR(env.currentLevel(), 0.7, 1.0e-6);

    env.noteOff();
    env.nextSample();
    CHECK_NEAR(env.currentLevel(), 0.0, 1.0e-6);
    CHECK(!env.isActive());
}

TEST(envelopeRetriggerClimbsFromCurrentLevel)
{
    // Retriggering mid-release must not snap to zero first: that click is one
    // of the more obvious ways a mono synth sounds broken under fast playing.
    Envelope env;
    env.setSampleRate(kSampleRate);
    env.setAttack(0.05f);
    env.setDecay(0.05f);
    env.setSustain(0.8f);
    env.setRelease(0.5f);

    env.noteOn();
    for (int i = 0; i < samplesFor(0.1); ++i)
        env.nextSample();

    env.noteOff();
    for (int i = 0; i < samplesFor(0.05); ++i)
        env.nextSample();

    const float beforeRetrigger = env.currentLevel();
    CHECK(beforeRetrigger > 0.1f);

    env.noteOn();
    const float afterRetrigger = env.nextSample();
    CHECK(afterRetrigger >= beforeRetrigger);
}

TEST(envelopeIsSilentUntilNoteOn)
{
    Envelope env;
    env.setSampleRate(kSampleRate);

    for (int i = 0; i < 1000; ++i)
        CHECK_NEAR(env.nextSample(), 0.0, 1.0e-9);
}
