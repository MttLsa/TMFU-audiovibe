#include "SampleBank.h"
#include <cmath>

namespace
{
    constexpr double SR = 44100.0;
    constexpr double twoPi = 6.283185307179586;

    struct Rng
    {
        uint32_t s;
        explicit Rng (uint32_t seed) : s (seed ? seed : 1u) {}
        float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 4294967295.0 * 2.0 - 1.0); }
    };

    // Simple state-variable filter (Chamberlin, oversampled 2x) for sample synthesis.
    struct SVF
    {
        float lp = 0, bp = 0;
        float f = 0.1f, q = 1.0f;
        void set (double hz, double res) // res 0..1
        {
            f = (float) (2.0 * std::sin (juce::MathConstants<double>::pi * juce::jlimit (10.0, SR * 0.22, hz) / (SR * 2.0)));
            q = (float) (2.0 - 1.9 * juce::jlimit (0.0, 1.0, res));
        }
        // mode 0 LP, 1 BP, 2 HP
        float process (float x, int mode)
        {
            float hp = 0;
            for (int i = 0; i < 2; ++i)
            {
                lp += f * bp;
                hp = x - lp - q * bp;
                bp += f * hp;
            }
            return mode == 0 ? lp : (mode == 1 ? bp : hp);
        }
    };

    float expEnv (double t, double tau) { return (float) std::exp (-t / std::max (1.0e-4, tau)); }

    void normalise (std::vector<float>& d, float peak = 0.9f)
    {
        float m = 0;
        for (auto v : d) m = std::max (m, std::abs (v));
        if (m > 0) for (auto& v : d) v *= peak / m;
        // tiny fade-out to avoid clicks
        const size_t fade = std::min<size_t> (d.size(), 256);
        for (size_t i = 0; i < fade; ++i) d[d.size() - 1 - i] *= (float) i / (float) fade;
    }

    SamplePtr make (const char* cat, juce::String name, double seconds, std::function<float (double t, int n)> fn)
    {
        auto s = std::make_shared<Sample>();
        s->name = std::move (name);
        s->category = cat;
        s->sourceRate = SR;
        s->data.resize ((size_t) (seconds * SR));
        for (size_t n = 0; n < s->data.size(); ++n)
            s->data[n] = fn ((double) n / SR, (int) n);
        normalise (s->data);
        return s;
    }

    // ---------------------------------------------------------------- KICK
    SamplePtr kick (juce::String name, double f0, double fSweep, double sweepTau, double ampTau, double click, double drive, double len, uint32_t seed)
    {
        auto phase = std::make_shared<double> (0.0);
        auto rng = std::make_shared<Rng> (seed);
        return make ("Kick", name, len, [=] (double t, int) mutable
        {
            const double f = f0 + fSweep * std::exp (-t / sweepTau);
            *phase += twoPi * f / SR;
            double v = std::sin (*phase) * expEnv (t, ampTau);
            v += click * rng->next() * expEnv (t, 0.0015);
            v = std::tanh (v * drive) / std::tanh (drive);
            return (float) v;
        });
    }

    // ---------------------------------------------------------------- SNARE / CLAP
    SamplePtr snare (juce::String name, double tone, double toneTau, double noiseTau, double noiseHz, double noiseMix, double len, uint32_t seed, int fmode = 2)
    {
        auto phase = std::make_shared<double> (0.0);
        auto rng = std::make_shared<Rng> (seed);
        auto flt = std::make_shared<SVF>();
        flt->set (noiseHz, 0.2);
        return make ("Snare", name, len, [=] (double t, int) mutable
        {
            const double f = tone * (1.0 + 0.5 * std::exp (-t / 0.01));
            *phase += twoPi * f / SR;
            double body = (std::sin (*phase) + 0.4 * std::sin (*phase * 1.47)) * expEnv (t, toneTau);
            double nz = flt->process (rng->next(), fmode) * expEnv (t, noiseTau);
            return (float) (body * (1.0 - noiseMix) + nz * noiseMix * 1.5);
        });
    }

    SamplePtr clap (juce::String name, double hz, double spacing, int bursts, double tail, double len, uint32_t seed)
    {
        auto rng = std::make_shared<Rng> (seed);
        auto flt = std::make_shared<SVF>();
        flt->set (hz, 0.45);
        return make ("Snare", name, len, [=] (double t, int) mutable
        {
            double env = 0;
            for (int b = 0; b < bursts; ++b)
            {
                const double tb = t - b * spacing;
                if (tb >= 0 && tb < spacing * 1.2) env = std::max (env, (double) expEnv (tb, spacing * 0.35));
            }
            const double tl = t - (bursts - 1) * spacing;
            if (tl >= 0) env = std::max (env, (double) expEnv (tl, tail));
            return (float) (flt->process (rng->next(), 1) * env);
        });
    }

    // ---------------------------------------------------------------- HATS (metallic: 6 square partials + noise)
    SamplePtr hat (const char* cat, juce::String name, double tau, double hpHz, double metal, double base, double len, uint32_t seed)
    {
        static const double ratios[] { 2.0, 3.0, 4.16, 5.43, 6.79, 8.21 };
        auto ph = std::make_shared<std::array<double, 6>>();
        ph->fill (0.0);
        auto rng = std::make_shared<Rng> (seed);
        auto flt = std::make_shared<SVF>();
        auto flt2 = std::make_shared<SVF>();
        flt->set (hpHz, 0.3);
        flt2->set (hpHz * 1.4, 0.5);
        return make (cat, name, len, [=] (double t, int) mutable
        {
            double m = 0;
            for (int i = 0; i < 6; ++i)
            {
                (*ph)[(size_t) i] += base * ratios[i] / SR;
                m += ((*ph)[(size_t) i] - std::floor ((*ph)[(size_t) i])) < 0.5 ? 1.0 : -1.0;
            }
            double x = metal * m / 6.0 + (1.0 - metal) * rng->next();
            x = flt2->process (flt->process ((float) x, 2), 1) + 0.3 * flt->process ((float) x, 2);
            return (float) (x * expEnv (t, tau) * (1.0 - std::exp (-t / 0.0005)));
        });
    }

    // ---------------------------------------------------------------- PERCUSSION / TONAL
    SamplePtr tonal (const char* cat, juce::String name, std::vector<double> partials, std::vector<double> amps, double tau, double pitchDrop, double noise, double len, uint32_t seed, double wave = 0.0)
    {
        auto ph = std::make_shared<std::vector<double>> (partials.size(), 0.0);
        auto rng = std::make_shared<Rng> (seed);
        return make (cat, name, len, [=] (double t, int) mutable
        {
            double v = 0;
            const double drop = 1.0 + pitchDrop * std::exp (-t / 0.02);
            for (size_t i = 0; i < partials.size(); ++i)
            {
                (*ph)[i] += partials[i] * drop / SR;
                const double p = (*ph)[i] - std::floor ((*ph)[i]);
                const double s = std::sin (twoPi * p);
                const double saw = 2.0 * p - 1.0;
                v += amps[i] * (s * (1.0 - wave) + saw * wave);
            }
            v += noise * rng->next() * expEnv (t, 0.004);
            return (float) (v * expEnv (t, tau) * (1.0 - std::exp (-t / 0.0008)));
        });
    }

    SamplePtr stab (juce::String name, std::vector<double> notesHz, double cutoff, double tau, double len, uint32_t seed)
    {
        auto ph = std::make_shared<std::vector<double>> (notesHz.size() * 2, 0.0);
        auto flt = std::make_shared<SVF>();
        auto rng = std::make_shared<Rng> (seed);
        return make ("Tonal", name, len, [=] (double t, int) mutable
        {
            double v = 0;
            for (size_t i = 0; i < notesHz.size(); ++i)
                for (int d = 0; d < 2; ++d)
                {
                    auto& p = (*ph)[i * 2 + (size_t) d];
                    p += notesHz[i] * (d == 0 ? 1.0 : 1.006) / SR;
                    v += 2.0 * (p - std::floor (p)) - 1.0;
                }
            flt->set (cutoff * (0.3 + 2.0 * std::exp (-t / 0.08)), 0.5);
            juce::ignoreUnused (rng);
            return (float) (flt->process ((float) (v / (double) notesHz.size()), 0) * expEnv (t, tau));
        });
    }

    SamplePtr fx (juce::String name, int type, double len, uint32_t seed)
    {
        auto rng = std::make_shared<Rng> (seed);
        auto flt = std::make_shared<SVF>();
        auto phase = std::make_shared<double> (0.0);
        return make ("FX", name, len, [=] (double t, int) mutable
        {
            const double x = t / len;
            switch (type)
            {
                case 0: { // zap down
                    *phase += twoPi * (60.0 + 3000.0 * std::exp (-t / 0.03)) / SR;
                    return (float) (std::sin (*phase) * expEnv (t, 0.12));
                }
                case 1: { // noise sweep up
                    flt->set (200.0 + 9000.0 * x * x, 0.7);
                    return (float) (flt->process (rng->next(), 1) * std::sin (juce::MathConstants<double>::pi * x));
                }
                case 2: { // laser
                    *phase += twoPi * (2000.0 * std::exp (-t / 0.06) + 200.0) / SR;
                    return (float) ((std::sin (*phase) > 0 ? 0.6 : -0.6) * expEnv (t, 0.15));
                }
                case 3: { // bitcrush blip
                    *phase += twoPi * 880.0 / SR;
                    const double s = std::round (std::sin (*phase) * 4.0) / 4.0;
                    return (float) (s * expEnv (t, 0.05));
                }
                case 4: { // reverse-ish swell
                    flt->set (1500.0, 0.6);
                    return (float) (flt->process (rng->next(), 1) * x * x * (1.0 - std::pow (x, 12.0)));
                }
                default: { // metallic ring
                    *phase += twoPi * 523.0 / SR;
                    return (float) ((std::sin (*phase) * std::sin (*phase * 2.76) + 0.5 * std::sin (*phase * 5.4)) * expEnv (t, 0.4));
                }
            }
        });
    }
}

const SampleBank& SampleBank::get()
{
    static SampleBank bank;
    return bank;
}

SampleBank::SampleBank()
{
    auto& s = samples;
    // ---- Kicks
    s.push_back (kick ("Kick 808 Long",  45, 120, 0.040, 0.55, 0.20, 1.2, 1.6, 11));
    s.push_back (kick ("Kick 909 Punch", 52, 220, 0.025, 0.22, 0.60, 2.0, 0.7, 12));
    s.push_back (kick ("Kick Tight",     60, 260, 0.012, 0.10, 0.40, 1.6, 0.4, 13));
    s.push_back (kick ("Kick Boom",      40,  90, 0.080, 0.80, 0.10, 1.1, 1.8, 14));
    s.push_back (kick ("Kick Click",     70, 400, 0.008, 0.08, 1.00, 2.5, 0.3, 15));
    s.push_back (kick ("Kick Soft",      55, 100, 0.030, 0.25, 0.05, 1.0, 0.6, 16));
    s.push_back (kick ("Kick Dist",      50, 200, 0.030, 0.35, 0.40, 6.0, 0.8, 17));
    s.push_back (kick ("Kick Deep Sub",  38,  60, 0.050, 0.90, 0.05, 1.0, 1.8, 18));
    s.push_back (kick ("Kick Thump",     65, 150, 0.020, 0.15, 0.30, 3.0, 0.5, 19));
    s.push_back (kick ("Kick Tek",       48, 300, 0.018, 0.30, 0.80, 4.0, 0.7, 20));
    // ---- Snares / claps
    s.push_back (snare ("Snare Tight",   190, 0.05, 0.08, 4000, 0.55, 0.4, 21));
    s.push_back (snare ("Snare 909",     180, 0.08, 0.16, 3500, 0.65, 0.6, 22));
    s.push_back (snare ("Snare Fat",     150, 0.12, 0.20, 2500, 0.50, 0.7, 23));
    s.push_back (snare ("Snare Noise",   220, 0.03, 0.15, 6000, 0.90, 0.5, 24));
    s.push_back (snare ("Snare Lo-Fi",   160, 0.06, 0.10, 1800, 0.60, 0.4, 25, 1));
    s.push_back (snare ("Rim Shot",      450, 0.02, 0.02, 3000, 0.30, 0.2, 26, 1));
    s.push_back (clap  ("Clap 808",      1200, 0.011, 4, 0.12, 0.6, 27));
    s.push_back (clap  ("Clap Tight",    1600, 0.008, 3, 0.06, 0.4, 28));
    s.push_back (clap  ("Clap Room",     1000, 0.013, 4, 0.30, 1.0, 29));
    s.push_back (snare ("Snap",          300, 0.01, 0.05, 7000, 0.80, 0.25, 30));
    // ---- Closed hats
    s.push_back (hat ("HatC", "Hat Closed 808", 0.030, 7000, 0.8, 360, 0.25, 31));
    s.push_back (hat ("HatC", "Hat Closed 909", 0.045, 8000, 0.3, 420, 0.30, 32));
    s.push_back (hat ("HatC", "Hat Closed Tick",0.015, 9500, 0.5, 500, 0.15, 33));
    s.push_back (hat ("HatC", "Hat Closed Dark",0.050, 4500, 0.6, 300, 0.30, 34));
    s.push_back (hat ("HatC", "Shaker",         0.060, 6000, 0.0, 400, 0.30, 35));
    s.push_back (hat ("HatC", "Hat Closed Metal",0.035, 6500, 1.0, 470, 0.25, 36));
    // ---- Open hats / cymbals
    s.push_back (hat ("HatO", "Hat Open 808",   0.30, 7000, 0.8, 360, 1.0, 41));
    s.push_back (hat ("HatO", "Hat Open 909",   0.40, 8000, 0.3, 420, 1.2, 42));
    s.push_back (hat ("HatO", "Hat Open Short", 0.15, 8000, 0.5, 500, 0.6, 43));
    s.push_back (hat ("HatO", "Hat Open Dark",  0.35, 4500, 0.6, 300, 1.2, 44));
    s.push_back (hat ("HatO", "Ride",           0.80, 5000, 0.9, 520, 2.0, 45));
    s.push_back (hat ("HatO", "Crash",          1.10, 3500, 0.4, 330, 2.5, 46));
    // ---- Percussion (Tie)
    s.push_back (tonal ("Perc", "Tom Low",   { 90 },         { 1.0 },             0.25, 0.6, 0.2, 0.8, 51));
    s.push_back (tonal ("Perc", "Tom Mid",   { 140 },        { 1.0 },             0.20, 0.6, 0.2, 0.7, 52));
    s.push_back (tonal ("Perc", "Tom High",  { 210 },        { 1.0 },             0.16, 0.6, 0.2, 0.6, 53));
    s.push_back (tonal ("Perc", "Conga",     { 330, 660 },   { 1.0, 0.2 },        0.12, 0.3, 0.3, 0.5, 54));
    s.push_back (tonal ("Perc", "Cowbell",   { 540, 800 },   { 0.7, 0.6 },        0.10, 0.0, 0.0, 0.5, 55, 0.6));
    s.push_back (tonal ("Perc", "Clave",     { 2500 },       { 1.0 },             0.03, 0.0, 0.1, 0.2, 56));
    s.push_back (tonal ("Perc", "Woodblock", { 1200, 2900 }, { 1.0, 0.3 },        0.04, 0.1, 0.2, 0.2, 57));
    s.push_back (tonal ("Perc", "Bongo",     { 400, 980 },   { 1.0, 0.2 },        0.08, 0.4, 0.3, 0.4, 58));
    s.push_back (tonal ("Perc", "Blip",      { 1760 },       { 1.0 },             0.05, 0.0, 0.0, 0.25, 59));
    s.push_back (tonal ("Perc", "Agogo",     { 950, 2540 },  { 0.8, 0.4 },        0.18, 0.0, 0.0, 0.6, 60));
    // ---- Tonal (Pod)
    s.push_back (tonal ("Tonal", "Bass C",      { 65.4, 130.8 },  { 1.0, 0.4 },   0.30, 0.0, 0.0, 0.8, 61, 0.5));
    s.push_back (tonal ("Tonal", "Bass Sub",    { 55.0 },         { 1.0 },        0.40, 0.1, 0.0, 1.0, 62));
    s.push_back (tonal ("Tonal", "Pluck A",     { 220, 440, 660 },{ 1.0, 0.5, 0.25 }, 0.15, 0.0, 0.1, 0.6, 63, 0.3));
    s.push_back (stab  ("Stab Minor", { 261.6, 311.1, 392.0 }, 3000, 0.25, 0.8, 64));
    s.push_back (stab  ("Stab Major", { 261.6, 329.6, 392.0 }, 3000, 0.25, 0.8, 65));
    s.push_back (stab  ("Stab Dub",   { 130.8, 155.6, 196.0, 233.1 }, 1200, 0.45, 1.2, 66));
    s.push_back (stab  ("Organ Hit",  { 261.6, 523.2, 784.0 }, 6000, 0.20, 0.6, 67));
    // ---- FX
    s.push_back (fx ("FX Zap",        0, 0.5, 71));
    s.push_back (fx ("FX Sweep",      1, 1.0, 72));
    s.push_back (fx ("FX Laser",      2, 0.6, 73));
    s.push_back (fx ("FX Bit Blip",   3, 0.3, 74));
    s.push_back (fx ("FX Swell",      4, 1.0, 75));
    s.push_back (fx ("FX Metal Ring", 5, 1.2, 76));
}

juce::StringArray SampleBank::names() const
{
    juce::StringArray n;
    for (auto& s : samples) n.add (s->name);
    return n;
}

int SampleBank::firstOfCategory (const juce::String& cat, int offset) const
{
    for (int i = 0; i < size(); ++i)
        if (samples[(size_t) i]->category == cat)
            return juce::jlimit (0, size() - 1, i + offset);
    return 0;
}
