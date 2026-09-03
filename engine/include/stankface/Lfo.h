#pragma once

namespace stankface {

enum class LfoShape
{
    Sine = 0,
    Triangle,
    Saw,
    Square,
    NumShapes
};

/** A single bipolar low-frequency oscillator, output in -1..1.

    Retriggered on note-on rather than left free-running: on a monophonic
    instrument that makes a wobble land in the same place every time a note is
    played, which is what you want when the LFO is driving wavetable position.
*/
class Lfo
{
public:
    void setSampleRate(double sampleRate);
    void setRate(float hz);
    void setShape(LfoShape shape);

    /** Restart the cycle. Called on note-on. */
    void retrigger();
    void reset();

    float nextSample();

private:
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;      // 0..1
    double increment_ = 0.0;
    float rateHz_ = 1.0f;
    LfoShape shape_ = LfoShape::Sine;

    void updateIncrement();
};

} // namespace stankface
