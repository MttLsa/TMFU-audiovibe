#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "DspCommon.h"

// Global effect chain: Drive -> Chorus (Juno-style I / II / I+II, Ensemble) -> Delay -> Reverb -> limiter.
namespace fx
{
    struct DelayLine
    {
        std::vector<float> buf; int w = 0, mask = 0;
        void prepare (int maxSamples) { int s = 1; while (s < maxSamples + 4) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        inline float read (float d) const   // d in samples (>= 1)
        {
            const float rp = (float) w - d;
            const int i0 = (int) std::floor (rp);
            const float fr = rp - (float) i0;
            const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
            return a + fr * (b - a);
        }
    };

    struct Chorus
    {
        DelayLine dl[2]; float ph = 0, ph2 = 0, sr = 48000;
        void prepare (double s) { sr = (float) s; dl[0].prepare ((int) (sr * 0.03f)); dl[1].prepare ((int) (sr * 0.03f)); }
        void process (float* L, float* R, int n, int mode, float mix)
        {
            if (mode == 0) return;
            float rate = 0.513f, depth = 1.85f, centre = 3.5f;
            if (mode == 2) rate = 0.863f;
            if (mode == 3) { rate = 9.75f; depth = 0.35f; centre = 3.2f; }
            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i], inR = R[i], mono = 0.5f * (inL + inR);
                dl[0].push (mono); dl[1].push (mono);
                ph += rate / sr; ph -= std::floor (ph);
                float wl, wr;
                if (mode == 4)   // ensemble: three phased taps, slow + fast LFO
                {
                    ph2 += 6.0f / sr; ph2 -= std::floor (ph2);
                    auto tap = [&] (float o) { return centre + 2.4f * dsp::sine (ph + o) + 0.25f * dsp::sine (ph2 + o); };
                    const float t0 = dl[0].read (tap (0.0f) * sr * 0.001f), t1 = dl[0].read (tap (0.333f) * sr * 0.001f), t2 = dl[1].read (tap (0.667f) * sr * 0.001f);
                    wl = (t0 + 0.6f * t1) * 0.7f; wr = (t2 + 0.6f * t1) * 0.7f;
                }
                else
                {
                    const float tri = dsp::triangle (ph);
                    wl = dl[0].read ((centre + depth * tri) * sr * 0.001f);
                    wr = dl[1].read ((centre - depth * tri) * sr * 0.001f);
                }
                L[i] = inL * (1.0f - 0.45f * mix) + wl * mix;
                R[i] = inR * (1.0f - 0.45f * mix) + wr * mix;
            }
        }
    };

    struct Delay
    {
        DelayLine dl[2]; float sr = 48000, time = 0.25f, lp[2] {};
        void prepare (double s) { sr = (float) s; dl[0].prepare ((int) (sr * 2.6f)); dl[1].prepare ((int) (sr * 2.6f)); time = 0; }
        void process (float* L, float* R, int n, float seconds, float fb, float mix, bool ping)
        {
            if (mix <= 0.0001f) { for (int i = 0; i < n; ++i) { dl[0].push (0); dl[1].push (0); } return; }
            const float target = std::clamp (seconds, 0.01f, 2.5f) * sr;
            if (time <= 0) time = target;
            const float lpC = 1.0f - std::exp (-dsp::twoPi * 4500.0f / sr);
            for (int i = 0; i < n; ++i)
            {
                time += (target - time) * 0.0005f;
                float a = dl[0].read (time), b = dl[1].read (time);
                lp[0] += (a - lp[0]) * lpC; lp[1] += (b - lp[1]) * lpC;
                a = lp[0]; b = lp[1];
                const float inL = L[i], inR = R[i];
                if (ping) { dl[0].push (0.5f * (inL + inR) + b * fb); dl[1].push (a * fb); }
                else      { dl[0].push (inL + a * fb); dl[1].push (inR + b * fb); }
                L[i] = inL + a * mix; R[i] = inR + b * mix;
            }
        }
    };

    // safety limiter with 1.3 ms look-ahead: the gain glides down before a peak arrives instead of jumping
    // (an instant gain step was audible as a click when a loud chord or a new note pushed the sum over the ceiling)
    struct Limiter
    {
        static constexpr int D = 64;   // look-ahead / latency in samples
        float bufL[D] {}, bufR[D] {};
        int w = 0, holdN = 0;
        float g = 1.0f, hold = 1.0f, att = 0.1f, rel = 0.9995f;
        void prepare (double sr)
        {
            rel = std::exp (-1.0f / ((float) sr * 0.12f));
            att = 1.0f - std::exp (-1.0f / ((float) D * 0.22f));   // reaches the target within the look-ahead
            g = hold = 1.0f; holdN = 0; w = 0;
            std::fill (bufL, bufL + D, 0.0f); std::fill (bufR, bufR + D, 0.0f);
        }
        void process (float* L, float* R, int n)
        {
            constexpr float ceiling = 0.966f;   // -0.3 dBFS
            for (int i = 0; i < n; ++i)
            {
                const float pk = std::max (std::abs (L[i]), std::abs (R[i]));
                const float target = pk > ceiling ? ceiling / pk : 1.0f;
                if (target <= hold) { hold = target; holdN = D; }
                else if (--holdN <= 0) hold = target;
                g = hold < g ? g + (hold - g) * att : rel * g + (1.0f - rel) * hold;
                const float dl = bufL[w], dr = bufR[w];
                bufL[w] = L[i]; bufR[w] = R[i];
                w = (w + 1) % D;
                L[i] = std::clamp (dl * g, -ceiling, ceiling);
                R[i] = std::clamp (dr * g, -ceiling, ceiling);
            }
        }
    };
}
