#include "stankface/Envelope.h"

#include <cmath>

namespace stankface {
namespace {

// How far past the target each stage aims. Overshooting is what makes the
// curve exponential yet still finish: the stage ends when the level crosses
// its real target, not when it asymptotically approaches it.
constexpr float kAttackOvershoot = 0.60653066f;   // exp(-0.5)
constexpr float kDecayOvershoot  = 0.00673794699f; // exp(-5)

float stageCoefficient(float samples, float overshoot)
{
    if (samples <= 0.0f)
        return 0.0f;

    return std::exp(-std::log((1.0f + overshoot) / overshoot) / samples);
}

} // namespace

void Envelope::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateAttack();
    updateDecay();
    updateRelease();
}

void Envelope::setAttack(float seconds)
{
    attackSeconds_ = seconds < 0.0f ? 0.0f : seconds;
    updateAttack();
}

void Envelope::setDecay(float seconds)
{
    decaySeconds_ = seconds < 0.0f ? 0.0f : seconds;
    updateDecay();
}

void Envelope::setSustain(float level)
{
    sustainLevel_ = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    updateDecay();
}

void Envelope::setRelease(float seconds)
{
    releaseSeconds_ = seconds < 0.0f ? 0.0f : seconds;
    updateRelease();
}

void Envelope::updateAttack()
{
    const float samples = static_cast<float>(attackSeconds_ * sampleRate_);
    attackCoef_ = stageCoefficient(samples, kAttackOvershoot);
    attackBase_ = (1.0f + kAttackOvershoot) * (1.0f - attackCoef_);
}

void Envelope::updateDecay()
{
    const float samples = static_cast<float>(decaySeconds_ * sampleRate_);
    decayCoef_ = stageCoefficient(samples, kDecayOvershoot);
    decayBase_ = (sustainLevel_ - kDecayOvershoot) * (1.0f - decayCoef_);
}

void Envelope::updateRelease()
{
    const float samples = static_cast<float>(releaseSeconds_ * sampleRate_);
    releaseCoef_ = stageCoefficient(samples, kDecayOvershoot);
    releaseBase_ = -kDecayOvershoot * (1.0f - releaseCoef_);
}

void Envelope::noteOn()
{
    // Deliberately does not zero the level: retriggering while a previous note
    // is still ringing should climb from where it is, not click back to zero.
    stage_ = Stage::Attack;
}

void Envelope::noteOff()
{
    if (stage_ != Stage::Idle)
        stage_ = Stage::Release;
}

void Envelope::reset()
{
    level_ = 0.0f;
    stage_ = Stage::Idle;
}

float Envelope::nextSample()
{
    switch (stage_)
    {
        case Stage::Idle:
            break;

        case Stage::Attack:
            level_ = attackBase_ + level_ * attackCoef_;
            if (level_ >= 1.0f || attackCoef_ == 0.0f)
            {
                level_ = 1.0f;
                stage_ = Stage::Decay;
            }
            break;

        case Stage::Decay:
            level_ = decayBase_ + level_ * decayCoef_;
            if (level_ <= sustainLevel_ || decayCoef_ == 0.0f)
            {
                level_ = sustainLevel_;
                stage_ = Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            level_ = sustainLevel_;
            break;

        case Stage::Release:
            level_ = releaseBase_ + level_ * releaseCoef_;
            if (level_ <= 0.0f || releaseCoef_ == 0.0f)
            {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
    }

    return level_;
}

} // namespace stankface
