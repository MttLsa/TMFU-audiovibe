#pragma once
#include "DspCommon.h"

// =====================================================================================================
//  TB-303 VCF model: coupled 4-pole diode ladder
//  In the 303 the four diode-pair stages are NOT buffered: every capacitor is loaded by its neighbours. Linearised,
//  this gives the "diode ladder" equations (Zavalishin, The Art of VA Filter Design; Pirkle, AN-6 Diode Ladder
//  Filter), solved here per sample with zero-delay feedback (TPT one-poles with the inter-stage coupling terms).
//  Behaviour of the circuit that follows from it:
//   * soft knee and a resonance peak that sits below the stage cutoff (the internal cutoff is scaled so the peak
//     lands on the Cut Off Freq knob),
//   * the passband (bass) drops as resonance rises, and the loop self-oscillates at K ~ 17,
//   * a high-pass (~150 Hz) inside the resonance feedback path, as in the original circuit,
//   * tanh saturation of the transistor pair that drives the ladder.
//  Run 2x oversampled by the caller.
// =====================================================================================================
namespace dsp
{
    struct Diode303
    {
        struct Pole
        {
            float z1 = 0, alpha = 1, beta = 0, gamma = 1, delta = 0, eps = 0, a0 = 1, fb = 0;
            inline float fbOut() const { return beta * (z1 + fb * delta); }
            inline float run (float x)
            {
                x = x * gamma + fb + eps * fbOut();
                const float v = (a0 * x - z1) * alpha, o = v + z1;
                z1 = v + o;
                return o;
            }
        };
        Pole p[4];
        float fbLp = 0, Gamma = 0, SG[4] {}, lastFc = -1, lastSr = -1;
        void reset() { for (auto& q : p) q.z1 = 0; fbLp = 0; }

        void setCutoff (float fc, float sr)
        {
            if (fc == lastFc && sr == lastSr) return;
            lastFc = fc; lastSr = sr;
            const float g = std::tan (pi * std::min (fc, sr * 0.45f) / sr);
            const float G4 = 0.5f * g / (1.0f + g);
            const float G3 = 0.5f * g / (1.0f + g - 0.5f * g * G4);
            const float G2 = 0.5f * g / (1.0f + g - 0.5f * g * G3);
            const float G1 = g / (1.0f + g - g * G2);
            Gamma = G4 * G3 * G2 * G1;
            SG[0] = G4 * G3 * G2; SG[1] = G4 * G3; SG[2] = G4; SG[3] = 1.0f;
            const float a = g / (1.0f + g);
            for (auto& q : p) { q.alpha = a; q.delta = g; }
            p[0].beta = 1.0f / (1.0f + g - g * G2);        p[0].gamma = 1.0f + G1 * G2; p[0].eps = G2; p[0].a0 = 1.0f;
            p[1].beta = 1.0f / (1.0f + g - 0.5f * g * G3); p[1].gamma = 1.0f + G2 * G3; p[1].eps = G3; p[1].a0 = 0.5f;
            p[2].beta = 1.0f / (1.0f + g - 0.5f * g * G4); p[2].gamma = 1.0f + G3 * G4; p[2].eps = G4; p[2].a0 = 0.5f;
            p[3].beta = 1.0f / (1.0f + g);                 p[3].gamma = 1.0f;           p[3].eps = 0.0f; p[3].a0 = 0.5f;
        }

        // fc in Hz (where the resonance peak sits), sr = (oversampled) sample rate, res 0..1, drive >= 1
        inline float process (float in, float fc, float sr, float res, float drive)
        {
            setCutoff (fc * 1.42f, sr);                    // the coupled ladder resonates at ~0.7 x its stage cutoff
            const float K = 16.8f * res;                   // ~17 = self-oscillation of the loop
            p[3].fb = 0.0f; p[2].fb = p[3].fbOut(); p[1].fb = p[2].fbOut(); p[0].fb = p[1].fbOut();
            const float sigma = SG[0] * p[0].fbOut() + SG[1] * p[1].fbOut() + SG[2] * p[2].fbOut() + SG[3] * p[3].fbOut();
            const float x = in * drive;
            // resonance path high-passed (~150 Hz): fbLp is the low-passed feedback from the previous sample
            const float u = fastTanh ((x - K * (sigma - fbLp)) / (1.0f + K * Gamma));
            const float y = p[3].run (p[2].run (p[1].run (p[0].run (u))));
            fbLp += (y - fbLp) * (1.0f - std::exp (-twoPi * 150.0f / sr));
            if (! std::isfinite (y)) { reset(); return 0.0f; }
            return y * (1.0f + 0.06f * K) * 1.6f / std::sqrt (drive);
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
