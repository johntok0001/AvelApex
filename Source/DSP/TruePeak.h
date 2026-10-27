#pragma once

#include <array>
#include <cmath>

namespace avel::dsp
{
    // 16x polyphase FIR interpolator used to estimate inter-sample (true) peaks (stricter than the 4x of
    // ITU-R BS.1770 Annex 2: 48 taps per phase, so the long sinc tails that a limiter can exploit are included).
    // process() returns the largest |value| of the interpolated phases, delayed by `delay` input samples.
    // Samples are only interpolated while the recent input is loud enough to possibly exceed `quietBelow`
    // after reconstruction; otherwise the plain |sample| is returned (saves CPU, never under-reports an over).
    class TruePeakDetector
    {
    public:
        static constexpr int phases = 16;
        static constexpr int tapsPerPhase = 48;
        static constexpr int delay = tapsPerPhase / 2;

        TruePeakDetector()
        {
            constexpr int N = phases * tapsPerPhase;
            constexpr double pi = 3.14159265358979323846;
            const double centre = (N - 1) * 0.5;
            std::array<double, N> h {};
            for (int n = 0; n < N; ++n)
            {
                const double t = (n - centre) / phases;
                const double sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (pi * t) / (pi * t);
                // Kaiser window, beta 7
                const double r = (n - centre) / centre;
                const double w = bessel0 (7.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (7.0);
                h[(size_t) n] = sinc * w;
            }
            double maxAbs = 0;
            for (int p = 0; p < phases; ++p)
            {
                double sum = 0, sumAbs = 0;
                for (int k = 0; k < tapsPerPhase; ++k) sum += h[(size_t) (p + k * phases)];
                for (int k = 0; k < tapsPerPhase; ++k)
                {
                    coeff[(size_t) p][(size_t) (tapsPerPhase - 1 - k)] = (float) (h[(size_t) (p + k * phases)] / sum);
                    sumAbs += std::abs (h[(size_t) (p + k * phases)] / sum);
                }
                maxAbs = std::max (maxAbs, sumAbs);
            }
            gainBound = (float) maxAbs;
            reset();
        }

        void reset() noexcept { hist.fill (0.0f); pos = 0; sinceLoud = tapsPerPhase + 1; }

        float process (float x, float quietBelow = 0.0f) noexcept
        {
            // history is stored twice so the last `tapsPerPhase` samples are always contiguous
            if (++pos >= tapsPerPhase) pos = 0;
            hist[(size_t) pos] = hist[(size_t) (pos + tapsPerPhase)] = x;
            const float* w = hist.data() + pos + 1;   // oldest .. newest

            if (std::abs (x) * gainBound >= quietBelow) sinceLoud = 0;
            else if (sinceLoud <= tapsPerPhase) ++sinceLoud;
            if (sinceLoud > tapsPerPhase)
                return std::abs (w[tapsPerPhase - 1 - delay]);

            float m = 0.0f;
            for (int p = 0; p < phases; ++p)
            {
                const float* c = coeff[(size_t) p].data();
                float a[8] {};
                for (int k = 0; k < tapsPerPhase; k += 8)
                    for (int j = 0; j < 8; ++j) a[j] += c[k + j] * w[k + j];
                const float y = ((a[0] + a[1]) + (a[2] + a[3])) + ((a[4] + a[5]) + (a[6] + a[7]));
                m = std::max (m, std::abs (y));
            }
            return m;
        }

    private:
        static double bessel0 (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 30; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
            return sum;
        }

        std::array<std::array<float, tapsPerPhase>, phases> coeff {};
        std::array<float, 2 * tapsPerPhase> hist {};
        static_assert (tapsPerPhase % 8 == 0);
        float gainBound = 2.0f;
        int pos = 0, sinceLoud = 0;
    };
}
