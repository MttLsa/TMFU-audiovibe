#pragma once
#include "DspCommon.h"

// =====================================================================================================
//  TB-303 VCF model
//  The 303 filter is a 4-pole *diode* ladder: its stages are not buffered from each other, so they do not
//  share one cutoff. Following the published analyses (Open303 TeeBeeFilter, Pirkle's TPT diode ladder and
//  the "first pole an octave higher" observation used by JC-303), it is modelled as:
//   * 4 TPT one-pole lowpass stages, first pole one octave above the others  -> the typical 18..24 dB slope
//   * a high-pass (~150 Hz) inside the resonance feedback path  -> resonance thins the bass instead of
//     boosting it, the "squelch" of the original
//   * tanh saturation at the ladder input (transistor/diode non-linearity) + resonance gain compensation
//  solved with zero-delay feedback, run 2x oversampled by the caller.
// =====================================================================================================
namespace dsp
{
    struct Diode303
    {
        float s[4] {}, fbLp = 0;
        void reset() { s[0] = s[1] = s[2] = s[3] = 0; fbLp = 0; }

        // fc in Hz, sr = (oversampled) sample rate, res 0..1, drive >= 1
        inline float process (float in, float fc, float sr, float res, float drive)
        {
            const float g1 = std::tan (pi * std::min (fc * 2.0f, sr * 0.45f) / sr);
            const float g  = std::tan (pi * std::min (fc, sr * 0.45f) / sr);
            const float G1 = g1 / (1.0f + g1), G = g / (1.0f + g);
            const float k = res * res * 5.2f + res * 1.6f;               // self-oscillates near res = 1
            const float c1 = (1.0f - G1) * s[0], c2 = (1.0f - G) * s[1], c3 = (1.0f - G) * s[2], c4 = (1.0f - G) * s[3];
            const float Gp = G1 * G * G * G;
            const float S = G * G * G * c1 + G * G * c2 + G * c3 + c4;
            const float x = in * drive;
            // feedback = y4 - LP150(y4)  (high-pass in the loop); fbLp from the previous sample
            const float yEst = (Gp * (x + k * fbLp) + S) / (1.0f + k * Gp);
            const float u = fastTanh (x - k * (yEst - fbLp));
            float y = u;
            const float Gs[4] = { G1, G, G, G };
            for (int i = 0; i < 4; ++i)
            {
                const float v = (y - s[i]) * Gs[i];
                const float o = v + s[i];
                s[i] = o + v;
                y = o;
            }
            const float a = 1.0f - std::exp (-twoPi * 150.0f / sr);
            fbLp += (y - fbLp) * a;
            if (! std::isfinite (y)) { reset(); return 0.0f; }
            return y * (1.0f + 0.45f * k) / std::sqrt (drive);
        }
    };

    // =================================================================================================
    //  Pedal distortions (post filter, 2x oversampled by the caller)
    //   RAT : LM308 gain stage (pre-emphasis legs at ~60 Hz / ~1.5 kHz, gain-dependent bandwidth because of
    //         the op-amp gain-bandwidth/slew limit) -> 1N914 silicon diode pair to ground (symmetric hard
    //         knee) -> passive "Filter" low-pass (475 Hz .. 20 kHz).  The classic acid-techno 303 pedal.
    //   DS-1: transistor booster + op-amp gain -> silicon diodes (slightly asymmetric) -> tone stack that
    //         blends a low-pass and a high-pass (mid scoop).
    //   Tube: asymmetric soft saturation.
    // =================================================================================================
    struct PedalDist
    {
        float hp1 = 0, hp2 = 0, opLp = 0, toneLp = 0, toneHp = 0, dc = 0, dcx = 0;
        void reset() { hp1 = hp2 = opLp = toneLp = toneHp = dc = dcx = 0; }

        static inline float diode (float x, float knee, float n)
        {
            const float a = std::abs (x / knee);
            return x / std::pow (1.0f + std::pow (a, n), 1.0f / n);
        }

        inline float process (int type, float x, float gain, float tone, float sr)
        {
            float y = x;
            if (type == 1)   // Tube
            {
                const float k = 1.0f + gain * 12.0f;
                y = fastTanh (x * k + 0.15f * gain) - fastTanh (0.15f * gain);
                y /= std::sqrt (k) * 0.8f;
            }
            else if (type == 2)   // RAT
            {
                const float a60 = 1.0f - std::exp (-twoPi * 60.0f / sr), a1k5 = 1.0f - std::exp (-twoPi * 1500.0f / sr);
                hp1 += (x - hp1) * a60; hp2 += (x - hp2) * a1k5;
                const float low = x - hp1, high = x - hp2;            // two RC legs of the op-amp feedback network
                const float A = 1.0f + gain * gain * 1800.0f;
                float v = low * (1.0f + A * 0.08f) + high * A;
                const float bw = std::clamp (900000.0f / A, 1800.0f, 20000.0f);   // gain-bandwidth limit
                opLp += (v - opLp) * (1.0f - std::exp (-twoPi * bw / sr));
                v = diode (opLp, 0.55f, 2.6f);                            // 1N914 pair
                const float fc = 475.0f * std::pow (20000.0f / 475.0f, tone);
                toneLp += (v - toneLp) * (1.0f - std::exp (-twoPi * fc / sr));
                y = toneLp * 1.25f;
            }
            else if (type == 3)   // DS-1
            {
                const float a7 = 1.0f - std::exp (-twoPi * 7.0f / sr);
                hp1 += (x - hp1) * a7;
                const float A = 1.0f + gain * gain * 300.0f;
                float v = (x - hp1) * 4.0f * A;
                const float bw = std::clamp (2500000.0f / A, 4000.0f, 20000.0f);
                opLp += (v - opLp) * (1.0f - std::exp (-twoPi * bw / sr));
                v = opLp > 0 ? diode (opLp, 0.6f, 3.2f) : diode (opLp, 0.55f, 3.2f);
                toneLp += (v - toneLp) * (1.0f - std::exp (-twoPi * 234.0f / sr));
                toneHp += (v - toneHp) * (1.0f - std::exp (-twoPi * 1060.0f / sr));
                const float hp = v - toneHp;
                y = (toneLp * (1.0f - tone) + hp * tone * 1.6f) * 1.4f;
            }
            // DC block
            const float out = y - dcx + 0.9995f * dc; dcx = y; dc = out;
            return std::isfinite (out) ? out : 0.0f;
        }
    };
}
