#pragma once
#include "Common.h"

// =====================================================================================================
//  Organic minimal percussion: modal synthesis (bank of 2-pole resonators) + exciters. No samples.
//  Models: Wood, Glass, Metal, Skin, Tube, Stone, Plastic.
// =====================================================================================================
namespace mv
{
    struct ModalModel { int n; float ratio[12]; float gain[12]; float decayMul; float click; };

    inline const ModalModel& modalModel (int m)
    {
        static const ModalModel models[] = {
            // Wood: free-free bar, short highs
            { 6, { 1, 2.572f, 4.644f, 6.984f, 9.723f, 12.8f }, { 1, 0.55f, 0.3f, 0.18f, 0.1f, 0.06f }, 0.35f, 0.35f },
            // Glass: wine-glass partials, long and bright
            { 7, { 1, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f, 15.9f }, { 1, 0.7f, 0.5f, 0.35f, 0.22f, 0.15f, 0.1f }, 1.6f, 0.15f },
            // Metal: plate / bell, dense inharmonic
            { 10, { 1, 1.35f, 2.01f, 2.4f, 2.8f, 3.4f, 4.1f, 4.6f, 5.4f, 6.3f }, { 1, 0.8f, 0.7f, 0.6f, 0.55f, 0.45f, 0.35f, 0.3f, 0.25f, 0.2f }, 1.3f, 0.25f },
            // Skin: circular membrane (Bessel zeros)
            { 8, { 1, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f }, { 1, 0.6f, 0.45f, 0.35f, 0.25f, 0.2f, 0.15f, 0.1f }, 0.5f, 0.5f },
            // Tube: open pipe, harmonic, odd emphasis
            { 8, { 1, 2, 3, 4, 5, 6, 7, 8 }, { 1, 0.35f, 0.6f, 0.2f, 0.35f, 0.12f, 0.2f, 0.08f }, 0.8f, 0.2f },
            // Stone: hard, short, inharmonic
            { 6, { 1, 1.73f, 2.95f, 4.12f, 5.67f, 7.3f }, { 1, 0.7f, 0.6f, 0.4f, 0.3f, 0.2f }, 0.18f, 0.7f },
            // Plastic: damped, few modes
            { 4, { 1, 2.4f, 3.9f, 5.1f }, { 1, 0.4f, 0.2f, 0.1f }, 0.25f, 0.45f },
        };
        return models[clampv (m, 0, 6)];
    }

    // =================================================================================================
    //  Analog-style 808 voices (synthesised, no samples).
    //  Kick / Tom : sine with pitch sweep (Inharmonic = sweep depth) + click (Material = click / tone)
    //  Snare      : two detuned sines (180 / 330 Hz x tune) + high-passed noise (Material = snappy)
    //  Clap       : band-passed noise, 4 rapid bursts + tail
    //  Hats       : six square oscillators (205-800 Hz x tune) through band-pass + high-pass (Material = brightness)
    // =================================================================================================
    struct Drum808
    {
        enum Type { Kick = 0, Tom, Snare, Clap, ClosedHat, OpenHat };
        int type = 0; float sr = 48000, t = 0, f0 = 50, sweep = 2, dec = 0.4f, tone = 0.5f, vel = 1;
        double ph[6] {}; float ph2 = 0;
        Svf bp1, bp2, hp, nhp, nlp; Rng rng { 0x808u };
        static constexpr float hatHz[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

        void trigger (int ty, float hz, float decayMs, float material, float inharm, float v, float sampleRate)
        {
            type = ty; sr = sampleRate; t = 0; f0 = hz; vel = v; tone = material;
            dec = std::max (0.005f, decayMs * 0.001f);
            sweep = 0.5f + 3.5f * inharm;
            for (auto& p : ph) p = 0;
            ph2 = 0;
            bp1.reset(); bp2.reset(); hp.reset(); nhp.reset(); nlp.reset();
            switch (type)
            {
                case Snare: nhp.set (1800, 0.7f, sr); nlp.set (9000, 0.7f, sr); break;
                case Clap:  bp1.set (clampv (hz, 400.0f, 4000.0f), 1.6f, sr); hp.set (500, 0.7f, sr); break;
                case ClosedHat: case OpenHat:
                {
                    const float c = 7000.0f + 4000.0f * material;
                    bp1.set (c, 1.2f, sr); bp2.set (c * 1.3f, 1.0f, sr); hp.set (6500.0f, 0.7f, sr);
                    break;
                }
                default: nhp.set (clampv (hz * 4.0f, 200.0f, 8000.0f), 1.0f, sr); break;
            }
        }

        float process()
        {
            const float dt = 1.0f / sr;
            float y = 0;
            switch (type)
            {
                case Kick: case Tom:
                {
                    const float swT = type == Kick ? 0.010f : 0.020f;
                    const float f = f0 * (1.0f + (type == Kick ? sweep : sweep * 0.25f) * std::exp (-t / swT));
                    ph[0] += f * dt; ph[0] -= std::floor (ph[0]);
                    const float amp = std::exp (-t * 6.9f / dec);
                    y = std::sin (kTwoPi * (float) ph[0]) * amp;
                    // click (attack transient) and a touch of noise for the tom
                    if (t < 0.004f) y += nhp.process (rng.bi()) * (1.0f - t / 0.004f) * (0.15f + 0.6f * tone) * (type == Kick ? 1.0f : 0.5f);
                    if (type == Tom) y += nhp.process (rng.bi()) * 0.06f * amp;
                    y = fastTanh (y * (1.2f + 0.8f * tone));
                    break;
                }
                case Snare:
                {
                    ph[0] += f0 * dt; ph[0] -= std::floor (ph[0]);
                    ph[1] += f0 * 1.83f * dt; ph[1] -= std::floor (ph[1]);
                    const float body = (std::sin (kTwoPi * (float) ph[0]) * 0.6f + std::sin (kTwoPi * (float) ph[1]) * 0.4f) * std::exp (-t * 6.9f / (dec * 0.35f));
                    nhp.process (rng.bi()); const float n = nlp.process (nhp.hp);
                    const float noise = n * std::exp (-t * 6.9f / dec) * (0.4f + 1.2f * tone);
                    y = fastTanh ((body * (1.1f - 0.5f * tone) + noise) * 1.3f);
                    break;
                }
                case Clap:
                {
                    // 4 bursts 9 ms apart, then the tail
                    float env = 0;
                    for (int k = 0; k < 4; ++k)
                    {
                        const float tk = t - 0.009f * k;
                        if (tk >= 0 && tk < 0.009f) env = std::max (env, std::exp (-tk * 380.0f));
                    }
                    if (t >= 0.027f) env = std::max (env, 0.7f * std::exp (-(t - 0.027f) * 6.9f / dec));
                    hp.process (bp1.process (rng.bi()) * 2.5f);
                    y = fastTanh (hp.hp * env * (1.4f + tone));
                    break;
                }
                default: // hats
                {
                    const float ratio = f0;          // tune ratio
                    float sq = 0;
                    for (int k = 0; k < 6; ++k)
                    {
                        ph[k] += hatHz[k] * ratio * dt; ph[k] -= std::floor (ph[k]);
                        sq += ph[k] < 0.5 ? 1.0f : -1.0f;
                    }
                    bp1.process (sq * 0.25f); bp2.process (bp1.bp);
                    hp.process (bp2.bp + rng.bi() * 0.05f * (0.5f + tone));
                    const float d = type == ClosedHat ? std::min (dec, 0.25f) : dec;
                    y = hp.hp * std::exp (-t * 6.9f / d) * 2.2f;
                    break;
                }
            }
            t += dt;
            return y * vel;
        }
        bool finished() const
        {
            const float len = type == ClosedHat ? std::min (dec, 0.25f) : (type == Snare || type == Clap ? dec + 0.05f : dec);
            return t > len * 1.2f + 0.03f;
        }
    };

    struct ModalVoice
    {
        struct Mode { float b0 = 0, a1 = 0, a2 = 0, y1 = 0, y2 = 0, g = 0; };
        std::array<Mode, 12> modes;
        int nModes = 0;
        // exciter
        int excType = 0, excLeft = 0, excLen = 1; float excAmp = 0;
        Svf excFilter; Rng rng { 0x777u };
        std::array<float, 2048> grab {}; int grabLen = 0, grabPos = 0;
        float clickAmp = 0, clickEnv = 0, clickDecay = 0.9f, panL = 0.7f, panR = 0.7f, level = 0;
        bool active = false; int silentCount = 0;
        bool is808 = false; Drum808 drum; float choke = 1.0f, chokeStep = 0;

        void trigger (float freq, int model, int exc, float decayMs, float material, float inharm, float pos, float vel,
                      float lvl, float pan, float sr, const float* inputHistory, int historyLen)
        {
            choke = 1.0f; chokeStep = 0;
            is808 = model >= 7;
            if (is808)
            {
                drum.trigger (model - 7, freq, decayMs, material, inharm, vel, sr);
                level = lvl;
                const float p = clampv (pan, -1.0f, 1.0f);
                panL = std::cos ((p + 1) * kPi * 0.25f); panR = std::sin ((p + 1) * kPi * 0.25f);
                active = true; silentCount = 0;
                return;
            }
            const auto& m = modalModel (model);
            nModes = 0;
            const float bright = 0.4f + 0.6f * vel;
            for (int k = 0; k < m.n; ++k)
            {
                float r = m.ratio[k];
                r = r * (1.0f + inharm * 0.12f * (r - 1.0f) * (r - 1.0f) / (1.0f + 0.1f * r * r));
                const float f = freq * r;
                if (f > sr * 0.45f || f < 15.0f) continue;
                const float t60 = std::max (0.005f, decayMs * 0.001f * m.decayMul * (0.6f + 0.4f * vel)
                                                   / (1.0f + material * 3.0f * (r - 1.0f)));
                const float rad = std::exp (-6.9078f / (t60 * sr));
                const float w = kTwoPi * f / sr;
                auto& md = modes[(size_t) nModes++];
                md.a1 = 2.0f * rad * std::cos (w); md.a2 = -rad * rad; md.b0 = std::sin (w);
                const float posGain = std::abs (std::sin (kPi * (float) (k + 1) * (0.08f + 0.84f * pos)));
                md.g = m.gain[k] * (0.3f + 0.7f * posGain) * (k == 0 ? 1.0f : std::pow (bright, 0.5f + 0.4f * k));
                md.y1 = md.y2 = 0;
            }
            excType = exc;
            switch (exc)
            {
                case 0: excLen = std::max (1, (int) (sr * 0.0006f)); break;          // click
                case 1: excLen = std::max (1, (int) (sr * (0.004f + 0.02f * material))); break;  // noise burst
                case 2: excLen = std::max (2, (int) (sr * (0.0015f + 0.004f * (1 - vel)))); break; // mallet
                default:                                                                       // input grab
                {
                    grabLen = std::min (historyLen, (int) grab.size());
                    for (int i = 0; i < grabLen; ++i) grab[(size_t) i] = inputHistory[i];
                    grabPos = 0; excLen = grabLen;
                    break;
                }
            }
            excLeft = excLen; excAmp = vel;
            excFilter.set (std::min (sr * 0.45f, freq * (2.0f + 10.0f * vel)), 0.7f, sr);
            clickAmp = m.click * vel * (exc == 2 ? 0.2f : 1.0f);
            clickEnv = 1; clickDecay = std::exp (-1.0f / (0.0015f * sr));
            level = lvl;
            const float p = clampv (pan, -1.0f, 1.0f);
            panL = std::cos ((p + 1) * kPi * 0.25f); panR = std::sin ((p + 1) * kPi * 0.25f);
            active = true; silentCount = 0;
        }

        // per-voice filter (Off / HP / BP / LP)
        void setFilter (int type, float cut, float res, float sr)
        {
            if (type == fType && std::abs (cut - fCut) < 0.5f && std::abs (res - fRes) < 0.001f) return;
            if (type != fType) flt.reset();
            fType = type; fCut = cut; fRes = res;
            if (type > 0) flt.set (cut, 0.6f + res * res * 12.0f, sr);
        }
        float filter (float y)
        {
            if (fType <= 0) return y;
            flt.process (y);
            return fType == 1 ? flt.hp : fType == 2 ? flt.bpNorm() : flt.lp;
        }
        Svf flt; int fType = 0; float fCut = -1, fRes = -1;

        // fast fade-out (closed hat chokes the open hat, transport stop)
        void startChoke (float ms, float sr) { if (active) chokeStep = 1.0f / std::max (1.0f, ms * 0.001f * sr); }
        int modelType() const { return is808 ? 7 + drum.type : -1; }

        void process (float& l, float& r)
        {
            if (! active) return;
            if (chokeStep > 0) { choke -= chokeStep; if (choke <= 0) { active = false; for (auto& m : modes) m.y1 = m.y2 = 0; return; } }
            if (is808)
            {
                const float y = filter (drum.process()) * level * choke;
                l += y * panL; r += y * panR;
                if (drum.finished()) active = false;
                return;
            }
            float e = 0;
            if (excLeft > 0)
            {
                const int i = excLen - excLeft;
                switch (excType)
                {
                    case 0: e = (i == 0 ? 1.0f : 0.0f) + rng.bi() * 0.3f * (1.0f - (float) i / excLen); break;
                    case 1: e = excFilter.process (rng.bi()) * (1.0f - (float) i / excLen) * 1.5f; break;
                    case 2: e = std::sin (kPi * (float) i / excLen) * 0.6f; break;
                    default: e = grabPos < grabLen ? grab[(size_t) grabPos++] * 2.0f * (1.0f - (float) i / std::max (1, excLen)) : 0.0f; break;
                }
                e *= excAmp; --excLeft;
            }
            float y = 0;
            for (int k = 0; k < nModes; ++k)
            {
                auto& m = modes[(size_t) k];
                const float v = m.b0 * e + m.a1 * m.y1 + m.a2 * m.y2;
                m.y2 = m.y1; m.y1 = v;
                y += v * m.g;
            }
            if (clickEnv > 1.0e-4f) { y += rng.bi() * clickAmp * clickEnv * 0.5f; clickEnv *= clickDecay; }
            y = filter (softClip (y * 1.4f * level)) * choke;
            l += y * panL; r += y * panR;
            if (std::abs (y) < 1.0e-5f && excLeft == 0) { if (++silentCount > 2048) { active = false; for (auto& m : modes) m.y1 = m.y2 = 0; } }
            else silentCount = 0;
        }
    };
}
