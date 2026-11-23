#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <vector>
#include "Transport.h"
#include "../Params.h"

// Source of parameter values for the engines (the APVTS raw values, or their LFO-modulated copies).
struct ParamSource
{
    virtual ~ParamSource() = default;
    virtual std::atomic<float>* get (const juce::String& id) = 0;
};

// 3 LFOs per section. Every parameter of a section is read by its engine through a "shadow" value:
// shadow = parameter + sum of the LFO offsets aimed at it (in normalised units, clamped to the range).
// Updated once per audio block, before the engines run. The UI keeps showing the unmodulated values.
class ModMatrix : public ParamSource
{
public:
    void init (juce::AudioProcessorValueTreeState& apvts);
    std::atomic<float>* get (const juce::String& id) override;

    void reset() { for (auto& s : lfos) for (auto& l : s) { l.phase = 0.0; l.lastCycle = -1; l.cur = l.next = 0.0f; } }
    void process (const BlockTime& bt, double sampleRate, int numSamples);

    // UI: current LFO output (-1..1) and normalised phase (0..1)
    float lfoValue (int section, int lfo) const { return lfos[(size_t) section][(size_t) lfo].out.load(); }
    float lfoPhase (int section, int lfo) const { return lfos[(size_t) section][(size_t) lfo].uiPhase.load(); }
    // UI: current modulation offset (normalised) applied to a parameter, 0 if none
    float offsetFor (const juce::String& id) const;

    static float shapeValue (int shape, double phase01, float randCur, float randNext);

private:
    struct Entry
    {
        juce::RangedAudioParameter* param = nullptr;
        std::atomic<float>* raw = nullptr;
        std::atomic<float> shadow { 0.0f };
        std::atomic<float> offset { 0.0f };
        float acc = 0.0f;
        bool wasModulated = false;
    };
    struct Lfo
    {
        std::atomic<float>* target = nullptr, *shape = nullptr, *sync = nullptr, *div = nullptr, *ms = nullptr, *depth = nullptr, *phaseOff = nullptr;
        double phase = 0.0;                 // running phase in cycles (free mode)
        long long lastCycle = -1;
        float cur = 0.0f, next = 0.0f;      // random values (S&H / smooth random)
        juce::Random rng;
        std::atomic<float> out { 0.0f }, uiPhase { 0.0f };
    };
    std::array<std::vector<std::unique_ptr<Entry>>, 2> entries;   // per section, same order as params::lfoTargets()
    std::map<juce::String, Entry*> byId;
    std::array<std::array<Lfo, params::numLfos>, 2> lfos;
    std::array<std::atomic<float>*, 2> count {};
    juce::AudioProcessorValueTreeState* state = nullptr;
};
