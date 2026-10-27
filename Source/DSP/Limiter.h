#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "TruePeak.h"

namespace avel::dsp
{
    // Character of each limiting style. Times are multipliers of the user's Attack / Release.
    struct Style
    {
        const char* name;
        const char* description;
        float fastRelease;  // release of the fast (transient) stage
        float slowAttack;   // attack of the slow (sustain) stage
        float slowRelease;  // release of the slow stage
        float slowWeight;   // how much of the slow stage's reduction is held (0..1)
        float clip;         // share of each transient left to the soft clipper (0..1)
        bool smoothAttack;  // S-shaped (two cascaded windows) or linear attack ramp
    };

    inline const std::array<Style, 8>& styles()
    {
        static const std::array<Style, 8> s {{
            { "Transparent", "Clean and open. Long attack ramp, program-dependent release.", 1.0f, 1.0f, 4.0f, 1.00f, 0.00f, true },
            { "Punchy",      "Keeps the snap of drums: shorter ramp, quick release, a touch of clipping.", 0.5f, 0.6f, 2.0f, 0.85f, 0.15f, false },
            { "Dynamic",     "Lets transients breathe: very fast transient release, slow body stage.", 0.25f, 1.5f, 3.0f, 1.00f, 0.05f, true },
            { "Aggressive",  "Loud and forward. Fast release and soft clipping of the peaks.", 0.35f, 0.5f, 1.5f, 0.70f, 0.35f, false },
            { "Allround",    "A safe default for most material, between Transparent and Punchy.", 0.7f, 1.0f, 3.0f, 0.90f, 0.10f, true },
            { "Bus",         "Gentle and glue-like, for groups and stems.", 1.5f, 2.0f, 5.0f, 1.00f, 0.00f, true },
            { "Safe",        "Avoids distortion at all cost: slow, smooth release.", 1.2f, 1.0f, 6.0f, 1.00f, 0.00f, true },
            { "Modern",      "Dense and loud, yet controlled. Adaptive two-stage release.", 0.6f, 0.8f, 3.0f, 0.95f, 0.10f, true },
        }};
        return s;
    }

    //==============================================================================
    // Sliding maximum over the last `window` values (monotonic deque, O(1) amortised).
    class SlidingMax
    {
    public:
        void prepare (int maxWindow) { cap = 1; while (cap < maxWindow + 2) cap <<= 1; val.assign ((size_t) cap, 0.0f); idx.assign ((size_t) cap, 0); reset (1); }
        void reset (int newWindow) noexcept { window = std::max (1, newWindow); head = tail = 0; n = 0; }

        float push (float v) noexcept
        {
            while (tail != head && val[(size_t) ((tail - 1) & (cap - 1))] <= v) tail = (tail - 1) & (cap - 1);
            val[(size_t) tail] = v; idx[(size_t) tail] = n; tail = (tail + 1) & (cap - 1);
            while (idx[(size_t) head] <= n - window) head = (head + 1) & (cap - 1);
            ++n;
            return val[(size_t) head];
        }

    private:
        std::vector<float> val;
        std::vector<juce::int64> idx;
        int cap = 1, head = 0, tail = 0, window = 1;
        juce::int64 n = 0;
    };

    // Moving average over the last `length` values.
    class BoxFilter
    {
    public:
        void prepare (int maxLength) { buf.assign ((size_t) maxLength + 1, 0.0f); reset (1, 0.0f); }
        void reset (int newLength, float fillValue) noexcept
        {
            len = std::max (1, std::min (newLength, (int) buf.size()));
            std::fill (buf.begin(), buf.begin() + len, fillValue);
            sum = (double) fillValue * len; pos = 0; count = 0;
        }

        float push (float v) noexcept
        {
            sum += (double) v - buf[(size_t) pos];
            buf[(size_t) pos] = v;
            if (++pos >= len) pos = 0;
            if (++count >= 4096) { count = 0; sum = 0; for (int i = 0; i < len; ++i) sum += buf[(size_t) i]; } // no drift
            return (float) std::max (0.0, sum / len);
        }

    private:
        std::vector<float> buf;
        double sum = 0;
        int len = 1, pos = 0, count = 0;
    };

    class DelayLine
    {
    public:
        void prepare (int maxDelay) { size = 1; while (size < maxDelay + 1) size <<= 1; buf.assign ((size_t) size, 0.0f); w = 0; }
        void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }
        float push (float x, int delay) noexcept
        {
            buf[(size_t) w] = x;
            const float y = buf[(size_t) ((w - delay) & (size - 1))];
            w = (w + 1) & (size - 1);
            return y;
        }
    private:
        std::vector<float> buf;
        int size = 1, w = 0;
    };

    //==============================================================================
    // Look-ahead peak limiter for 1 or 2 channels.
    //
    // Gain computer (all in dB of gain reduction, per sample):
    //   required GR  -> transient channel link -> fast stage (instant attack, release)
    //                                          + slow stage (attack, slow release, weighted)
    //   -> release channel link -> sliding max over the look-ahead window -> 1 or 2 moving averages.
    // The moving averages are fed by a max over a window that covers them, so the reduction is
    // always fully in place when the peak reaches the gain stage: no overshoot by construction.
    // A final clipper at the ceiling (soft for styles with `clip` > 0, hard otherwise) catches the
    // share of the transient the style leaves to clipping and any rounding.
    // Latency is constant (max look-ahead + true-peak detector delay), independent of the settings.
    class Limiter
    {
    public:
        struct Settings
        {
            float ceilingDb = -1.0f;
            float lookaheadMs = 2.0f;
            float attackMs = 20.0f;
            float releaseMs = 120.0f;
            float linkTransients = 0.75f;
            float linkRelease = 1.0f;
            int style = 0;
            bool truePeakDetect = false; // inter-sample detection (only useful without oversampling)
        };

        void prepare (double sampleRate, double maxLookaheadMs, int latencyMultiple)
        {
            rate = sampleRate;
            maxLook = (int) std::ceil (maxLookaheadMs * 0.001 * rate);
            delay = maxLook + TruePeakDetector::delay;
            if (latencyMultiple > 1) delay = ((delay + latencyMultiple - 1) / latencyMultiple) * latencyMultiple;
            for (auto& c : ch)
            {
                c.det.prepare (delay + 2);
                c.audio.prepare (delay + 2);
                c.peakMax.prepare (maxLook + 2);
                c.box1.prepare (maxLook + 2);
                c.box2.prepare (maxLook + 2);
            }
            currentLook = -1;
            reset();
        }

        void reset()
        {
            for (auto& c : ch)
            {
                c.det.reset(); c.audio.reset(); c.tp.reset();
                c.fast = c.slow = c.smoothed = 0.0f;
            }
            currentLook = -1;
            applyLookahead (settings.lookaheadMs, 0.0f);
        }

        int getLatency() const noexcept { return delay; }

        void setSettings (const Settings& s)
        {
            settings = s;
            const auto& st = styles()[(size_t) juce::jlimit (0, (int) styles().size() - 1, s.style)];
            style = st;
            ceilingLin = std::pow (10.0f, s.ceilingDb / 20.0f);
            fastRel = coef (s.releaseMs * st.fastRelease);
            slowRel = coef (s.releaseMs * st.slowRelease);
            slowAtk = 1.0f - coef (s.attackMs * st.slowAttack);
            linkT = juce::jlimit (0.0f, 1.0f, s.linkTransients);
            linkR = juce::jlimit (0.0f, 1.0f, s.linkRelease);
            softKnee = 1.0f - 0.9f * st.clip;    // soft clipper starts at this fraction of the ceiling
            applyLookahead (s.lookaheadMs, ch[0].smoothed);
        }

        // Processes in place. Returns the largest gain reduction in dB applied during the block.
        float process (float* const* data, int numChannels, int numSamples) noexcept
        {
            const int nc = juce::jlimit (1, 2, numChannels);
            const int detDelay = juce::jmax (0, delay - look - (settings.truePeakDetect ? TruePeakDetector::delay : 0));
            const float clipShare = style.clip, clipMaxDb = 3.0f;
            const float invCeil = 1.0f / ceilingLin;
            float maxGr = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                float req[2] = { 0.0f, 0.0f };
                for (int c = 0; c < nc; ++c)
                {
                    const float x = data[c][i];
                    const float d = ch[(size_t) c].det.push (x, detDelay);
                    const float p = settings.truePeakDetect ? ch[(size_t) c].tp.process (d, ceilingLin) : std::abs (d);
                    float r = p > ceilingLin ? 20.0f * std::log10 (p * invCeil) : 0.0f;
                    if (clipShare > 0.0f) r -= std::min (r * clipShare, clipMaxDb);
                    req[c] = r;
                }

                const float rMax = nc > 1 ? std::max (req[0], req[1]) : req[0];
                float env[2] = { 0.0f, 0.0f };
                for (int c = 0; c < nc; ++c)
                {
                    auto& s = ch[(size_t) c];
                    const float d = req[c] + linkT * (rMax - req[c]);
                    s.fast = std::max (d, s.fast * fastRel);
                    if (s.fast > s.slow) s.slow += (s.fast - s.slow) * slowAtk;
                    else            s.slow *= slowRel;
                    env[c] = std::max (s.fast, s.slow * style.slowWeight);
                }
                const float eMax = nc > 1 ? std::max (env[0], env[1]) : env[0];

                for (int c = 0; c < nc; ++c)
                {
                    auto& s = ch[(size_t) c];
                    const float e = env[c] + linkR * (eMax - env[c]);
                    float a = s.peakMax.push (e);
                    a = s.box1.push (a);
                    if (twoBoxes) a = s.box2.push (a);
                    s.smoothed = a;
                    maxGr = std::max (maxGr, a);

                    const float gain = a > 1.0e-6f ? std::pow (10.0f, -0.05f * a) : 1.0f;
                    float y = s.audio.push (data[c][i], delay) * gain;
                    data[c][i] = clipToCeiling (y);
                }
            }
            return maxGr;
        }

    private:
        float coef (float ms) const noexcept { return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (std::max (0.05, (double) ms) * 0.001 * rate)); }

        float clipToCeiling (float y) const noexcept
        {
            const float a = std::abs (y);
            if (style.clip > 0.0f)
            {
                const float k = softKnee * ceilingLin;
                if (a > k)
                {
                    const float span = ceilingLin - k;
                    const float out = k + span * std::tanh ((a - k) / span);
                    return std::copysign (out, y);
                }
                return y;
            }
            return a > ceilingLin ? std::copysign (ceilingLin, y) : y;
        }

        void applyLookahead (float ms, float fillValue)
        {
            const int L = juce::jlimit (0, maxLook, (int) std::round (ms * 0.001 * rate));
            const bool smooth = style.smoothAttack;
            if (L == currentLook && smooth == twoBoxes) return;
            currentLook = L;
            look = L;
            twoBoxes = smooth && L >= 2;
            int a1 = L + 1, a2 = 1;
            if (twoBoxes) { a1 = (L + 2) / 2; a2 = L + 2 - a1; }  // a1 + a2 - 2 == L
            for (auto& c : ch)
            {
                c.peakMax.reset (L + 1);
                c.box1.reset (a1, fillValue);
                c.box2.reset (a2, fillValue);
            }
        }

        struct Channel
        {
            DelayLine det, audio;
            TruePeakDetector tp;
            SlidingMax peakMax;
            BoxFilter box1, box2;
            float fast = 0, slow = 0, smoothed = 0;
        };

        std::array<Channel, 2> ch;
        Settings settings;
        Style style = styles()[0];
        double rate = 48000.0;
        int maxLook = 240, delay = 248, look = 96, currentLook = -1;
        bool twoBoxes = true;
        float ceilingLin = 1.0f, fastRel = 0.0f, slowRel = 0.0f, slowAtk = 1.0f, linkT = 0.75f, linkR = 1.0f, softKnee = 1.0f;
    };
}
