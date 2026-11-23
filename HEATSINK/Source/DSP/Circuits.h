#pragma once
#include "Utils.h"

namespace hs
{
// Drive circuits, run at the oversampled rate (4x). Each circuit: pre tone filter -> gain -> nonlinearity
// -> post tone filter (12 dB/oct) -> DC blocker -> makeup.
enum Circuit
{
    OpAmp = 0,
    Diode,
    Asymmetric,
    Fuzz,
    Rectifier,
    Tube,
    HardClip,
    Wavefolder,
    numCircuits
};

inline int clampCircuit (int c) noexcept { return c < 0 ? 0 : (c >= numCircuits ? numCircuits - 1 : c); }

struct Voicing
{
    float preHz;    // pre high-pass corner (what reaches the clipper)
    float lowKeep;  // amount of the lows below preHz kept (0 = full high-pass, 1 = flat)
    float postHz;   // post low-pass corner (12 dB/oct)
    float minDb;    // gain at drive 0 %
    float rangeDb;  // extra gain at drive 100 %
    float makeup;   // output trim
};

inline const Voicing& voicing (int c) noexcept
{
    static const Voicing v[numCircuits] = {
        { 720.0f, 0.18f, 5200.0f, 0.0f, 42.0f, 0.95f },   // Op-Amp: mid hump, rail clip with smooth knee
        { 110.0f, 0.55f, 6500.0f, 0.0f, 36.0f, 1.00f },   // Diode: symmetric exponential diode pair
        { 220.0f, 0.60f, 9000.0f, 0.0f, 36.0f, 1.05f },   // Asymmetric: soft positive / hard negative, even harmonics
        {  90.0f, 0.80f, 3300.0f, 6.0f, 42.0f, 0.80f },   // Fuzz: biased two-stage, sputtering
        { 150.0f, 0.70f, 7000.0f, 0.0f, 30.0f, 1.00f },   // Rectifier: full-wave blend, octave up
        {  40.0f, 1.00f, 11000.0f, 0.0f, 30.0f, 1.05f },  // Tube: biased triode-like curve, gentle
        { 300.0f, 0.50f, 8000.0f, 0.0f, 40.0f, 0.85f },   // Hard Clip: transistor rail clip
        {  80.0f, 0.90f, 9000.0f, 0.0f, 20.0f, 0.90f }    // Wavefolder: sine fold
    };
    return v[clampCircuit (c)];
}

inline float nonlinearity (int c, float u) noexcept
{
    switch (c)
    {
        case OpAmp:
        {
            const float x = clampf (u * 0.66f, -1.0f, 1.0f);
            return 1.5f * x - 0.5f * x * x * x;
        }
        case Diode:
        {
            const float a = 1.0f - std::exp (-1.4f * std::abs (u));
            return u < 0.0f ? -a : a;
        }
        case Asymmetric:
        {
            if (u >= 0.0f) return fastTanh (u);
            return -(1.0f - std::exp (1.8f * u)) * (1.0f / 1.8f);
        }
        case Fuzz:
        {
            const float s1 = fastTanh (2.0f * u + 0.4f) - 0.379949f;
            const float x = clampf (2.5f * s1, -1.0f, 1.0f);
            return 1.5f * x - 0.5f * x * x * x;
        }
        case Rectifier:
            return 0.35f * fastTanh (u) + 0.65f * fastTanh (1.5f * std::abs (u));
        case Tube:
            return (fastTanh (u + 0.3f) - 0.291313f) * 1.1f;
        case HardClip:
            return clampf (u, -0.9f, 0.9f);
        case Wavefolder:
            return std::sin (u * (0.5f * pi));
        default:
            return u;
    }
}

class DriveStage
{
public:
    void prepare (double osRate)
    {
        fs = (float) osRate;
        fadeInc = 1.0f / (0.012f * fs);
        for (int c = 0; c < numCircuits; ++c)
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& s = state[c][ch];
                s.pre.setCutoff (voicing (c).preHz, fs);
                s.post1.setCutoff (voicing (c).postHz, fs);
                s.post2.setCutoff (voicing (c).postHz, fs);
                s.dc.setCutoff (8.0f, fs);
            }
        reset();
    }

    void reset() noexcept
    {
        for (auto& c : state)
            for (auto& s : c) s.reset();
        prev = cur = target;
        fade = 1.0f;
    }

    void setCircuit (int c) noexcept { target = clampCircuit (c); }

    // call once per block, before processing
    void beginBlock() noexcept
    {
        if (fade >= 1.0f && target != cur)
        {
            prev = cur;
            cur = target;
            for (auto& s : state[cur]) s.reset();
            fade = 0.0f;
        }
    }

    bool isFading() const noexcept { return fade < 1.0f; }
    int current() const noexcept { return cur; }
    int previous() const noexcept { return prev; }

    static float gainFor (int c, float driveNorm) noexcept
    {
        const auto& v = voicing (c);
        return dbToGain (v.minDb + driveNorm * v.rangeDb);
    }

    inline float process (int ch, float x, float gainCur, float gainPrev) noexcept
    {
        float y = run (cur, ch, x, gainCur);
        if (fade < 1.0f)
        {
            const float yp = run (prev, ch, x, gainPrev);
            y = yp + (y - yp) * fade;
        }
        return y;
    }

    // call once per (oversampled) frame
    inline void advance() noexcept
    {
        if (fade < 1.0f) fade = std::min (1.0f, fade + fadeInc);
    }

private:
    struct State
    {
        OnePole pre, post1, post2, dc;
        void reset() noexcept
        {
            pre.reset();
            post1.reset();
            post2.reset();
            dc.reset();
        }
    };

    inline float run (int c, int ch, float x, float gain) noexcept
    {
        auto& s = state[c][ch];
        const auto& v = voicing (c);
        const float hp = s.pre.hp (x);
        const float u = (hp + v.lowKeep * (x - hp)) * gain;
        float y = nonlinearity (c, u);
        y = s.post2.lp (s.post1.lp (y));
        y = s.dc.hp (y);
        return y * v.makeup;
    }

    State state[numCircuits][2];
    float fs = 176400.0f, fade = 1.0f, fadeInc = 0.001f;
    int cur = 0, prev = 0, target = 0;
};
} // namespace hs
