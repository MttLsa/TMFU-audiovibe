// Offline test of the AutoNormalizer engine: simulates a plugin that changes preset.
#include "../Source/NormalizerEngine.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

using namespace an;

static constexpr double kSr = 48000.0;
static constexpr double kPi = 3.14159265358979323846;
static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (! ok)
        ++failures;
}

struct Sim
{
    NormalizerEngine eng;
    double t = 0;
    float outPeakLastSec = 0;
    float outPeakEver = 0; // must never exceed -1 dBFS

    explicit Sim (Settings s = {})
    {
        eng.prepare (kSr, 2);
        eng.setSettings (s);
    }

    // gen(t) returns the mono sample of the "previous plugin" output.
    void run (double seconds, const std::function<float (double)>& gen, float measureFromSec = 1.0e9f)
    {
        const int block = 512;
        std::vector<float> l (block), r (block);
        float* ch[2] = { l.data(), r.data() };
        long total = (long) (seconds * kSr);
        outPeakLastSec = 0;
        for (long done = 0; done < total; done += block)
        {
            const int n = (int) std::min<long> (block, total - done);
            for (int i = 0; i < n; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = gen (t);
                t += 1.0 / kSr;
            }
            eng.process (ch, 2, n);
            for (int i = 0; i < n; ++i)
                outPeakEver = std::max ({ outPeakEver, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
            if ((double) done / kSr >= seconds - measureFromSec)
                for (int i = 0; i < n; ++i)
                    outPeakLastSec = std::max (outPeakLastSec, std::abs (l[(size_t) i]));
        }
    }
};

static float saw (double t, double f)
{
    double ph = t * f;
    return (float) (2.0 * (ph - std::floor (ph)) - 1.0);
}

int main()
{
    std::mt19937 rng (1);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    const float ceiling = dbToLin (NormalizerEngine::kCeilingDb) * 1.000001f;
    float worstPeak = 0;

    // 1) first insertion: waits for signal, measures loudness, locks gain = target - LUFS
    {
        Sim s;
        s.run (1.0, [] (double) { return 0.0f; });
        check (s.eng.getState() == State::WaitingForSignal, "silence -> still waiting for signal");
        s.run (4.0, [] (double t) { return 0.1f * (float) std::sin (2 * kPi * 1000 * t); }, 1.0f);
        std::printf ("      measured=%.2f LUFS gain=%.2f dB out peak=%.2f dBFS\n", s.eng.getLastMeasuredLufs(),
                     s.eng.getGainDb(), linToDb (s.outPeakLastSec));
        check (s.eng.getState() == State::Locked, "locked after measurement");
        check (std::abs (s.eng.getLastMeasuredLufs() + 20.0f) < 0.3f, "1 kHz stereo sine, 0.1 peak = -20 LUFS (BS.1770)");
        check (std::abs (s.eng.getGainDb() - 6.0f) < 0.3f, "gain = -14 LUFS target - (-20 LUFS) = +6 dB");
        check (std::abs (linToDb (s.outPeakLastSec) - (-20.0f + 6.0f)) < 0.3f, "output = input + gain (no limiting needed)");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 2) preset change: timbre + level change -> auto detection -> re-normalize
    {
        Sim s;
        auto presetA = [] (double t) { return 0.05f * saw (t, 110.0); };                       // quiet bright saw
        auto presetB = [] (double t) { return 0.9f * (float) std::sin (2 * kPi * 80 * t); };  // loud sub sine
        s.run (5.0, presetA, 1.0f);
        const int countA = s.eng.getNormalizeCount();
        const float gainA = s.eng.getGainDb();
        std::printf ("      preset A: %.2f LUFS gain=%.2f dB\n", s.eng.getLastMeasuredLufs(), gainA);
        s.run (6.0, presetB, 1.0f);
        std::printf ("      preset B: %.2f LUFS gain=%.2f dB trigger=%d\n", s.eng.getLastMeasuredLufs(), s.eng.getGainDb(),
                     (int) s.eng.getLastTrigger());
        check (s.eng.getNormalizeCount() == countA + 1, "auto-detected preset change exactly once");
        check (s.eng.getLastTrigger() == TriggerSource::Auto, "trigger source = Auto");
        check (std::abs (s.eng.getGainDb() - (-14.0f - s.eng.getLastMeasuredLufs())) < 0.01f, "new preset brought to -14 LUFS");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 3) same preset, different notes and dynamics -> must NOT re-trigger
    {
        Sim s;
        s.run (4.0, [] (double t) { return 0.3f * saw (t, 110.0); });
        const int count = s.eng.getNormalizeCount();
        float maxScore = 0;
        const double notes[] = { 110, 147, 165, 131, 196, 98, 220, 123 };
        for (int k = 0; k < 16; ++k)
        {
            const double f = notes[k % 8];
            const float amp = 0.3f * dbToLin ((float) ((k % 4) - 2) * 2.0f); // +-4 dB dynamics
            s.run (0.5, [=] (double t) { return amp * saw (t, f); });
            maxScore = std::max (maxScore, s.eng.getChangeScore());
        }
        std::printf ("      max change score while playing = %.2f\n", maxScore);
        check (s.eng.getNormalizeCount() == count, "no false trigger while playing the same preset");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 4) silence gaps don't trigger
    {
        Sim s;
        s.run (4.0, [] (double t) { return 0.3f * saw (t, 110.0); });
        const int count = s.eng.getNormalizeCount();
        for (int k = 0; k < 5; ++k)
        {
            s.run (1.0, [] (double) { return 0.0f; });
            s.run (1.0, [] (double t) { return 0.3f * saw (t, 110.0); });
        }
        check (s.eng.getNormalizeCount() == count, "silence gaps don't trigger");
    }

    // 5) MIDI trigger, preset changed during silence, custom target
    {
        Settings st;
        st.targetLufs = -18.0f;
        st.autoDetect = false;
        Sim s (st);
        s.run (4.0, [&] (double) { return 0.05f * nd (rng); });
        s.run (1.0, [] (double) { return 0.0f; });
        s.eng.requestNormalize (TriggerSource::Midi);
        s.run (0.5, [] (double) { return 0.0f; });
        check (s.eng.getState() == State::WaitingForSignal, "MIDI trigger -> waiting for signal");
        s.run (4.0, [] (double t) { return 0.5f * (float) std::sin (2 * kPi * 1000 * t); }); // ~ -6 LUFS
        std::printf ("      measured=%.2f LUFS gain=%.2f dB (expected about -12)\n", s.eng.getLastMeasuredLufs(), s.eng.getGainDb());
        check (std::abs (s.eng.getGainDb() + 12.0f) < 0.3f, "-18 LUFS target reached");
        check (s.eng.getLastTrigger() == TriggerSource::Midi, "trigger source = MIDI");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 6) CEILING: loud target on spiky material -> limiter keeps every sample <= -1 dBFS
    {
        Settings st;
        st.targetLufs = -6.0f;
        Sim s (st);
        auto spiky = [&] (double t)
        {
            const double ph = std::fmod (t * 4.0, 1.0); // 4 hits per second, sharp attack
            return (float) (0.3 * std::exp (-ph * 30.0) * std::sin (2 * kPi * 180 * t)) + 0.002f * nd (rng);
        };
        s.run (6.0, spiky);
        std::printf ("      gain=%.2f dB  limiter GR=%.2f dB  max out=%.3f dBFS\n", s.eng.getGainDb(),
                     s.eng.getGainReductionDb(), linToDb (s.outPeakEver));
        check (s.outPeakEver <= ceiling, "spiky material at -6 LUFS never exceeds -1 dBFS");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 7) CEILING: lock on soft notes, then play 30 dB louder (same preset) -> still <= -1 dBFS
    {
        Settings st;
        st.autoDetect = false;
        Sim s (st);
        s.run (4.0, [] (double t) { return 0.02f * saw (t, 110.0); });
        s.run (3.0, [] (double t) { return 0.63f * saw (t, 110.0); }, 3.0f);
        std::printf ("      gain=%.2f dB  max out=%.3f dBFS\n", s.eng.getGainDb(), linToDb (s.outPeakEver));
        check (s.outPeakEver <= ceiling, "much louder notes after lock never exceed -1 dBFS");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 8) CEILING: full-scale white noise + full-scale square, max boost
    {
        Settings st;
        st.targetLufs = -6.0f;
        st.maxBoostDb = 48.0f;
        Sim s (st);
        s.run (3.0, [&] (double) { return std::clamp (0.4f * nd (rng), -1.0f, 1.0f); });
        s.run (2.0, [] (double t) { return std::sin (2 * kPi * 50 * t) > 0 ? 1.0f : -1.0f; });
        check (s.outPeakEver <= ceiling, "noise / full-scale square never exceed -1 dBFS");
        worstPeak = std::max (worstPeak, s.outPeakEver);
    }

    // 9) safety while re-measuring: a much louder new preset doesn't slam the limiter
    {
        Sim s;
        s.run (4.0, [] (double t) { return 0.01f * saw (t, 110.0); }); // gain clamped to +24
        s.eng.requestNormalize (TriggerSource::Manual);
        s.run (1.0, [] (double t) { return 0.9f * saw (t, 110.0); }, 0.8f);
        std::printf ("      during re-measure applied gain=%.2f dB, limiter GR=%.2f dB\n", s.eng.getAppliedGainDb(),
                     s.eng.getGainReductionDb());
        check (s.eng.getGainReductionDb() > -0.5f, "safety gain keeps the limiter idle while measuring");
        check (s.outPeakEver <= ceiling, "no output above -1 dBFS while measuring");
    }

    // 10) freeze ignores triggers; snapshot restore keeps gain; latency reported
    {
        Settings st;
        Sim s (st);
        s.run (4.0, [] (double t) { return 0.5f * saw (t, 110.0); });
        auto snap = s.eng.getSnapshot();
        st.frozen = true;
        s.eng.setSettings (st);
        s.eng.requestNormalize (TriggerSource::Manual);
        s.run (0.5, [] (double t) { return 0.5f * saw (t, 110.0); });
        check (s.eng.getState() == State::Locked, "frozen: manual trigger ignored");

        Sim s2;
        s2.eng.restoreSnapshot (snap);
        s2.run (0.5, [] (double t) { return 0.5f * saw (t, 110.0); }, 0.25f);
        check (s2.eng.getState() == State::Locked && std::abs (s2.eng.getGainDb() - snap.gainDb) < 0.001f,
               "restored project keeps locked gain");
        std::printf ("      latency = %d samples\n", s.eng.getLatencySamples());
        check (s.eng.getLatencySamples() == 71, "1.5 ms look-ahead latency at 48 kHz");
    }

    std::printf ("      worst output peak over all tests: %.3f dBFS\n", linToDb (worstPeak));
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
