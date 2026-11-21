#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/SynthEngine.h"
#include "dsp/Effects.h"

class FivesynthProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    FivesynthProcessor();
    ~FivesynthProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return juce::jmax (1, (int) PresetLibrary::get().all().size()); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { loadPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);                       // message thread
    void loadModelPatch (int module, int modelIndex);  // the "change synth" action of a module
    int  presetIndex() const { return currentPreset.load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;
    float meter() const { return outMeter.load(); }
    float prePeak() const { return peakPre.load(); }   // pre-limiter peak (calibration)
    void resetPrePeak() { peakPre.store (0.0f); }
    float lfoValue (int l) const { return lfoVals[(size_t) l].load(); }
    float moduleMeter (int m) const { return engine.moduleLevel (m); }
    double sampleRate() const { return sr; }
    juce::ValueTree uiState { "UI" };

private:
    void parameterChanged (const juce::String& id, float v) override;
    void handleAsyncUpdate() override;
    void setParam (int index, float realValue);
    void applyModuleValues (int slot, const PresetModule& pm);
    void renderChunk (float* L, float* R, int n, const juce::MidiBuffer& midi, double bpm, double ppq, bool playing);

    std::vector<std::atomic<float>*> raw;
    std::vector<juce::RangedAudioParameter*> prm;
    std::vector<float> snap;
    SynthEngine engine;
    fx::Chorus chorus;
    juce::Reverb reverb;
    fx::Limiter limiter;
    dsp::Biquad eq[params::numEqBands][2];
    float eqKey[params::numEqBands][5] {};
    dsp::Svf outF[3][2];
    juce::AudioBuffer<float> revBuf;
    juce::MidiBuffer chunkMidi;
    std::atomic<int> currentPreset { -1 };
    std::atomic<bool> resetPending { false }, loading { false };
    std::atomic<int> pendingModel { 0 };
    std::atomic<float> outMeter { 0.0f }, peakPre { 0.0f };
    std::array<std::atomic<float>, params::numLfos> lfoVals {};
    double lfoPhase[params::numLfos] {};
    float lfoSh[params::numLfos] {}, lfoRnd[params::numLfos][2] {};
    juce::Random rnd;
    double sr = 48000.0, internalPpq = 0;
    float gainSm = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FivesynthProcessor)
};
