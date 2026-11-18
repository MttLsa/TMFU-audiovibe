#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>

// NORM: output normaliser of a section.
//  1) slow automatic gain that brings the average (RMS) level to a reference of -16 dBFS (max +-12 dB of correction),
//  2) a peak limiter with a -1 dBFS ceiling (fast attack, smooth release) so the section never clips.
// When switched off it is bypassed (gain glides back to 0 dB).
class OutputNorm
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        rmsCoef  = (float) std::exp (-1.0 / (sr * 0.40));     // 400 ms RMS window
        upCoef   = (float) std::exp (-1.0 / (sr * 1.50));     // gain rises slowly (1.5 s)
        downCoef = (float) std::exp (-1.0 / (sr * 0.25));     // and falls faster (250 ms)
        limRel   = (float) std::exp (-1.0 / (sr * 0.08));     // limiter release 80 ms
        ms = 0.0f; gain = 1.0f; lim = 1.0f;
    }

    void process (float* L, float* R, int n, bool on)
    {
        float minLim = 1.0f;
        constexpr float target = 0.1585f;          // -16 dBFS RMS
        constexpr float ceiling = 0.891f;          // -1 dBFS
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            const float p = 0.5f * (l * l + r * r);
            ms = rmsCoef * ms + (1.0f - rmsCoef) * p;
            float want = 1.0f;
            if (on)
            {
                const float rms = std::sqrt (ms);
                want = rms > 1.0e-4f ? std::clamp (target / rms, 0.25f, 4.0f) : gain;   // silence: hold the gain
            }
            const float c = want > gain ? upCoef : downCoef;
            gain = c * gain + (1.0f - c) * want;
            l *= gain; r *= gain;
            if (on)
            {
                const float pk = std::max (std::abs (l), std::abs (r));
                const float need = pk > ceiling ? ceiling / pk : 1.0f;
                lim = need < lim ? need : limRel * lim + (1.0f - limRel) * need;
                l *= lim; r *= lim;
                minLim = std::min (minLim, lim);
            }
            else lim = 1.0f;
            L[i] = l; R[i] = r;
        }
        limiting.store (minLim < 0.97f);                     // limiter working (> 0.25 dB of reduction)
    }
    std::atomic<bool> limiting { false };
    float currentGainDb() const { return 20.0f * std::log10 (std::max (1.0e-6f, gain * lim)); }

private:
    double sr = 44100.0;
    float rmsCoef = 0, upCoef = 0, downCoef = 0, limRel = 0, ms = 0, gain = 1, lim = 1;
};

// Transparent safety limiter on the plug-in output: ceiling -0.3 dBFS, instant attack, 60 ms release.
class SafetyLimiter
{
public:
    void prepare (double sr) { rel = (float) std::exp (-1.0 / (sr * 0.06)); g = 1.0f; }
    void process (float* L, float* R, int n)
    {
        constexpr float ceiling = 0.966f;
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            const float need = pk > ceiling ? ceiling / pk : 1.0f;
            g = need < g ? need : rel * g + (1.0f - rel) * need;
            L[i] *= g; R[i] *= g;
        }
    }
private:
    float rel = 0.999f, g = 1.0f;
};
