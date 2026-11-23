#pragma once
#include "Utils.h"

namespace hs
{
// 2-band shelving EQ on TPT state-variable cores (modulation-stable coefficient updates).
class ShelfEq
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        reset();
        set (120.0f, 0.0f, 6000.0f, 0.0f);
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch) lowS[ch] = highS[ch] = State {};
    }

    void set (float lowHz, float lowDb, float highHz, float highDb) noexcept
    {
        low = make (lowHz, lowDb, false);
        high = make (highHz, highDb, true);
    }

    inline float process (int ch, float x) noexcept
    {
        return tick (high, highS[ch], tick (low, lowS[ch], x));
    }

private:
    struct Coef
    {
        float a1 = 1, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
    };
    struct State
    {
        float ic1 = 0, ic2 = 0;
    };

    Coef make (float f, float db, bool highShelf) const noexcept
    {
        constexpr float k = 1.41421356f; // Q = 0.707
        const float A = std::pow (10.0f, db / 40.0f);
        float g = tptG (f, fs);
        Coef c;
        if (highShelf)
        {
            g *= std::sqrt (A);
            c.m0 = A * A;
            c.m1 = k * (1.0f - A) * A;
            c.m2 = 1.0f - A * A;
        }
        else
        {
            g /= std::sqrt (A);
            c.m0 = 1.0f;
            c.m1 = k * (A - 1.0f);
            c.m2 = A * A - 1.0f;
        }
        c.a1 = 1.0f / (1.0f + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        return c;
    }

    static inline float tick (const Coef& c, State& s, float v0) noexcept
    {
        const float v3 = v0 - s.ic2;
        const float v1 = c.a1 * s.ic1 + c.a2 * v3;
        const float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }

    Coef low, high;
    State lowS[2], highS[2];
    float fs = 44100.0f;
};
} // namespace hs
