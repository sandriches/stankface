#pragma once

namespace stankface {

/** Analogue-style ADSR.

    Each stage is a one-pole moving toward an overshoot target, so the curve is
    exponential -- the shape a hardware envelope actually makes -- while still
    reaching its destination in the time asked for, which a plain one-pole
    aiming straight at the target never would.
*/
class Envelope
{
public:
    enum class Stage
    {
        Idle = 0,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void setSampleRate(double sampleRate);

    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float level);
    void setRelease(float seconds);

    void noteOn();
    void noteOff();
    void reset();

    float nextSample();

    float currentLevel() const { return level_; }
    Stage stage() const { return stage_; }

    /** False once the release has run out, so a voice knows it can stop. */
    bool isActive() const { return stage_ != Stage::Idle; }

private:
    double sampleRate_ = 44100.0;

    float attackSeconds_ = 0.005f;
    float decaySeconds_ = 0.1f;
    float sustainLevel_ = 0.8f;
    float releaseSeconds_ = 0.2f;

    float attackCoef_ = 0.0f,  attackBase_ = 0.0f;
    float decayCoef_ = 0.0f,   decayBase_ = 0.0f;
    float releaseCoef_ = 0.0f, releaseBase_ = 0.0f;

    float level_ = 0.0f;
    Stage stage_ = Stage::Idle;

    void updateAttack();
    void updateDecay();
    void updateRelease();
};

} // namespace stankface
