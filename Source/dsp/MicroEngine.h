#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Transport.h"
#include "../Params.h"

struct TriggerEvent
{
    int   offset = 0;   // sample offset in the block
    int   target = 0;   // channel / part
    float velocity = 1.0f;
};

// Topology-preserving-transform state-variable filter (Zavalishin / Cytomic).
struct TptSvf
{
    float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    void set (double fc, double sr, double q)
    {
        fc = juce::jlimit (10.0, sr * 0.49, fc);
        g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
        k = (float) (1.0 / juce::jmax (0.05, q));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // mode: 0 LP, 1 BP, 2 HP
    inline float process (float v0, int mode)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (mode == 0) return v2;
        if (mode == 1) return k * v1;     // constant-peak (normalised) band-pass
        return v0 - k * v1 - v2;
    }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void setPeak (double fc, double sr, double gainDb, double q)
    {
        fc = juce::jlimit (10.0, sr * 0.45, fc);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * juce::MathConstants<double>::pi * fc / sr;
        const double alpha = std::sin (w) / (2.0 * q);
        const double c = std::cos (w);
        const double a0 = 1.0 + alpha / A;
        b0 = (float) ((1.0 + alpha * A) / a0);
        b1 = (float) ((-2.0 * c) / a0);
        b2 = (float) ((1.0 - alpha * A) / a0);
        a1 = (float) ((-2.0 * c) / a0);
        a2 = (float) ((1.0 - alpha / A) / a0);
    }
    inline float process (float x, int ch)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
};

// Snapshot of one drum patch (read once per block)
struct MicroPatch
{
    int wave = 0;  float freq = 100, atkMs = 0, dcyMs = 300;
    int pmMode = 0; float pmAmt = 0, pmRate = 0;
    int nMode = 0; float nFreq = 1000, nQ = 1; bool stereo = false; int nEnv = 0; float nAtkMs = 0, nDcyMs = 100;
    float mix = 0, dist = 0, eqFreq = 1000, eqGain = 0, levelDb = 0, pan = 0;
    bool choke = false, mute = false;
    float velOsc = 0, velNoise = 0, velMod = 0;
};

// One Section-A drum channel: oscillator + pitch modulator, filtered noise generator,
// mix, distortion, peaking EQ, level/pan. Monophonic (retriggers).
class MicroVoice
{
public:
    void prepare (double sampleRate);
    void trigger (const MicroPatch& p, float velocity);
    void choke() { if (active) chokeGain = 0.999f, choking = true; }
    void updateFilters (const MicroPatch& p);
    void render (const MicroPatch& p, float* L, float* R, int numSamples);
    bool isActive() const { return active; }

private:
    double sr = 44100.0;
    bool active = false, choking = false;
    float chokeGain = 1.0f;
    double t = 0.0;           // seconds since trigger
    double phase = 0.0;
    float  oscVelGain = 1, noiseVelGain = 1, modVelScale = 1;
    uint32_t rngL = 0x1234567, rngR = 0x7654321;
    float randLp = 0, randTarget = 0; int randCount = 0;
    TptSvf noiseFilt[2];
    Biquad eq;

    inline float white (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s * (2.0f / 4294967295.0f) - 1.0f; }
};

class MicroEngine
{
public:
    static constexpr int NC = params::numMicroChannels;
    static constexpr int NP = params::numMicroPatterns;
    static constexpr int NS = params::numSteps;

    enum StepFlags : uint8_t { trig = 1, accent = 2, fill = 4 };

    struct Pattern
    {
        std::atomic<int> length { NS };
        std::array<std::array<std::atomic<uint8_t>, NS>, NC> steps;
        Pattern() { clear(); }
        void clear() { for (auto& c : steps) for (auto& s : c) s.store (0); }
    };

    MicroEngine();
    void bind (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate);

    // Renders (adds) Section A into L/R. midiEvents are external triggers (target = channel).
    void process (float* L, float* R, int numSamples, const BlockTime& bt, const std::vector<TriggerEvent>& midiEvents);

    Pattern& pattern (int i) { return patterns[(size_t) juce::jlimit (0, NP - 1, i)]; }
    int  playingPattern() const { return currentPattern.load(); }
    int  playingStep() const    { return playStep.load(); }
    int  hitCount (int ch) const { return hits[(size_t) ch].load(); }
    bool fillActiveNow() const  { return fillNow.load(); }
    void auditionChannel (int ch, float vel = 1.0f);   // UI pad (lock-free queue)

    void loadDefaultPatterns();
    juce::ValueTree saveState() const;
    void loadState (const juce::ValueTree& vt);

private:
    void readPatch (int ch, MicroPatch& p) const;
    void startVoice (int ch, float vel);

    struct ChannelParams
    {
        std::atomic<float>* v[26] {};
    };
    std::array<ChannelParams, NC> cp;
    std::atomic<float>* pPattern = nullptr, *pSwing = nullptr, *pFillRate = nullptr, *pFill = nullptr, *pLevel = nullptr;

    std::array<Pattern, NP> patterns;
    std::array<MicroVoice, NC> voices;
    std::array<MicroPatch, NC> patch;
    std::atomic<int> currentPattern { 0 }, playStep { -1 };
    std::atomic<bool> fillNow { false };
    std::array<std::atomic<int>, NC> hits {};
    std::array<std::atomic<int>, NC> audition {};   // pending UI triggers (velocity * 1000)
    double sampleRate = 44100.0;
    float levelGain = 1.0f;
};
