#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "DSP/Utils.h"
#include "DSP/Circuits.h"
#include "DSP/ZdfFilter.h"
#include "DSP/ShelfEq.h"
#include "DSP/Modulators.h"

namespace pid
{
inline constexpr const char* input      = "input";
inline constexpr const char* drive      = "drive";
inline constexpr const char* circuit    = "circuit";
inline constexpr const char* filterOn   = "filterOn";
inline constexpr const char* filterType = "filterType";
inline constexpr const char* cutoff     = "cutoff";
inline constexpr const char* reso       = "reso";
inline constexpr const char* lowFreq    = "lowFreq";
inline constexpr const char* lowGain    = "lowGain";
inline constexpr const char* highFreq   = "highFreq";
inline constexpr const char* highGain   = "highGain";
inline constexpr const char* mix        = "mix";
inline constexpr const char* output     = "output";
inline constexpr const char* lfoShape   = "lfoShape";
inline constexpr const char* lfoRate    = "lfoRate";
inline constexpr const char* lfoSync    = "lfoSync";
inline constexpr const char* lfoDiv     = "lfoDiv";
inline constexpr const char* lfoDepth   = "lfoDepth";
inline constexpr const char* lfoTarget  = "lfoTarget";
inline constexpr const char* envMode    = "envMode";
inline constexpr const char* envAttack  = "envAttack";
inline constexpr const char* envRelease = "envRelease";
inline constexpr const char* envThresh  = "envThresh";
inline constexpr const char* envDepth   = "envDepth";
inline constexpr const char* envTarget  = "envTarget";

// modulation destinations (index 0 = none)
enum Target
{
    tNone = 0,
    tInput,
    tDrive,
    tCutoff,
    tReso,
    tLowFreq,
    tLowGain,
    tHighFreq,
    tHighGain,
    tMix,
    tOutput,
    tLfoRate,
    tLfoDepth,
    tEnvDepth,
    numTargets
};

const char* targetParamId (int t);
juce::StringArray circuitNames();
juce::StringArray filterNames();
juce::StringArray lfoShapeNames();
juce::StringArray lfoDivNames();
juce::StringArray envModeNames();
juce::StringArray targetNames();
double lfoDivBeats (int index);
} // namespace pid

class HeatsinkProcessor : public juce::AudioProcessor
{
public:
    HeatsinkProcessor();
    ~HeatsinkProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> lfoMeter { 0.0f }, envMeter { 0.0f };

private:
    static constexpr int osFactorLog2 = 2;
    static constexpr int osFactor = 1 << osFactorLog2;
    static constexpr int controlStep = 16;

    struct HostTime
    {
        double bpm = 120.0, ppq = 0.0;
        bool playing = false, hasPpq = false;
    };

    void processChunk (juce::AudioBuffer<float>&, int numCh, int start, int len, const HostTime&);
    float modulated (int target, float base, int lfoT, int envT, float lfoAmt, float envAmt) const noexcept;

    std::atomic<float>* param (const char* id) const { return apvts.getRawParameterValue (id); }

    std::atomic<float> *pInput, *pDrive, *pCircuit, *pFilterOn, *pFilterType, *pCutoff, *pReso, *pLowFreq, *pLowGain,
        *pHighFreq, *pHighGain, *pMix, *pOutput, *pLfoShape, *pLfoRate, *pLfoSync, *pLfoDiv, *pLfoDepth, *pLfoTarget,
        *pEnvMode, *pEnvAttack, *pEnvRelease, *pEnvThresh, *pEnvDepth, *pEnvTarget;
    std::array<juce::NormalisableRange<float>, pid::numTargets> modRange;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    hs::DriveStage driver;
    hs::MultiFilter filter;
    hs::ShelfEq eq;
    hs::Lfo lfo;
    hs::EnvMod env;

    hs::Smoother smIn, smDrive, smCutLog, smReso, smMix, smOut, smFOn;    // per sample
    hs::Smoother smLowF, smLowG, smHighF, smHighG;                        // per control step
    std::vector<float> aIn, aDrive, aCut, aReso, aMix, aOut, aFOn;
    std::vector<float> segLowF, segLowG, segHighF, segHighG;

    juce::AudioBuffer<float> dryBuf, dryLine;
    int dryWrite = 0, latency = 0, maxBlock = 0;
    double fs = 44100.0;
    float lastLfo = 0.0f, lastEnv = 0.0f;
    std::atomic<int> currentProgram { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatsinkProcessor)
};
