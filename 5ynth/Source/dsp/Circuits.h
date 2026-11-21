#pragma once
#include "DspCommon.h"

// =====================================================================================================
//  Circuit models of the real instruments' filters, chosen by the synth model of a module.
// =====================================================================================================
namespace dsp
{
    enum class Circuit : int
    {
        Generic = 0,
        Moog, Minimoog,         // Moog transistor ladder (Minimoog also: 3-stage contours + DECAY switch)
        RolandVCO, RolandDCO    // Roland IR3109 OTA filter; VCO = analog oscillators (drift), DCO = stable digital clocks
    };

    // Moog transistor ladder (Minimoog, modular 904A, Polymoog, Source, Sub 37), Huovilainen's non-linear model
    // (DAFx 2004): every stage is a differential transistor pair -> its own tanh; run 2x oversampled inside, with
    // the half-sample delay of the feedback compensated and the cutoff / resonance tuning corrected.
    // Behaviour of the circuit: -12 dB at the cutoff with no resonance, bass loss as resonance rises, self-
    // oscillation exactly at the cutoff at full Emphasis, progressive saturation when the mixer drives it hard.
    struct MoogLadder
    {
        float stage[4] {}, stageTanh[3] {}, delay[6] {};
        float tune = 0, resQuad = 0, lastFc = -1, lastRes = -1, lastSr = -1;
        void reset() { for (auto& v : stage) v = 0; for (auto& v : stageTanh) v = 0; for (auto& v : delay) v = 0; }

        void set (float cutoff, float sr, float res)
        {
            if (cutoff == lastFc && res == lastRes && sr == lastSr) return;
            lastFc = cutoff; lastRes = res; lastSr = sr;
            const float fc = std::min (cutoff, sr * 0.45f) / sr, f = fc * 0.5f, fc2 = fc * fc, fc3 = fc2 * fc;
            const float fcr = 1.8730f * fc3 + 0.4955f * fc2 - 0.6490f * fc + 0.9988f;
            const float acr = -3.9364f * fc2 + 1.8409f * fc + 0.9968f;
            tune = 1.0f - std::exp (-twoPi * f * fcr);
            resQuad = 4.0f * res * acr;
        }

        // cutoff in Hz, sr = sample rate, res 0..1 (1 = self-oscillation), drive >= 1 (mixer level into the ladder)
        inline float process (float in, float cutoff, float sr, float res, float drive)
        {
            set (cutoff, sr, res);
            const float x = in * drive;
            for (int j = 0; j < 2; ++j)
            {
                const float input = x - resQuad * delay[5];
                delay[0] = stage[0] = delay[0] + tune * (fastTanh (input) - stageTanh[0]);
                for (int k = 1; k < 4; ++k)
                {
                    stageTanh[k - 1] = fastTanh (stage[k - 1]);
                    stage[k] = delay[k] + tune * (stageTanh[k - 1] - (k != 3 ? stageTanh[k] : fastTanh (delay[k])));
                    delay[k] = stage[k];
                }
                delay[5] = (stage[3] + delay[4]) * 0.5f;
                delay[4] = stage[3];
            }
            if (! std::isfinite (delay[5])) { reset(); return 0.0f; }
            return delay[5] / std::sqrt (drive);
        }
    };

    // Roland IR3109 (Juno-60 / 106, Jupiter-8, SH-101, JX-8P, Alpha Juno): four OTA integrators, buffered from each
    // other. Each OTA charges its capacitor with a current gm * tanh (input - output): the saturation acts on the
    // difference, stage by stage. Global resonance feedback with the input level partly compensated (the bass drops
    // much less than on a Moog when resonance rises), self-oscillation at full resonance. 2x oversampled inside, with
    // the half-sample delay of the feedback compensated.
    struct RolandOta
    {
        float y[4] {}, d4 = 0, out = 0;
        float tune = 0, k = 0, lastFc = -1, lastRes = -1, lastSr = -1;
        void reset() { for (auto& v : y) v = 0; d4 = out = 0; }
        void set (float cutoff, float sr, float res)
        {
            if (cutoff == lastFc && res == lastRes && sr == lastSr) return;
            lastFc = cutoff; lastRes = res; lastSr = sr;
            const float fc = std::min (cutoff, sr * 0.45f) / sr, f = fc * 0.5f, fc2 = fc * fc, fc3 = fc2 * fc;
            const float fcr = 1.8730f * fc3 + 0.4955f * fc2 - 0.6490f * fc + 0.9988f;
            const float acr = -3.9364f * fc2 + 1.8409f * fc + 0.9968f;
            tune = 1.0f - std::exp (-twoPi * f * fcr);
            k = 4.0f * res * acr;
        }
        inline float process (float in, float cutoff, float sr, float res, float drive)
        {
            set (cutoff, sr, res);
            const float x = in * drive * (1.0f + 0.5f * k);
            for (int j = 0; j < 2; ++j)
            {
                const float u = fastTanh (x - k * out);
                y[0] += tune * fastTanh (u - y[0]);
                y[1] += tune * fastTanh (y[0] - y[1]);
                y[2] += tune * fastTanh (y[1] - y[2]);
                y[3] += tune * fastTanh (y[2] - y[3]);
                out = (y[3] + d4) * 0.5f; d4 = y[3];
            }
            if (! std::isfinite (out)) { reset(); return 0.0f; }
            return out / std::sqrt (drive);
        }
    };
}
