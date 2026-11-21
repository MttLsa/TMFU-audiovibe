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
        RolandVCO, RolandDCO,   // Roland IR3109 OTA filter; VCO = analog oscillators (drift), DCO = stable digital clocks
        OberheimSEM,            // Oberheim OB-X: 2-pole state-variable filter (SEM style), no self-oscillation
        Cem3320,                // Curtis CEM3320 4-pole (Oberheim OB-Xa, Sequential Prophet-5 rev 3)
        Korg35,                 // Korg MS-20 2-pole with diode-clipped, self-oscillating resonance
        YamahaCS,               // Yamaha CS-80 12 dB/oct state-variable VCF (no self-oscillation)
        Arp,                    // ARP 4072 / 4012 4-pole transistor ladder (2500, 2600, Odyssey)
        Ems,                    // EMS VCS3 / Synthi diode ladder
        Ppg,                    // PPG Wave 2.3: 8-bit wavetables stepped wave by wave + SSM2044 4-pole VCF
        YamahaFM                // DX7 / TX-816 / DX21 / TX81Z: floating-point 12-bit DAC
    };

    // Yamaha DX output DAC: 12-bit mantissa scaled by a 3-bit exponent (analog gain switch) -> quantisation noise that
    // follows the signal level, the slight grit of the original
    inline float dxDac (float x)
    {
        const float a = std::abs (x);
        if (a < 1.0e-6f) return 0.0f;
        const float range = std::exp2 (std::min (0.0f, std::ceil (std::log2 (a))));   // 1, 1/2, 1/4 ... (3-bit exponent)
        const float step = std::max (range, 1.0f / 128.0f) / 2048.0f;
        return std::round (x / step) * step;
    }

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
        float comp = 0.5f, headroom = 1.0f;   // IR3109: 0.5 / 1.0;  CEM3320: less bass compensation, more input headroom
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
            const float x = in * drive * (1.0f + comp * k) * headroom;
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
            return out / (std::sqrt (drive) * headroom);
        }
    };

    // Oberheim SEM-style 2-pole state-variable filter (OB-X): 12 dB/oct, the bass stays put when resonance rises,
    // resonance peaks up to ~ +16 dB but never self-oscillates (the SEM's damping never reaches zero); the band-pass
    // integrator saturates softly (OTA). TPT / zero-delay feedback, 2x oversampled.
    struct SemSvf
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        inline float process (float in, float cutoff, float sr, float res, float drive)
        {
            const float g = std::tan (pi * std::min (cutoff, sr * 0.9f) / (2.0f * sr));
            const float k = 2.0f - 1.85f * res;
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float x = in * drive;
            float y = 0;
            for (int j = 0; j < 2; ++j)
            {
                const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = fastTanh (2.0f * v1 - ic1);
                ic2 = 2.0f * v2 - ic2;
                y = v2;
            }
            if (! std::isfinite (y)) { reset(); return 0.0f; }
            return y / std::sqrt (drive);
        }
    };

    // Korg MS-20 (Korg35, second version: OTA): 2-pole low-pass whose resonance can go past self-oscillation; the
    // resonance loop is limited by diodes (soft clip of the band-pass state) -> the screaming, gritty MS-20 peak.
    struct Korg35
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        inline float process (float in, float cutoff, float sr, float res, float drive)
        {
            const float g = std::tan (pi * std::min (cutoff, sr * 0.9f) / (2.0f * sr));
            const float k = 2.0f - 2.1f * res;                       // below 0 at the top: self-oscillation
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float x = fastTanh (in * drive);
            float y = 0;
            for (int j = 0; j < 2; ++j)
            {
                const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 1.2f * fastTanh ((2.0f * v1 - ic1) * (1.0f / 1.2f));   // diode limiter in the loop
                ic2 = 2.0f * v2 - ic2;
                y = v2;
            }
            if (! std::isfinite (y)) { reset(); return 0.0f; }
            return y / std::sqrt (drive);
        }
    };
}
