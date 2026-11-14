// AutoNormalizer - core DSP engine (no JUCE dependency, unit-testable).
//
// Idea: the plugin sits after another plugin. When that plugin changes preset
// (detected automatically from the signal, from a MIDI Program Change, or from
// the "Normalize now" button / automation), the engine:
//   1. arms itself and waits for incoming signal (above the gate),
//   2. measures the signal for N seconds of *active* audio,
//   3. measures its loudness (LUFS, ITU-R BS.1770 K-weighting) and computes the
//      gain that brings it to the target loudness (default -14 LUFS),
//   4. locks that gain and stops measuring. The gain then stays fixed: the
//      plugin is NOT a compressor/AGC, it never rides the level.
// A look-ahead brick-wall limiter after the gain guarantees that the output
// never exceeds -1 dBFS (sample peak). It only works on peaks above -1 dBFS.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace an
{

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline float process (float xIn) noexcept
    {
        const double x = xIn;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

enum class State : int { Locked = 0, WaitingForSignal = 1, Measuring = 2 };
enum class TriggerSource : int { None = 0, Initial, Manual, Midi, Auto };

struct Settings
{
    float targetLufs = -14.0f;      // target loudness (LUFS)
    float measureSeconds = 2.0f;    // seconds of active signal to analyse
    float gateDb = -50.0f;          // below this the input counts as silence
    bool autoDetect = true;         // detect preset changes from the signal
    float sensitivity = 0.5f;       // 0..1 auto-detect sensitivity
    float maxBoostDb = 24.0f;       // never boost more than this
    float maxCutDb = 48.0f;         // never cut more than this
    bool safetyCeiling = true;      // while waiting/measuring reduce gain so the new preset never hits the limiter
    bool frozen = false;            // ignore every trigger, keep the current gain
};

class NormalizerEngine
{
public:
    static constexpr int kNumBands = 8;
    static constexpr int kMaxChannels = 16;
    static constexpr float kCeilingDb = -1.0f;      // hard output ceiling (sample peak)
    static constexpr double kLookaheadSec = 0.0015; // limiter look-ahead

    void prepare (double sampleRate, int numChannels);
    void setSettings (const Settings& s) noexcept { settings = s; }

    // Thread-safe: can be called from any thread (GUI, MIDI handling, ...).
    void requestNormalize (TriggerSource src) noexcept;
    void requestResetGain() noexcept { resetRequested.store (true); }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Latency introduced by the limiter look-ahead (report it to the host).
    int getLatencySamples() const noexcept { return limLen - 1; }

    // ---- read-only state for the GUI (all atomics, lock-free)
    State getState() const noexcept { return (State) uiState.load(); }
    float getGainDb() const noexcept { return uiGainDb.load(); }
    float getAppliedGainDb() const noexcept { return uiAppliedGainDb.load(); }
    float getLastMeasuredLufs() const noexcept { return uiMeasuredDb.load(); }
    float getLastMeasuredPeakDb() const noexcept { return uiMeasuredPeak.load(); }
    float getGainReductionDb() const noexcept { return uiGrDb.load(); }     // limiter, <= 0
    float getOutputPeakDb() const noexcept { return uiOutDb.load(); }
    float getChangeScore() const noexcept { return uiScore.load(); }         // >= 1 means "preset change"
    float getMeasureProgress() const noexcept { return uiProgress.load(); }  // 0..1
    float getInputLevelDb() const noexcept { return uiInputDb.load(); }
    TriggerSource getLastTrigger() const noexcept { return (TriggerSource) uiLastTrigger.load(); }
    int getNormalizeCount() const noexcept { return uiCount.load(); }

    // ---- persistence (saved in the DAW project)
    struct Snapshot
    {
        bool normalizedOnce = false;
        float gainDb = 0.0f;
        bool hasReference = false;
        float refLevelDb = -100.0f;
        std::array<float, kNumBands> refShape {};
    };
    Snapshot getSnapshot() const noexcept;
    void restoreSnapshot (const Snapshot& s) noexcept; // applied at the start of the next block

private:
    void startArming (TriggerSource src) noexcept;
    void endFrame() noexcept;
    void finishMeasurement() noexcept;
    void computeShape (const double* energies, float* shapeOut) const noexcept;
    void applySnapshot (const Snapshot& s) noexcept;

    Settings settings;
    double sr = 48000.0;
    int numCh = 2;

    // analysis filters
    std::array<Biquad, kNumBands> bands {};
    std::array<Biquad, kMaxChannels> kPre {}, kRlb {};

    // per-frame accumulators (frame = 50 ms)
    int hop = 2400, frameN = 0;
    double frameSec = 0.05;
    std::array<double, kNumBands> frameBand {};
    double frameMonoSq = 0, frameSq = 0, frameKSq = 0;
    float framePeak = 0;

    // detector (EMA over frames)
    std::array<double, kNumBands> bandEma {};
    double levelEma = 0;
    double emaAlpha = 0.1;
    bool emaValid = false;
    double overTime = 0, cooldown = 0, settle = 0;

    // reference fingerprint captured when the gain was locked
    bool hasReference = false;
    std::array<float, kNumBands> refShape {};
    float refLevelDb = -100.0f;

    // measurement
    State state = State::WaitingForSignal;
    TriggerSource pendingSource = TriggerSource::Initial;
    double measActive = 0, measSq = 0, measKSq = 0, measSamples = 0, measMonoSq = 0;
    float measPeak = 0, peakSinceArm = 0;
    std::array<double, kNumBands> measBand {};

    // gain
    bool normalizedOnce = false;
    float gainDb = 0.0f;
    float gainTargetLin = 1.0f, gainSmoothed = 1.0f, lastApplied = 1.0f;
    float smoothCoef = 0.001f;
    float inputPeakHold = 0, outputPeakHold = 0, grHold = 1.0f;

    // look-ahead limiter (stereo-linked)
    float ceilingLin = 0.891f;
    int limLen = 72, limPos = 0;
    std::vector<float> limDelay;   // kMaxChannels * (limLen - 1)
    std::vector<float> limReq;     // last limLen required gains
    std::vector<double> limBox;    // last limLen held gains (box average)
    double limBoxSum = 0;
    float limHold = 1.0f, limRelease = 0.001f;
    float limiterProcess (float* const* channels, int numChannels, int i) noexcept;

    // cross-thread requests
    std::atomic<int> triggerRequest { 0 }; // TriggerSource as int, 0 = none
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> snapshotPending { false };
    Snapshot pendingSnapshot;

    // published state
    std::atomic<int> uiState { (int) State::WaitingForSignal }, uiLastTrigger { (int) TriggerSource::Initial }, uiCount { 0 };
    std::atomic<float> uiGainDb { 0 }, uiAppliedGainDb { 0 }, uiMeasuredDb { -100 }, uiMeasuredPeak { -100 }, uiGrDb { 0 }, uiOutDb { -100 }, uiScore { 0 }, uiProgress { 0 }, uiInputDb { -100 };
    std::atomic<bool> pubNormalizedOnce { false }, pubHasRef { false };
    std::atomic<float> pubRefLevel { -100 };
    std::array<std::atomic<float>, kNumBands> pubRefShape {};
};

inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (double lin) noexcept { return lin > 1.0e-10 ? (float) (20.0 * std::log10 (lin)) : -200.0f; }
inline float powToDb (double p) noexcept { return p > 1.0e-20 ? (float) (10.0 * std::log10 (p)) : -200.0f; }

} // namespace an
