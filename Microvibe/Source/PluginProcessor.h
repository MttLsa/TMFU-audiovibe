#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/Engine.h"

class MicrovibeProcessor : public juce::AudioProcessor
{
public:
    MicrovibeProcessor();
    ~MicrovibeProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return juce::jmax (1, (int) PresetLibrary::get().all().size()); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { loadPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- message thread API (editor)
    void loadPreset (int index);
    int presetIndex() const { return currentPreset.load(); }
    std::function<void()> onPresetChanged;

    int assignModulation (int source, int paramIndex);     // returns the matrix slot used, -1 if full
    void clearModulation (int slot);
    void setParam (int index, float realValue);
    float getParam (int index) const { return raw[(size_t) index]->load(); }

    bool loadScale (const juce::File& scl, const juce::File& kbm, juce::String& error);
    juce::String scaleName() const { return userSclText.isEmpty() ? juce::String() : engine.tuning.userName(); }

    void randomizeLane (int pattern, int lane);
    void clearLane (int pattern, int lane);
    void copyPattern (int from, int to);

    juce::AudioProcessorValueTreeState apvts;
    mv::Engine engine;
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

private:
    void resetSharedData();
    juce::ValueTree sharedToTree() const;
    void sharedFromTree (const juce::ValueTree&);

    std::vector<std::atomic<float>*> raw;
    std::vector<float> base;
    std::atomic<int> currentPreset { -1 };
    juce::String userSclText, userKbmText;
    double sr = 48000.0;
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicrovibeProcessor)
};
