#pragma once
#include "MicroEngine.h"   // TriggerEvent, TptSvf, BlockTime
#include "SampleLibrary.h"
#include "LimeFX.h"

// =================================================================================================
//  Section B : groovebox modelled on Limelite 1.3 (Reaktor, NI 2004)
//   Voices    : Kick, Snare, Hat (closed/open), Tie (tonal), Pod (perc/fx), Pulp (pitch-tracking osc + comb)
//   Sequencer : patterns A-D, main grid Kick/Snare/C-Hat/O-Hat (off/on/accent), value lanes Tie/Pod/Mod,
//               own lengths (up to 64) + stretch, start step, swing, half tempo, pattern chain (8 slots),
//               Remix engine (127 remix patterns, rate, shift, NOW)
//   FX        : Filter (env) > EQ/Sat > Magic > Delay > xFx > FX Mix > Dirt > Norm > Gain
// =================================================================================================
class LimeEngine
{
public:
    static constexpr int NP = params::numLimePatterns;   // A-D
    static constexpr int NS = params::numLimeSteps;      // 64
    enum TrigLane { laneKick = 0, laneSnare, laneCHat, laneOHat, numTrigLanes };
    enum ValueLane { laneTie = 0, lanePod, laneMod, numValueLanes };
    // trigger targets (MIDI 48.. / audition)
    enum Target { tKick = 0, tSnare, tCHat, tOHat, tTie, tPod, tPulp, numTargets };

    struct Pattern
    {
        std::array<std::array<std::atomic<uint8_t>, NS>, numTrigLanes> trig;   // 0 off, 1 on, 2 accent
        std::array<std::array<std::atomic<float>, NS>, numValueLanes> value;   // Tie/Pod: 0 = off; Mod: 0..1
        Pattern() { clear(); }
        void clear()
        {
            for (auto& l : trig) for (auto& s : l) s.store (0);
            for (auto& l : value) for (auto& s : l) s.store (0.0f);
        }
        void copyFrom (const Pattern& o)
        {
            for (size_t l = 0; l < trig.size(); ++l) for (size_t s = 0; s < (size_t) NS; ++s) trig[l][s].store (o.trig[l][s].load());
            for (size_t l = 0; l < value.size(); ++l) for (size_t s = 0; s < (size_t) NS; ++s) value[l][s].store (o.value[l][s].load());
        }
    };

    explicit LimeEngine (SampleLibrary& lib);
    void bind (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate, int blockSize);
    void process (float* L, float* R, int numSamples, const BlockTime& bt, const std::vector<TriggerEvent>& midiEvents);

    Pattern& pattern (int i) { return patterns[(size_t) juce::jlimit (0, NP - 1, i)]; }
    int  playingPattern() const   { return currentPattern.load(); }
    int  chainSlot() const        { return chainIndex.load(); }
    int  laneStep (int lane) const { return laneSteps[(size_t) lane].load(); }   // 0 main, 1 tie, 2 pod, 3 mod
    bool remixActive() const      { return remixing.load(); }
    int  hitCount (int target) const { return hits[(size_t) target].load(); }
    float modValueNow() const     { return modNow.load(); }
    void audition (int target, float vel = 1.0f);
    void remixNow()               { remixNowFlag.store (true); }

    // xFx XY motion recorder (REC records pad movements in a tempo-synced loop of LEN, PLAY loops them)
    void xyTouch (float x, float y, bool down);          // from the UI pad (0..1)
    bool  xyPlayingBack() const { return xyPlayback.load(); }
    bool  xyRecording() const   { return xyRecNow.load(); }
    bool  xyHasMotion() const   { return xyHas.load(); }
    float xyCurrentX() const    { return xyCurX.load(); }
    float xyCurrentY() const    { return xyCurY.load(); }
    float xyLoopPos() const     { return xyPos.load(); }
    void  xyClear()             { xyClearReq.store (true); }

    juce::ValueTree saveState() const;
    void loadState (const juce::ValueTree& vt);
    void loadDefaultPatterns();

private:
    struct Voice
    {
        SamplePtr smp;
        bool active = false, fading = false;
        double pos = 0.0, t = 0.0, fmPhase = 0.0, rate = 1.0, rateTarget = 1.0;
        float gain = 1.0f, fade = 1.0f, prevShape[2] {};
        float holdS = 0, tau = 0.1f, attS = 0;
        bool fullLen = false;
        float fmAmt = 0, zap = 0, cut = 20000;
        TptSvf flt[2], hp[2];
    };

    void readParams();
    void trigger (int target, float vel, float laneValue);
    void renderSampleVoice (int voice, Voice& v, float* dl, float* dr, int n);
    void renderPulp (const float* trackSrc, float* dl, float* dr, int n);
    int  remap (int pos, int len) const;
    std::atomic<float>* p (const juce::String& id) const;

    SampleLibrary& library;
    std::array<std::array<std::atomic<float>*, 40>, params::numLimeVoices> vk {};   // per-voice parameter pointers
    juce::AudioProcessorValueTreeState* state = nullptr;
    std::array<Pattern, NP> patterns;
    std::array<std::array<int8_t, 16>, 128> remixTable {};

    // voices: 0 kick, 1 snare, 2 hat, 3 tie, 4 pod (2 voices each for declick)
    std::array<std::array<Voice, 2>, 5> voices;
    std::array<int, 5> nextVoice {};
    double tiePrevRate = 1.0;

    // Pulp
    struct Pulp
    {
        bool active = false; double t = 0, phase = 0; float vel = 1;
        float freq = 110, freqTarget = 110, tracked = 110;
        float lastIn = 0; int sinceCross = 0; float hpS = 0, lpS = 0, hpPrev = 0;
        DelayLine comb; TptSvf combLp;
        uint32_t rng = 0x1357;
    } pulpV;

    // FX
    EnvFilter envFilter; EqSat eq; Magic magic; TapeDelay delay; XFx xfx; GentleSat dirtSat;
    static constexpr int xyN = 1024;
    std::array<float, xyN> xyBufX {}, xyBufY {};
    std::atomic<bool> xyHas { false }, xyTouching { false }, xyPlayback { false }, xyRecNow { false }, xyClearReq { false };
    std::atomic<int> xyTouchCount { 0 };
    std::atomic<float> xyTouchX { 0.5f }, xyTouchY { 0.5f }, xyCurX { 0.5f }, xyCurY { 0.5f }, xyPos { 0.0f };
    int xyLastTouchCount = 0, xyLastIdx = -1;
    float xySmX = 0.5f, xySmY = 0.5f;
    double xyFreePpq = 0.0;
    std::vector<float> dryL, dryR, tieL, tieR, sendL, sendR, dlyL, dlyR, xL, xR, srcMono;
    float normEnv = 0.0f, modSmooth = 0.0f;

    // sequencer state
    std::atomic<int> currentPattern { 0 }, chainIndex { 0 };
    std::array<std::atomic<int>, 4> laneSteps {};
    std::array<std::atomic<int>, numTargets> hits {};
    std::array<std::atomic<int>, numTargets> auditionQ {};
    std::atomic<bool> remixing { false }, remixNowFlag { false };
    std::atomic<float> modNow { 0.0f };
    float modHeld = 0.0f, remixPos = 0.0f;
    long long lastLoop = -1;
    bool remixThisLoop = false, nowPending = false;
    double sr = 44100.0;
};
