#pragma once

#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

// Spectrum helpers. The aliasing tests need to see which frequencies actually
// came out of the oscillator, which is the only way to check the band limiting
// does what it claims.

namespace test {

constexpr double kPi = 3.14159265358979323846;

inline void fftInPlace(std::vector<std::complex<double>>& a)
{
    const std::size_t n = a.size();

    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * kPi / static_cast<double>(len);
        const std::complex<double> wLen(std::cos(angle), std::sin(angle));

        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wLen;
            }
        }
    }
}

/** Magnitude spectrum, bins 0..N/2.

    No window is applied, and none is wanted: the tests choose frequencies that
    are an exact whole number of cycles in the buffer, so every partial lands
    dead on a bin. A window would smear each partial across its neighbours and
    hide the very thing being measured.
*/
inline std::vector<double> magnitudeSpectrum(const std::vector<float>& signal)
{
    std::vector<std::complex<double>> buffer(signal.size());
    for (std::size_t i = 0; i < signal.size(); ++i)
        buffer[i] = std::complex<double>(signal[i], 0.0);

    fftInPlace(buffer);

    std::vector<double> magnitude(signal.size() / 2 + 1);
    for (std::size_t i = 0; i < magnitude.size(); ++i)
        magnitude[i] = std::abs(buffer[i]);

    return magnitude;
}

inline double toDecibels(double magnitude, double reference)
{
    if (reference <= 0.0)
        return -400.0;
    const double ratio = magnitude / reference;
    return ratio <= 1.0e-20 ? -400.0 : 20.0 * std::log10(ratio);
}

} // namespace test
