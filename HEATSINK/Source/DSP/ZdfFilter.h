#pragma once
#include "Utils.h"

namespace hs
{
// Zero-delay-feedback (TPT) state variable filter with tanh saturation in the resonance feedback.
// The damping term 2R*bp is split as 2Rb*bp - 2Rb*reso*tanh(bp): the positive (resonant) feedback saturates,
// so large signals are damped harder. The nonlinearity is linearised around the current state
// (tanh(s)/s) and the resulting linear ZDF equation is solved exactly each sample.
struct NlSvf
{
    float s1 = 0.0f, s2 = 0.0f;

    void reset() noexcept { s1 = s2 = 0.0f; }

    inline void tick (float x, float g, float Rb, float reso, float& lp, float& bp, float& hp) noexcept
    {
        const float sv = s1 * 0.7f;
        const float sat = std::abs (sv) < 1.0e-4f ? 1.0f : fastTanh (sv) / sv;
        const float R = Rb * (1.0f - reso * sat);
        hp = (x - (2.0f * R + g) * s1 - s2) / (1.0f + 2.0f * R * g + g * g);
        const float v1 = g * hp;
        bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        lp = v2 + s2;
        s2 = lp + v2;
    }
};

class MultiFilter
{
public:
    enum Type
    {
        LowPass12 = 0,
        LowPass24,
        HighPass12,
        HighPass24,
        BandPass,
        Notch,
        Peak,
        numTypes
    };

    static constexpr float maxReso = 0.98f;

    void prepare (double osRate)
    {
        fadeInc = 1.0f / (0.01f * (float) osRate);
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            s[ch].reset();
            la[ch].reset();
            lb[ch].reset();
            ha[ch].reset();
            hb[ch].reset();
        }
        prev = cur = target;
        fade = 1.0f;
    }

    void setType (int t) noexcept { target = t < 0 ? 0 : (t >= numTypes ? numTypes - 1 : t); }

    void beginBlock() noexcept
    {
        if (fade >= 1.0f && target != cur)
        {
            prev = cur;
            cur = target;
            const int gNew = group (cur);
            if (gNew != group (prev))
                for (int ch = 0; ch < 2; ++ch) resetGroup (gNew, ch);
            fade = 0.0f;
        }
    }

    // g = tptG(cutoff, osRate), reso in [0, 1]
    inline float process (int ch, float x, float g, float reso) noexcept
    {
        const float rr = reso * maxReso;
        const int mask = (1 << group (cur)) | (fade < 1.0f ? (1 << group (prev)) : 0);
        Taps t;
        if (mask & 1) s[ch].tick (x, g, 0.70710678f, rr, t.lp, t.bp, t.hp);
        if (mask & 2)
        {
            float l, b, h;
            la[ch].tick (x, g, 0.92387953f, rr * 0.25f, l, b, h);
            lb[ch].tick (l, g, 0.38268343f, rr, t.lp24, b, h);
        }
        if (mask & 4)
        {
            float l, b, h;
            ha[ch].tick (x, g, 0.92387953f, rr * 0.25f, l, b, h);
            hb[ch].tick (h, g, 0.38268343f, rr, l, b, t.hp24);
        }
        t.bpGain = 2.0f * 0.70710678f * (1.0f - rr);
        float y = tap (cur, t);
        if (fade < 1.0f)
        {
            const float yp = tap (prev, t);
            y = yp + (y - yp) * fade;
        }
        return y;
    }

    inline void advance() noexcept
    {
        if (fade < 1.0f) fade = std::min (1.0f, fade + fadeInc);
    }

private:
    struct Taps
    {
        float lp = 0, bp = 0, hp = 0, lp24 = 0, hp24 = 0, bpGain = 1;
    };

    static int group (int t) noexcept { return t == LowPass24 ? 1 : (t == HighPass24 ? 2 : 0); }

    static float tap (int t, const Taps& x) noexcept
    {
        switch (t)
        {
            case LowPass12:  return x.lp;
            case LowPass24:  return x.lp24;
            case HighPass12: return x.hp;
            case HighPass24: return x.hp24;
            case BandPass:   return x.bp * x.bpGain;
            case Notch:      return x.lp + x.hp;
            case Peak:       return x.lp - x.hp;
            default:         return x.lp;
        }
    }

    void resetGroup (int g, int ch) noexcept
    {
        if (g == 0) s[ch].reset();
        if (g == 1) { la[ch].reset(); lb[ch].reset(); }
        if (g == 2) { ha[ch].reset(); hb[ch].reset(); }
    }

    NlSvf s[2], la[2], lb[2], ha[2], hb[2];
    float fade = 1.0f, fadeInc = 0.001f;
    int cur = 0, prev = 0, target = 0;
};
} // namespace hs
