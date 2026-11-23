#pragma once
#include "MicroEngine.h"   // TptSvf

// Master filter of the GLOBAL CONTROL panel: High Pass -> Band Pass -> Low Pass in series, each switchable, with a
// shared Resonance. Frequencies are smoothed per sample (coefficients updated every 16 samples, no zipper noise).
// HP fully left (20 Hz) and LP fully right (20 kHz) are transparent: the two knobs close towards each other.
class MasterFilter
{
public:
    struct Settings { bool hpOn = false, bpOn = false, lpOn = false; float hp = 20.0f, bp = 1000.0f, lp = 20000.0f, res = 0.0f; };

    void reset() { for (auto* f : { hp, bp, lp }) { f[0].reset(); f[1].reset(); } }

    void process (float* L, float* R, int n, double sr, const Settings& s)
    {
        const double q = 0.5 + std::pow (s.res * 0.01, 1.5) * 14.5;
        const bool resOn = s.res > 0.5f;
        const bool useHp = s.hpOn && (s.hp > 20.5f || resOn || hpSm > 20.5f);
        const bool useBp = s.bpOn;
        const bool useLp = s.lpOn && (s.lp < 19900.0f || resOn || lpSm < 19900.0f);
        if (! useHp) hpSm = s.hp;
        if (! useBp) bpSm = s.bp;
        if (! useLp) lpSm = s.lp;
        if (! useHp && ! useBp && ! useLp) return;
        const float bpMakeup = (float) std::max (1.0, std::sqrt (q));   // keeps the band pass usable at high Q
        if (useHp) set (hp, hpSm, sr, q);
        if (useBp) set (bp, bpSm, sr, q);
        if (useLp) set (lp, lpSm, sr, q);
        for (int i = 0; i < n; ++i)
        {
            hpSm += 0.004f * (s.hp - hpSm); bpSm += 0.004f * (s.bp - bpSm); lpSm += 0.004f * (s.lp - lpSm);
            if ((counter++ & 15) == 0)
            {
                if (useHp) set (hp, hpSm, sr, q);
                if (useBp) set (bp, bpSm, sr, q);
                if (useLp) set (lp, lpSm, sr, q);
            }
            float l = L[i], r = R[i];
            if (useHp) { l = hp[0].process (l, 2); r = hp[1].process (r, 2); }
            if (useBp) { l = bp[0].process (l, 1) * bpMakeup; r = bp[1].process (r, 1) * bpMakeup; }
            if (useLp) { l = lp[0].process (l, 0); r = lp[1].process (r, 0); }
            L[i] = l; R[i] = r;
        }
    }

private:
    static void set (TptSvf* f, float fc, double sr, double q)
    {
        f[0].set (fc, sr, q);
        f[1].g = f[0].g; f[1].k = f[0].k; f[1].a1 = f[0].a1; f[1].a2 = f[0].a2; f[1].a3 = f[0].a3;
    }
    TptSvf hp[2], bp[2], lp[2];
    float hpSm = 20.0f, bpSm = 1000.0f, lpSm = 20000.0f;
    unsigned counter = 0;
};
