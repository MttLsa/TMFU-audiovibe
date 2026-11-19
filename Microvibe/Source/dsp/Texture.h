#pragma once
#include "Common.h"
#include "Tuning.h"

// =====================================================================================================
//  TEXTURE: background beds for minimal / microhouse.
//    bed     = coloured noise through a bank of tuned resonant band-passes + a drifting sine drone
//    grains  = granular cloud reading a 4 s buffer of the input and/or the bed (freeze-able)
// =====================================================================================================
namespace mv
{
    struct TextureParams
    {
        float bed = 0.5f, resq = 0.5f, drone = 0.2f, drift = 0.3f, grain = 0.4f, dens = 20, sizeMs = 80, spray = 0.3f,
              pitch = 0, prand = 0, spread = 0.7f, tilt = 0, rev = 0.2f, snap = 1;
        int noise = 1, gsrc = 2, win = 0;
        bool freeze = false;
        float noteSemis[8] {}; int nNotes = 1;          // chord (semitones from middle C, already in scale)
        const tuning::Tuning* tuning = nullptr;
    };

    class Texture
    {
    public:
        void prepare (float sampleRate)
        {
            sr = sampleRate;
            bufL.prepare ((int) (sr * 4)); bufR.prepare ((int) (sr * 4));
            for (auto& g : grains) g.active = false;
            tiltLo.setHz (700, sr); tiltLoR.setHz (700, sr);
            for (auto& d : drift) d = 0;
        }

        void process (float inL, float inR, float& outL, float& outR, const TextureParams& p)
        {
            // ---------------------------------------------------- noise bed
            float nL = 0, nR = 0;
            const float wL = rng.bi(), wR = rng.bi();
            switch (p.noise)
            {
                case 0: nL = wL * 0.5f; nR = wR * 0.5f; break;
                case 1: nL = pinkL.process (wL); nR = pinkR.process (wR); break;
                case 2: nL = brownL.process (wL); nR = brownR.process (wR); break;
                default: nL = rng.chance (0.002f) ? rng.bi() : 0.0f; nR = rng.chance (0.002f) ? rng.bi() : 0.0f; break;
            }
            float bedL = 0, bedR = 0;
            if (p.bed > 0.0005f)
            {
                if (--coefCounter <= 0)
                {
                    coefCounter = 64;
                    const float q = 2.0f + p.resq * p.resq * 120.0f;
                    nb = std::min (8, p.nNotes * 2);
                    for (int k = 0; k < nb; ++k)
                    {
                        const float semis = p.noteSemis[k % p.nNotes] + 12.0f * (float) (k / p.nNotes) - 12.0f + drift[k] * p.drift * 0.3f;
                        const float hz = (float) tuning::Tuning::toHz (semis);
                        bandL[k].set (hz, q, sr); bandR[k].set (hz * 1.0007f, q, sr);
                    }
                    bandNorm = 0.9f / (std::sqrt (q) * std::sqrt ((float) std::max (1, nb)));
                }
                for (int k = 0; k < nb; ++k)
                {
                    bandL[k].process (nL); bandR[k].process (nR);
                    bedL += bandL[k].bp; bedR += bandR[k].bp;
                }
                const float dry = (1.0f - p.resq) * 0.25f;
                bedL = (bedL * bandNorm + nL * dry) * p.bed;
                bedR = (bedR * bandNorm + nR * dry) * p.bed;
            }
            // ---------------------------------------------------- drone
            if (p.drone > 0.0005f)
            {
                if (++driftCounter >= 256)
                {
                    driftCounter = 0;
                    for (int k = 0; k < 8; ++k) { drift[k] += rng.bi() * 0.08f; drift[k] *= 0.995f; drift[k] = clampv (drift[k], -1.0f, 1.0f); }
                }
                float dl = 0, dr = 0;
                const int nd = std::min (4, p.nNotes);
                for (int k = 0; k < nd; ++k)
                {
                    const float semis = p.noteSemis[k] - 12.0f + drift[k + 4] * p.drift * 0.5f;
                    const float hz = (float) tuning::Tuning::toHz (semis);
                    dph[k] += hz / sr; dph[k] -= std::floor (dph[k]);
                    const float s = std::sin (kTwoPi * dph[k]) + 0.15f * std::sin (kTwoPi * 2.0f * dph[k]);
                    const float pan = (k & 1) ? 0.35f : 0.65f;
                    dl += s * (1 - pan); dr += s * pan;
                }
                const float g = p.drone * 0.22f / std::sqrt ((float) std::max (1, nd));
                bedL += dl * g; bedR += dr * g;
            }
            // ---------------------------------------------------- grains
            float gL = 0, gR = 0;
            if (p.grain > 0.0005f)
            {
                if (! p.freeze)
                {
                    float sL = 0, sR = 0;
                    if (p.gsrc != 1) { sL += inL; sR += inR; }
                    if (p.gsrc != 0) { sL += bedL; sR += bedR; }
                    bufL.write (sL); bufR.write (sR);
                }
                const float sizeS = clampv (p.sizeMs, 5.0f, 500.0f) * 0.001f * sr;
                spawnAcc += p.dens / sr;
                if (spawnAcc >= 1.0f)
                {
                    spawnAcc -= 1.0f + (rng.uni() - 0.5f) * 0.6f;
                    spawn (sizeS, p);
                }
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    const float ph = g.age / g.len;
                    float w = 1;
                    if (g.win == 0) w = 0.5f - 0.5f * std::cos (kTwoPi * ph);
                    else if (g.win == 1) { const float e = 0.25f; w = ph < e ? 0.5f - 0.5f * std::cos (kPi * ph / e) : (ph > 1 - e ? 0.5f - 0.5f * std::cos (kPi * (1 - ph) / e) : 1.0f); }
                    else w = std::min (1.0f, std::min (ph, 1 - ph) * 40.0f);
                    const float a = bufL.linear (g.pos) * w, b = bufR.linear (g.pos) * w;
                    gL += a * g.pl + b * g.cross; gR += b * g.pr + a * g.cross;
                    g.pos += g.inc; g.age += 1;
                    if (g.age >= g.len) g.active = false;
                }
                const float overlap = std::max (1.0f, p.dens * sizeS / sr);
                const float norm = p.grain * 1.4f / std::sqrt (overlap);
                gL *= norm; gR *= norm;
            }
            float oL = bedL + gL, oR = bedR + gR;
            // ---------------------------------------------------- tilt
            if (std::abs (p.tilt) > 0.01f)
            {
                const float lo = tiltLo.lp (oL), loR = tiltLoR.lp (oR);
                const float gl = dbToGain (-p.tilt * 9.0f), gh = dbToGain (p.tilt * 9.0f);
                oL = lo * gl + (oL - lo) * gh; oR = loR * gl + (oR - loR) * gh;
            }
            outL = flushDenorm (oL); outR = flushDenorm (oR);
        }

    private:
        struct Grain { bool active = false; double pos = 0; double inc = 1; float len = 1, age = 0, pl = 1, pr = 1, cross = 0; int win = 0; };

        void spawn (float sizeS, const TextureParams& p)
        {
            for (auto& g : grains)
            {
                if (g.active) continue;
                double semis = p.pitch + rng.bi() * p.prand;
                if (p.tuning != nullptr && (p.prand > 0.01f || std::abs (p.pitch) > 0.01f))
                {
                    const double base = p.noteSemis[0];
                    semis = p.tuning->snap (base + semis, p.snap) - base;
                }
                const double ratio = std::exp2 (semis / 12.0);
                const bool rev = rng.chance (p.rev);
                const double newest = (double) bufL.w - 4.0;
                const double back = (double) p.spray * rng.uni() * sr * 1.5;
                double start = newest - back - (rev ? 0.0 : sizeS * std::max (1.0, ratio));
                if (start < (double) bufL.w - (double) bufL.size() + sizeS * 3) start = newest - sizeS * std::max (1.0, ratio) - 8;
                g.pos = start; g.inc = rev ? -ratio : ratio; g.len = sizeS; g.age = 0; g.win = p.win;
                const float pan = rng.bi() * p.spread;
                g.pl = std::cos ((pan + 1) * kPi * 0.25f) * 1.41f; g.pr = std::sin ((pan + 1) * kPi * 0.25f) * 1.41f;
                g.cross = 0;
                g.active = true;
                return;
            }
        }

        float sr = 48000;
        Rng rng { 0xC0FFEEu };
        Pink pinkL, pinkR; Brown brownL, brownR;
        Svf bandL[8], bandR[8];
        int nb = 0, coefCounter = 0, driftCounter = 0;
        float bandNorm = 1;
        float drift[8] {};
        float dph[4] {};
        Ring bufL, bufR;
        std::array<Grain, 48> grains;
        float spawnAcc = 0;
        OnePole tiltLo, tiltLoR;
    };
}
