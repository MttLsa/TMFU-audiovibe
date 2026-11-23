#pragma once
#include "Utils.h"

namespace hs
{
// Bipolar LFO (-1..1), advanced at control rate. Free-running or locked to the host position.
class Lfo
{
public:
    enum Shape
    {
        Sine = 0,
        Triangle,
        SawUp,
        SawDown,
        Square,
        SampleHold,
        SmoothRandom,
        numShapes
    };

    void reset() noexcept
    {
        phase = 0.0;
        rng.state = 0x2545f491u;
        a = rng.bipolar();
        b = rng.bipolar();
    }

    void advance (double inc) noexcept
    {
        phase += inc;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            newCycle();
        }
    }

    void setPhase (double p) noexcept
    {
        p -= std::floor (p);
        if (p < phase) newCycle();
        phase = p;
    }

    float value (int shape) const noexcept
    {
        const float p = (float) phase;
        switch (shape)
        {
            case Sine:       return std::sin (2.0f * pi * p);
            case Triangle:   return 1.0f - 4.0f * std::abs (p - 0.5f);
            case SawUp:      return 2.0f * p - 1.0f;
            case SawDown:    return 1.0f - 2.0f * p;
            case Square:     return p < 0.5f ? 1.0f : -1.0f;
            case SampleHold: return b;
            case SmoothRandom:
            {
                const float s = p * p * (3.0f - 2.0f * p);
                return a + (b - a) * s;
            }
            default: return 0.0f;
        }
    }

private:
    void newCycle() noexcept
    {
        a = b;
        b = rng.bipolar();
    }

    double phase = 0.0;
    float a = 0.0f, b = 0.0f;
    Rng rng;
};

// Envelope modulator (0..1): input follower, or attack/release generator triggered by the input crossing a threshold.
class EnvMod
{
public:
    enum Mode
    {
        Follower = 0,
        Generator,
        numModes
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        reset();
    }

    void reset() noexcept
    {
        level = det = gen = 0.0f;
        stage = Idle;
        armed = true;
    }

    // peak: max |input| over the n samples of this control step
    float process (float peak, int n, int mode, float thresholdDb, float attackMs, float releaseMs) noexcept
    {
        const float dt = (float) n / fs;
        const float thr = std::min (thresholdDb, -1.0f);
        if (mode == Follower)
        {
            stage = Idle;
            gen = 0.0f;
            const float a = 1.0f - std::exp (-dt / (attackMs * 0.001f));
            const float r = 1.0f - std::exp (-dt / (releaseMs * 0.001f));
            level += (peak > level ? a : r) * (peak - level);
            if (level < 1.0e-9f) level = 0.0f;
            return clampf ((gainToDb (level) - thr) / -thr, 0.0f, 1.0f);
        }

        const float a = 1.0f - std::exp (-dt / 0.0005f);
        const float r = 1.0f - std::exp (-dt / 0.04f);
        det += (peak > det ? a : r) * (peak - det);
        if (det < 1.0e-9f) det = 0.0f;
        const float db = gainToDb (det);
        if (armed && db > thr)
        {
            stage = Attack;
            armed = false;
        }
        else if (! armed && db < thr - 6.0f)
        {
            armed = true;
        }

        if (stage == Attack)
        {
            gen += dt / (attackMs * 0.001f);
            if (gen >= 1.0f)
            {
                gen = 1.0f;
                stage = Release;
            }
        }
        else if (stage == Release)
        {
            gen *= std::exp (-6.9077553f * dt / (releaseMs * 0.001f));
            if (gen < 1.0e-4f)
            {
                gen = 0.0f;
                stage = Idle;
            }
        }
        return gen;
    }

private:
    enum Stage
    {
        Idle,
        Attack,
        Release
    };

    float fs = 44100.0f, level = 0.0f, det = 0.0f, gen = 0.0f;
    Stage stage = Idle;
    bool armed = true;
};
} // namespace hs
