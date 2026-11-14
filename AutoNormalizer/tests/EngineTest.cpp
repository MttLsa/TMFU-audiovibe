// Offline test of the AutoNormalizer engine: simulates a plugin that changes preset.
#include "../Source/NormalizerEngine.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

using namespace an;

static constexpr double kSr = 48000.0;
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

    // 1) first insertion: waits for signal, measures, locks at 0 dBFS peak
    {
        Sim s;
        s.run (1.0, [] (double) { return 0.0f; });
        check (s.eng.getState() == State::WaitingForSignal, "silence -> still waiting for signal");
        s.run (4.0, [] (double t) { return 0.25f * (float) std::sin (2 * M_PI * 220 * t); }, 1.0f);
        std::printf ("      gain=%.2f dB  out peak=%.2f dBFS\n", s.eng.getGainDb(), linToDb (s.outPeakLastSec));
        check (s.eng.getState() == State::Locked, "locked after measurement");
        check (std::abs (linToDb (s.outPeakLastSec)) < 0.1f, "output peak ~ 0 dBFS");
    }

    // 2) preset change: timbre + level change -> auto detection -> re-normalize
    {
        Sim s;
        auto presetA = [] (double t) { return 0.2f * saw (t, 110.0); };                         // bright saw, -14 dB
        auto presetB = [] (double t) { return 0.9f * (float) std::sin (2 * M_PI * 80 * t); };  // loud sub sine
        s.run (5.0, presetA, 1.0f);
        const int countA = s.eng.getNormalizeCount();
        std::printf ("      preset A: gain=%.2f dB out=%.2f dBFS\n", s.eng.getGainDb(), linToDb (s.outPeakLastSec));
        s.run (6.0, presetB, 1.0f);
        std::printf ("      preset B: gain=%.2f dB out=%.2f dBFS trigger=%d\n", s.eng.getGainDb(),
                     linToDb (s.outPeakLastSec), (int) s.eng.getLastTrigger());
        check (s.eng.getNormalizeCount() == countA + 1, "auto-detected preset change exactly once");
        check (s.eng.getLastTrigger() == TriggerSource::Auto, "trigger source = Auto");
        check (std::abs (linToDb (s.outPeakLastSec)) < 0.1f, "new preset also at 0 dBFS");
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

    // 5) manual / MIDI trigger, preset changed during silence, RMS mode
    {
        Settings st;
        st.mode = MeasureMode::RMS;
        st.targetDb = -12.0f;
        st.autoDetect = false;
        Sim s (st);
        s.run (4.0, [&] (double) { return 0.05f * nd (rng); });
        s.run (1.0, [] (double) { return 0.0f; });
        s.eng.requestNormalize (TriggerSource::Midi);
        s.run (0.5, [] (double) { return 0.0f; });
        check (s.eng.getState() == State::WaitingForSignal, "MIDI trigger -> waiting for signal");
        s.run (4.0, [] (double t) { return 0.5f * (float) std::sin (2 * M_PI * 440 * t); }); // RMS = -9 dBFS
        std::printf ("      RMS mode gain=%.2f dB (expected -3.0)\n", s.eng.getGainDb());
        check (std::abs (s.eng.getGainDb() + 3.0f) < 0.1f, "RMS target -12 dB reached");
        check (s.eng.getLastTrigger() == TriggerSource::Midi, "trigger source = MIDI");
    }

    // 6) loudness mode: 1 kHz stereo sine, 0.1 peak (-23 dB mean square per channel, +3 dB channel sum) => -20 LUFS
    {
        Settings st;
        st.mode = MeasureMode::Loudness;
        st.targetDb = -14.0f;
        Sim s (st);
        s.run (4.0, [] (double t) { return 0.1f * (float) std::sin (2 * M_PI * 1000 * t); });
        std::printf ("      measured=%.2f LUFS gain=%.2f dB\n", s.eng.getLastMeasuredDb(), s.eng.getGainDb());
        check (std::abs (s.eng.getLastMeasuredDb() + 20.0f) < 0.3f, "K-weighted loudness of -20 dBFS sine ~ -20 LUFS");
    }

    // 7) safety ceiling: louder new preset never clips during re-measurement
    {
        Sim s;
        s.run (4.0, [] (double t) { return 0.05f * saw (t, 110.0); }); // gain ~ +26 -> clamped +24
        s.eng.requestNormalize (TriggerSource::Manual);
        s.run (1.0, [] (double t) { return 0.9f * saw (t, 110.0); }, 1.0f);
        std::printf ("      during re-measure out peak=%.2f dBFS\n", linToDb (s.outPeakLastSec));
        check (s.outPeakLastSec <= 1.0001f, "no output above 0 dBFS while measuring");
    }

    // 8) freeze ignores triggers; snapshot restore keeps gain
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
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
