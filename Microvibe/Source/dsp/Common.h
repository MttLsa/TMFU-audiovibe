#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

// =====================================================================================================
//  Small DSP building blocks shared by every module (header-only, allocation only in prepare()).
// =====================================================================================================
namespace mv
{
    constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

    template <typename T> inline T clampv (T x, T a, T b) { return x < a ? a : (x > b ? b : x); }
    inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
    inline float semiToRatio (float st) { return std::exp2 (st / 12.0f); }

    inline float fastTanh (float x)
    {
        x = clampv (x, -4.5f, 4.5f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
    inline float softClip (float x) { return fastTanh (x); }
    inline float flushDenorm (float x) { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

    // ------------------------------------------------------------------------------------- random
    struct Rng
    {
        uint32_t s = 0x9E3779B9u;
        explicit Rng (uint32_t seed = 0x1234567u) : s (seed ? seed : 1u) {}
        void seed (uint32_t x) { s = x ? x : 1u; }
        uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }        // [0,1)
        float bi() { return uni() * 2.0f - 1.0f; }                                   // [-1,1)
        int below (int n) { return n <= 1 ? 0 : (int) (uni() * (float) n) % n; }
        bool chance (float p) { return uni() < p; }
    };

    // pink noise (Paul Kellet economy)
    struct Pink
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float process (float w)
        {
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
        }
    };
    struct Brown
    {
        float z = 0;
        float process (float w) { z = (z + 0.02f * w) * 0.998f; return z * 3.5f; }
    };

    // ------------------------------------------------------------------------------------- filters
    struct OnePole
    {
        float z = 0, a = 1;
        void setHz (float hz, float sr) { a = 1.0f - std::exp (-kTwoPi * clampv (hz, 1.0f, sr * 0.49f) / sr); }
        void setTime (float ms, float sr) { a = 1.0f - std::exp (-1.0f / std::max (1.0e-4f, ms * 0.001f * sr)); }
        float lp (float x) { z += a * (x - z); return z; }
        float hp (float x) { return x - lp (x); }
        void reset (float v = 0) { z = v; }
    };

    // Zavalishin TPT state-variable filter
    struct Svf
    {
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        float lp = 0, bp = 0, hp = 0;
        void set (float hz, float q, float sr)
        {
            g = std::tan (kPi * clampv (hz, 5.0f, sr * 0.48f) / sr);
            k = 1.0f / std::max (0.05f, q);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        float process (float v0)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
            return lp;
        }
        float notch() const { return lp + hp; }
        float bpNorm() const { return bp * k; }      // unity gain at centre
        void reset() { ic1 = ic2 = 0; }
    };

    // RBJ biquad (peaking / shelves)
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void peak (float hz, float q, float db, float sr)
        {
            const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * clampv (hz, 10.0f, sr * 0.45f) / sr;
            const float al = std::sin (w) / (2.0f * q), c = std::cos (w), a0 = 1 + al / A;
            b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0; a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
        }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0; }
    };

    // ------------------------------------------------------------------------------------- delay
    // ring buffer with absolute indices: write() appends; read (pos) reads at an absolute (fractional) index.
    struct Ring
    {
        std::vector<float> buf;
        int mask = 0;
        int64_t w = 0;                          // total samples written (next write index)
        void prepare (int minSize)
        {
            int n = 1; while (n < minSize + 8) n <<= 1;
            buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        int size() const { return mask + 1; }
        void write (float x) { buf[(size_t) (w & mask)] = x; ++w; }
        float at (int64_t i) const { return buf[(size_t) (i & mask)]; }
        // cubic hermite at absolute position (must be <= w-3 for full accuracy)
        float read (double pos) const
        {
            const int64_t i = (int64_t) std::floor (pos);
            const float f = (float) (pos - (double) i);
            const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
        // delay in samples behind the newest sample (>= 2)
        float tap (float delay) const { return read ((double) (w - 1) - (double) std::max (2.0f, delay)); }
        float linear (double pos) const
        {
            const int64_t i = (int64_t) std::floor (pos);
            const float f = (float) (pos - (double) i);
            return at (i) + f * (at (i + 1) - at (i));
        }
    };

    // two-head granular pitch shifter (delay-line crossfade), ratio any value
    struct PitchShifter
    {
        Ring ring; float phase = 0, win = 2048;
        void prepare (float sr) { ring.prepare ((int) (sr * 0.25f)); win = sr * 0.05f; phase = 0; }
        void clear() { ring.clear(); }
        float process (float x, float ratio)
        {
            ring.write (x);
            if (std::abs (ratio - 1.0f) < 1.0e-4f) return ring.tap (2.0f);
            phase += (1.0f - ratio) / win;
            phase -= std::floor (phase);
            const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
            const float d1 = 2.0f + phase * win, d2 = 2.0f + p2 * win;
            const float g1 = std::sin (kPi * phase), g2 = std::sin (kPi * p2);
            return ring.tap (d1) * g1 * g1 + ring.tap (d2) * g2 * g2;
        }
    };

    // ------------------------------------------------------------------------------------- Hilbert
    // 90-degree phase-difference network (Olli Niemitalo, 2 x 4 allpass sections)
    struct Hilbert
    {
        struct AP { float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                    float process (float x) { const float y = a * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
        AP p[4], q[4]; float qd = 0;
        Hilbert()
        {
            const float ca[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            const float cb[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            for (int k = 0; k < 4; ++k) { p[k].a = ca[k] * ca[k]; q[k].a = cb[k] * cb[k]; }
        }
        void process (float x, float& re, float& im)
        {
            float a = x, b = x;
            for (auto& s : p) a = s.process (a);
            for (auto& s : q) b = s.process (b);
            re = a; im = qd; qd = b;
        }
    };

    // ------------------------------------------------------------------------------------- envelope follower
    struct Follower
    {
        float env = 0, att = 0.1f, rel = 0.001f;
        void set (float attMs, float relMs, float sr)
        {
            att = 1.0f - std::exp (-1.0f / std::max (1.0f, attMs * 0.001f * sr));
            rel = 1.0f - std::exp (-1.0f / std::max (1.0f, relMs * 0.001f * sr));
        }
        float process (float x) { x = std::abs (x); env += (x > env ? att : rel) * (x - env); return env; }
    };

    // ------------------------------------------------------------------------------------- limiter
    struct Limiter
    {
        float env = 1, rel = 0.0005f, thr = 0.966f;
        void prepare (float sr, float ceilingDb) { rel = 1.0f - std::exp (-1.0f / (0.08f * sr)); thr = dbToGain (ceilingDb); env = 1; }
        bool process (float& l, float& r)
        {
            const float pk = std::max (std::abs (l), std::abs (r));
            const float target = pk > thr ? thr / pk : 1.0f;
            env = target < env ? target : env + rel * (target - env);
            l *= env; r *= env;
            l = clampv (l, -thr, thr); r = clampv (r, -thr, thr);
            return env < 0.99f;
        }
    };
}
