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

        void trigger (float freq, int model, int exc, float decayMs, float material, float inharm, float pos, float vel,
                      float lvl, float pan, float sr, const float* inputHistory, int historyLen)
        {
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

        void process (float& l, float& r)
        {
            if (! active) return;
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
            y = softClip (y * 1.4f * level);
            l += y * panL; r += y * panR;
            if (std::abs (y) < 1.0e-5f && excLeft == 0) { if (++silentCount > 2048) { active = false; for (auto& m : modes) m.y1 = m.y2 = 0; } }
            else silentCount = 0;
        }
    };
}
