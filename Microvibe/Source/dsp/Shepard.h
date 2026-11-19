#pragma once
#include "Common.h"

// =====================================================================================================
//  Shepard / Risset: endless glissando generator and "endless" processors (filter, phaser, flanger,
//  inverted notch bank, after the DAFx-15 idea of replacing the spectral peaks with notches).
//  pos = continuous position in intervals; every partial k sits at  centre * ratio^(k - N/2 + frac(pos))
//  with a gaussian (log-frequency) amplitude, so a wrap of frac(pos) is inaudible.
// =====================================================================================================
namespace mv
{
    inline float shepardWeight (float d, float width)     // d in [-1, 1] (normalised log distance)
    {
        const float s = std::max (0.05f, width) * 0.5f;
        return std::exp (-0.5f * (d / s) * (d / s));
    }

    class ShepardGen
    {
    public:
        void prepare (float sampleRate) { sr = sampleRate; pos = 0; for (auto& p : ph) p = rng.uni(); }

        // speed in intervals per second (signed)
        void process (float& l, float& r, int partials, float intervalSemis, float speed, float centreHz, float width, int wave, float level)
        {
            const double before = pos;
            pos += speed / sr;
            const double fl0 = std::floor (before), fl1 = std::floor (pos);
            if (fl1 > fl0) { for (int k = 9; k > 0; --k) ph[k] = ph[k - 1]; ph[0] = rng.uni(); for (int k = 9; k > 0; --k) { noiseL[k] = noiseL[k - 1]; } }
            else if (fl1 < fl0) { for (int k = 0; k < 9; ++k) ph[k] = ph[k + 1]; ph[9] = rng.uni(); }
            const float frac = (float) (pos - fl1);
            const float ratioOct = intervalSemis / 12.0f;
            const int n = clampv (partials, 3, 10);
            float yl = 0, yr = 0, norm = 0;
            for (int k = 0; k < n; ++k)
            {
                const float rel = (float) k - n * 0.5f + frac;
                const float hz = centreHz * std::exp2 (rel * ratioOct);
                const float w = shepardWeight (rel / (n * 0.5f), width);
                norm += w;
                if (hz > sr * 0.45f || hz < 10.0f) continue;
                ph[k] += hz / sr; ph[k] -= std::floor (ph[k]);
                float s;
                if (wave == 0) s = std::sin (kTwoPi * ph[k]);
                else if (wave == 1) { const float q = ph[k]; s = q < 0.25f ? 4 * q : (q < 0.75f ? 2 - 4 * q : 4 * q - 4); }
                else
                {
                    // narrow noise band: random-phase sine pair
                    noiseL[k] += 0.002f * (rng.bi() - noiseL[k] * 0.02f);
                    s = std::sin (kTwoPi * ph[k] + noiseL[k] * 30.0f);
                }
                const float pan = (k & 1) ? 0.6f : 0.4f;
                yl += s * w * (1 - pan); yr += s * w * pan;
            }
            const float g = level * 0.5f / std::max (0.5f, norm * 0.5f);
            l += yl * g; r += yr * g;
        }

    private:
        float sr = 48000;
        double pos = 0;
        float ph[10] {}, noiseL[10] {};
        Rng rng { 0x5E9Au };
    };

    class ShepardFx
    {
    public:
        void prepare (float sampleRate)
        {
            sr = sampleRate; pos = 0;
            for (auto& d : dl) d.prepare ((int) (sr * 0.05f));
        }

        // mode: 0 filter, 1 phaser (barber-pole frequency shift), 2 flanger (barber-pole), 3 notch bank
        void process (float& l, float& r, int mode, float speed, float res, int bands, float mix)
        {
            pos += speed / sr;
            const double fl = std::floor (pos);
            const float frac = (float) (pos - fl);
            if ((int64_t) fl != lastFloor)
            {
                // rotate band filter states so the wrap is continuous
                const bool up = (int64_t) fl > lastFloor;
                if (up) { for (int k = 11; k > 0; --k) { bl[k] = bl[k - 1]; br[k] = br[k - 1]; } bl[0].reset(); br[0].reset(); }
                else    { for (int k = 0; k < 11; ++k) { bl[k] = bl[k + 1]; br[k] = br[k + 1]; } bl[11].reset(); br[11].reset(); }
                lastFloor = (int64_t) fl;
            }
            const float inL = l, inR = r;
            float oL = l, oR = r;
            const int n = clampv (bands, 4, 10);
            if (mode == 0 || mode == 3)
            {
                const float q = 1.5f + res * res * 25.0f;
                float sL = 0, sR = 0, norm = 0;
                for (int k = 0; k < n; ++k)
                {
                    const float rel = (float) k - n * 0.5f + frac;
                    const float hz = 600.0f * std::exp2 (rel * 1.0f);
                    const float w = shepardWeight (rel / (n * 0.5f), 0.55f);
                    bl[k].set (hz, q, sr); br[k].set (hz, q, sr);
                    bl[k].process (inL); br[k].process (inR);
                    sL += bl[k].bpNorm() * w; sR += br[k].bpNorm() * w; norm += w;
                }
                if (mode == 0) { const float g = 1.6f / std::max (0.5f, std::sqrt (norm)); oL = sL * g; oR = sR * g; }
                else { const float d = 0.5f + 0.5f * res; oL = inL - sL * d; oR = inR - sR * d; }
            }
            else if (mode == 1)
            {
                // barber-pole phaser: dry + frequency-shifted copy -> endlessly moving notches
                const float shift = speed * 4.0f;
                fph += shift / sr; fph -= std::floor (fph);
                const float c = std::cos (kTwoPi * fph), s = std::sin (kTwoPi * fph);
                float re, im;
                hl.process (inL + fbL * res * 0.7f, re, im); const float yL = re * c - im * s;
                hr.process (inR + fbR * res * 0.7f, re, im); const float yR = re * c - im * s;
                fbL = yL; fbR = yR;
                oL = 0.5f * (inL + yL); oR = 0.5f * (inR + yR);
            }
            else
            {
                // barber-pole flanger: two delay taps sweeping exponentially, crossfaded
                dl[0].write (inL + fbL * res * 0.8f); dl[1].write (inR + fbR * res * 0.8f);
                float yL = 0, yR = 0;
                for (int h = 0; h < 2; ++h)
                {
                    float p = frac + 0.5f * h; p -= std::floor (p);
                    const float ms = 0.3f * std::exp2 ((1.0f - p) * 5.0f);      // 9.6 ms -> 0.3 ms (notches move up)
                    const float g = std::sin (kPi * p);
                    yL += dl[0].tap (ms * 0.001f * sr) * g * g;
                    yR += dl[1].tap (ms * 0.001f * sr * 1.03f) * g * g;
                }
                fbL = yL; fbR = yR;
                oL = 0.5f * (inL + yL); oR = 0.5f * (inR + yR);
            }
            l = inL + (oL - inL) * mix;
            r = inR + (oR - inR) * mix;
        }

    private:
        float sr = 48000;
        double pos = 0;
        int64_t lastFloor = 0;
        Svf bl[12], br[12];
        Hilbert hl, hr;
        float fph = 0, fbL = 0, fbR = 0;
        Ring dl[2];
    };
}
