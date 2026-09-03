#include "stankface/Filter.h"

#include <cmath>

namespace stankface {
namespace {

constexpr float kPi = 3.14159265358979323846f;

/** Bounded soft clipper.

    A Pade approximation of tanh, which is cheap but runs away outside roughly
    +/-3, so the input is clamped first. At the clamp point the approximation
    reads 1.0 against tanh's 0.995, close enough that the join is inaudible,
    and clamping is what makes it safe to put in a feedback path.
*/
inline float softClip(float x)
{
    x = x < -3.0f ? -3.0f : (x > 3.0f ? 3.0f : x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** The input level the drive stage is trimmed around -- roughly what the
    oscillator puts out. */
constexpr float kDriveReferenceLevel = 0.7f;

/** Keeps denormals out of the integrator states, where they would otherwise
    settle after a note ends and cost far more than the arithmetic. */
inline float flushDenormal(float x)
{
    return (x > -1.0e-25f && x < 1.0e-25f) ? 0.0f : x;
}

} // namespace

void Filter::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateCoefficients();
}

void Filter::setCutoff(float hz)
{
    cutoffHz_ = hz;
    updateCoefficients();
}

void Filter::setResonance(float resonance)
{
    resonance_ = resonance < 0.0f ? 0.0f : (resonance > 1.0f ? 1.0f : resonance);
    updateCoefficients();
}

void Filter::setDrive(float drive)
{
    driveAmount_ = drive < 0.0f ? 0.0f : (drive > 1.0f ? 1.0f : drive);
    updateDrive();
}

void Filter::updateDrive()
{
    // 1x to 24x into the clipper, with the output scaled so that a signal at
    // kDriveReferenceLevel comes out at the same level whatever the drive is
    // set to. Compensating at a realistic level rather than at small-signal
    // gain is the point: the oscillator runs near full scale, so that is the
    // level the knob should feel neutral at. Quieter material still gets
    // pushed up, which is what a drive stage is supposed to do.
    driveGain_ = 1.0f + driveAmount_ * 23.0f;
    driveCompensation_ =
        kDriveReferenceLevel / softClip(driveGain_ * kDriveReferenceLevel);
}

void Filter::updateCoefficients()
{
    // Hold the cutoff below Nyquist. The prewarp tangent runs away as it
    // approaches, and modulation can otherwise push it there.
    const float maxCutoff = static_cast<float>(sampleRate_ * 0.45);
    float fc = cutoffHz_ < 20.0f ? 20.0f : (cutoffHz_ > maxCutoff ? maxCutoff : cutoffHz_);

    g_ = std::tan(kPi * fc / static_cast<float>(sampleRate_));

    // Damping runs from 2 (Q = 0.5, no peak) down towards 0 (self-oscillation).
    // The floor keeps it just short of oscillating on its own.
    k_ = 2.0f - 1.94f * resonance_;

    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
}

void Filter::reset()
{
    ic1eq_ = 0.0f;
    ic2eq_ = 0.0f;
}

float Filter::process(float input)
{
    const float driven = softClip(input * driveGain_) * driveCompensation_;

    // Topology-preserving state variable filter, lowpass output.
    const float v3 = driven - ic2eq_;
    const float v1 = a1_ * ic1eq_ + a2_ * v3;
    const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;

    // Saturating the bandpass integrator is what makes resonance compress as
    // it builds rather than ring on cleanly.
    ic1eq_ = flushDenormal(softClip(2.0f * v1 - ic1eq_));
    ic2eq_ = flushDenormal(2.0f * v2 - ic2eq_);

    return v2;
}

} // namespace stankface
