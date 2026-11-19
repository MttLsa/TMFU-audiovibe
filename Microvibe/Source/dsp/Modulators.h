#pragma once
#include "Common.h"

// =====================================================================================================
//  Modulation sources, evaluated once per control block (32 samples).
// =====================================================================================================
namespace mv
{
    struct Lfo
    {
        enum Shape { Sine = 0, Tri, SawUp, SawDown, Square, SH, SmoothRnd, Custom };
        double phase = 0;             // free-running phase (cycles)
        float out = 0, smoothed = 0, held = 0, prevHeld = 0;
        bool stopped = false;
        Rng rng { 0xA5A5u };
        std::array<float, 16> custom {};     // custom shape points (0..1), copied from the state

        void seed (uint32_t s) { rng.seed (s); held = rng.bi(); prevHeld = rng.bi(); }
        void retrigger() { phase = 0; stopped = false; }

        // phaseNow: externally locked phase (cycles) or < 0 to use the internal accumulator
        float tick (double dtCycles, double lockedPhase, int shape, float phaseOff, float skew, float smooth, bool oneShot, float sr, int blk)
        {
            double before = phase;
            if (lockedPhase >= 0) phase = lockedPhase;
            else if (! stopped) phase += dtCycles;
            if (oneShot && lockedPhase < 0 && phase >= 1.0) { phase = 0.999999; stopped = true; }
            if (std::floor (phase) != std::floor (before)) { prevHeld = held; held = rng.bi(); }
            double p = phase + phaseOff;
            p -= std::floor (p);
            // skew: move the mid point of the cycle
            const float q = (float) (p < skew ? 0.5 * p / skew : 0.5 + 0.5 * (p - skew) / (1.0 - skew));
            float v = 0;
            switch (shape)
            {
                case Sine:      v = std::sin (kTwoPi * q); break;
                case Tri:       v = q < 0.25f ? 4 * q : (q < 0.75f ? 2 - 4 * q : 4 * q - 4); break;
                case SawUp:     v = 2 * q - 1; break;
                case SawDown:   v = 1 - 2 * q; break;
                case Square:    v = q < 0.5f ? 1.0f : -1.0f; break;
                case SH:        v = held; break;
                case SmoothRnd: { const float t = 0.5f - 0.5f * std::cos (kPi * (float) p); v = prevHeld + (held - prevHeld) * t; break; }
                case Custom:
                {
                    const float x = q * 16.0f; const int i = (int) x; const float f = x - (float) i;
                    v = (custom[(size_t) (i & 15)] + (custom[(size_t) ((i + 1) & 15)] - custom[(size_t) (i & 15)]) * f) * 2 - 1;
                    break;
                }
                default: break;
            }
            if (smooth > 0.001f)
            {
                const float tau = smooth * smooth * 0.5f;    // up to 0.5 s
                const float a = 1.0f - std::exp (-(float) blk / std::max (1.0f, tau * sr));
                smoothed += a * (v - smoothed);
            }
            else smoothed = v;
            out = smoothed;
            return out;
        }
    };

    struct Env
    {
        enum Stage { Idle, Attack, Hold, Decay, Sustain, Release };
        Stage st = Idle; float v = 0, t = 0, relFrom = 0; float gateLeft = -1;   // gateLeft < 0: held by note
        void trigger (float gateSeconds) { st = Attack; t = 0; gateLeft = gateSeconds; }
        void release() { if (st != Idle) { st = Release; t = 0; relFrom = v; } }
        static float shape (float x, float curve)   // x in [0,1]
        {
            if (std::abs (curve) < 0.01f) return x;
            const float k = curve * 6.0f;
            return (std::exp (k * x) - 1.0f) / (std::exp (k) - 1.0f);
        }
        float tick (float dt, float a, float h, float d, float s, float r, float curve)
        {
            if (gateLeft >= 0 && st != Idle && st != Release) { gateLeft -= dt; if (gateLeft <= 0) { gateLeft = -1; release(); } }
            t += dt;
            switch (st)
            {
                case Idle: v = 0; break;
                case Attack: v = shape (std::min (1.0f, t / std::max (1.0e-4f, a)), -curve); if (t >= a) { st = Hold; t = 0; } break;
                case Hold: v = 1; if (t >= h) { st = Decay; t = 0; } break;
                case Decay: v = s + (1 - s) * (1 - shape (std::min (1.0f, t / std::max (1.0e-4f, d)), -curve)); if (t >= d) st = Sustain; break;
                case Sustain: v = s; if (s <= 1.0e-4f) st = Idle; break;
                case Release: v = relFrom * (1 - shape (std::min (1.0f, t / std::max (1.0e-4f, r)), -curve)); if (t >= r) { st = Idle; v = 0; } break;
            }
            return v;
        }
    };

    struct RandomMod
    {
        enum Mode { SH = 0, Smooth, Drift, Brown };
        double phase = 0; float a = 0, b = 0, out = 0, sm = 0, walk = 0, d1 = 0, d2 = 0, d1t = 0, d2t = 0;
        Rng rng { 0xBEEFu };
        void seed (uint32_t s) { rng.seed (s); a = rng.bi(); b = rng.bi(); }
        float tick (double dtCycles, int mode, float smooth, float sr, int blk)
        {
            const double before = phase;
            phase += dtCycles;
            const bool wrapped = std::floor (phase) != std::floor (before);
            if (wrapped) { a = b; b = rng.bi(); }
            const float p = (float) (phase - std::floor (phase));
            float v = 0;
            switch (mode)
            {
                case SH: v = b; break;
                case Smooth: v = a + (b - a) * (0.5f - 0.5f * std::cos (kPi * p)); break;
                case Drift:
                {
                    // two slow smooth-random octaves
                    if (wrapped) { d1 = d1t; d1t = rng.bi(); }
                    v = 0.7f * (a + (b - a) * (0.5f - 0.5f * std::cos (kPi * p))) + 0.3f * (d1 + (d1t - d1) * p);
                    break;
                }
                case Brown:
                {
                    walk += rng.bi() * (float) std::min (1.0, dtCycles * 4.0);
                    if (walk > 1) walk = 2 - walk; if (walk < -1) walk = -2 - walk;
                    v = walk; break;
                }
                default: break;
            }
            if (smooth > 0.001f)
            {
                const float tau = smooth * smooth * 0.5f;
                sm += (1.0f - std::exp (-(float) blk / std::max (1.0f, tau * sr))) * (v - sm);
            }
            else sm = v;
            out = sm;
            return out;
        }
    };
}
