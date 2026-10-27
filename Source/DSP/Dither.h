#pragma once

#include <cmath>
#include <cstdint>

namespace avel::dsp
{
    // TPDF dither with optional 3-tap error-feedback noise shaping (moves the noise above ~10 kHz).
    class Dither
    {
    public:
        void reset() noexcept { for (auto& e : err) e[0] = e[1] = e[2] = 0.0f; }

        void process (float* const* data, int numChannels, int numSamples, int bits, bool shaped) noexcept
        {
            if (bits <= 0) return;
            const float q = std::ldexp (1.0f, -(bits - 1));   // one LSB for a +-1.0 full scale
            const float invQ = 1.0f / q;
            for (int c = 0; c < numChannels && c < 2; ++c)
            {
                auto* x = data[c];
                auto& e = err[c];
                for (int i = 0; i < numSamples; ++i)
                {
                    const float v = shaped ? x[i] - (1.623f * e[0] - 0.982f * e[1] + 0.109f * e[2]) : x[i];
                    const float tpdf = (rnd() - rnd()) * q;
                    const float out = std::floor ((v + tpdf) * invQ + 0.5f) * q;
                    e[2] = e[1]; e[1] = e[0]; e[0] = out - v;
                    x[i] = out;
                }
            }
        }

    private:
        float rnd() noexcept
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }

        float err[2][3] {};
        uint32_t state = 0x9e3779b9u;
    };
}
