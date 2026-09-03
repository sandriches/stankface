#pragma once

namespace stankface {

/** Saturating drive into a resonant lowpass.

    A topology-preserving-transform state variable filter (Zavalishin), which
    stays well-behaved when the cutoff is modulated hard -- the naive digital
    forms blow up or detune under exactly the fast sweeps this synth is built
    to do.

    Two nonlinearities give it its character, and between them they are most of
    what makes the "UK bass" tone rather than a clean sweep:
      - a saturating input stage, so pushing level into the filter adds
        harmonics instead of just getting louder;
      - a saturator on the resonant integrator state, which compresses the
        resonant peak as it grows instead of letting it ring away cleanly.
*/
class Filter
{
public:
    void setSampleRate(double sampleRate);

    /** Cutoff in Hz. Clamped internally to a stable range for the sample rate. */
    void setCutoff(float hz);

    /** 0..1, where 1 approaches self-oscillation. */
    void setResonance(float resonance);

    /** 0..1 input saturation. Output is level-compensated, so turning drive up
        changes the tone rather than just the volume. */
    void setDrive(float drive);

    void reset();

    float process(float input);

private:
    double sampleRate_ = 44100.0;

    float cutoffHz_ = 1000.0f;
    float resonance_ = 0.0f;
    float driveAmount_ = 0.0f;

    float g_ = 0.0f;   // tan(pi * fc / sr)
    float k_ = 2.0f;   // damping, 1/Q
    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;

    float driveGain_ = 1.0f;
    float driveCompensation_ = 1.0f;

    float ic1eq_ = 0.0f;
    float ic2eq_ = 0.0f;

    void updateCoefficients();
    void updateDrive();
};

} // namespace stankface
