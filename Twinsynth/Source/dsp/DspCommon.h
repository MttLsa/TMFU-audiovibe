#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

// =====================================================================================================
//  Small DSP building blocks shared by Section A (analog) and Section D (digital).
// =====================================================================================================
namespace dsp
{
    constexpr float pi = 3.14159265358979f;
    constexpr float twoPi = 6.28318530717959f;

    inline float fastTanh (float x)
    {
        x = std::clamp (x, -4.5f, 4.5f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float exp2Fast (float x) { return std::exp2 (x); }
    inline float noteToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

    // ------------------------------------------------------------------ sine lookup (phase 0..1)
    struct SineTable
    {
        static constexpr int N = 4096;
        float t[N + 1];
        SineTable() { for (int i = 0; i <= N; ++i) t[i] = std::sin (twoPi * (float) i / (float) N); }
        static const SineTable& get() { static SineTable s; return s; }
        inline float operator() (float ph) const
        {
            ph -= std::floor (ph);
            const float f = ph * (float) N;
            const int i = (int) f;
            const float fr = f - (float) i;
            return t[i] + fr * (t[i + 1] - t[i]);
        }
    };
    inline float sine (float phase) { return SineTable::get() (phase); }

    // ------------------------------------------------------------------ random
    struct Rng
    {
        uint32_t s = 0x12345678u;
        void seed (uint32_t v) { s = v ? v : 1u; }
        inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        inline float bip() { return (float) (int32_t) next() * (1.0f / 2147483648.0f); }   // -1..1
        inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }       // 0..1
    };

    // ------------------------------------------------------------------ polyBLEP
    inline float blep (float t, float dt)
    {
        if (t < dt)         { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    inline float sawBlep (float ph, float dt) { return 2.0f * ph - 1.0f - blep (ph, dt); }
    inline float pulseBlep (float ph, float dt, float pw)
    {
        float ph2 = ph + (1.0f - pw); ph2 -= std::floor (ph2);
        return (sawBlep (ph, dt) - sawBlep (ph2, dt)) * 0.5f;
    }
    inline float triangle (float ph) { return 1.0f - 4.0f * std::abs (ph - 0.5f); }

    // ------------------------------------------------------------------ ADSR with analog-style curves
    struct ADSR
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage = Idle;
        float level = 0.0f, ca = 1, cd = 1, cr = 1, sus = 1;

        static float coef (float ms, float sr, float k)
        {
            const float t = std::max (0.0001f, ms * 0.001f) / k;
            return 1.0f - std::exp (-1.0f / (t * sr));
        }
        void set (float aMs, float dMs, float s, float rMs, float sr)
        {
            ca = coef (aMs, sr, 1.466f); cd = coef (dMs, sr, 4.6f); cr = coef (rMs, sr, 4.6f); sus = s;
        }
        void gateOn (bool hardReset) { if (hardReset) level = 0.0f; stage = Attack; }
        void gateOff() { if (stage != Idle) stage = Release; }
        void kill() { stage = Idle; level = 0.0f; }
        bool active() const { return stage != Idle; }
        inline float next()
        {
            switch (stage)
            {
                case Attack:  level += (1.3f - level) * ca; if (level >= 1.0f) { level = 1.0f; stage = Decay; } break;
                case Decay:   level += (sus - level) * cd; if (std::abs (level - sus) < 1.0e-4f) { level = sus; stage = Sustain; } break;
                case Sustain: level = sus; break;
                case Release: level -= level * cr; if (level < 1.0e-5f) { level = 0.0f; stage = Idle; } break;
                case Idle: break;
            }
            return level;
        }
    };

    // ------------------------------------------------------------------ LFO (-1..1)
    struct LFO
    {
        float phase = 0, value = 0, sh = 0;
        Rng rng;
        inline float next (int shape, float inc)
        {
            phase += inc;
            if (phase >= 1.0f) { phase -= 1.0f; sh = rng.bip(); }
            switch (shape)
            {
                case 0: value = triangle (phase); break;
                case 1: value = sine (phase); break;
                case 2: value = 2.0f * phase - 1.0f; break;
                case 3: value = 1.0f - 2.0f * phase; break;
                case 4: value = phase < 0.5f ? 1.0f : -1.0f; break;
                default: value = sh; break;
            }
            return value;
        }
    };

    // ------------------------------------------------------------------ ZDF ladder (4-pole, or 3-pole "diode" flavour)
    struct Ladder
    {
        float s[4] {};
        void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }
        // g = tan(pi fc / fs), res 0..1, poles 3 or 4, drive >= 1
        inline float process (float in, float g, float res, int poles, float drive)
        {
            const float G = g / (1.0f + g);
            const float k = poles == 4 ? res * 4.1f : res * 8.3f;
            const float G1m = 1.0f - G;
            float S, Gn;
            if (poles == 4)
            {
                S = G * G * G * G1m * s[0] + G * G * G1m * s[1] + G * G1m * s[2] + G1m * s[3];
                Gn = G * G * G * G;
            }
            else
            {
                S = G * G * G1m * s[0] + G * G1m * s[1] + G1m * s[2];
                Gn = G * G * G;
            }
            // compensate the bass loss of the ladder a little at high resonance
            const float x = in * drive * (1.0f + 0.5f * k * (poles == 4 ? 1.0f : 0.35f));
            const float yEst = (Gn * x + S) / (1.0f + k * Gn);
            float u = fastTanh (x - k * yEst);
            float y = u;
            for (int i = 0; i < poles; ++i)
            {
                const float v = (y - s[i]) * G;
                const float o = v + s[i];
                s[i] = o + v;
                y = o;
            }
            if (! std::isfinite (y)) { reset(); y = 0.0f; }
            return y / std::sqrt (drive);
        }
    };

    // ------------------------------------------------------------------ TPT state variable filter
    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        // mode: 0 LP, 1 BP, 2 HP, 3 Notch
        inline float process (float v0, float g, float res, int mode, bool saturate = false)
        {
            const float k = 2.0f - 1.96f * res;
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1, a3 = g * a2;
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            if (saturate) { ic1 = fastTanh (ic1 * 0.5f) * 2.0f; }
            if (! std::isfinite (ic1) || ! std::isfinite (ic2)) { reset(); return 0.0f; }
            switch (mode)
            {
                case 0: return v2;
                case 1: return v1 * k;
                case 2: return v0 - k * v1 - v2;
                default: return v0 - k * v1;
            }
        }
    };

    inline float cutoffG (float hz, float sr)
    {
        hz = std::clamp (hz, 16.0f, std::min (20000.0f, sr * 0.45f));
        return std::tan (pi * hz / sr);
    }

    // ------------------------------------------------------------------ one-pole DC blocker
    struct DcBlock
    {
        float x1 = 0, y1 = 0, r = 0.9995f;
        inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    };
}
