#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hs
{
constexpr float pi = 3.14159265358979323846f;

inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

// rational tanh approximation, max error ~1e-5 inside [-5, 5], hard-limited to [-1, 1]
inline float fastTanh (float x) noexcept
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return clampf (a / b, -1.0f, 1.0f);
}

inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925465f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }

// prewarped TPT integrator gain
inline float tptG (float fc, float fs) noexcept { return std::tan (pi * clampf (fc, 1.0f, 0.49f * fs) / fs); }

// one-pole parameter smoother (exponential, per tick)
struct Smoother
{
    float y = 0.0f, a = 1.0f;
    void prepare (double tickRate, float timeMs) noexcept
    {
        a = (float) (1.0 - std::exp (-1.0 / (tickRate * (double) timeMs * 0.001)));
    }
    void reset (float v) noexcept { y = v; }
    inline float next (float target) noexcept
    {
        y += a * (target - y);
        return y;
    }
};

// one-pole TPT filter (low-pass / high-pass outputs)
struct OnePole
{
    float s = 0.0f, G = 0.0f;
    void setCutoff (float fc, float fs) noexcept
    {
        const float g = tptG (fc, fs);
        G = g / (1.0f + g);
    }
    void reset() noexcept { s = 0.0f; }
    inline float lp (float x) noexcept
    {
        const float v = (x - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }
    inline float hp (float x) noexcept { return x - lp (x); }
};

// xorshift32, allocation-free random source
struct Rng
{
    uint32_t state = 0x9e3779b9u;
    inline float bipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (state >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
};
} // namespace hs
