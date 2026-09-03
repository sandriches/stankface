#include "stankface/WavetableOscillator.h"

#include "stankface/WavetableData.h"

namespace stankface {
namespace {

/** How close a level may get to its own Nyquist limit before the oscillator
    starts crossfading into the next level down.

    Expressed as headroom: at 1.25x the level is still used on its own, at 1.0x
    it has been fully replaced. It has to stay below the ratio between adjacent
    levels (1.41 at half-octave spacing, and 1.33 at the coarsest of the
    rounded top levels) so that a crossfade always finishes before the level
    below it comes into play, or the two blend regions would overlap and the
    band limit would jump.
*/
constexpr float kMipBlendHeadroom = 0.25f;

/** One sample from a single frame at a single mip level, four-point cubic.

    Catmull-Rom rather than linear. Linear interpolation is itself a distortion
    generator -- its error falls only with the square of the sample spacing, so
    on the short tables used for high notes it produces more spurious content
    than the band limiting removes, and it shows up in a spectrum as aliasing
    even though nothing has actually folded. Cubic error falls with the fourth
    power, which buys far more than spending the same memory on longer tables
    would.

    No wrap checks: each level is stored with guard samples either side, so
    index-1 through index+2 are always in bounds.
*/
inline float readFrame(const float* frame, int length, double phase)
{
    const double x = phase * static_cast<double>(length);
    const int index = static_cast<int>(x);
    const float t = static_cast<float>(x - static_cast<double>(index));

    const float p0 = frame[index - 1];
    const float p1 = frame[index];
    const float p2 = frame[index + 1];
    const float p3 = frame[index + 2];

    const float a = 3.0f * (p1 - p2) + p3 - p0;
    const float b = 2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3;
    const float c = p2 - p0;

    return p1 + 0.5f * t * (c + t * (b + t * a));
}

/** Both frames either side of `position`, crossfaded, at one mip level. */
inline float readMipLevel(int table, int mip, int frameLow, int frameHigh,
                          float frameFrac, double phase)
{
    const int length = kMipLength[mip];
    const float low  = readFrame(wavetableFrame(table, frameLow,  mip), length, phase);
    const float high = readFrame(wavetableFrame(table, frameHigh, mip), length, phase);
    return low + frameFrac * (high - low);
}

} // namespace

void WavetableOscillator::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    increment_ = static_cast<double>(frequency_) / sampleRate_;
    updateMipLevel();
}

void WavetableOscillator::setTable(int tableIndex)
{
    tableIndex_ = tableIndex < 0 ? 0
                : (tableIndex >= kNumWavetables ? kNumWavetables - 1 : tableIndex);
}

void WavetableOscillator::setFrequency(float hz)
{
    frequency_ = hz < 0.0f ? 0.0f : hz;
    increment_ = static_cast<double>(frequency_) / sampleRate_;
    updateMipLevel();
}

void WavetableOscillator::setPosition(float position)
{
    position_ = position < 0.0f ? 0.0f : (position > 1.0f ? 1.0f : position);
}

void WavetableOscillator::updateMipLevel()
{
    // The highest harmonic that still fits under Nyquist at this pitch.
    const float nyquist = static_cast<float>(sampleRate_ * 0.5);
    const float freq = frequency_ > 1.0e-4f ? frequency_ : 1.0e-4f;
    const float maxHarmonic = nyquist / freq;

    // The brightest level that fits. Searched against the table rather than
    // derived from a formula: the harmonic counts are rounded to integers, so
    // a closed form would be off by one around the boundaries -- and being off
    // by one on the bright side is exactly the aliasing this is here to stop.
    int safe = 0;
    while (safe < kNumMipLevels - 1
           && static_cast<float>(kMipHarmonics[safe]) > maxHarmonic)
    {
        ++safe;
    }

    // Crossfade into the next level down as this one approaches its limit, so
    // that a glide or heavy pitch modulation changes band limit smoothly
    // instead of stepping. The fade only ever goes darker: blending in the
    // brighter neighbour would blend in its aliasing along with it.
    const float headroom = maxHarmonic / static_cast<float>(kMipHarmonics[safe]);
    float blend = (1.0f + kMipBlendHeadroom - headroom) / kMipBlendHeadroom;
    blend = blend < 0.0f ? 0.0f : (blend > 1.0f ? 1.0f : blend);

    const float maxLevel = static_cast<float>(kNumMipLevels - 1);
    const float level = static_cast<float>(safe) + blend;
    mipLevel_ = level > maxLevel ? maxLevel : level;
}

void WavetableOscillator::resetPhase()
{
    phase_ = 0.0;
}

void WavetableOscillator::reset()
{
    phase_ = 0.0;
}

float WavetableOscillator::nextSample()
{
    const float framePos = position_ * static_cast<float>(kNumFrames - 1);
    int frameLow = static_cast<int>(framePos);
    if (frameLow > kNumFrames - 2)
        frameLow = kNumFrames - 2;
    if (frameLow < 0)
        frameLow = 0;
    const float frameFrac = framePos - static_cast<float>(frameLow);

    const int mipLow = static_cast<int>(mipLevel_);
    const int mipHigh = mipLow + 1 < kNumMipLevels ? mipLow + 1 : mipLow;
    const float mipFrac = mipLevel_ - static_cast<float>(mipLow);

    const float low = readMipLevel(tableIndex_, mipLow, frameLow, frameLow + 1,
                                   frameFrac, phase_);

    float out = low;
    if (mipHigh != mipLow && mipFrac > 0.0f)
    {
        // Crossfaded rather than switched: a hard change of band limit is
        // audible as a click when a glide crosses the boundary.
        const float high = readMipLevel(tableIndex_, mipHigh, frameLow, frameLow + 1,
                                        frameFrac, phase_);
        out = low + mipFrac * (high - low);
    }

    phase_ += increment_;
    if (phase_ >= 1.0)
        phase_ -= static_cast<double>(static_cast<int>(phase_));

    return out;
}

} // namespace stankface
