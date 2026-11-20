#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "NormalizerEngine.h"

class AutoNormalizerProcessor : public juce::AudioProcessor
{
public:
    AutoNormalizerProcessor();
    ~AutoNormalizerProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.0015; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    an::NormalizerEngine engine;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    an::Settings readSettings() const;

    std::atomic<float>* pTarget = nullptr;
    std::atomic<float>* pMeasure = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pAuto = nullptr;
    std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pMidiPC = nullptr;
    std::atomic<float>* pMaxBoost = nullptr;
    std::atomic<float>* pSafety = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pTrigger = nullptr;
    bool lastTriggerParam = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoNormalizerProcessor)
};
