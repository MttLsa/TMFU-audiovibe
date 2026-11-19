#pragma once
#include "Common.h"

// =====================================================================================================
//  Rhythm lanes: polymeter (own length), polyrhythm (division x tuplet ratio or free Hz), Euclid,
//  grid mode, swing, probability, ratchets, conditions, humanize; clocked by the host transport,
//  by transients of the input or by MIDI notes.
// =====================================================================================================
namespace mv
{
    constexpr int kMaxSteps = 64;

    struct LaneConfig
    {
        bool on = true, sync = true, grid = false;
        int steps = 16, pulses = 4, rot = 0, ratch = 1, src = 0, cond = 0;
        float stepBeats = 0.25f, hz = 4, swing = 0, prob = 1, rprob = 0, hum = 0, density = 0.5f;
        const uint8_t* gates = nullptr;    // grid gates (kMaxSteps)
        const float* values = nullptr;     // per-step values (kMaxSteps), also velocity in grid mode
    };

    struct LaneTrigger { float vel = 1; bool ratchet = false; };

    class Lane
    {
    public:
        static bool euclid (int i, int steps, int pulses, int rot)
        {
            if (steps <= 0 || pulses <= 0) return false;
            if (pulses >= steps) return true;
            int j = (i - rot) % steps; if (j < 0) j += steps;
            return ((j * pulses) % steps) < pulses;
        }

        static float densityProb (float p, float density)
        {
            // density 0.5 = as set, 0 = nothing, 1 = 1-(1-p)^2
            p = clampv (p, 0.0f, 1.0f);
            const float e = density * 2.0f;
            if (e <= 0.0001f) return 0.0f;
            return 1.0f - std::pow (1.0f - p, e);
        }

        void reset() { lastStep = INT64_MIN; npend = 0; freePos = 0; extCounter = 0; curIdx = 0; }
        void seed (uint32_t s) { rng.seed (s); }

        int currentIndex() const { return curIdx; }
        float currentValue() const { return curValue; }
        int64_t currentStep() const { return lastStep; }

        // one sample; beat = host/internal beat position; returns true if a trigger fires now
        bool tick (const LaneConfig& c, double beat, bool running, float sr, LaneTrigger& out)
        {
            if (! c.on) { npend = 0; return false; }
            double s;
            if (c.src != 0) return firePending (freePos, out);     // event clocked: only pending ratchets
            if (c.sync)
            {
                if (! running) { npend = 0; return false; }
                s = beat / std::max (1.0e-4f, c.stepBeats);
            }
            else { freePos += c.hz / sr; s = freePos; }

            const int64_t si = (int64_t) std::floor (s);
            if (si != lastStep)
            {
                const bool jump = lastStep == INT64_MIN || si < lastStep || si > lastStep + 2;
                lastStep = si;
                const double frac = s - (double) si;
                npend = 0;
                if (! jump || frac < 0.05) evaluate (c, si, (double) si, true);
                else updateValue (c, si);
            }
            return firePending (s, out);
        }

        // event clock (transient / MIDI): advance one step and fire immediately
        bool external (const LaneConfig& c, LaneTrigger& out)
        {
            if (! c.on) return false;
            const int64_t si = extCounter++;
            lastStep = si;
            npend = 0;
            evaluate (c, si, -1.0, false);
            if (npend == 0) return false;
            out = pend[0].trig; npend = 0;
            return true;
        }

    private:
        void updateValue (const LaneConfig& c, int64_t si)
        {
            const int steps = std::max (1, c.steps);
            int idx = (int) (si % steps); if (idx < 0) idx += steps;
            curIdx = idx;
            curValue = c.values ? c.values[idx] : 0.0f;
        }

        void evaluate (const LaneConfig& c, int64_t si, double base, bool timed)
        {
            const int steps = std::max (1, c.steps);
            int idx = (int) (si % steps); if (idx < 0) idx += steps;
            int64_t cycle = si / steps; if (si < 0 && si % steps != 0) --cycle;
            curIdx = idx;
            curValue = c.values ? c.values[idx] : 0.0f;
            const bool gate = c.grid ? (c.gates && c.gates[idx] != 0) : euclid (idx, steps, c.pulses, c.rot);
            if (! gate) return;
            bool ok = true;
            switch (c.cond)
            {
                case 1: ok = (cycle % 2) == 0; break;
                case 2: ok = (cycle % 2) == 1; break;
                case 3: ok = (cycle % 3) == 0; break;
                case 4: ok = (cycle % 4) == 0; break;
                case 5: ok = c.density > 0.75f; break;
                case 6: ok = c.density <= 0.75f; break;
                case 7: ok = cycle == 0; break;
                default: break;
            }
            if (! ok || ! rng.chance (densityProb (c.prob, c.density))) return;
            float vel = c.grid ? 0.35f + 0.65f * (c.values ? c.values[idx] : 1.0f) : (idx == 0 ? 1.0f : 0.8f);
            if (! timed) { pend[0] = { { vel, false }, 0.0 }; npend = 1; return; }
            double off = (si & 1) ? c.swing * 0.5 : 0.0;
            off += rng.uni() * c.hum * 0.2f;
            const int r = (c.ratch > 1 && rng.chance (densityProb (c.rprob, c.density))) ? std::min (8, c.ratch) : 1;
            for (int k = 0; k < r; ++k)
            {
                const double t = base + off + (1.0 - off) * (double) k / r;
                pend[npend++] = { { k == 0 ? vel : vel * (0.85f - 0.05f * k), k > 0 }, t };
            }
        }

        bool firePending (double s, LaneTrigger& out)
        {
            for (int k = 0; k < npend; ++k)
                if (pend[k].t <= s)
                {
                    out = pend[k].trig;
                    for (int j = k + 1; j < npend; ++j) pend[j - 1] = pend[j];
                    --npend;
                    return true;
                }
            return false;
        }

        struct Pending { LaneTrigger trig; double t; };
        Pending pend[8];
        int npend = 0;
        int64_t lastStep = INT64_MIN, extCounter = 0;
        double freePos = 0;
        int curIdx = 0;
        float curValue = 0;
        Rng rng { 0x51Fu };
    };
}
