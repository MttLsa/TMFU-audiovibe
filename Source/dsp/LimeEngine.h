#pragma once
#include "MicroEngine.h"   // TriggerEvent, TptSvf
#include "SampleBank.h"

// Section B : five-sampler groovebox modelled on the Limelite architecture
//   Sequencer (Kick / Snare / Hat main grid + Tie / Pod / Mod lanes with own length & stretch)
//   Samplers  : Kick, Snare, Hat (closed+open with Link), Tie (osc FM), Pod (Zap env FM)
//   FX        : Delay (synced, LFO DM), Filter (LFO FM), Dirt, Norm, Gain, Pitch
class LimeEngine
{
public:
    static constexpr int NP = params::numLimePatterns;
    static constexpr int NS = params::numSteps;

    enum Lane { laneKick = 0, laneSnare, laneHat, laneTie, lanePod, numTrigLanes };
    enum StepFlags : uint8_t { trig = 1, accent = 2, open = 4 };      // 'open' only used by the Hat lane
    // user-sample slots (Hat has two: closed & open)
    enum Slot { slotKick = 0, slotSnare, slotHatClosed, slotHatOpen, slotTie, slotPod, numSlots };

    struct Pattern
    {
        std::array<std::array<std::atomic<uint8_t>, NS>, numTrigLanes> steps;
        std::array<std::atomic<float>, NS> mod;
        Pattern() { clear(); }
        void clear() { for (auto& l : steps) for (auto& s : l) s.store (0); for (auto& m : mod) m.store (0.5f); }
    };

    LimeEngine();
    void bind (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate);
    // midi target: 0 kick, 1 snare, 2 hat closed, 3 hat open, 4 tie, 5 pod
    void process (float* L, float* R, int numSamples, const BlockTime& bt, const std::vector<TriggerEvent>& midiEvents);

    Pattern& pattern (int i) { return patterns[(size_t) juce::jlimit (0, NP - 1, i)]; }
    int playingPattern() const       { return currentPattern.load(); }
    int laneStep (int lane) const    { return laneSteps[(size_t) lane].load(); }   // 0 main, 1 tie, 2 pod, 3 mod
    int hitCount (int slot) const    { return hits[(size_t) slot].load(); }
    float modValueNow() const        { return modNow.load(); }
    void audition (int slot, float vel = 1.0f);

    // Sample handling
    SamplePtr sampleFor (int slot) const;                 // what is currently playing for a slot
    void setUserSample (int slot, SamplePtr s);
    SamplePtr userSample (int slot) const { return std::atomic_load (&user[(size_t) slot]); }
    juce::String userSamplePath (int slot) const { return userPaths[(size_t) slot]; }
    void setUserSamplePath (int slot, const juce::String& p) { userPaths[(size_t) slot] = p; }

    void loadDefaultPatterns();
    juce::ValueTree saveState() const;
    void loadState (const juce::ValueTree& vt);

private:
    struct PartParams
    {
        float tune = 0, dcy = 50, dcyOpen = 50, cut = 20000, res = 0, fmFreq = 100, fmAmt = 0, zap = 0, zapDcy = 30;
        float lvl = 0, pan = 0, send = 0;
        int fmode = 0; bool lowcut = false, link = true, mute = false;
        int smp = 0, smpOpen = 0;
    };

    struct Voice
    {
        SamplePtr smp;
        bool active = false;
        double pos = 0.0, t = 0.0, fmPhase = 0.0;
        float gain = 1.0f, fade = 1.0f; bool fading = false;
        int slot = 0;
        // per-trigger values (sample & hold of mod lane)
        float tuneSt = 0, decayPct = 50, cutHz = 20000, fmAmt = 0, zap = 0;
        TptSvf flt, hp;
    };

    void readParams();
    void trigger (int slot, float vel);
    void renderVoice (Voice& v, float* dL, float* dR, float* sL, float* sR, int n);

    std::atomic<float>* raw (const juce::String& id);
    juce::AudioProcessorValueTreeState* state = nullptr;
    struct PartRaw { std::atomic<float>* p[20] {}; };
    std::array<PartRaw, params::numLimeParts> pr;
    std::array<PartParams, params::numLimeParts> pp;

    std::atomic<float>* pPattern {}, *pLen {}, *pTieLen {}, *pPodLen {}, *pModLen {}, *pTieStr {}, *pPodStr {}, *pModStr {},
                      *pShuffle {}, *pAccent {}, *pGain {}, *pPitch {}, *pDirt {}, *pNorm {}, *pLfoRate {}, *pLfoFM {}, *pLfoDM {},
                      *pDlyTime {}, *pDlyFb {}, *pDlyDamp {}, *pDlyLvl {}, *pFltCut {}, *pFltRes {}, *pModDest {}, *pModAmt {};

    std::array<Pattern, NP> patterns;
    std::array<std::shared_ptr<const Sample>, numSlots> user;
    std::array<juce::String, numSlots> userPaths;

    static constexpr int voicesPerSlot = 2;
    std::array<std::array<Voice, voicesPerSlot>, numSlots> voices;
    std::array<int, numSlots> nextVoice {};

    std::atomic<int> currentPattern { 0 };
    std::array<std::atomic<int>, 4> laneSteps {};
    std::array<std::atomic<int>, numSlots> hits {};
    std::array<std::atomic<int>, numSlots> auditionQ {};
    std::atomic<float> modNow { 0.5f };
    float modHeld = 0.5f;     // current mod lane value (bipolar mapped later)
    float modSmooth = 0.0f;

    double sr = 44100.0;
    // FX state
    std::vector<float> dlyL, dlyR;
    int dlyW = 0;
    float dlyTimeSm = 0.0f, dampL = 0, dampR = 0;
    double lfoPhase = 0.0;
    TptSvf fltL, fltR;
    float normEnv = 0.0f;
    std::vector<float> busDL, busDR, busSL, busSR;
    double bpm = 120.0;
};
