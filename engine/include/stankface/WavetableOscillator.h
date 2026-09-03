#pragma once

namespace stankface {

/** Morphing wavetable oscillator with mipmap selection.

    Two interpolations happen per sample:

    - across frames, so `position` sweeps continuously through the table
      rather than stepping between its frames;
    - across mip levels, so the band limit follows the pitch. Levels are
      crossfaded rather than switched, because a hard switch is audible as a
      click when a glide or heavy pitch modulation crosses a boundary.
*/
class WavetableOscillator
{
public:
    void setSampleRate(double sampleRate);

    /** Index into the generated wavetable sets. Clamped. */
    void setTable(int tableIndex);

    /** Sets the pitch, and with it the mip level: the table can only carry
        harmonics up to Nyquist / frequency without aliasing. */
    void setFrequency(float hz);

    /** 0..1 across the frames of the current table. */
    void setPosition(float position);

    void resetPhase();
    void reset();

    float nextSample();

    /** The fractional mip level currently in use. Exposed for tests. */
    float currentMipLevel() const { return mipLevel_; }

private:
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;        // 0..1
    double increment_ = 0.0;

    int tableIndex_ = 0;
    float frequency_ = 440.0f;
    float position_ = 0.0f;
    float mipLevel_ = 0.0f;     // fractional; interpolated between neighbours

    void updateMipLevel();
};

} // namespace stankface
