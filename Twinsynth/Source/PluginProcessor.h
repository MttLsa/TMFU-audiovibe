#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/SynthEngine.h"
#include "dsp/Effects.h"

class TwinsynthProcessor : public juce::AudioProcessor
{
public:
    TwinsynthProcessor();
    ~TwinsynthProcessor() override = default;

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

    // host programs = factory presets
    int getNumPrograms() override { return juce::jmax (1, (int) PresetLibrary::get().all().size()); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { loadPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);          // message thread
    int  presetIndex() const { return currentPreset.load(); }
    std::function<void()> onPresetChanged;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;
    float meter() const { return outMeter.load(); }

private:
    std::array<std::atomic<float>*, params::NUM> raw {};
    float snap[params::NUM] {};
    SynthEngine engine;
    fx::Chorus chorus;
    fx::Delay delay;
    juce::Reverb reverb;
    fx::Limiter limiter;
    juce::AudioBuffer<float> revBuf;
    std::atomic<int> currentPreset { -1 };
    std::atomic<bool> resetPending { false };
    std::atomic<float> outMeter { 0.0f };
    double sr = 48000.0;
    float gainSm = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinsynthProcessor)
};
