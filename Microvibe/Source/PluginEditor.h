#pragma once
#include "PluginProcessor.h"
class MicrovibeEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit MicrovibeEditor (MicrovibeProcessor& p) : juce::GenericAudioProcessorEditor (p) {}
    void showTab (const juce::String&) {}
};
