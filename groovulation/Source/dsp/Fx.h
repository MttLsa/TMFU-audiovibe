#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <cmath>
#include <atomic>

namespace fx
{
    constexpr double kPi = 3.14159265358979323846;
    inline float dbToGain (float db) { return db <= -59.9f ? 0.0f : std::pow (10.0f, db * 0.05f); }

    // ---------------------------------------------------------------- TPT state-variable filter
    struct Svf
    {
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        void set (double sr, double fc, double q)
        {
            fc = juce::jlimit (10.0, sr * 0.49, fc);
            g = (float) std::tan (kPi * fc / sr); k = (float) (1.0 / juce::jmax (0.05, q));
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        // mode 0 LP, 1 BP, 2 HP
        inline float process (float x, int mode)
        {
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            return mode == 0 ? v2 : (mode == 1 ? v1 * k : x - k * v1 - v2);
        }
    };

    // ---------------------------------------------------------------- DIRT: gentle saturation
    // Soft-knee curve (linear on quiet signals, rounds only the peaks), light even harmonics,
    // parallel blend and level compensation -> "gentle saturation" character.
    class GentleSat
    {
    public:
        void prepare (double sr) { fs = sr; driveSm = 0; for (auto* p : { dcX, dcY }) p[0] = p[1] = 0; }
        void process (float* L, float* R, int n, float amount);
    private:
        double fs = 44100.0;
        float driveSm = 0.0f, dcX[2] {}, dcY[2] {};
    };

    // ---------------------------------------------------------------- tempo-synced stereo delay
    class SyncDelay
    {
    public:
        void prepare (double sr, int maxSamples);
        void reset();
        void process (float* L, float* R, int n, double delaySamples, float feedback, float mix, bool pingPong);
    private:
        std::vector<float> bl, br;
        int w = 0, size = 1;
        double fs = 44100.0, dSm = -1.0;
        float lpL = 0, lpR = 0;
    };

    // ---------------------------------------------------------------- algorithmic reverb
    // 8-line feedback delay network (Householder mixing), modulated lines, input diffusers,
    // frequency dependent decay (damping) and predelay: smooth, dense, clean tail.
    class FdnReverb
    {
    public:
        void prepare (double sr);
        void reset();
        void process (float* L, float* R, int n, float size, float decaySec, float damping, float predelayMs, float mix);
    private:
        static constexpr int N = 8;
        double fs = 44100.0;
        std::array<std::vector<float>, N> lines;
        std::array<int, N> pos {}, len {};
        std::array<float, N> lp {};
        std::array<std::vector<float>, 4> ap; std::array<int, 4> apPos {}, apLen {};
        std::vector<float> pre; int prePos = 0;
        double lfo = 0.0;
        float sizeSm = -1.0f;
        bool active = false; int silentFor = 0;
    };

    // ---------------------------------------------------------------- Beat Repeat
    struct BeatRepeatParams
    {
        double intervalBeats = 4, offsetBeats = 2, gridBeats = 0.25, gateBeats = 1;
        float variation = 0, chance = 1, decay = 0, pitch = 0, pitchDecay = 0, volume = 1;
        bool noTriplets = false;
        int mode = 1;   // 0 Mix, 1 Insert, 2 Gate
    };
    class BeatRepeat
    {
    public:
        void prepare (double sr);
        void reset();
        // ppq of sample 0, ppq per sample, playing flag
        void process (float* L, float* R, int n, double ppq0, double ppqPerSample, bool playing, const BeatRepeatParams& p);
        bool isRepeating() const { return repeating; }
    private:
        double fs = 44100.0;
        std::vector<float> bufL, bufR; int w = 0, size = 1;
        bool repeating = false;
        double gateEndPpq = 0, chunkStartPpq = 0;
        int captureStart = 0, chunkLen = 1, repIndex = 0;
        double readPos = 0, rate = 1;
        long long lastInterval = -1;
        juce::Random rnd;
        float fadeGain = 0;
    };

    // ---------------------------------------------------------------- Tape machine / turntable
    // A varispeed "deck": incoming audio is recorded into a ring buffer; when a hand acts on the reel /
    // record, the read head moves at a variable speed (stop, start, wobble, pull, rewind, scratch ...).
    // When the hands leave the deck, it crossfades back to the live signal.
    class Deck
    {
    public:
        enum Action { none = 0, stop, start, wobble, pull, rewind, scratch, brake, spinback, random };
        void prepare (double sr);
        void reset();
        void process (float* L, float* R, int n, int deckType /*1 tape, 2 vinyl*/, float controller /*-1..1*/, float noise, double bpm);

        // UI (message thread) -> audio
        std::atomic<int> pendingAction { none };
        std::atomic<bool> handHeld { false };
        std::atomic<float> handSpeed { 1.0f };      // target speed while held (can be negative)
        // audio -> UI
        std::atomic<float> currentSpeed { 1.0f };
        std::atomic<int> currentAction { none };
        std::atomic<bool> engagedFlag { false };

    private:
        double fs = 44100.0;
        std::vector<float> bufL, bufR; int w = 0, size = 1;
        double r = 0, speed = 1.0;
        bool engaged = false;
        int action = none; double actionT = 0, actionLen = 0; float actionParam = 0;
        float xfade = 0.0f;   // 0 = live, 1 = deck
        double wowPh = 0, flutPh = 0;
        juce::Random rnd;
        float crackleLp = 0, hissLp = 0, rumblePh = 0;
        int lastType = 0;
    };
}
