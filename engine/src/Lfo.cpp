#include "stankface/Lfo.h"

#include <cmath>

namespace stankface {

void Lfo::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateIncrement();
}

void Lfo::setRate(float hz)
{
    rateHz_ = hz < 0.0f ? 0.0f : hz;
    updateIncrement();
}

void Lfo::setShape(LfoShape shape)
{
    shape_ = shape;
}

void Lfo::updateIncrement()
{
    increment_ = static_cast<double>(rateHz_) / sampleRate_;
}

void Lfo::retrigger()
{
    phase_ = 0.0;
}

void Lfo::reset()
{
    phase_ = 0.0;
}

float Lfo::nextSample()
{
    const double p = phase_;

    phase_ += increment_;
    if (phase_ >= 1.0)
        phase_ -= 1.0;

    switch (shape_)
    {
        case LfoShape::Sine:
            return static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * p));

        case LfoShape::Triangle:
            // Rises through the first half of the cycle, falls through the second.
            return static_cast<float>(p < 0.5 ? (4.0 * p - 1.0) : (3.0 - 4.0 * p));

        case LfoShape::Saw:
            return static_cast<float>(2.0 * p - 1.0);

        case LfoShape::Square:
            return p < 0.5 ? 1.0f : -1.0f;

        case LfoShape::NumShapes:
        default:
            return 0.0f;
    }
}

} // namespace stankface
